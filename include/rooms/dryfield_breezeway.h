#ifndef ROOMS_DRYFIELD_BREEZEWAY_H
#define ROOMS_DRYFIELD_BREEZEWAY_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/task.h"

#include "rooms/room_common.h"

/// `GpMsgEntry` (`gameplay/D4.h`), forward-declared because that header's
/// four-argument `Gp_DispatchMsg` prototype is not in scope in
/// `dryfield_breezeway.c`, which calls the dispatcher with only the task.
struct _GpMsgEntry;

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
    /* 0x0C */ u16   field_C;
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
///
/// `promptKind` is the display mode the hotspot scan
/// `func_dryfield_breezeway_8017E65C` copies off the `RoomHotspot` the cursor
/// landed on (whose id it parks at 0x4C) before it picks state 3;
/// `func_dryfield_breezeway_8017FD9C` forwards it to `func_800D4E78` when it
/// re-spawns the prompt.
///
/// `cursorX` / `cursorY` are the on-screen pair the same scan is hit-tested
/// against: `func_dryfield_breezeway_8017E464` seeds them with the reset
/// position (0, 0x20) `func_dryfield_breezeway_8017FD9C` also passes to
/// `func_dryfield_breezeway_8017EB8C`, and `func_dryfield_breezeway_8017E81C`
/// feeds them to `RoomsShared8017ecb4`.
///
/// `light` / `color` are the room's own lighting pair, the block's whole first
/// 0x40 bytes: `func_dryfield_breezeway_8017E464` publishes them onto
/// `TmdObject::field_1C` / `field_20` -- the slots `Gp_BindDefaultMtx` otherwise
/// points at `Gp_DefaultMtx` / `Gp_DefaultMtx2` -- so the event object draws
/// with this lighting rather than the shared defaults, and
/// `Gp_SetObjTrans` writes the 0x800 translation into `color.t`.
typedef struct DbwEventWork {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ s32    field_40;
    /* 0x44 */ byte   pad_44[0x8];
    /* 0x4C */ s16    field_4C;
    /* 0x4E */ s16    cursorX;
    /* 0x50 */ s16    cursorY;
    /* 0x52 */ byte   pad_52[0xA];
    /* 0x5C */ s8     promptKind;
    /* 0x5D */ byte   pad_5D[0x3];
} DbwEventWork;
STATIC_ASSERT_SIZEOF(DbwEventWork, 0x60);

/// A `MATRIX` plus the word-wise view `func_dryfield_breezeway_8017E464` splats
/// the light / colour pair through: five aligned stores rather than nine
/// halfword ones (the same idiom as `Actor311900MatWords`). The pairs the
/// compiler folds are the ones whose two halfwords are both 0x1000 or both
/// zero -- the only two adjacent pairs of either splat that agree -- which is
/// why `m02_m10` and `m22` sit between them.
typedef union DbwMatWords {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} DbwMatWords;
STATIC_ASSERT_SIZEOF(DbwMatWords, 0x20);

/// The `TaskDesc` `func_dryfield_breezeway_8017E464` spawns from as the room's
/// event task, and the single-entry `GpMsgEntry[]` it parks in `Task::field_24`
/// so `Gp_DispatchMsg` routes the family's messages (the 0x13F1 "can this key
/// item be used here?" query) into it. Both sit in the room's trailing data
/// blob, the table immediately after the descriptor.
extern TaskDesc           D_dryfield_breezeway_80182DC0;
extern struct _GpMsgEntry D_dryfield_breezeway_80182DCC[];

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

/// The one scratch buffer `func_dryfield_breezeway_8017E390` builds both of its
/// payloads in, which is why they share a frame slot: `rec` is the 0x14-byte
/// slot-3 weapon record msg 0x3E8 takes (the `GpRec14` `Gp_MsgPlayerWeapon`
/// also sends, with `field_4` set to this room's 9 and `field_C`/`field_10`
/// zeroed), and `msg` the `DbwMsg7DA` the 0x7DA prompt takes right after it.
typedef union DbwMsgBuf {
    /* 0x0 */ GpRec14   rec;
    /* 0x0 */ DbwMsg7DA msg;
} DbwMsgBuf;
STATIC_ASSERT_SIZEOF(DbwMsgBuf, 0x14);

/// This room's `GpMsgEntry` id/handler table, the one
/// `func_dryfield_breezeway_8017DDB0` parks in `Task::field_24` so
/// `Gp_DispatchMsg` routes messages into the room at all: 0x13EE ->
/// `func_dryfield_breezeway_8017D940`, 0x13EF ->
/// `func_dryfield_breezeway_8017DBD8` (the hotspot gate, whose sub-id 1 arms
/// the room's task), 0x13F0 -> `func_dryfield_breezeway_8017DA48` (the
/// weapon/state sequencer), 0x13F1 -> `func_dryfield_breezeway_8017D90C` and
/// 0x13F2 -> `func_dryfield_breezeway_8017DBA4`, terminated by 0x7FFFFFFF.
extern struct _GpMsgEntry D_dryfield_breezeway_80181DE0[];

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

/// A point in the plane the room works in: an `SVECTOR`'s three components plus
/// the halfword that rounds the record up to the 8-byte stride the room's stack
/// slots for it have, with `vz` a real component the room pins to zero.
/// `func_dryfield_breezeway_8017FAD0` reads `vx` and `vy` unsigned and truncates
/// each difference back to 16 bits before squaring it, so the sign of the load
/// never reaches its result.
typedef struct DbwVec {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad;
} DbwVec;
STATIC_ASSERT_SIZEOF(DbwVec, 0x8);

/// Room task published by `func_dryfield_breezeway_8017E010` and
/// `func_dryfield_breezeway_8017E114` once they have built its work block.
extern Task* D_dryfield_breezeway_801843C0;

/// Placement this room hands on with message 0x7D4 from
/// `func_dryfield_breezeway_8017E2D4`, `func_dryfield_breezeway_8017E390` and
/// `func_dryfield_breezeway_8017DEC0`: world x 17000, y 0, z 3000, yaw 0xA00.
extern DbwPlacement D_dryfield_breezeway_80181E28;

/// The two placements that follow it in the same three-record run, which
/// `func_dryfield_breezeway_8017DEC0` sends to slot 3 as the second and third
/// message of its state-1 sequence: `[0]` is the record message 0x3E9 places the
/// player with, and `[1]` -- the run's third record -- the one message 0x3EE
/// does. The label the decomp references is the start of this array, so the
/// third record is reached as `[1]` rather than by a symbol of its own.
extern DbwPlacement D_dryfield_breezeway_80181E40[];

/// The key-item prompt's own hotspot table: the one-entry 0xFFFF-terminated
/// `RoomHotspot` run `func_dryfield_breezeway_8017E65C` hit-tests at the
/// prompt's own screen position and walks for the entry the cursor landed on,
/// where the prop table below is hit-tested at the cursor itself. Its `id` is
/// the script variant the prompt confirms, which the scan parks in the event
/// work block (`DbwEventWork.field_4C`, with `promptKind` at 0x5C) before state
/// 3. `func_dryfield_breezeway_8017E464` clears its `hit` along with the other
/// table's.
extern RoomHotspot D_dryfield_breezeway_80182E00[];

/// This room's prop hotspot table, the 0xFFFF-terminated `RoomHotspot` run
/// `RoomsShared8017ecb4` hit-tests the action cursor against. Its entries are
/// the room's interactive props: `func_dryfield_breezeway_8017E464` clears
/// every entry's `hit` through it before the first frame -- both tables', so
/// the key-item prompt above starts clean too -- and the scan in
/// `func_dryfield_breezeway_8017E81C` walks it for the entry the cursor landed
/// on.
extern RoomHotspot D_dryfield_breezeway_80182DDC[];

/// Secondary task spawned from the room data table by
/// `func_dryfield_breezeway_8017DC3C` (state 0) and cleared again once
/// `Task_PollKill` reaps it; `func_dryfield_breezeway_8017DDB0` clears it on
/// its way out. `func_dryfield_breezeway_8017D90C` forwards it to
/// `Gp_DispatchMsg` (with no message id of its own).
extern Task* D_dryfield_breezeway_801843A8;

/// The breezeway's cursor scan, run every frame its key-item event task is on a
/// state that watches the cursor. `arg1` / `arg2` are the position the scan
/// starts from - the prompt's own `screen` coordinates from
/// `func_dryfield_breezeway_8017E81C`, or the reset pair (0, 0x20) the other
/// states pass - and the scan answers by writing `D_80114D28::mode` (1 = over a
/// hotspot, 2 = confirmed) as well as advancing the hotspot's own animation.
void func_dryfield_breezeway_8017EB8C(Task* task, s16 arg1, s16 arg2);

/// Parks the room task's display object on the hotspot cursor. `arg1` / `arg2`
/// are the cursor position the scan above advanced to, written into the
/// object's own coordinate scaled by the depth it is placed at (`0x5DC` over
/// 680); clearing `flg` is what makes the next coord-tree update rebuild the
/// world matrix from the new translation.
void func_dryfield_breezeway_8017FB30(Task* task, s16 arg1, s16 arg2);

#endif // ROOMS_DRYFIELD_BREEZEWAY_H
