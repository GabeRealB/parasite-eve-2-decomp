#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/tonfa_baton.h"

void func_tonfa_baton_8011DA48(Task* arg0);
void func_tonfa_baton_8011DA74(Task* arg0);
void func_tonfa_baton_8011DB6C(Task* arg0);
void WeaponsShared8011db78(Task* arg0);

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_tonfa_baton_8011DB98(Task* arg0)
{
    TaskFunc states[4] = {
        func_tonfa_baton_8011DA48,
        func_tonfa_baton_8011DA74,
        func_tonfa_baton_8011DB6C,
        WeaponsShared8011db78,
    };

    states[arg0->state](arg0);
}

/// Per-frame swing state machine for the tonfa baton, and the only weapon here
/// that moves the player: while `field_973` is set the third column of the
/// weapon coordinate is scaled by 1/84 and added to the coordinate's
/// translation, which is what carries the lunge. Case 0 arms the swing (8-tick
/// wind-up) and queues the ready animation. Cases 1 and 2 run the wind-up: on
/// the tick it expires the weapon becomes solid, the swing report plays and the
/// trail effect is parented to the weapon task; pressing again during the
/// window (`field_966 & 0xA`) upgrades to the second swing, which case 2 turns
/// into the follow-through, otherwise the state falls back to the 10-tick
/// recovery of case 5. Case 3 is the follow-through: it re-arms the hitbox
/// three ticks in and parks in case 4, whose 9 ticks clear the hit flag again.
/// Cases 1/2 and 4 also play the connect sound once per swing when
/// `Gp_CountRec18Hi` reports a hit.
void func_tonfa_baton_8011DBFC(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    TonfaSwing*    swing;
    GpEffWork*     eff;
    s32            delay;
    s32            step;
    s32            fade;
    s32            swinging;

    swinging              = 0;
    actor                 = arg0->actor;
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD - 0x18;
    swing                 = (TonfaSwing*)*(u8**)G_SCRATCH_HEAD;
    switch (actor->field_95E) {
        case 0:
            actor->field_956 = 4;
            actor->field_95E = 1;
            actor->field_954 = 0;
            actor->field_958 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_934 = 8;
            actor->field_93E = 0;
            func_80106518(0x13);
            actor->field_124 = 0x21317;
            actor->field_12A = (actor->field_12A & 0xF7FF) | 0x400;
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            break;
        case 1:
        case 2:
            delay = actor->field_934;
            if (delay == 0) {
                actor->field_973 = 1;
                swinging         = 1;
                if (actor->field_966 & 0xA) {
                    actor->field_95E = 2;
                }
            } else {
                delay--;
                actor->field_934 = delay;
                if (delay == 0) {
                    actor->field_91C->spawnArg1 = 1;
                    actor->field_12A           |= 0x8000;
                    func_80106238(arg0, 0, 0);
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20130001, 0);
                    eff = Gp_SpawnEff(0x6003A,
                                      (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                                      0, NULL);
                    if (eff != NULL) {
                        Task_Reparent(actor->field_91C, eff->field_0);
                    }
                }
            }
            if (actor->field_93E != 1 && Gp_CountRec18Hi(actor->field_32C, 0x30000) != 0) {
                actor->field_93E = 1;
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20130003, 0);
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                actor->field_12A &= 0x7FFF;
                if (actor->field_95E == 2) {
                    actor->field_95E = 3;
                    actor->field_934 = 0xC;
                    func_80106518(0x13);
                    actor->field_124 = 0x21315;
                    eff              = Gp_SpawnEff(
                        0x6003A, (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8, 1,
                        NULL);
                    if (eff != NULL) {
                        Task_Reparent(actor->field_91C, eff->field_0);
                    }
                    Gp_AnimResetChildSlots(arg0, 0xB);
                } else {
                    actor->field_95E            = 5;
                    actor->field_934            = 0xA;
                    actor->field_91C->spawnArg1 = 0;
                    Gp_AnimResetChildSlots(arg0, 0xE);
                }
            }
            break;
        case 3:
            if (actor->field_934 != 0) {
                actor->field_973 = 1;
                swinging         = 1;
                step             = actor->field_934 - 1;
                actor->field_934 = step;
                if (step == 3) {
                    actor->field_12A |= 0x8000;
                    func_80106238(arg0, 0, 1);
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20130002, 0);
                } else if (step == 0) {
                    actor->field_95E            = 4;
                    actor->field_934            = 9;
                    actor->field_91C->spawnArg1 = 0;
                }
            }
            /* fallthrough */
        case 4:
            if (actor->field_95E == 4) {
                fade = actor->field_934;
                fade--;
                actor->field_934 = fade;
                if (fade == 0) {
                    actor->field_12A &= 0x7FFF;
                }
            }
            if (actor->field_93E != 2 && Gp_CountRec18Hi(actor->field_32C, 0x30000) != 0) {
                actor->field_93E = 2;
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8, 0x20130004, 0);
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
        case 5:
            if (actor->field_934 != 0) {
                actor->field_973 = 1;
                swinging         = 1;
                actor->field_934 = actor->field_934 - 1;
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    coord = (GsCOORDINATE2*)arg0->extra->field_8;
    Gfx_MatrixCol2(&coord->coord, &swing->dir);
    swing->vx             = (s16)(swing->dir.vx / 84) * swinging;
    swing->vy             = (s16)(swing->dir.vy / 84) * swinging;
    swing->vz             = (s16)(swing->dir.vz / 84) * swinging;
    coord->coord.t[0]    += swing->vx;
    coord->coord.t[1]    += swing->vy;
    coord->coord.t[2]    += swing->vz;
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x18;
}
