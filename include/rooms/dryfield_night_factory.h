#ifndef ROOMS_DRYFIELD_NIGHT_FACTORY_H
#define ROOMS_DRYFIELD_NIGHT_FACTORY_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

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

/// The cutscene sequence's handler table: the flag watcher of state 0 and the
/// two movements it arms.
extern const NightFactoryCutsceneTable3 D_dryfield_night_factory_8017D5DC;

/// A `MATRIX` plus a word-wise view of its first 0x12 bytes, used to reset a
/// rotation to identity with five aligned stores rather than nine halfword ones
/// before the model's rotation is rebuilt from an angle.
typedef union NightFactoryMatWords {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} NightFactoryMatWords;
STATIC_ASSERT_SIZEOF(NightFactoryMatWords, 0x20);

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
extern RoomHotspot D_dryfield_night_factory_80186EBC[];

/// The collision grids of the two stage variants, whose faces the factory
/// model's handlers rewrite as it moves.
extern GpGridParams D_dryfield_night_factory_80187BF0;
extern GpGridParams D_dryfield_night_factory_80187BF8;

/// State handlers of the factory model task: set-up, the per-frame state and
/// `taskKill`.
extern const TaskFuncTable3 D_dryfield_night_factory_8017D5C4;

/// State handlers of the cutscene task: set-up, the cutscene sequence and
/// `taskKill`.
extern const TaskFuncTable3 D_dryfield_night_factory_8017D5D0;

/// State handlers of the room entry task: set-up, an empty tick and
/// `taskKill`.
extern const TaskFuncTable3 D_dryfield_night_factory_8017D638;

/// The state handlers of the room's script task, run through
/// `func_dryfield_night_factory_8018169C`.
extern const TaskFuncTable7 D_dryfield_night_factory_8017D678;

/// Task callback of the descriptor at `D_dryfield_night_factory_80186E94`:
/// allocates the script work block, spawns the room's child task, picks the
/// global mode byte from game flag 0x48, steps the task on one state and clears
/// the room's hotspot list.
void func_dryfield_night_factory_8018182C(Task* task);

/// Runs the factory model for the bit of game flag 0x49 the task last saw: bit
/// 1 picks the first handler pair and bit 0 the second of the pair, the frame
/// counter at `NightFactoryWork::field_14` is bumped, and the model's coordinate
/// is rebuilt and handed to `func_800D7A9C` together with its translation.
void func_dryfield_night_factory_8017FA08(Task* task);

/// Rebuilds four faces of the stage variant's collision grid from a template
/// moved into the frame of the task's model: the template normals are rotated
/// into grid normals 2..5 and its corners rotated and translated into corners
/// 8..15. `useAltTemplate` picks the second template, and `remapFaces` also
/// copies the template's four face records into faces 2..5, rebased onto those
/// slots. The model's set-up state remaps; the per-frame state passes bit 0 of
/// game flag 0x49 as `useAltTemplate`.
void func_dryfield_night_factory_8017D858(Task* task, s32 remapFaces, s32 useAltTemplate);

/// The handler the model runs while bit 1 of game flag 0x49 is set, and -- when
/// bit 0 is set with it -- the handler that follows.
s32 func_dryfield_night_factory_8017E13C(Task* task);
s32 func_dryfield_night_factory_8017DA54(Task* task);

/// The handler that follows `func_dryfield_night_factory_8017E13C` when bit 0 of
/// game flag 0x49 is clear.
s32 func_dryfield_night_factory_8017DDD4(Task* task);

/// The handler the model runs while bit 1 of game flag 0x49 is clear, and --
/// when bit 0 is set with it -- the handler that follows.
s32 func_dryfield_night_factory_8017E480(Task* task);
s32 func_dryfield_night_factory_8017E7A4(Task* task);

/// The handler that follows `func_dryfield_night_factory_8017E480` when bit 0 of
/// game flag 0x49 is clear.
s32 func_dryfield_night_factory_8017EBD4(Task* task);

/// Kills the task; the factory model's exit callback.
void func_dryfield_night_factory_8017FB48(Task* task);

/// Binds the model to the light and colour matrices in the task's work block
/// and rebuilds its lighting.
void func_dryfield_night_factory_8017FB68(Task* task);

/// Sends message 0x13F3 to `task`, if there is one.
void func_dryfield_night_factory_8017FBC8(Task* task);

/// The room's event gate: answers 1 when the request's flag says the event
/// already happened, 0 (after running the request's cap command) when its
/// item prerequisite is missing, and otherwise latches the request, writes
/// the flag and spawns the room's event task, for 2. A non-zero `field_5` on
/// the message only asks for the answer.
s32 func_dryfield_night_factory_80180164(RoomEventReq* req, RoomEventMsg* msg);

/// Runs cap step `step` of the room's script, picking the sound, the progress
/// flags and the cap slot for the step.
void func_dryfield_night_factory_80180DE8(Task* task, s16 step);

/// Moves both action-prompt cursors from the pads and draws them.
void func_dryfield_night_factory_801810D8(Task* task);

/// Shows (non-zero) or hides (zero) the second sprite command of view 9 of the
/// current room, in stage 2 only.
void func_dryfield_night_factory_80181620(s32 show);

/// Marks every hotspot of `table` under (`x`, `y`) as hit; answers whether any
/// was.
s32 func_dryfield_night_factory_80181778(RoomHotspot* table, s16 x, s16 y);

/// As `func_dryfield_night_factory_80181620`, for view 11.
void func_dryfield_night_factory_80181B38(s32 show);

/// Resets both action-prompt slots and steps the caller on one state.
void func_dryfield_night_factory_80181BB4(Task* task);

/// Draws a tinted, flickering glow disc at the world-space point `pos`.
void func_dryfield_night_factory_80181C14(SVECTOR* pos, s32 size, s32 tint);

#endif // ROOMS_DRYFIELD_NIGHT_FACTORY_H
