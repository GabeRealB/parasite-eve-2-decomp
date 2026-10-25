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
 * with lit-panel background sprites and lamp nibble 0x4A; lamp to 2; a barrier
 * transition scene that fades to black, sets 0x47, reloads variant 2 and slides a
 * barrier's collision 2000 units along x), the room's warp filter, its command
 * handler and a one-time examine action.
 *
 * One source builds both packages; the manifest sets `DRYFIELD_TIME` to
 * `DRYFIELD_DAY` or `DRYFIELD_NIGHT` for each.
 */

#ifndef STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_PRIVATE_H
#define STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_PRIVATE_H

#include "types.h"

#include "gameplay/action_prompt.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/geometry.h"
#include "gameplay/pad_script.h"

#include "rooms/dryfield_night_factory.h"

#include "../../dryfield_time.h"

#include "config.h"

#if DRYFIELD_TIME == DRYFIELD_DAY
#include "factory_day.h"
#else
#include "factory_night.h"
#endif

/// Snapshot of the accepted transition, copied by the gate for the deferred event.
///
/// All eight bytes are replaced on another accepted start. Keep this room's
/// snapshot unchanged and loaded until the task commits its destination.
extern RoomEventMsg gRoomEventMsg;

/// Selects the factory's retained destination for deferred room events.
///
/// Include before `room_events.h` in every room translation unit. The binding
/// is a writable `RoomEventMsg` lvalue with no side effects; it allocates no
/// storage. The gate replaces all eight bytes on eligible execution, even if
/// task allocation fails; queries leave them unchanged. Keep the snapshot
/// loaded and unchanged until the task commits area/warp/room and requests reload.
#define ROOM_EVENT_DESTINATION (gRoomEventMsg)

/// Lifecycle slots in the room-entry, lift and hatch task handler tables.
///
/// Task state advances from setup to run; teardown calls `taskKill`.
/// This selector is independent of the hatch work's movement state.
enum {
    FACTORY_TASK_STATE_SETUP = 0,
    FACTORY_TASK_STATE_RUN,
    FACTORY_TASK_STATE_TEARDOWN,
};

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
/// vertical movement then runs towards `FACTORY_LIFT_Y_RAISED_Q16` or 0 and the
/// turn towards `FACTORY_LIFT_YAW_TURNED_Q16` or 0, each by the same steps:
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
    Fixed16 y;           // Model Y translation, 16.16 world units; 0 lowered, `FACTORY_LIFT_Y_RAISED_Q16` raised
    Fixed16 yaw;         // Rotation about Y, 16.16 angle units; 0 home, `FACTORY_LIFT_YAW_TURNED_Q16` turned
    s16     moveFrames;  // Frames since a movement was last requested; gates the button skip
    s8      yawStep;     // Step of the turn (-1 at rest since set-up, 0-3 moving, 4 at rest)
    s8      yStep;       // Step of the vertical movement, as `yawStep`
    MATRIX  light;       // Light-direction matrix the lift model and the hatch draw with
    MATRIX  color;       // Light-colour matrix the lift model and the hatch draw with
} FactoryLiftWork;
STATIC_ASSERT_SIZEOF(FactoryLiftWork, 0x58);

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

/// Choice IDs stored by the operator panel's hotspot table and work block.
enum {
    FACTORY_PANEL_CHOICE_RAISE = 0,
    FACTORY_PANEL_CHOICE_LOWER,
    FACTORY_PANEL_CHOICE_TURN,
    FACTORY_PANEL_CHOICE_CAPTION6,
    FACTORY_PANEL_CHOICE_CAPTION7
};

/// Prompt kinds forwarded by the operator panel's hotspots.
enum {
    FACTORY_PANEL_PROMPT_EXAMINE = 0,
    FACTORY_PANEL_PROMPT_PUSH    = 1
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
    s16 choice;       // Confirmed hotspot ID, a `FACTORY_PANEL_CHOICE_` (0 raise, 1 lower, 2 turn, 3 caption 6, 4 caption 7)
    s8  promptKind;   // `ActionPromptHotspot::promptKind` of that hotspot, forwarded when its command prompt opens
    u8  unknown_F;    // Never read or written by the room; role unproven
} FactoryPanelWork;
STATIC_ASSERT_SIZEOF(FactoryPanelWork, 0x10);

/* Defined by each build. */
/// Unturned lift footprint used to rebuild the live room's moving collision.
///
/// Four normals/faces and eight vertices are in lift-model coordinates;
/// the lift's local matrix applies its current height and yaw. The one-by-two
/// cell directory uses 4000-unit cells and a ten-halfword list pool. The
/// descriptor and pools are borrowed from the loaded factory instance.
extern WorldCollisionGrid gFactoryLiftTemplate;
/// Alternate lift footprint selected while the lift-position TURNED bit is set.
///
/// Four normals/faces and eight model-space vertices use the same topology
/// as `gFactoryLiftTemplate`, with different longitudinal limits. The lift's
/// local matrix still supplies the turn and height. Its one-by-two directory
/// uses 4000-unit cells and an eight-halfword list pool; storage belongs to
/// the loaded factory instance.
extern WorldCollisionGrid gFactoryLiftTurnedTemplate;
/// Closed barrier geometry restored into the live room's leading collision slots.
///
/// Two normals/faces and eight room-space vertices describe its two sides.
/// Clearing the barrier shifts the live copy 2000 world units along X;
/// the source stays in its closed position. The one-cell, 4000-unit grid
/// borrows a four-halfword face-list pool, with one unused trailing word.
/// Keep the defining factory instance loaded while its task reads the source.
extern WorldCollisionGrid gFactoryBarrierTemplate;
/// Borrowed eight-entry factory task descriptor table selected at room setup.
///
/// Runtime stage picks the day table for daytime Dryfield, the night table
/// otherwise. Slots 0..7 are power, lamp, CAP, barrier transition, lift,
/// barrier collision, hatch scene and hatch model. Room commands use 0..3 and
/// 6; room setup uses 4..5. NULL before setup; keep the selected overlay and
/// its callbacks and model resources loaded while spawning or running tasks.
extern TaskDesc* gFactorySpawnTable;

/// Borrowed operator-panel task descriptor selected at factory room setup.
///
/// Runtime stage picks the single day or night panel descriptor.
/// The panel spawner reads entry 0 synchronously; NULL before room setup.
/// Keep the selected overlay loaded while its panel task can run.
extern TaskDesc* gFactoryPanelDesc;

/// Room-owned one-pointer slot shared by the lift and panel-session poller.
///
/// Room setup allocates a zeroed `Task*`, stores the allocation in `Task::work`
/// and publishes its address here; teardown releases that allocation. The
/// lift borrows the slot in `spawnArg2.pointer`, and the poller writes and
/// polls the panel task stored there. The allocation must outlive both users.
/// Panel teardown leaves the stored pointer unchanged; it does not guarantee
/// that a non-NULL value still denotes a live panel. NULL before room setup,
/// and this global itself is not cleared when the room releases its work.
extern Task** gFactoryPanelSlot;
/// The functions each package exports under its own name: the map overlays
/// and the other package call them by these names.
#if DRYFIELD_TIME == DRYFIELD_DAY
#define FACTORY_DRAW_GLOWS_TASK                         dryfieldFactoryDrawGlowsTask
#define FACTORY_ROOM_INSTANCE_SET_VIEW9_SPRITE_VISIBLE  dryfieldFactorySetView9SpriteVisible
#define FACTORY_ROOM_INSTANCE_SET_VIEW11_SPRITE_VISIBLE dryfieldFactorySetView11SpriteVisible
#define FACTORY_ROOM_INSTANCE_ENTRY_TASK                dryfieldFactoryEntryTask
#define FACTORY_OWN_SPAWN_TABLE                         gFactoryDaySpawnTable
#define FACTORY_OWN_PANEL_DESC                          gFactoryDayPanelDesc
#else
#define FACTORY_DRAW_GLOWS_TASK                         dryfieldNightFactoryDrawGlowsTask
#define FACTORY_ROOM_INSTANCE_SET_VIEW9_SPRITE_VISIBLE  dryfieldNightFactorySetView9SpriteVisible
#define FACTORY_ROOM_INSTANCE_SET_VIEW11_SPRITE_VISIBLE dryfieldNightFactorySetView11SpriteVisible
#define FACTORY_ROOM_INSTANCE_ENTRY_TASK                dryfieldNightFactoryEntryTask
#define FACTORY_OWN_SPAWN_TABLE                         gDryfieldNightFactoryTaskDescs
#define FACTORY_OWN_PANEL_DESC                          gDryfieldNightFactoryPanelTaskDesc
#endif

/// Runs the factory power-on CAP choice and restores player/companion control.
///
/// Start in state 0 with a valid in-place CAP command in `spawnArg1.value`.
/// Holds scripted control, hides the player and view-11 sprite while unpowered,
/// then waits for CAP completion. Choice 3 enables power and lamp progress,
/// shows the panel/light sprites and queues that command's explicit variant
/// key 2 with a display transition. Every choice waits for playback before
/// restoring controls/model drawing and killing the task. Borrows the live
/// room's CAP and sprite resources.
void factoryPowerScene(Task* task);
/// Covers the factory barrier change with a thirty-frame subtractive fade.
///
/// Start in state 0 with a valid transition CAP command in `spawnArg1.value`.
/// An already-cleared barrier ends immediately. Otherwise holds both actors'
/// scripted control; after one intervening tick, choice 1 ramps the overlay to
/// black, records the barrier clear and switches live/saved room to 2
/// while requesting object rebuild. The reverse ramp then restores control and
/// ends the task. Other choices restore control on the following tick.
/// Playback completion is not polled. `killCountdown` counts callback ticks, not remaining lifetime;
/// room/CAP/save resources must remain live throughout.
void factoryBarrierTransitionScene(Task* task);
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
void        factoryBarrierCollision(Task* task);
static void _factoryLiftBindLighting(Task* task);
/// Runs the factory lamp scene and records CAP choice 3 before restoring the actors.
///
/// spawnArg1 selects an in-place CAP command in live room resources. Entry holds
/// scripted control for player and companion. The next tick hides the player model
/// unless lamp progress is already 2, then waits for CAP completion. Choice 3 records
/// progress 2; every choice restores both controls, shows the player and kills the
/// task. The task owns no work or playback data. The completion test retains the
/// state local whose value is also the accepted CAP choice.
void factoryLampScene(Task* task);
/// Opens the factory hatch for a timed CAP scene, then restores scripted control.
///
/// spawnArg1 is a valid command in live CAP resources. Entry raises the transient
/// hatch flag. The predecrement countdown advances only below zero, so initial
/// values 60 and 30 produce 61 and 31 waiting ticks. The first wait precedes CAP
/// start and flag clear; the second precedes restoring player/companion control
/// and killing the task. Playback is not polled. The live saved flag stays raised
/// through the first wait; hatch setup must not assume it is always clear.
void factoryHatchScene(Task* task);
/// Resolves factory departures to the driveway, garage and breezeway.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with borrowed complete eight-byte records;
/// request and reply may alias. Copies the request before choosing execution-only
/// destination variants. Garage departure requires lamp progress 2; breezeway
/// departure requires its door-unlocked flag. Refused execution plays CAP and
/// writes 2 to the optional request flag; queries suppress those effects.
/// Driveway departure uses the room event gate while flag 0x30 is set, clearing
/// that latch when execution starts its event. Returns 0 refused, 1 direct or
/// 2 managed.
/// The event gate may retain copies for its deferred task; keep the room loaded.
s32 roomVariantFactoryMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
/// Spawns the selected factory panel script and waits for its exit request.
///
/// The room entry must first publish a live `gFactoryPanelSlot` and a valid
/// `gFactoryPanelDesc`. State 0 stores the spawned task in that borrowed slot;
/// state 1 dispatches its requested exit and kills this poller. The script's
/// result is ignored. Spawning must succeed, since the poll requires a live
/// task; the room entry retains ownership of the slot.
void factoryPanelSpawn(Task* task);
/// Dispatches the factory room's integer command payload in either day or night.
///
/// Commands 1/2/3/5 spawn the CAP/whiteout/lamp/power scene from the session-selected
/// `gFactorySpawnTable`. Command 6 holds and hides the player and companion and
/// starts the operator-panel session. Command 12 holds both and starts the hatch
/// scene only at the exactly turned, lowered lift position. Other commands do
/// nothing. Requires room initialization to have selected the spawn table.
/// The receiver, message ID and second payload are ignored; always returns 0.
s32 factoryCommand(Task* task, s32 messageId, s32 command, s32 unusedArgument);
/// Handles factory sound cues delivered by `ROOM_MESSAGE_SOUND`.
///
/// `soundCue` is the first integer payload: 7 starts factory script 7; 21
/// starts script 21 and records the lamp's second position. Other cues do
/// nothing. Uses the current stage's sound bank with centred pan/depth;
/// the receiver, message ID and second payload are ignored. Always returns 0.
s32 factorySoundCommand(Task* task, s32 messageId, s32 soundCue, s32 secondArg);
/// Starts the factory's one-time action-1 scene from a direction trigger.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a complete four-byte request. Action
/// 1 while flag 0x2C is clear requests CAP command 11, sets that flag, advances
/// the objective byte to 10 and starts daytime factory sound 10.
/// Progress is committed even when CAP is busy and declines the request.
/// Other actions have no effect. Retains no pointer and always returns 0;
/// the receiver, message ID and second payload are unused.
s32         factoryRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _factoryPanelApplyChoice(Task* task, s16 choice);
static void _factoryPanelInit(Task* task);

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
/// Requires a live hatch task with `Task::state` in 0..2. Copies the unit's
/// three callbacks onto the stack and calls the selected slot without a bounds
/// check. The update state separately dispatches `FactoryHatchWork::state`;
/// this wrapper does not interpret its movement-handler completion result.
void factoryHatchRun(Task* task);
/// Runs the factory barrier dialogue once while the barrier remains closed.
///
/// spawnArg1 is a valid command index in the live room CAP resources. A cleared
/// barrier skips playback; both paths kill the task immediately. The callback
/// waits for no playback completion and owns no work allocation.
void factoryCapScene(Task* task);
/// Refuses the factory room's `ROOM_MESSAGE_USE_KEY_ITEM` requests.
///
/// Ignores all arguments and returns `ROOM_KEY_ITEM_USE_REFUSED`; no item or
/// room state changes and no payload pointer is retained.
s32 factoryIgnoreMessage(Task* task, s32 messageId, s32 firstArg, s32 secondArg);

static s32 _factoryHatchOpen(Task* task);
static s32 _factoryHatchClose(Task* task);
static s32 _factoryHatchWatch(Task* task);

#endif /* STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_PRIVATE_H */
