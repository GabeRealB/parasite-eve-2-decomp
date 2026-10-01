/* The Dryfield factory room, shared by its day (stage 2) and night packages;
 * the code names both packages' tables and picks one by stage, so each package
 * reaches the other's at a fixed address. Its centrepiece is a lift platform
 * model: bit 1 of progress nibble 0x49 raises it 570 units and bit 0 turns it
 * a quarter turn (0x400) about Y. Each move eases to its end, can be skipped
 * with a button after 11 frames, plays start/stop sounds and rebuilds four
 * collision faces to follow the model. A turn while the lift is lowered jams:
 * it swings about 11 degrees, jolts the screen, swings back and clears the
 * bit. A hatch model parented to the lift swings open about X (-0x300) and
 * shut again when nibble 0x4E toggles. The operator panel is an action-prompt
 * script over five hotspots: raise, lower, turn and two dialogue spots. It
 * moves the lift only after the power scene has set nibble 0x48; until then
 * each press only plays a caption. Other parts: cap-driven scenes (power on
 * with lit-panel background sprites and lamp nibble 0x4A; lamp to 2; a white-
 * out scene that sets 0x47, reloads the room as variant 2 and slides a
 * barrier's collision 2000 units along x), the room's warp filter, its command
 * handler and a one-time examine action.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_FACTORY_LIFT_H
#define SRC_SHARED_FACTORY_LIFT_H

#include "types.h"

#include <psyq/libgte.h>

#include "gameplay/collision.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"

#include "main/task_types.h"

#include "overlay.h"

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
/// the model by the integer half.
typedef struct FactoryHatchWork {
    /* 0x0 */ s32     field_0;
    /* 0x4 */ Fixed16 field_4;
    /* 0x8 */ u8      state;
    /* 0x9 */ u8      step;
    /* 0xA */ u8      prevFlag;
    /* 0xB */ byte    pad_B[0x1];
} FactoryHatchWork;
STATIC_ASSERT_SIZEOF(FactoryHatchWork, 0xC);

/// A handler of the cutscene sequence. Unlike `TaskFunc` these report back: a
/// non-zero return means the handler has finished its part of the scene, and
/// the sequence drops back to state 0.
typedef s32 (*FactoryHatchStateFunc)(Task*);

/// The cutscene sequence's three handler slots, which the dispatcher copies
/// onto the stack before calling through them.
typedef struct FactoryHatchStates {
    /* 0x0 */ FactoryHatchStateFunc funcs[3];
} FactoryHatchStates;
STATIC_ASSERT_SIZEOF(FactoryHatchStates, 0xC);

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
/// clamp the result, and the model's Y translation is the integer half.
///
/// `field_10` is the model's 16.16 yaw: the turn handlers accelerate `field_8`
/// towards a limit, add it to `field_10`, and rebuild the model's rotation
/// from the integer half alone.
///
/// `light` and `color` are the model's own light and colour matrices, which the
/// lighting helper publishes onto the model's `TmdObject::lightMtx` /
/// `colorMtx`.
typedef struct FactoryLiftWork {
    /* 0x00 */ s32     field_0;
    /* 0x04 */ s32     field_4;
    /* 0x08 */ s32     field_8;
    /* 0x0C */ Fixed16 field_C;
    /* 0x10 */ Fixed16 field_10;
    /* 0x14 */ u16     field_14;
    /* 0x16 */ s8      field_16;
    /* 0x17 */ s8      field_17;
    /* 0x18 */ MATRIX  light;
    /* 0x38 */ MATRIX  color;
} FactoryLiftWork;
STATIC_ASSERT_SIZEOF(FactoryLiftWork, 0x58);

/// Work block the room's script task allocates (memCalloc(0x10)) and hangs off
/// `Task::work` -- that slot is *not* a `TaskIdMap` here. Reach it with
/// `(FactoryPanelWork*)task->work`.
///
/// `field_8` is the countdown the prompt states arm with 0xA and the idle state
/// runs down before it will scan the hotspots again. `field_A` is the one-shot
/// trigger a script message raises and the cursor state consumes. `field_C` and
/// `field_E` are the hotspot `id` and `promptKind` the idle state copies in
/// when the cursor confirms one: `field_C` is the cap step the script then
/// runs, and `field_E` the display mode the prompt is spawned with, read
/// signed.
typedef struct FactoryPanelWork {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ u16  field_8;
    /* 0xA */ s16  field_A;
    /* 0xC */ s16  field_C;
    /* 0xE */ s8   field_E;
    /* 0xF */ byte pad_F[0x1];
} FactoryPanelWork;
STATIC_ASSERT_SIZEOF(FactoryPanelWork, 0x10);

/// A message handler of the panel task's table.
typedef struct {
    s32  id;
    void (*handler)(Task*);
} FactoryControlMessageEntry;
STATIC_ASSERT_SIZEOF(FactoryControlMessageEntry, 8);

/* Defined by each build. */
/// Collision templates the lift and barrier rebuild their grid faces from:
/// the lift raised, the lift turned, and the barrier.
extern WorldCollisionGrid gFactoryLiftTemplate;
extern WorldCollisionGrid gFactoryLiftTurnedTemplate;
extern WorldCollisionGrid gFactoryBarrierTemplate;
/// The room task's message table and the panel session's task descriptors.
extern GpMsgEntry gFactoryMsgTable[];
extern TaskDesc   gFactoryPanelSessionDesc[];
/// The spawn table and panel descriptor picked for this stage, and the slot
/// holding the panel task.
extern TaskDesc* gFactorySpawnTable;
extern TaskDesc* gFactoryPanelDesc;
extern Task**    gFactoryPanelSlot;
/// The panel's prompt state machine descriptor, its 0xFFFF-terminated hotspot
/// table and its message table.
extern TaskDesc                   gFactoryPromptDesc[];
extern OverlayHotspot             gFactoryPanelHotspots[];
extern FactoryControlMessageEntry gFactoryPanelMsgTable[2];

/* The room is built once per stage, day (stage 2) and night, from the same
 * source. Each build defines its own spawn table, collision grid, jolt script,
 * panel descriptor and view-sprite routines, and names the other build's by
 * address; the code picks between the two by stage. */
extern TaskDesc                  gFactoryDaySpawnTable[];
extern TaskDesc                  gFactoryNightSpawnTable[];
extern TaskDesc                  gFactoryDayPanelDesc[];
extern TaskDesc                  gFactoryNightPanelDesc[];
extern WorldCollisionGrid        gFactoryDayGrid;
extern WorldCollisionGrid        gFactoryNightGrid;
extern PadScriptCmd              gFactoryDayJoltCmds[3];
extern PadScriptCmd              gFactoryNightJoltCmds[3];
extern PadScriptVibrationSegment gFactoryDayJoltRecs[3];
extern PadScriptVibrationSegment gFactoryNightJoltRecs[3];

void factoryDayShowView9Sprite(s32 show);
void factoryNightShowView9Sprite(s32 show);
void factoryDayShowView11Sprite(s32 show);
void factoryNightShowView11Sprite(s32 show);

// `FACTORY_ROOM_NIGHT_INSTANCE` selects the night image's entry-task and
// view-sprite function bindings. Both dryfield_night_factory TUs define this
// empty marker before including the header, then undefine it. Both
// dryfield_factory TUs omit it to select day. The aliases remain available to
// the included function fragments; runtime stage selection is independent.
#ifdef FACTORY_ROOM_NIGHT_INSTANCE
#define factoryShowView9Sprite  factoryNightShowView9Sprite
#define factoryShowView11Sprite factoryNightShowView11Sprite
#define factoryEntryTask        factoryNightEntryTask
#else
#define factoryShowView9Sprite  factoryDayShowView9Sprite
#define factoryShowView11Sprite factoryDayShowView11Sprite
#define factoryEntryTask        factoryDayEntryTask
#endif

void factoryLiftInit(Task* task);
void factoryLiftSyncCollision(Task* task, s32 remapFaces, s32 useAltTemplate);
s32  factoryLiftTurnOut(Task* task);
s32  factoryLiftTurnBack(Task* task);
s32  factoryLiftRaise(Task* task);
s32  factoryLiftLower(Task* task);
s32  factoryLiftJamTurnOut(Task* task);
s32  factoryLiftJamTurnBack(Task* task);
s32  factoryHatchOpen(Task* task);
s32  factoryHatchClose(Task* task);
void factoryPowerScene(Task* task);
void factoryWhiteoutScene(Task* task);
void factoryBarrierCollision(Task* task);
void factoryLiftUpdate(Task* task);
void factoryLiftBindLighting(Task* task);
void factoryHatchInit(Task* task);
void factoryHatchUpdate(Task* task);
s32  factoryHatchWatch(Task* task);
void factoryLampScene(Task* task);
void factoryHatchScene(Task* task);
void factoryRoomInit(Task* arg0);
s32  factoryResolveWarp(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
void factoryPanelSpawn(Task* task);
s32  factoryCommand(Task* arg0, s32 arg1, s32 cmd, TaskMessageArg arg3);
s32  factorySoundCommand(Task* task, s32 msgId, s32 arg2, s32 arg3);
s32  factoryRoomAction(Task* task, s32 msgId, DirectionActionRequest* request, TaskMessageArg arg3);
void factoryPanelIdle(Task* task);
void factoryPanelRunStep(Task* task, s16 step);
void factoryPanelInit(Task* task);
void factoryPanelOpenPrompt(Task* task);
void factoryPanelPrompt(Task* task);
void factoryPanelExit(Task* arg0);
void factoryPanelWaitMove(Task* task);

void factoryLiftExit(Task* task);
void factoryLiftNotifyPanel(Task* arg0);
void factoryLiftRun(Task* task);
void factoryHatchRun(Task* task);
void factoryCapScene(Task* arg0);
void factoryEntryIdle(Task* task);
s32  factoryIgnoreMessage(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3);
void factoryPanelRun(Task* task);
void factoryPromptTask(Task* task);
void factoryPanelTrigger(Task* task);
void factoryPanelArmPrompt(Task* task);

#endif /* SRC_SHARED_FACTORY_LIFT_H */
