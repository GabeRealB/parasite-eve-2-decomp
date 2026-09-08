#ifndef ACTOR_104000_H
#define ACTOR_104000_H

#include "common.h"

/// Payload `func_actor_104000_80138CC8` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA, which the slot-4 task forwards to the 0x7DB handlers.
/// The same four bytes as `Actor444000Msg7DA`: two id bytes followed by a
/// halfword the receiver switches on.
typedef struct Actor104000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor104000Msg7DA;
STATIC_ASSERT_SIZEOF(Actor104000Msg7DA, 0x4);

/// Two counters the overlay clears together with `D_actor_104000_8013E538`
/// when it restarts its run; still assembly everywhere they are written.
extern s32 D_actor_104000_8013E530[2];

/// Six-entry counter table the overlay resets before dispatching its 0x7DA
/// message.
extern s32 D_actor_104000_8013E538[8];

#endif
