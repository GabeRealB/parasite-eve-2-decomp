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

#include "gameplay/action_prompt.h"
#include "gameplay/collision.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"

#include "main/task_types.h"

#include "dryfield_time.h"

/// Values of `FactoryHatchWork::state`: the slot of the hatch's handler table
/// that runs this frame.
enum {
    FACTORY_HATCH_STATE_WATCH, // Idle, waiting for the hatch-open flag to change
    FACTORY_HATCH_STATE_OPEN,  // Swinging open
    FACTORY_HATCH_STATE_CLOSE, // Swinging shut
    FACTORY_HATCH_STATE_COUNT, // Number of handler slots; not a state
};

/// Work block of the hatch task: the swing of the hatch model about X and the
/// state of the sequence driving it.
///
/// The task's set-up state allocates it zeroed and parks it at `Task::work`.
/// The hatch is shut at angle 0 and open at -0x300. A movement state restarts
/// `angularVelocity` from rest, accelerates it each frame up to its own limit
/// and adds it to `angle`; the model's rotation is rebuilt every frame from
/// the integer half of `angle` alone.
///
/// Set-up also stores 0xFF in `state` when it finds the hatch-open flag
/// already set, and turns the model to the open angle itself. That value
/// names no handler and the dispatcher does not range-check it; what it was
/// meant to select is unproven.
typedef struct {
    s32     angularVelocity; // Swing rate, 16.16 angle units (0x1000 a turn) per frame
    Fixed16 angle;           // Rotation about X, 16.16 angle units; 0 shut, negative towards open
    u8      state;           // Handler running, a `FACTORY_HATCH_STATE_`
    u8      step;            // Phase of the running movement; zeroed when one is armed
    u8      prevFlag;        // Hatch-open flag nibble the watch state last saw (0 shut, 1 open)
} FactoryHatchWork;
STATIC_ASSERT_SIZEOF(FactoryHatchWork, 0xC);

/// A per-frame handler of the hatch, one for each `FACTORY_HATCH_STATE_`.
///
/// It receives the hatch task, whose `Task::work` is the `FactoryHatchWork`
/// and whose body is the hatch model. Unlike a `TaskFunc` it reports back:
/// non-zero means the movement it runs has settled, and the hatch then
/// returns to `FACTORY_HATCH_STATE_WATCH`; zero keeps the state the handler
/// left in `FactoryHatchWork::state`, which is how the watch handler, always
/// answering zero, arms a movement itself.
typedef s32 (*FactoryHatchStateFunc)(Task* task);

/// The hatch's state handlers stored as a value, so that a dispatcher can take
/// the whole table by assignment.
///
/// Each build defines one constant table of this type. The hatch task's
/// per-frame state copies it onto its stack every frame and calls the slot
/// `FactoryHatchWork::state` selects, so every slot must hold a handler. The
/// call is made without a range check: a selector outside the
/// `FACTORY_HATCH_STATE_` values reads past the copy.
typedef struct {
    FactoryHatchStateFunc funcs[FACTORY_HATCH_STATE_COUNT]; // Handlers in `FACTORY_HATCH_STATE_` order: watch, open, close
} FactoryHatchStateFuncTable;
STATIC_ASSERT_SIZEOF(FactoryHatchStateFuncTable, 0xC);

/// Bits of the lift position nibble, game flag
/// `GAME_FLAG_FACTORY_LIFT_POSITION`, and of the copy of it in
/// `FactoryLiftWork::position`. The operator panel requests a move by changing
/// a bit; the lift then moves until it stands where the nibble says.
enum {
    FACTORY_LIFT_POSITION_TURNED = 1 << 0, // Turned a quarter turn about Y
    FACTORY_LIFT_POSITION_RAISED = 1 << 1, // Raised
};

/// Rest positions of the lift's two movements, as words of
/// `FactoryLiftWork::y` and `FactoryLiftWork::yaw`. The other end of each
/// movement is 0. Y grows downwards, so the raised position is negative.
#define FACTORY_LIFT_Y_RAISED   (-570 * 0x10000)
#define FACTORY_LIFT_YAW_TURNED (0x400 * 0x10000)

/// Value of `FactoryLiftWork::moveFrames` from which a button press ends the
/// running movement at once.
#define FACTORY_LIFT_SKIP_FRAMES 11

/// Work block of the lift task: where the lift model stands, how fast it is
/// moving, and the progress of the two movements that take it to the position
/// the lift position nibble asks for.
///
/// The task's set-up state allocates it zeroed, parks it at `Task::work` and
/// seeds `y` and `yaw` from the nibble, so a lift loaded raised or turned
/// starts there with both movements at rest.
///
/// Every frame the task compares the nibble with `position`. A changed bit
/// restarts that bit's movement from step 0 and restarts `moveFrames`. The
/// vertical movement then runs towards `FACTORY_LIFT_Y_RAISED` or 0 and the
/// turn towards `FACTORY_LIFT_YAW_TURNED` or 0, each by the same steps:
///
/// - 0 brings the velocity to rest;
/// - 1 starts the movement's sound;
/// - 2 accelerates up to a limit, adding the velocity to the position, until
///   the position passes its target;
/// - 3 does the same in reverse until it is back on the target, then tells the
///   operator panel and plays the stop sound;
/// - 4, like the -1 set-up leaves, is at rest.
///
/// A turn asked for while the lift is lowered cannot complete. Its step 2 ends
/// 0x80 past the starting yaw with a bang, and its step 3 swings back and
/// rewrites both `position` and the nibble to the bit's previous value, so
/// the frame after sees no change.
///
/// The model's Y translation and its rotation about Y are rebuilt every frame
/// from the integer halves of `y` and `yaw` alone.
typedef struct {
    s32     position;    // Lift position nibble this task last saw, `FACTORY_LIFT_POSITION_` bits
    s32     yVelocity;   // Vertical speed, 16.16 world units per frame; negative rises
    s32     yawVelocity; // Turn rate, 16.16 angle units (0x1000 a turn) per frame
    Fixed16 y;           // Model Y translation, 16.16 world units; 0 lowered, `FACTORY_LIFT_Y_RAISED` raised
    Fixed16 yaw;         // Rotation about Y, 16.16 angle units; 0 home, `FACTORY_LIFT_YAW_TURNED` turned
    s16     moveFrames;  // Frames since a movement was last requested; gates the button skip
    s8      yawStep;     // Step of the turn (-1 at rest since set-up, 0-3 moving, 4 at rest)
    s8      yStep;       // Step of the vertical movement, as `yawStep`
    MATRIX  light;       // Light-direction matrix the lift model and the hatch draw with
    MATRIX  color;       // Light-colour matrix the lift model and the hatch draw with
} FactoryLiftWork;
STATIC_ASSERT_SIZEOF(FactoryLiftWork, 0x58);

/// Value `FactoryPanelWork::scanDelay` is armed with: the frames the operator
/// panel keeps its cursor hidden after a choice has been dealt with.
#define FACTORY_PANEL_SCAN_DELAY_FRAMES 10

/// Dispatch slots of the operator panel task, selected by `Task::state`.
enum {
    FACTORY_PANEL_STATE_INIT = 0,
    FACTORY_PANEL_STATE_ARM_PROMPT,
    FACTORY_PANEL_STATE_IDLE,
    FACTORY_PANEL_STATE_OPEN_PROMPT,
    FACTORY_PANEL_STATE_PROMPT,
    FACTORY_PANEL_STATE_EXIT,
    FACTORY_PANEL_STATE_WAIT_MOVE,
    FACTORY_PANEL_STATE_COUNT
};

/// Saved views used while scanning the operator panel and after leaving it.
enum {
    FACTORY_PANEL_VIEW_RETURN    = 3,
    FACTORY_PANEL_VIEW_POWERED   = 5,
    FACTORY_PANEL_VIEW_UNPOWERED = 12
};

/// Work block of the operator panel task: the hotspot the player confirmed and
/// the two waits that keep the panel from taking another choice too early.
///
/// The task's set-up state allocates it zeroed and parks it at `Task::work`.
/// The idle state tests the cursor against the panel's hotspot table and
/// latches a confirmed entry in `choice` and `promptKind`; the states after it
/// open the command prompt for that entry and, when the player accepts it,
/// carry the choice out. A choice that moves the lift leaves the panel waiting
/// for `moveSettled`. Whichever way a choice ends, `scanDelay` is armed, and
/// the idle state hides the cursor until it has run out and no caption is
/// playing.
typedef struct {
    u8  unknown_0[8]; // Never read or written by the room; role unproven
    u16 scanDelay;    // Frames left before the idle state tests the hotspots again; armed with FACTORY_PANEL_SCAN_DELAY_FRAMES
    s16 moveSettled;  // Whether the lift has reported the end of its movement (0 not yet, 1 reported); cleared when the panel resumes
    s16 choice;       // `ActionPromptHotspot::id` of the confirmed hotspot (0 raise, 1 lower, 2 turn, 3 and 4 the two caption-only spots)
    s8  promptKind;   // `ActionPromptHotspot::promptKind` of that hotspot, forwarded when its command prompt opens
    u8  unknown_F;    // Never read or written by the room; role unproven
} FactoryPanelWork;
STATIC_ASSERT_SIZEOF(FactoryPanelWork, 0x10);

/// Messages the operator panel task receives.
enum {
    /// The lift has finished the movement the panel asked for, whether it ran
    /// to its end or was skipped. Both argument words are zero and no result
    /// is defined.
    FACTORY_PANEL_MESSAGE_MOVE_SETTLED = 0x13F3,
};

/* Defined by each build. */
/// Collision templates the lift and barrier rebuild their grid faces from:
/// the lift raised, the lift turned, and the barrier.
extern WorldCollisionGrid gFactoryLiftTemplate;
extern WorldCollisionGrid gFactoryLiftTurnedTemplate;
extern WorldCollisionGrid gFactoryBarrierTemplate;
/// The room task's message table and the panel session's task descriptors.
extern TaskMessageEntry gFactoryMsgTable[];
extern TaskDesc         gFactoryPanelSessionDesc[];
/// The spawn table and panel descriptor picked for this stage, and the slot
/// holding the panel task.
extern TaskDesc* gFactorySpawnTable;
extern TaskDesc* gFactoryPanelDesc;
extern Task**    gFactoryPanelSlot;
/// The panel's prompt state machine descriptor, its hotspot table ended by
/// `ACTION_PROMPT_HOTSPOT_END`, and its message table.
///
/// The set-up state installs the message table in `Task::msgTable`, which
/// borrows it for as long as the panel can receive messages. Its one callback
/// uses the receiver alone and returns nothing; dispatch forwards whatever the
/// result register holds, so a sender must not read a result from this table.
extern TaskDesc            gFactoryPromptDesc[];
extern ActionPromptHotspot gFactoryPanelHotspots[];
extern TaskMessageEntry    gFactoryPanelMsgTable[2];

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

/// Binds the factory glow callback definition to this carrier's exported task.
///
/// `DRYFIELD_TIME` must select `DRYFIELD_DAY` or `DRYFIELD_NIGHT` before this
/// header is included and remain bound through `factory_draw_glows.inc.c`.
/// Expands to a function identifier with signature `void (Task*)`, declared in
/// the corresponding public room header. It has no arguments, captured values,
/// stringification or token pasting, and does not call or evaluate the task.
#if DRYFIELD_TIME == DRYFIELD_NIGHT
#define FACTORY_DRAW_GLOWS_TASK dryfieldNightFactoryDrawGlowsTask
#else
#define FACTORY_DRAW_GLOWS_TASK dryfieldFactoryDrawGlowsTask
#endif

// Each build exports the view-sprite functions under its
// own name, which gameplay and the other build refer to; the library's names
// map onto the build's own.
#if DRYFIELD_TIME == DRYFIELD_NIGHT
#define factoryShowView9Sprite  factoryNightShowView9Sprite
#define factoryShowView11Sprite factoryNightShowView11Sprite
#else
#define factoryShowView9Sprite  factoryDayShowView9Sprite
#define factoryShowView11Sprite factoryDayShowView11Sprite
#endif

/// Binds the shared factory entry task definition to this room's exported callback.
///
/// `DRYFIELD_TIME` must select `DRYFIELD_DAY` or `DRYFIELD_NIGHT` before this
/// header is included and remain bound through `factory_entry_task.inc.c`.
/// Expands to a function identifier with signature `void (Task*)`, declared in
/// the corresponding public room header and imported by its map overlay. It
/// has no arguments, captured values, stringification or token pasting, and
/// does not call or evaluate the task.
#if DRYFIELD_TIME == DRYFIELD_NIGHT
#define FACTORY_ROOM_INSTANCE_ENTRY_TASK dryfieldNightFactoryEntryTask
#else
#define FACTORY_ROOM_INSTANCE_ENTRY_TASK dryfieldFactoryEntryTask
#endif

void factoryLiftInit(Task* task);
void factoryPowerScene(Task* task);
void factoryWhiteoutScene(Task* task);
/// Restores and then moves the factory barrier's two reserved collision faces.
///
/// Both grid variants need at least two normals/faces and eight leading vertices
/// reserved for the barrier; the template supplies exactly those records. State
/// 0 restores XYZ and faces while preserving SVECTOR pad words. A cleared-barrier
/// flag shifts all eight X coordinates by 2000 world units and ends the task;
/// otherwise state 1 waits for that flag, shifts once and advances to teardown.
/// Runtime stage selects the day grid for daytime Dryfield and the night grid
/// otherwise. Cell lists are retained, so both positions must use their existing
/// reserved memberships. No work block or collision records are owned here.
void factoryBarrierCollision(Task* task);
void factoryLiftUpdate(Task* task);
void factoryLiftBindLighting(Task* task);
void factoryLampScene(Task* task);
void factoryHatchScene(Task* task);
void factoryRoomInit(Task* arg0);
s32  factoryResolveWarp(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
void factoryPanelSpawn(Task* task);
s32  factoryCommand(Task* arg0, s32 arg1, s32 cmd, s32 arg3);
/// Handles factory sound cues delivered by `ROOM_MESSAGE_SOUND`.
///
/// `soundCue` is the first integer payload: 7 starts factory script 7; 21
/// starts script 21 and records the lamp's second position. Other cues do
/// nothing. Uses the current stage's sound bank with centred pan/depth;
/// the receiver, message ID and second payload are ignored. Always returns 0.
s32  factorySoundCommand(Task* task, s32 messageId, s32 soundCue, s32 secondArg);
s32  factoryRoomAction(Task* task, s32 msgId, const void* firstArg, s32 arg3);
void factoryPanelRunStep(Task* task, s16 step);
void factoryPanelInit(Task* task);
void factoryPanelPrompt(Task* task);

/// Dispatches the factory lift task's setup, frame update or teardown state.
///
/// Requires a live task with `Task::state` in 0..2; the three callbacks are
/// copied onto the stack and indexed without a bounds check. Setup owns a
/// `FactoryLiftWork` and a TMD model; movement updates borrow the room-owned
/// panel-task slot passed in `Task::spawnArg2.pointer`. That slot must outlive
/// the lift, and may hold NULL while no panel session is open.
void factoryLiftRun(Task* task);
/// Dispatches the hatch task's setup, per-frame update or teardown state.
///
/// Requires a live hatch task with `Task::state` in 0..2. Copies the carrier's
/// three callbacks onto the stack and calls the selected slot without a bounds
/// check. The update state separately dispatches `FactoryHatchWork::state`;
/// this wrapper does not interpret its movement-handler completion result.
void factoryHatchRun(Task* task);
void factoryCapScene(Task* arg0);
/// Refuses the factory room's `ROOM_MESSAGE_USE_KEY_ITEM` requests.
///
/// Ignores all arguments and returns `ROOM_KEY_ITEM_USE_REFUSED`; no item or
/// room state changes and no payload pointer is retained.
s32 factoryIgnoreMessage(Task* task, s32 messageId, s32 firstArg, s32 secondArg);

static s32 _factoryHatchOpen(Task* task);
static s32 _factoryHatchClose(Task* task);
static s32 _factoryHatchWatch(Task* task);

/// Where `FACTORY_DRAW_GLOWS_TASK` draws the disc nibble 0x48 enables, and the two
/// it alternates between by nibble 0x4A's value.
extern SVECTOR gFactoryGlowPos48;
extern SVECTOR gFactoryGlowPos4A1;
extern SVECTOR gFactoryGlowPos4A2;

#endif /* SRC_SHARED_FACTORY_LIFT_H */
