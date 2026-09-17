#ifndef ROOMS_DRYFIELD_WATER_TOWER_H
#define ROOMS_DRYFIELD_WATER_TOWER_H

#include "common.h"

#include "main/task.h"

/// Work block of the water tower's script task, allocated as 0x18 zeroed bytes
/// by `func_dryfield_water_tower_8017FD64` and hung off `Task::idMap` (0x1C) --
/// that slot is *not* a `TaskIdMap` here. Reach it with
/// `(DwtwWork*)task->idMap`.
///
/// The first three fields are the tasks the room's script dispatches its
/// messages to: `field_0` is the slot-3 game pointer (`Game_GetPtrSlot(3)`),
/// and `field_4` / `field_8` are `Gp_FindWorkById(...)->field_0` for two ids
/// built from the session's `field_6` / `field_7` bytes (the second id has
/// 0x1000 OR'd in). `func_dryfield_water_tower_80180220` sends the 0x7D4 pair
/// to `field_8` / `field_4` and the 0x3F3 / 0x3E9 messages to `field_0`.
///
/// `field_C` / `field_E` are written together as a pair -- `field_E` always
/// cleared -- by `func_dryfield_water_tower_80180174`, a body the breezeway
/// room carries as `func_dryfield_breezeway_8017E370`; their meaning is not yet
/// known. `field_14` is a 0/1 latch that lets
/// `func_dryfield_water_tower_80180194` dispatch its one-shot message once.
typedef struct DwtwWork {
    /* 0x00 */ Task* field_0; // Game_GetPtrSlot(3), Gp_DispatchMsg target
    /* 0x04 */ Task* field_4; // Gp_FindWorkById(...)->field_0
    /* 0x08 */ Task* field_8; // Gp_FindWorkById(...)->field_0
    /* 0x0C */ s16   field_C;
    /* 0x0E */ s16   field_E;
    /* 0x10 */ byte  pad_10[0x4];
    /* 0x14 */ s16   field_14;
    /* 0x16 */ byte  pad_16[0x2];
} DwtwWork;
STATIC_ASSERT_SIZEOF(DwtwWork, 0x18);

/// Fade block `func_dryfield_water_tower_80180038` allocates with
/// `Mem_Malloc(8, 0)` and parks in `Task::idMap` -- a second, smaller idMap
/// block in this room, distinct from `DwtwWork`. Reach it with
/// `(DwtwFadeWork*)task->idMap`.
///
/// The three halfwords are the RGB channels `Fade_DrawOverlay` draws: the task
/// raises all three by `spawnArg1` each frame (so `spawnArg1` is the fade rate,
/// not a colour) and kills itself once the red channel passes 0x100. `field_0`
/// is never touched.
///
/// The same 8-byte block the actors' fade tasks carry as
/// `Actor560800FadeWork`, with one difference: this one neither reparents
/// itself nor clears the display mask before killing the task, so the room sees
/// the fade end where the actors darken the screen. The task is the second of
/// the three descriptors in `D_dryfield_water_tower_8018277C`.
///
/// This layout is the fade-*out* direction only. Entry 2 of that table runs it
/// backwards, from 0xFF down past zero, and the water tower shares that body
/// with the warehouse as `RoomsShared8017ff5c` (`src/rooms/lib/`), which
/// carries its own copy of these three halfwords rather than including this
/// header.
typedef struct DwtwFadeWork {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  r;
    /* 0x4 */ u16  g;
    /* 0x6 */ u16  b;
} DwtwFadeWork;
STATIC_ASSERT_SIZEOF(DwtwFadeWork, 0x8);

/// Payload `Gp_DispatchMsg` carries for message 0x7DB, the record this room's
/// script table `D_dryfield_water_tower_80181B00` pairs with
/// `func_dryfield_water_tower_8017F808` next to its `Room_Util08` 0x7D4 entry.
/// Only the halfword at 0x2 is read; it becomes the receiving task's state. The
/// same four bytes the actors send in `Actor210600Msg` / `Actor560800Msg`, and
/// the shape `AcropolisBridgeMsg7DB` gives them on the receiving side.
///
/// `func_dryfield_water_tower_80180194` is the send side: it fills the two id
/// bytes from the session's `field_7` / `field_6` and zeroes the halfword
/// before handing the record to `Gp_DispatchMsg` as message 0x7DA.
typedef struct DwtwMsg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} DwtwMsg7DB;
STATIC_ASSERT_SIZEOF(DwtwMsg7DB, 0x4);

/// One of the per-view objects the room's sprite-table record points at.
/// `field_C` is the skip-OT-link byte: non-zero leaves the view's sprites out
/// of the ordering table, zero draws them. `func_dryfield_water_tower_801802D8`
/// is this room's writer, driven from the `GameFlag_GetNibble(0x55)` band by
/// `func_dryfield_water_tower_8017DCB4`; the byte at 0x14 sits next to it and
/// is written by the other rooms carrying the same object (the dryfield night
/// motel balcony's `func_dryfield_night_motel_balcony_8017E4B8` writes both).
typedef struct DwtwSprtViewState {
    /* 0x0 */ byte pad_0[0xC];
    /* 0xC */ u8   field_C; // skip-OT-link; 0 draws the view's sprites
} DwtwSprtViewState;
STATIC_ASSERT_SIZEOF(DwtwSprtViewState, 0xD);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at:
/// a room-sized block, far larger than the 0xC-byte `GpSprtRec` the table's
/// element type declares, so the room reaches its tail through a cast. The tail
/// is a run of pointers to `DwtwSprtViewState`; the sibling overlays carrying
/// the same record keep them at 0xC4 / 0xD0 / 0xDC / 0x100
/// (`shelter_b3_dumping_hole_6.c` models the shape privately as `SprtBigRec`,
/// with its own pointer at 0xA0).
typedef struct DwtwSprtRec {
    /* 0x000 */ byte               pad_0[0xDC];
    /* 0x0DC */ DwtwSprtViewState* field_DC;
} DwtwSprtRec;
STATIC_ASSERT_SIZEOF(DwtwSprtRec, 0xE0);

/// One step of the room's rotation schedule, the table
/// `func_dryfield_water_tower_8017FB4C` walks: `field_0` is the step's
/// threshold and `field_2` its duration. Four of them sit at 0x8018767C --
/// `{0, 25}`, `{1, 30}`, `{2, 35}` and the `{0xFFFF, 40}` terminator, so the
/// thresholds ascend and the last entry is the cap the walk can never pass --
/// and the duration is read out as `field_2 * 30` with the low bit cleared.
///
/// `func_dryfield_water_tower_8017EB7C` counts the step up in
/// `DryfieldWaterTowerState::field_72` and walks the same table inline,
/// handing the duration to the accumulator `D_dryfield_water_tower_801876AA`
/// at 0x801876AA, the halfword its 0x801876A8 frame counter is compared
/// against.
typedef struct DwtwStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} DwtwStep;
STATIC_ASSERT_SIZEOF(DwtwStep, 0x4);

/// Scratch state of the room's cap script, stored at `Task::idMap`: the
/// 0x7C-byte block `func_dryfield_water_tower_8017F128` allocates for its own
/// task before it runs. Every task the room spawns off `D_..._80182384`
/// allocates the same block, so the prop tasks reached through `field_44` /
/// `field_48` carry one too.
///
/// `field_40` is the slot-3 game pointer (`Game_GetPtrSlot(3)`), the task the
/// 0x3E9 player-placement messages go to. `field_44` / `field_48` are the two
/// prop tasks `func_dryfield_water_tower_8017F128` spawns as types 1 and 2 of
/// `D_..._80182384` -- the 0x7D4 (`Room_Util08`) targets -- and `field_4C` is
/// the task it spawns off a second table, `D_..._8018277C`.
///
/// The tail is halfword slots -- the pairs are timers the instructions above
/// the state switch count down -- and the ones the decomp has named so far are
/// `field_58` / `field_5A` / `field_60`, the three
/// `func_dryfield_water_tower_8017F808` clears on message 0x7DB, and
/// `field_5C` / `field_5E`, which `func_dryfield_water_tower_8017F8E8` writes
/// together. `field_6C` / `field_6E` are 0/1 latches set by
/// `func_dryfield_water_tower_8017FBC8` / `8017FBD8`; `field_70` is a third,
/// set by script opcode `func_dryfield_water_tower_8017FA5C` and read back by
/// the prop task `func_dryfield_water_tower_8017E1DC`. `field_72` is the
/// rotation's step, advanced by `func_dryfield_water_tower_8017EB7C` and the
/// index `func_dryfield_water_tower_8017FB4C` reads its `DwtwStep` with.
/// `field_76` is also a
/// 0/1 latch, set once by `func_dryfield_water_tower_8017F82C`, the 0x0D
/// handler both of the room's script tables carry. `field_78` is a fourth:
/// `func_dryfield_water_tower_8017E93C` sets it as it spawns
/// `Gp_SpawnScript18(0x80187628, 0x8018763C)`, `8017EB7C` clears it, and
/// `func_dryfield_water_tower_8017F908` reads it to decide whether its event
/// 0x5214000C is due.
typedef struct DryfieldWaterTowerState {
    /* 0x00 */ u8    pad_0[0x40];
    /* 0x40 */ Task* field_40; // Game_GetPtrSlot(3)
    /* 0x44 */ Task* field_44;
    /* 0x48 */ Task* field_48;
    /* 0x4C */ Task* field_4C;
    /* 0x50 */ u8    pad_50[0x8];
    /* 0x58 */ s16   field_58;
    /* 0x5A */ s16   field_5A;
    /* 0x5C */ s16   field_5C;
    /* 0x5E */ s16   field_5E;
    /* 0x60 */ s16   field_60;
    /* 0x62 */ u8    pad_62[0x4];
    /* 0x66 */ u16   field_66;
    /* 0x68 */ u8    field_68;
    /* 0x69 */ u8    pad_69[0x3];
    /* 0x6C */ s16   field_6C;
    /* 0x6E */ s16   field_6E;
    /* 0x70 */ u16   field_70;
    /* 0x72 */ u16   field_72;
    /* 0x74 */ u8    pad_74[0x2];
    /* 0x76 */ u16   field_76;
    /* 0x78 */ u16   field_78;
    /* 0x7A */ u8    pad_7A[0x2];
} DryfieldWaterTowerState;
STATIC_ASSERT_SIZEOF(DryfieldWaterTowerState, 0x7C);

/// The water tower's script task, published by its state-0 init
/// `func_dryfield_water_tower_8017FD64`. `DwtwWork` hangs off its `idMap`.
extern Task* D_dryfield_water_tower_801876AC;

/// The water tower's cap-script task, published by
/// `func_dryfield_water_tower_8017F128`, which allocates the cap script's
/// 0x7C-byte scratch block into the task's `idMap` first.
extern Task* D_dryfield_water_tower_801876A4;

#endif // ROOMS_DRYFIELD_WATER_TOWER_H
