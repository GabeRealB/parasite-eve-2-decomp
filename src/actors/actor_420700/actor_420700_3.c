#include "common.h"
#include "main/task.h"
#include "actors/actor_420700.h"
#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_420700_801325C8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, ActorsShared80131f9cWork->field_4B8, 0, 8);
        i++;
    } while (i < 0x14);
    ActorsShared80131f9cWork->field_4B6 = ActorsShared80131f9cWork->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_3", func_actor_420700_80132644);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_3", func_actor_420700_801326F4);

/// Message 0x7DB handler: records the `field_4BA` mode the ramp
/// `ActorsShared80131f9cSub1` runs and seeds `field_4BC` at the end that mode
/// walks away from -- 0 for the rising modes 1 and 3, 0x1000 for the falling
/// mode 2. Mode 0 is accepted as a no-op, and a block whose leading id is not
/// 0x1B02 is rejected with -1 without touching the work block.
///
/// The empty `case 0` is what the decision tree is built from: with the three
/// live cases alone GCC balances the list at the middle node and comes out one
/// test short, and adding the fourth node is what makes it split at the first
/// case instead. See DECOMPILATION_LEARNINGS.md, "An empty case node changes
/// the switch decision tree".
s32 func_actor_420700_80132784(Task* task, s32 arg1, Actor420700ModeArgs* args)
{
    if (args->id != 0x1B02) {
        return -1;
    }
    ActorsShared80131f9cWork->field_4BA = args->mode;
    switch (args->mode) {
        case 0:
            break;
        case 1:
        case 3:
            ActorsShared80131f9cWork->field_4BC = 0;
            break;
        case 2:
            ActorsShared80131f9cWork->field_4BC = 0x1000;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_3", func_actor_420700_801327EC);
