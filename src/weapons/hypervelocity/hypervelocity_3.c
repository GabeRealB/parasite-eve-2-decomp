#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/hypervelocity.h"

void func_hypervelocity_8011F374(Task* arg0);
void func_hypervelocity_8011F570(Task* arg0);
void func_hypervelocity_8011F694(Task* arg0);
void WeaponsShared8011db78(Task* arg0);

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_hypervelocity_8011F6C0(Task* arg0)
{
    TaskFunc states[4] = {
        func_hypervelocity_8011F570,
        func_hypervelocity_8011F374,
        func_hypervelocity_8011F694,
        WeaponsShared8011db78,
    };

    states[arg0->state](arg0);
}

/// Per-frame state machine for the hypervelocity's charge-up shot. Case 0 arms
/// the charge: it resets the weapon slots, wakes the muzzle-glow task
/// (`field_914`), sets the charge bit on the barrel effect task (`field_91C`)
/// and starts the wind-up animation. Case 1 runs the charge while the fire
/// button is still held (`field_962 & 0xA`): the charge ticks up, crosses a
/// half-way mark at 60 that adds the second glow stage, and completes at 90 by
/// consuming a round and firing. Releasing the button early jumps straight to
/// case 3 and cancels both loops. Case 2 is the 0x15-tick recoil: for the last
/// 18 ticks the third column of the weapon coordinate is scaled by the
/// remaining ticks over 378 (or 244 on the first tick) and subtracted from the
/// coordinate's translation, kicking the gun back along its own barrel.
void func_hypervelocity_8011F724(GpActorWork* arg0)
{
    void**         scratch;
    u8*            head;
    HyperRecoil*   rec;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    Task*          eff;
    s32            div;
    s32            count;
    s32            step;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    rec      = (HyperRecoil*)(head - 0x18);
    *scratch = rec;
    actor    = arg0->actor;
    eff      = actor->field_91C;
    switch (actor->field_95E) {
        case 0:
            actor->field_954            = 0;
            actor->field_956            = 4;
            actor->field_958            = 0;
            actor->field_95A            = 0;
            actor->field_95C            = 0;
            actor->field_95E            = 1;
            actor->field_914->spawnArg1 = 1;
            actor->field_934            = 0;
            eff->spawnArg1             |= 0x10;
            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20160003, 0);
            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20160005, 0);
            Gp_AnimPlayChildSlotsEx(arg0, 0xE, 0, 3);
            /* fallthrough */
        case 1:
            if (actor->field_962 & 0xA) {
                count            = actor->field_934 + 1;
                actor->field_934 = count;
                if (count >= 0x5A) {
                    actor->field_981 = 0;
                    actor->field_95E++;
                    eff->spawnArg1   = 0;
                    actor->field_934 = 0x15;
                    Gp_ConsumeSlotQty(0x95, 1);
                    SndEvt_EnqueueType7(0x20160005, 1);
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20160007, 1);
                    Gp_AnimResetChildSlots(arg0, 0xB);
                } else if (count == 0x3C) {
                    eff->spawnArg1 |= 0x20;
                    SndEvt_EnqueueType7(0x20160003, 1);
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20160002, 0);
                }
                SndEvt_EnqueueType7(0x20160004, 1);
            } else {
                actor->field_95E            = 3;
                actor->field_914->spawnArg1 = -1;
                eff->spawnArg1              = 0;
                SndEvt_EnqueueType7(0x20160003, 1);
                SndEvt_EnqueueType7(0x20160005, 1);
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20160004, 0);
                Gp_AnimPlayChildSlotsEx(arg0, 0xF, 0, 3);
            }
            break;
        case 2:
            step             = actor->field_934 - 1;
            actor->field_934 = step;
            if (step != 0) {
                if (step < 0x13) {
                    coord = (GsCOORDINATE2*)arg0->extra->field_8;
                    div   = 0x17A;
                    if (step == 0x12) {
                        div = 0xF4;
                    }
                    actor->field_973 = -1;
                    Gfx_MatrixCol2(&coord->coord, (SVECTOR*)(head - 8));
                    rec->vx            = -(rec->dir.vx * actor->field_934 / div);
                    rec->vy            = -(rec->dir.vy * actor->field_934 / div);
                    rec->vz            = -(rec->dir.vz * actor->field_934 / div);
                    coord->coord.t[0] += rec->vx;
                    coord->coord.t[1] += rec->vy;
                    coord->coord.t[2] += rec->vz;
                }
            } else {
                actor->field_95E++;
            }
            /* fallthrough */
        case 3:
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x18;
}
