#ifndef ACTORS_SHARED_80132724_H
#define ACTORS_SHARED_80132724_H

#include "common.h"

/// Payload this body passes as `Gp_DispatchMsg`'s `arg2` for message 0x7DA:
/// the same four bytes as `Actor444000Msg7DA`, two id bytes followed by a
/// halfword the 0x7DB receiver switches on.
typedef struct ActorsShared80132724Msg {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} ActorsShared80132724Msg;
STATIC_ASSERT_SIZEOF(ActorsShared80132724Msg, 0x4);

/// Sends message 0x7DA to the slot-4 task, tagged with the current session's
/// two id bytes and the caller's selector.
void ActorsShared80132724(s16 arg0);

#endif
