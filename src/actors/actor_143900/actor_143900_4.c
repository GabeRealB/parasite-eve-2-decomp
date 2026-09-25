#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reset argument this overlay forwards to every reseeded slot.
extern s16 D_actor_143900_801413B8;

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_143900_801325A4(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_143900_801496B8->anim, i, D_actor_143900_801496B8->field_4B8, 0,
                      D_actor_143900_801413B8);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496B8->field_4B6 = D_actor_143900_801496B8->field_4B8;
}

/// Play-animation message handler: adopts `preset`'s animation id when it is
/// one of the first 0x14, latching the reset mode and argument the reseed uses,
/// then hands the published task to the per-frame update. Ids past the range
/// are rejected with -1 and leave the work block untouched.
s32 func_actor_143900_80132624(Task* task, s32 arg1, Actor143900AnimPreset* preset)
{
    if (preset->field_4 < 0x14) {
        D_actor_143900_801496B8->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_143900_801496B8->field_4B4 = 1;
            D_actor_143900_801413B8            = preset->field_C;
        } else {
            D_actor_143900_801496B8->field_4B4 = 2;
        }
        D_actor_143900_801496B8->field_4BA = 0;
        func_actor_143900_80131FD4(D_actor_143900_801496BC);
        return 0;
    }
    return -1;
}
