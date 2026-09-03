#ifndef ROOMS_ACROPOLIS_SECURITY_ROOM_H
#define ROOMS_ACROPOLIS_SECURITY_ROOM_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/libgte.h>

/// 0xA work block of the security-monitor task, hung off the `Task::idMap`
/// slot (0x1C) -- that slot is *not* a `TaskIdMap` here, it is the
/// `Mem_Calloc(0xA)` block `func_acropolis_security_room_8017D9DC` allocates.
/// Reach it with `(AsrMonitorWork*)task->idMap`.
///
/// `cameraId` is the camera the monitor is currently showing, seeded from the
/// `GameFlag_GetNibble(0x2A)` lookup table and offset by 0x7F before being
/// handed to the panel drawer `func_acropolis_security_room_8017E0C4`.
/// `blinkTimer` is the cursor blink counter the overlay drawer
/// `func_acropolis_security_room_8017E37C` advances, `selection` the row the
/// player has highlighted, and `promptKind` the display mode forwarded to
/// `func_800D4E78` when the action prompt is spawned.
typedef struct AsrMonitorWork {
    /* 0x00 */ u16  cameraId;
    /* 0x02 */ s16  blinkTimer;
    /* 0x04 */ s16  selection;
    /* 0x06 */ s8   promptKind;
    /* 0x07 */ s8   field_7;
    /* 0x08 */ s8   field_8;
    /* 0x09 */ byte pad_9[0x1];
} AsrMonitorWork;
STATIC_ASSERT_SIZEOF(AsrMonitorWork, 0xA);

/// The security-monitor task's state table: seven handlers plus the NULL slot
/// the original `.rodata` table ends with. Only the seven handlers are copied
/// onto the stack by the dispatcher `func_acropolis_security_room_8017ED68`,
/// which is why the terminator is a separate member rather than an eighth
/// entry of `states`.
typedef struct AsrMonitorStateTable {
    /* 0x00 */ TaskFuncTable7 states;
    /* 0x1C */ TaskFunc       end;
} AsrMonitorStateTable;
STATIC_ASSERT_SIZEOF(AsrMonitorStateTable, 0x20);

/// One entry of the room's 0xFFFF-terminated hotspot table
/// (`D_acropolis_security_room_801826DC`). `x` / `y` / `w` / `h` are the screen
/// rectangle `func_acropolis_security_room_8017FCB0` tests the action cursor
/// against; on a hit it raises `hit` on that entry and clears it on every other.
/// `func_acropolis_security_room_8017EE44` then scans for the raised entry and
/// copies `id` and `promptKind` into the script's state block, so `id` is the
/// script variant the hotspot selects rather than an on-screen target id.
typedef struct AsrHotspot {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 w;
    /* 0x6 */ s16 h;
    /* 0x8 */ s16 id; // list terminator is -1
    /* 0xA */ u8  promptKind;
    /* 0xB */ s8  hit;
} AsrHotspot;
STATIC_ASSERT_SIZEOF(AsrHotspot, 0xC);

/// Screen rectangle outlined by `func_acropolis_security_room_8017DE80`: the
/// corners it draws are (`x`, `y`) and (`x + w`, `y + h`), so `w` and `h` are
/// extents rather than a second corner. The fields are unsigned because the
/// drawer loads every one of them with `lhu`.
typedef struct AsrRect {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
    /* 0x4 */ u16 w;
    /* 0x6 */ u16 h;
} AsrRect;
STATIC_ASSERT_SIZEOF(AsrRect, 0x8);

/// Scratch state of the security-room cap script, stored at `Task::idMap`.
///
/// `func_acropolis_security_room_8017FA18` -- state 0 of the family whose
/// handler table is `D_acropolis_security_room_8017D63C` -- allocates it with
/// `Mem_Calloc(0x10, 0)` and stores it straight into the `Task::idMap` slot,
/// so the size below is the allocation and not a guess; the same function
/// parks the family's `GpMsgEntry[]` in `Task::field_24`. The overlay's other
/// two allocators (`Mem_Calloc(0xA)` in `func_acropolis_security_room_8017D9DC`
/// and `Mem_Calloc(4)` in `func_acropolis_security_room_80180368`) belong to
/// other task families and to a different block.
typedef struct AcropolisSecurityRoomState {
    /* 0x0 */ s32   field_0;    // sub-step picked by the previous cap event
    /* 0x4 */ Task* child;      // task this state spawned, polled by Task_PollKill
    /* 0x8 */ u16   frames;     // frames the current state has been running
    /* 0xA */ s16   variant;    // 0 = the pair-4 cap script, 1 = the pair-3 one
    /* 0xC */ s8    promptKind; // display mode forwarded to `func_800D4E78`
    /* 0xD */ byte  pad_D[0x3];
} AcropolisSecurityRoomState;
STATIC_ASSERT_SIZEOF(AcropolisSecurityRoomState, 0x10);

/// One frame of the 128x128 textured quad `func_acropolis_security_room_80180E34`
/// draws: the sprite is centred on (`x`, `y`) with texture page 0xAB, the CLUT
/// `clut << 6` and its top-left texel at (`u`, `v`), the other three corners
/// being that texel plus 0x7F on each axis. The table
/// (`D_acropolis_security_room_80183970`) has four entries and is indexed by
/// the low two bits of the drawing task's `Task::spawnArg1`.
typedef struct AsrSpriteFrame {
    /* 0x0 */ u16  clut;
    /* 0x2 */ u8   u;
    /* 0x3 */ byte pad_3[1];
    /* 0x4 */ u8   v;
    /* 0x5 */ byte pad_5[1];
    /* 0x6 */ u16  x;
    /* 0x8 */ u16  y;
} AsrSpriteFrame;
STATIC_ASSERT_SIZEOF(AsrSpriteFrame, 0xA);

/// The four sprite frames `func_acropolis_security_room_80180E34` picks from.
extern AsrSpriteFrame D_acropolis_security_room_80183970[];

/// The script's hotspot table, terminated by an entry whose `id` is -1.
extern AsrHotspot D_acropolis_security_room_801826DC[];

/// Hit-tests the action cursor at (`x`, `y`) against the 0xFFFF-terminated
/// hotspot table `table`. Same body as `func_acropolis_security_room_8017ECB4`.
s32 func_acropolis_security_room_8017FCB0(AsrHotspot* table, s16 x, s16 y);

/// Draws the security-monitor panel for the camera `id` (the work block's
/// `cameraId` biased by -0x7F); a negative `id` draws the "no signal" panel.
void func_acropolis_security_room_8017E0C4(s16 id);

/// Draws the blinking cursor overlay on top of the monitor panel.
void func_acropolis_security_room_8017E37C(Task* task);

/// Outlines `rect` on screen with four flat `LINE_F2` primitives in the colour
/// (`r`, `g`, `b`), linked into `Gpu_CurrentOt[3]`.
void func_acropolis_security_room_8017DE80(AsrRect* rect, u8 r, u8 g, u8 b);

/// 0x14-byte scratch block `func_acropolis_security_room_80180A78` takes from
/// `G_SCRATCH_HEAD` while it draws the security laser. `a` and `b` are the two
/// endpoints of the beam in the emitter's local frame; each is rotated by the
/// emitter's `GsCOORDINATE2::workm` and then biased by that matrix's
/// translation, so both end up in world space. `otz` receives `SZ3 >> 2` from
/// the `RTPS` of `a` and doubles as the OT slot selector.
typedef struct AsrBeamScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR a;
    /* 0x0C */ SVECTOR b;
} AsrBeamScratch;
STATIC_ASSERT_SIZEOF(AsrBeamScratch, 0x14);

/// Draws the security room's sweeping laser beam for this frame.
void func_acropolis_security_room_80180A78(Task* task);

#endif // ROOMS_ACROPOLIS_SECURITY_ROOM_H
