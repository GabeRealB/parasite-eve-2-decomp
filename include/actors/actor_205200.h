#ifndef ACTOR_205200_H
#define ACTOR_205200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// its halfword at 0x2 is the only part the handlers read. Senders seed it
/// from a `Task`'s `spawnArg1` halfword -- `Gp_DispatchMsg` in `3CD8.c` sends
/// `D_801155A0`.
typedef struct Actor205200Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor205200Msg7DB;
STATIC_ASSERT_SIZEOF(Actor205200Msg7DB, 0x4);

#endif
