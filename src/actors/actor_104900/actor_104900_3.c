#include "common.h"

#include "actors/actors_shared_80138efc.h"
#include "actors/actors_shared_801388e8.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/tmd.h"

extern u32 Gp_LcgState;
extern s32 D_80070F70;

void func_actor_104900_80137498(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);
s32  func_actor_104900_80132D78(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);

/// Points the enemy's link transform at the model's fourth part coordinate -
/// the same `TmdObject::field_8[3]` that `Gp_UpdateLinkXforms` reads back
/// through `GpEnemy.field_18` - and arms the 0xC8-box local offset the actor
/// spawns inside. `GpLinkXform::field_4` clears the node's slot byte.
///
/// The restart path then needs three things at once: the state machine at
/// 0x80132D78 idle, `Task::spawnArg1` clear, and the work block's trigger pair
/// (`field_BC9`, `field_BA9`) both at 1. With them, and only while the squared
/// distance to the player's slot-3 coordinate stays above 0xA62B10, the
/// 0xC000 pair is masked back out of both `GpObj` nodes in the motion block,
/// and one LCG draw picks the next state: 4 for three draws in four, else 0.
/// `field_BA8` is cleared either way, so the sub-state re-arms from the top.
void func_actor_104900_80138A2C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, void* scratch)
{
    GpLinkXform* xform;
    s32          off;
    s32          i;
    u8           trigger;

    xform               = (GpLinkXform*)&enemy->node;
    enemy->node.field_4 = 0;
    xform->coord        = &((TmdObject*)task->extra)->field_8[3];
    xform->src.vx       = 0;
    xform->src.vy       = -0xC8;
    xform->src.vz       = 0xC8;
    if ((func_actor_104900_80132D78(enemy, task, work, scratch) == 0) && (task->spawnArg1 == 0)) {
        trigger = work->field_BC9;
        if ((trigger == 1) && (work->field_BA9 == trigger)) {
            if (ActorsShared801388e8(((TmdObject*)task->extra)->field_8) > 0xA62B10) {
                i   = 0;
                off = 0x9C8;
                do {
                    ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
                    off                                += 0x20;
                    i++;
                } while (i < 2);
                work->field_BA6 = 0;
                Gp_LcgState     = (Gp_LcgState * 5) + 0x71357911;
                if (((Gp_LcgState >> 0x10) & 0xF) < 0xC) {
                    work->state = 4;
                } else {
                    work->state = 0;
                }
                work->field_BA8 = 0;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138B5C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138C6C);

/// First frame of the sub-state arms motion 5 and the 0x64-frame countdown at
/// 0xB8C, then bumps the run-once latch at 0xBA8. Every later frame steps that
/// countdown, and on the frame it reaches zero cues the 0x400B0004 event - the
/// actor's id byte at 0xB88 in bits 8..15, the variant byte at 0xBB8 in bit 22,
/// pan and depth from the frame block - through `SndEvt_EnqueueType6`. The
/// scratch byte at 0x64 then takes 0xC while bit 0 of `D_80070F70` is set and 8
/// otherwise, and the trigger at 0xBA9 ends the sub-state by clearing both the
/// state and the latch.
void func_actor_104900_80138D58(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    if (work->field_BA8 == 0) {
        work->field_BA4 = 5;
        work->field_B8C = 0x64;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        return;
    }
    if (work->field_B8C != 0) {
        work->field_B8C--;
        if (work->field_B8C == 0) {
            SndEvt_EnqueueType6((work->field_BB8 << 22) | ((work->field_B88 << 8) | 0x400B0004), arg->pan, arg->depth);
        }
    }
    if (D_80070F70 & 1) {
        arg->field_64 = 0xC;
    } else {
        arg->field_64 = 8;
    }
    if (work->field_BA9 == 1) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

/// Arms the 0x15 / 0x16 motion pair on the first frame of the sub-state, then
/// runs the sub-state proper: while bit 1 of the motion flags at 0x9C is set,
/// either keeps the state on the 0x17 motion once `field_B92` has run out and
/// the enemy is not carrying flag 0x2 in `field_4C`, or hands the frame to the
/// shared routine at 0x80137498 on the 0x18 motion.
void func_actor_104900_80138E34(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, void* scratch)
{
    ActorsShared80138efcMotion* motion = &work->motion;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0x15;
        } else {
            work->field_BA4 = 0x16;
        }
        work->field_BA5 = 1;
        work->field_B8C = 0xA;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (motion->flags & 2) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->field_4C & 2)) {
                work->field_BA6 = 1;
                work->field_BAB = 0x10;
                work->state     = 0x17;
                work->field_BA8 = 0;
                work->field_BAF = 2;
            }
        } else {
            work->state     = 0x18;
            work->field_BA8 = 0;
            func_actor_104900_80137498(enemy, task, work, scratch);
        }
    }
}
