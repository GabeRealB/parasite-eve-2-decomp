#include "common.h"

#include "actors/actors_shared_80138efc.h"
#include "actors/actors_shared_801388e8.h"
#include "gameplay/gameplay.h"
#include "main/tmd.h"

extern u32 Gp_LcgState;

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

/// Decays the four counters at 0xB94..0xB9A - the axis pair by 0x400, the two
/// after them by 0x100, each clamped at zero once it falls below its step - and
/// walks `field_B8E` 0x30 back toward zero from either end of the +-0x30 band.
/// `field_BAF` gates the walk, `field_BAB` the whole block, which is why the
/// locals read signed for the test and unsigned for the step.
///
/// Either way the link transform is re-armed exactly as `func_actor_104900_80138A2C`
/// arms it - model part 3 through `TmdObject::field_8[3]` as `coord`, the
/// 0xC8-box local offset through `src` - and the state machine at 0x80132D78
/// runs last.
void func_actor_104900_80138B5C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, void* scratch)
{
    GpLinkXform* xform;
    s16          walk;

    if (work->field_BAB != 1) {
        if (work->field_B96 >= 0x400) {
            work->field_B96 = (s16)((u16)work->field_B96 - 0x400);
        } else {
            work->field_B96 = 0;
        }
        if (work->field_B94 >= 0x400) {
            work->field_B94 = (s16)((u16)work->field_B94 - 0x400);
        } else {
            work->field_B94 = 0;
        }
        if (work->field_B98 >= 0x100) {
            work->field_B98 = (s16)((u16)work->field_B98 - 0x100);
        } else {
            work->field_B98 = 0;
        }
        if (work->field_B9A >= 0x100) {
            work->field_B9A = (s16)((u16)work->field_B9A - 0x100);
        } else {
            work->field_B9A = 0;
        }
        if (work->field_BAF != 0) {
            walk = work->field_B8E;
            if (walk >= 0x31) {
                work->field_B8E = (s16)((u16)work->field_B8E - 0x30);
            } else if (walk < -0x30) {
                work->field_B8E = (s16)((u16)work->field_B8E + 0x30);
            }
        }
    }
    xform         = (GpLinkXform*)&enemy->node;
    xform->coord  = &((TmdObject*)task->extra)->field_8[3];
    xform->src.vx = 0;
    xform->src.vy = -0xC8;
    xform->src.vz = 0xC8;
    func_actor_104900_80132D78(enemy, task, work, scratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_80138C6C);
