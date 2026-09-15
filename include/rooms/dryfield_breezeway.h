#ifndef ROOMS_DRYFIELD_BREEZEWAY_H
#define ROOMS_DRYFIELD_BREEZEWAY_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// 0x14 work block the breezeway's room task hangs off the `Task::idMap` slot
/// (0x1C) -- that slot is *not* a `TaskIdMap` here. Reach it with
/// `(DbwWork*)task->idMap`.
///
/// `func_dryfield_breezeway_8017E010` (and its twin
/// `func_dryfield_breezeway_8017E114`) allocates the block
/// (`Mem_Malloc(0x14, 0)`), fills the three leading pointers and publishes the
/// owning task in `D_dryfield_breezeway_801843C0`: the slot-3 game pointer
/// (`Game_GetPtrSlot(3)`), then `field_0` of the work `Gp_FindWorkById` finds
/// for the id formed from `Game_Session` bytes 6/7 and for that id OR'd with
/// 0x1000. `field_8` is the dispatch slot
/// `func_dryfield_breezeway_8017E390` hands to `Gp_DispatchMsg`.
///
/// `field_C` and `field_E` are the two shorts the breezeway hotspot action
/// `func_dryfield_breezeway_8017E370` writes, which the room's data table calls
/// with 1 or 2.
typedef struct DbwWork {
    /* 0x00 */ void* field_0;
    /* 0x04 */ void* field_4;
    /* 0x08 */ void* field_8;
    /* 0x0C */ s16   field_C;
    /* 0x0E */ s16   field_E;
    /* 0x10 */ byte  pad_10[0x4];
} DbwWork;
STATIC_ASSERT_SIZEOF(DbwWork, 0x14);

/// 0x60 work block of the second task family in this room, also hung off
/// `Task::idMap` (0x1C): `func_dryfield_breezeway_8017E464` allocates it with
/// `Mem_Calloc(0x60, 0)` and parks the family's `GpMsgEntry[]`
/// (`D_dryfield_breezeway_80182DCC`, a single 0x13F1 entry) in
/// `Task::field_24`, which is what makes `Gp_DispatchMsg` route messages into
/// this family at all. Reach the block with `(DbwEventWork*)task->idMap`.
///
/// `field_40` is the answer latch the message handler
/// `func_dryfield_breezeway_8017FBC8` sets: message 0x13F1 is the "can this key
/// item be used here?" query `Gp_UseKeyItemRow` sends to slot 7, carrying the
/// highlighted item as its payload, and the handler stores 1 exactly when that
/// payload is 0x11B, the one item this room accepts. The block starts out
/// zeroed by the allocator and by `func_dryfield_breezeway_8017E464` itself;
/// `func_dryfield_breezeway_8017FE08` reads the latch back and picks state 6
/// when it is 1 and state 2 otherwise.
typedef struct DbwEventWork {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ s32  field_40;
    /* 0x44 */ byte pad_44[0x1C];
} DbwEventWork;
STATIC_ASSERT_SIZEOF(DbwEventWork, 0x60);

/// 4-byte payload this room sends as `Gp_DispatchMsg`'s `arg2` for message
/// 0x7DA, which the slot-4 task forwards to the 0x7DB handlers tagged with the
/// action taken. `field_0` / `field_1` are the session's two id bytes and
/// `field_2` the halfword the receiver switches on: plain 2 from
/// `func_dryfield_breezeway_8017E2D4` and the tail of
/// `func_dryfield_breezeway_8017E390`. Same four bytes as
/// `ActorsShared80132724Msg` and `AcropolisBridgeMsg7DA`.
typedef struct DbwMsg7DA {
    /* 0x0 */ u8  field_0; // GameSession::field_7
    /* 0x1 */ u8  field_1; // GameSession::field_6
    /* 0x2 */ s16 field_2;
} DbwMsg7DA;
STATIC_ASSERT_SIZEOF(DbwMsg7DA, 0x4);

/// 0x18-byte placement record this room sends as `Gp_DispatchMsg`'s `arg2` for
/// message 0x7D4, the reset that drops `pos` into the receiving display object's
/// coordinate frame -- the actor side's handler of that message reads only the
/// yaw back out (`rot.vy`). Same shape as the shared
/// `ActorShared8013411cPlacement`, and the same shape the cafeteria's 0x7D4
/// payload has. `D_dryfield_breezeway_80181E28` is the first of a
/// three-record run in the room's data blob, stride 0x18.
typedef struct DbwPlacement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} DbwPlacement;
STATIC_ASSERT_SIZEOF(DbwPlacement, 0x18);

/// Room task published by `func_dryfield_breezeway_8017E010` and
/// `func_dryfield_breezeway_8017E114` once they have built its work block.
extern Task* D_dryfield_breezeway_801843C0;

/// Placement this room hands on with message 0x7D4 from
/// `func_dryfield_breezeway_8017E2D4`, `func_dryfield_breezeway_8017E390` and
/// `func_dryfield_breezeway_8017DEC0`: world x 17000, y 0, z 3000, yaw 0xA00.
extern DbwPlacement D_dryfield_breezeway_80181E28;

/// Secondary task spawned from the room data table by
/// `func_dryfield_breezeway_8017DC3C` (state 0) and cleared again once
/// `Task_PollKill` reaps it; `func_dryfield_breezeway_8017DDB0` clears it on
/// its way out. `func_dryfield_breezeway_8017D90C` forwards it to
/// `Gp_DispatchMsg` (with no message id of its own).
extern Task* D_dryfield_breezeway_801843A8;

#endif // ROOMS_DRYFIELD_BREEZEWAY_H
