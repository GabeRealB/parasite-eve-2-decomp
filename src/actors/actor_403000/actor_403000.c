#include "common.h"

#include "actors/actor_403000.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "psyq/inline_c.h"

/// `gpf 12`. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132348);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", D_actor_403000_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801324EC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801327B0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801330D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801332E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133444);

void func_actor_403000_801336B4(Actor403000* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor403000AnimWork* work;

    work   = (Actor403000AnimWork*)arg0->field_1C;
    weight = work->field_AD4;
    anim   = &work->anim;
    for (i = 1; i < 0x18; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_AD2;
            work->slots[i].field_9      = (u8)(work->field_ACA - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_ACA - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801337E0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133AF8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133FC0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134204);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801343B8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134910);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134E00);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134F44);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80135F08);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013603C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801365D0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136B14);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136D68);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80137084);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801377C8);

/// Per-frame push: on the frame `field_4` is set, turn the display object's
/// first matrix column into a short push vector and play the enemy's sound,
/// then send it to the player as message 0x3FE for the first 0x28 frames.
/// Bit 0 of `field_60` moves the state machine to 4 and flips `field_FD3`.
void func_actor_403000_801384E8(Actor403000* arg0)
{
    Actor403000Work*        work;
    GpEnemy*                enemy;
    Task*                   player;
    Actor403000PushScratch* scratch;
    s32                     sound;
    s32                     pan;
    s32                     ret;

    work                                      = arg0->field_1C;
    player                                    = Game_GetPtrSlot(3);
    scratch                                   = *(Actor403000PushScratch**)G_SCRATCH_HEAD - 1;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD = scratch;
    if (work->field_4 != 0) {
        enemy         = arg0->field_20;
        work->field_6 = 0;
        Gfx_MatrixCol0(&arg0->field_2C->field_8->coord, &scratch->dir);
        VectorNormalSS(&scratch->dir, &scratch->dir);
        gte_lddp(0x55);
        gte_ldsv(&scratch->dir);
        gte_gpf12_real();
        gte_stsv(&scratch->dir);
        D_actor_403000_80158DB0.x        = scratch->dir.vx;
        D_actor_403000_80158DB0.y        = 0;
        D_actor_403000_80158DB0.z        = scratch->dir.vz;
        D_actor_403000_80158DB0.field_10 = 7;
        D_actor_403000_80158DB0.field_12 = 1;
        sound                            = ((enemy->field_8 >> 0xC) << 8) | 7;
        pan                              = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
    if ((s16)work->field_6 < 0x28) {
        ret = Gp_DispatchMsg(player, 0x3FE, (s32)&D_actor_403000_80158DB0, 0);
        if (ret == 1) {
            D_actor_403000_80158DB0.x        = 0;
            D_actor_403000_80158DB0.y        = 0;
            D_actor_403000_80158DB0.z        = 0;
            D_actor_403000_80158DB0.field_10 = 7;
            D_actor_403000_80158DB0.field_12 = ret;
        }
    }
    if (work->field_60.half & 1) {
        work->field_0   = 4;
        work->field_FD3 = work->field_FD5 = work->field_FD2 = -work->field_FD3;
    }
    func_actor_403000_80133AF8(arg0);
    work->field_6++;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801386E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80138DB0);

void func_actor_403000_801399A0(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C   = 0;
        work->objB50.obj.field_1C = 0x3E8;
        work->field_AC0           = 1;
        work->field_AC6           = 0xE;
        work->field_ACA           = 0x10;
        work->field_FCA           = 0;
        work->field_AE0           = 0;
        work->field_AD8           = 0;
        work->field_AD6           = 0;
        work->objD18.obj.flags   |= 0x4000;
        work->objB50.obj.flags   |= 0x4000;
    }
    if (func_actor_403000_80132348(arg0->field_2C->field_8, work->objD18.rec, 5) == 0) {
        func_actor_403000_80132348(arg0->field_2C->field_8, work->objB50.rec, 5);
    }
    func_actor_403000_80133AF8(arg0);
    if ((work->field_60.half & 0x100) && work->field_AC6 == 0xE) {
        work->objB50.obj.flags &= 0xBFFF;
        if (enemy->field_40 > 0) {
            if (enemy->field_4C & 2) {
                work->field_0 = 0x10;
            } else {
                work->field_0 = 0x12;
            }
        } else {
            work->field_F8C = 1;
            work->field_0   = 0x14;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80139AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A08C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A678);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013ACBC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B238);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B74C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013BDE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C050);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C2D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C864);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", ActorsShared80135df4Table);

void func_actor_403000_8013D260(void)
{
}
