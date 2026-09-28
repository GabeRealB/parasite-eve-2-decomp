#ifndef ROOMS_DRYFIELD_NIGHT_FACTORY_H
#define ROOMS_DRYFIELD_NIGHT_FACTORY_H

#include "common.h"

#include <psyq/libgte.h>
#include "rooms/room_common.h"

#include "gameplay/collision.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Cutscene work block the room's cutscene task allocates as 0xC zeroed bytes
/// in its state 0 and parks at `Task::work` -- that slot is *not* a `TaskIdMap`
/// here.
///
/// `state` selects the handler out of the room's cutscene handler table, `step`
/// is the counter the movement handlers advance, and `prevFlag` is the nibble of
/// game flag 0x4E the state-0 handler last saw. The set-up state parks `state`
/// at 0xFF when the scene is already on.
///
/// `field_0` is the angular velocity the two movement handlers accelerate
/// towards their own limit and `field_4` is the 16.16 angle it drives: each
/// handler adds the first to the second, clamps it at its limit, and rotates
/// the model by the integer part. Both views of `field_4` live in one union,
/// the way `NightFactoryWork::field_C` does -- the whole 32 bits go in and the
/// high half alone comes back out.
typedef struct NightFactoryCutsceneWork {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ union {
        /* 0x4 */ s32 value;
        struct {
            /* 0x4 */ s16 frac;
            /* 0x6 */ s16 whole;
        } part;
    } field_4;
    /* 0x8 */ u8   state;
    /* 0x9 */ u8   step;
    /* 0xA */ u8   prevFlag;
    /* 0xB */ byte pad_B[0x1];
} NightFactoryCutsceneWork;
STATIC_ASSERT_SIZEOF(NightFactoryCutsceneWork, 0xC);

/// A handler of the cutscene sequence. Unlike `TaskFunc` these report back: a
/// non-zero return means the handler has finished its part of the scene, and
/// the sequence drops back to state 0.
typedef s32 (*NightFactoryCutsceneFunc)(Task*);

/// The cutscene sequence's three handler slots, which the dispatcher copies
/// onto the stack before calling through them.
typedef struct NightFactoryCutsceneTable3 {
    /* 0x0 */ NightFactoryCutsceneFunc funcs[3];
} NightFactoryCutsceneTable3;
STATIC_ASSERT_SIZEOF(NightFactoryCutsceneTable3, 0xC);

/// Work block the room's factory task allocates as 0x58 zeroed bytes in its
/// state 0 and parks at `Task::work`.
///
/// `field_0` is the nibble of game flag 0x49 the task last saw, `field_14`
/// counts the frames since that nibble changed, and `field_16` / `field_17` are
/// the step counters of the handlers driven by its bit 0 and bit 1: each drops
/// back to 0 when its bit changes, and the set-up state seeds both to -1 when
/// it allocates the block.
///
/// `field_C` is a 16.16 accumulator: the lift handlers add `field_4` to it and
/// clamp the result, and the model's Y translation is read straight out of its
/// integer part. Both views live in one union -- the target stores the whole 32
/// bits and loads the high half.
///
/// `field_10` is the model's 16.16 yaw, laid out the same way: the turn
/// handlers accelerate `field_8` towards a limit, add it to `field_10`, and
/// rebuild the model's rotation from the integer part alone.
///
/// `light` and `color` are the model's own light and colour matrices, which the
/// lighting helper publishes onto the model's `TmdObject::lightMtx` /
/// `colorMtx`.
typedef struct NightFactoryWork {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ union {
        /* 0x0C */ s32 value;
        struct {
            /* 0x0C */ s16 frac;
            /* 0x0E */ s16 whole;
        } part;
    } field_C;
    /* 0x10 */ union {
        /* 0x10 */ s32 value;
        struct {
            /* 0x10 */ s16 frac;
            /* 0x12 */ s16 whole;
        } part;
    } field_10;
    /* 0x14 */ u16    field_14;
    /* 0x16 */ s8     field_16;
    /* 0x17 */ s8     field_17;
    /* 0x18 */ MATRIX light;
    /* 0x38 */ MATRIX color;
} NightFactoryWork;
STATIC_ASSERT_SIZEOF(NightFactoryWork, 0x58);

/// Work block the room's script task allocates (memCalloc(0x10)) and hangs off
/// `Task::work` -- that slot is *not* a `TaskIdMap` here. Reach it with
/// `(NightFactoryScriptWork*)task->work`.
///
/// `field_8` is the countdown the prompt states arm with 0xA and the idle state
/// runs down before it will scan the hotspots again. `field_A` is the one-shot
/// trigger a script message raises and the cursor state consumes. `field_C` and
/// `field_E` are the hotspot `id` and `promptKind` the idle state copies in
/// when the cursor confirms one: `field_C` is the cap step the script then
/// runs, and `field_E` the display mode the prompt is spawned with, read
/// signed.
typedef struct NightFactoryScriptWork {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ u16  field_8;
    /* 0xA */ s16  field_A;
    /* 0xC */ s16  field_C;
    /* 0xE */ s8   field_E;
    /* 0xF */ byte pad_F[0x1];
} NightFactoryScriptWork;
STATIC_ASSERT_SIZEOF(NightFactoryScriptWork, 0x10);

/// The single-entry `TaskDesc` table the room's script task spawns its child
/// task from: the prompt state machine `func_dryfield_night_factory_80181718`.
extern TaskDesc D_dryfield_night_factory_80186E94[];
/// The script's message table, parked in `Task::msgTable`.
extern GpMsgEntry D_dryfield_night_factory_80186EAC[];
/// The room's 0xFFFF-terminated hotspot table.
extern OverlayHotspot D_dryfield_night_factory_80186EBC[];

/// The collision grids of the two stage variants, whose faces the factory
/// model's handlers rewrite as it moves.
extern GpGridParams D_dryfield_night_factory_80187BF0;
extern GpGridParams D_dryfield_night_factory_80187BF8;

/// Shows (non-zero) or hides (zero) the second sprite command of view 9 of the
/// current room, in stage 2 only.
void func_dryfield_night_factory_80181620(s32 show);

/// As `func_dryfield_night_factory_80181620`, for view 11.
void func_dryfield_night_factory_80181B38(s32 show);

#endif // ROOMS_DRYFIELD_NIGHT_FACTORY_H
