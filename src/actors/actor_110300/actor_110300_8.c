#include "common.h"

#include "actors/actor_110300.h"
#include "gameplay/1BC.h"

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x13 of the work block's animation context from
/// the current animation id through `func_800B4114` (arguments 0 and 8), then
/// records that id as the one now playing.
void func_actor_110300_80132208(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_110300_8013A0A0->anim, i, (s16)D_actor_110300_8013A0A0->animId, 0, 8);
        i++;
    } while (i < 0x14);
    D_actor_110300_8013A0A0->field_476 = D_actor_110300_8013A0A0->animId;
}
