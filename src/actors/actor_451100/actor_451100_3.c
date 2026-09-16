#include "common.h"

#include "actors/actor_451100.h"
#include "actors/actors_shared_801326b4.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reset argument this overlay forwards to every reseeded slot.
extern s16 D_actor_451100_8013F700;

/// Reseeds animation slots 1..0x12 from the current animation id and records
/// that id as the one now playing.
void func_actor_451100_801324B8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, (s16)ActorsShared80131f9cWork->animId, 0,
                      D_actor_451100_8013F700);
        i++;
    } while (i < 0x13);
    ActorsShared80131f9cWork->field_47E = ActorsShared80131f9cWork->animId;
}

/// Script opcode: start animation `args->animId` on this actor through
/// `func_actor_451100_80131F84` rather than `func_actor_451100_80132A1C`.
///
/// The same argument block and the same two-way `withArg` start as
/// `func_actor_451100_80132E98`; only the accepted id range (0x25 instead of
/// 0x12) and the run entry point differ. `animArg` goes to the overlay's reset
/// word instead of the work block's own slot.
s32 func_actor_451100_80132538(Task* task, s32 arg1, Actor451100AnimArgs* args)
{
    if (args->animId < 0x25) {
        ActorsShared80131f9cWork->animId = args->animId;
        if (args->withArg != 0) {
            ActorsShared80131f9cWork->state = 1;
            D_actor_451100_8013F700         = args->animArg;
        } else {
            ActorsShared80131f9cWork->state = 2;
        }
        ActorsShared80131f9cWork->field_482 = 0;
        func_actor_451100_80131F84(ActorsShared801326b4Task);
        return 0;
    }
    return -1;
}
