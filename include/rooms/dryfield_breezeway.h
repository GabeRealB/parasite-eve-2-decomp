#ifndef ROOMS_DRYFIELD_BREEZEWAY_H
#define ROOMS_DRYFIELD_BREEZEWAY_H

#include "common.h"

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

/// Room task published by `func_dryfield_breezeway_8017E010` and
/// `func_dryfield_breezeway_8017E114` once they have built its work block.
extern Task* D_dryfield_breezeway_801843C0;

/// Secondary task spawned from the room data table by
/// `func_dryfield_breezeway_8017DC3C` (state 0) and cleared again once
/// `Task_PollKill` reaps it; `func_dryfield_breezeway_8017DDB0` clears it on
/// its way out. `func_dryfield_breezeway_8017D90C` forwards it to
/// `Gp_DispatchMsg` (with no message id of its own).
extern Task* D_dryfield_breezeway_801843A8;

#endif // ROOMS_DRYFIELD_BREEZEWAY_H
