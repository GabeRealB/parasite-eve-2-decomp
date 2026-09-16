#include "common.h"

#include "actors/actors_shared_80132208.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x13 of the published work block's animation
/// context from the current animation id, then records that id as the one now
/// playing. The two carriers of this body, `actor_110300` and `actor_110800`,
/// reach the same block through the global, so one object serves both.
void ActorsShared80132208(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, (s16)ActorsShared80131f9cWork->animId, 0, 8);
        i++;
    } while (i < 0x14);
    ActorsShared80131f9cWork->field_476 = ActorsShared80131f9cWork->animId;
}
