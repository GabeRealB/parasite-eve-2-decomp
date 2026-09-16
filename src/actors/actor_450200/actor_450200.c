#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
extern TaskDesc D_actor_450200_8013FB40;

extern void func_8017FA98(s32);

extern s32      D_actor_450200_80138870;
extern s32      D_actor_450200_80138A68;
extern s32      D_actor_450200_80138C60;
extern s32      D_actor_450200_80138E88;
extern s32      D_actor_450200_80139098;
extern TaskDesc D_actor_450200_80137A60;
extern Task*    D_actor_450200_801401E0;
extern Task*    D_actor_450200_801401E4;

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200", func_actor_450200_80131E24);

/// Head-aim record `func_actor_450200_80131FA8` allocates and parks in
/// `Task::idMap`, handed straight to `func_800B17D4` as its `arg2`: the yaw
/// and pitch clamps that function widens against the head's current pose, and
/// the `rate` fraction of the remaining angle this overlay ramps by 0x200 a
/// frame. The twin of `Actor361100HeadAim`, and the same 12-byte reading of
/// `GpHeadAim` `func_mine_mesa_8017E15C` also builds; gameplay's 10 is the
/// most `func_800B17D4` alone can see of it. `rate` is `u16` because the body
/// reads it unsigned and reinterprets the stored value as `s16` for the
/// clamp, which is what the ROM's `lhu` / `sll` / `sra` sequence says.
typedef struct Actor450200HeadAim {
    /* 0x0 */ s16  yawLimit;
    /* 0x2 */ s16  pitchLimit;
    /* 0x4 */ u16  rate;
    /* 0x6 */ s16  lastPitch;
    /* 0x8 */ s8   inited;
    /* 0x9 */ byte pad_9[0x3];
} Actor450200HeadAim;
STATIC_ASSERT_SIZEOF(Actor450200HeadAim, 0xC);

/// Head-aim state of this actor's second sub-task: state 0 allocates the
/// `Actor450200HeadAim` record into `Task::idMap` and seeds both clamps to
/// 0x100, state 1 ramps its `rate` up toward 0x1000 while `Task::spawnArg1` is
/// set and back down toward 0 while it is not, then hands the record to
/// `func_800B17D4` between the slot-3 task whose head turns and the
/// `Game_GetPtrSlot(0xA)` task it turns toward. A failed allocation, and every
/// state past 1, kill the task; only the latter clears
/// `D_actor_450200_801401E0`, which is why the two `Task_Kill` calls are
/// distinct.
void func_actor_450200_80131FA8(Task* arg0)
{
    Task*               looker;
    Actor450200HeadAim* aim;
    u16                 rate;

    looker = Game_GetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            aim = Mem_Calloc(sizeof(Actor450200HeadAim), false);
            if (aim == NULL) {
                Task_Kill(arg0);
                return;
            }
            arg0->idMap     = (TaskIdMap*)aim;
            aim->yawLimit   = 0x100;
            aim->pitchLimit = 0x100;
            arg0->state++;
            /* fallthrough */
        case 1:
            aim = (Actor450200HeadAim*)arg0->idMap;
            if (arg0->spawnArg1 != 0) {
                rate      = aim->rate + 0x200;
                aim->rate = rate;
                if ((s16)rate >= 0x1001) {
                    aim->rate = 0x1000;
                }
            } else {
                rate      = aim->rate - 0x100;
                aim->rate = rate;
                if ((s16)rate < 0) {
                    aim->rate = 0;
                }
            }
            func_800B17D4(looker, Game_GetPtrSlot(0xA), (GpHeadAim*)aim);
            return;
        default:
            Task_Kill(arg0);
            D_actor_450200_801401E0 = NULL;
            return;
    }
}

/// Three-way control for the second spawned sub-task: 0 tears the live one
/// down, 1 spawns it fresh, anything else is a state write the sub-task sees.
/// Spawning is skipped when the sub-task is already running.
void func_actor_450200_801320D4(s32 arg0)
{
    if (arg0 == 0) {
        if (D_actor_450200_801401E4 != NULL) {
            Task_Kill(D_actor_450200_801401E4);
            D_actor_450200_801401E4 = NULL;
        }
    } else if (arg0 == 1) {
        D_actor_450200_801401E4 = Task_SpawnFromTable(&D_actor_450200_80137A60, 1, 0, 0);
    } else if (D_actor_450200_801401E4 != NULL) {
        D_actor_450200_801401E4->state = arg0;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_450200/actor_450200", D_actor_450200_80131E20);
