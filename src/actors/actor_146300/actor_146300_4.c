#include "common.h"

#include "actors/actor_146300.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x13 of the work block from the current animation
/// id with the latched reset argument `D_actor_146300_8014279C`, and records
/// that id as the one now playing.
void func_actor_146300_8013291C(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_146300_80142828->anim, i, D_actor_146300_80142828->field_4B8, 0,
                      D_actor_146300_8014279C);
        i++;
    } while (i < 0x14);
    D_actor_146300_80142828->field_4B6 = D_actor_146300_80142828->field_4B8;
}

/// Play-animation message handler: adopts `preset`'s animation id when it is
/// one of the first 0x11, latching the reset mode and the reset argument the
/// reseed forwards, then hands the published task to the per-frame update. Ids
/// past the range are rejected with -1 and leave the work block untouched.
s32 func_actor_146300_8013299C(Task* task, s32 arg1, Actor146300AnimPreset* preset)
{
    if (preset->field_4 < 0x11) {
        D_actor_146300_80142828->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_146300_80142828->field_4B4 = 1;
            D_actor_146300_8014279C            = preset->field_C;
        } else {
            D_actor_146300_80142828->field_4B4 = 2;
        }
        D_actor_146300_80142828->field_4BA = 0;
        func_actor_146300_801327CC(D_actor_146300_8014282C);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: shows or hides the actor's model and its companion's
/// together. Bit 0 of `flags` clears both models' `TmdObject::flags` (shown);
/// without it both get 0x80 (hidden). Bit 1 additionally ORs in 0x4 on both.
s32 func_actor_146300_80132A2C(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = D_actor_146300_8014282C->extra;
    other = (TmdObject*)D_actor_146300_80142830->extra;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}
