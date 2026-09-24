#ifndef ROOMS_DRYFIELD_BREEZEWAY_H
#define ROOMS_DRYFIELD_BREEZEWAY_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/task.h"

#include "rooms/room_common.h"

/// 4-byte payload this room sends as `Gp_DispatchMsg`'s `arg2` for message
/// 0x7DA, which the slot-4 task forwards to the 0x7DB handlers tagged with the
/// action taken. `field_0` / `field_1` are the session's two id bytes and
/// `field_2` the halfword the receiver switches on: plain 2 from
/// `func_dryfield_breezeway_8017E2D4` and the tail of
/// `func_dryfield_breezeway_8017E390`. Same four bytes as
/// `ActorsShared80132724Msg` and `AcropolisBridgeMsg7DA`.
typedef struct DbwMsg7DA {
    /* 0x0 */ u8  field_0; // GameSession::at4.loc.stage
    /* 0x1 */ u8  field_1; // GameSession::at4.loc.area
    /* 0x2 */ s16 field_2;
} DbwMsg7DA;
STATIC_ASSERT_SIZEOF(DbwMsg7DA, 0x4);

#endif // ROOMS_DRYFIELD_BREEZEWAY_H
