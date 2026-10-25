#include "rooms/dryfield_water_tower.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_water_tower_private.h"

#include "actors/actor_00100.h"
#include "actors/actor_02500.h"
#include "actors/actor_421600.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield.h"

#include "../../shared/room/room_events.h"

#include "assets/engine/animation/player_schema.h"

static void _screenFadeInTask(Task* task);
/// Binds the shared fade-in declaration and definition to this file's private task.
///
/// Requires the bare identifier of the preceding static `void (Task*)` callback.
/// Keep defined from `screen_fade.h` through `screen_fade_in.inc.c`, then undefine.
/// This object-like binding has no arguments, captures or token construction.
#define SCREEN_FADE_IN_TASK _screenFadeInTask
#include "../../shared/screen/screen_fade.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/water_tower.h"

static void _screenFadeOutTask(Task* task);

/// Requests the actor scene's event script posts, held for one frame in
/// `_DryfieldWaterTowerActorSceneWork::request`. The script never posts 3.
enum {
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_NONE,              // Nothing posted
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_SET_UP,            // Place both actors and start their clips, set the player's draw mode 2, fade in
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_MOVE_FIRST_ACTOR,  // Place the first actor again and start its second clip
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_PLACE_PLAYER = 4,  // Show the player and place them at the scene's mark
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_FIRST_ACTOR_CLIP,  // Restart the first actor's opening clip
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_SECOND_ACTOR_CLIP, // Restart the second actor's opening clip
};

/// Work block of the water tower room's actor scene, kept at `Task::work`.
///
/// The scene is the task the prop scene's driver spawns on the room's first
/// action. It starts the player's weapon animation, runs an event script over
/// the room's two placed actors and ends with the event. The script reaches
/// this block through its callbacks, which post `request` for the task to carry
/// out on its next frame.
typedef struct {
    Task* playerTask;       // Player task at allocation; receives the scene's draw-mode and placement messages
    Task* firstActorTask;   // Task of the placed actor whose key is the room's stage and area
    Task* secondActorTask;  // Task of the placed actor with that key and 0x1000 set
    u16   request;          // Request to carry out this frame, then cleared (DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_*)
    s16   field_E;          // Cleared whenever a request is posted and never read; role unproven
    byte  field_10[4];      // Never accessed; role unproven
    s16   actorCommandSent; // Set once the scene has broadcast actor command 0 (0 no, 1 yes)
    byte  field_16[2];      // Never accessed; role unproven
} _DryfieldWaterTowerActorSceneWork;
STATIC_ASSERT_SIZEOF(_DryfieldWaterTowerActorSceneWork, 0x18);

/// Driver frames per stored mechanism-run duration unit (30).
///
/// Multiplying the selected u16 duration produces a u16 limit of 750..1200
/// frames. These are task updates; no conversion to wall-clock seconds is made.
#define DRYFIELD_WATER_TOWER_TIME_LIMIT_UNIT_FRAMES 30

/// Inclusive timeout-count threshold covering every remaining mechanism run.
///
/// The last time-limit entry uses the maximum u16 count so the ascending
/// threshold scan always finds a duration. This entry is consumed normally;
/// its value is a timeout count, not a frame limit or a skipped terminator.
#define DRYFIELD_WATER_TOWER_TIME_LIMIT_ALL_TIMEOUTS 0xFFFF

/// One entry of the mechanism's time limits: how long a timed run lasts, by how
/// many earlier runs have timed out.
///
/// A run takes the first entry whose `maxTimeouts` is not below the count, so
/// the thresholds ascend and the last entry serves every later run. Each
/// timeout therefore buys the next run a longer limit, up to the last entry's.
typedef struct {
    u16 maxTimeouts; // Highest count of timed-out runs the entry serves (DRYFIELD_WATER_TOWER_TIME_LIMIT_ALL_TIMEOUTS in the last entry)
    u16 duration;    // Length of the run, in units of DRYFIELD_WATER_TOWER_TIME_LIMIT_UNIT_FRAMES
} _DryfieldWaterTowerTimeLimit;
STATIC_ASSERT_SIZEOF(_DryfieldWaterTowerTimeLimit, 0x4);

/// Unsigned view-index sentinel ending the mechanism's running-sound volume table.
///
/// The sentinel row's percentage is never read. Unlisted view indices use full
/// script volume, so the lookup must test this marker before accessing a mix.
#define DRYFIELD_WATER_TOWER_VIEW_VOLUME_END 0xFFFF

/// One entry of the room's per-view volume table for the mechanism's running
/// sound.
///
/// While a run is being timed the sound is replayed every frame at the volume
/// of the entry `viewFindLogicalIndex` selects for the view the driver last
/// recorded. A view with no entry plays at full volume.
typedef struct {
    u16 viewIndex; // Logical room-view index (DRYFIELD_WATER_TOWER_VIEW_VOLUME_END ends the table)
    u16 percent;   // Volume in that view, as a percentage of full volume
} _DryfieldWaterTowerViewVolume;
STATIC_ASSERT_SIZEOF(_DryfieldWaterTowerViewVolume, 0x4);

/// Requests the prop scene's event scripts post, held for one frame in
/// `_DryfieldWaterTowerPropSceneWork::request`.
enum {
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_NONE,              // Nothing posted
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_OUT,         // Show the player and start the sliding prop towards its far placement
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_APPLY_VIEW,        // Switch the room to `nextView`
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_BACK,        // Re-enable the trigger a run disables, walk or hide the player, start the sliding prop back
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SHOW_PLAYER,       // Show the player, placed first after a completed run, then switch to `nextView`
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_UNHANDLED,         // Posted by the closing script; the driver has no case for it
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_FALL,              // Start the falling prop's fall to its hanging height
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_DROP,              // Place the player and start the falling prop's drop to the ground
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_RUN_RUMBLE,        // Start the run's vibration script and its sounds
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_BACK_RUMBLE, // Start the slide back's vibration script and sound
    DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_DROP_RUMBLE,       // Start the drop's vibration script and sound
};

/// How a timed run of the mechanism ended, held in
/// `_DryfieldWaterTowerPropSceneWork::runResult` and reported by the run's step.
enum {
    DRYFIELD_WATER_TOWER_RUN_UNDER_WAY, // The step's answer while the run has not ended; never stored
    DRYFIELD_WATER_TOWER_RUN_TIMED_OUT, // The time limit passed; the driver waits for the next run
    DRYFIELD_WATER_TOWER_RUN_COMPLETED, // The player took the room's second action in time; the reason byte of that action
};

/// Phases of a timed run, held in `_DryfieldWaterTowerPropSceneWork::phase`
/// while the driver is in its run state.
enum {
    DRYFIELD_WATER_TOWER_RUN_PHASE_START,          // Play the opening script, or restore the opened room directly on a later run
    DRYFIELD_WATER_TOWER_RUN_PHASE_OPENING_SCRIPT, // Wait for the opening script's event to end
    DRYFIELD_WATER_TOWER_RUN_PHASE_START_TIMER,    // Command the actors and restart the frame count
    DRYFIELD_WATER_TOWER_RUN_PHASE_TIMING,         // Count frames until the limit or the room's second action
    DRYFIELD_WATER_TOWER_RUN_PHASE_ENDING_SCRIPT,  // Wait for the ending script's event to end and report the result
};

/// Phases of the driver's two steps after a completed run, held in
/// `_DryfieldWaterTowerPropSceneWork::phase`.
enum {
    DRYFIELD_WATER_TOWER_CLOSING_PHASE_START, // Send the step's command or start its script
    DRYFIELD_WATER_TOWER_CLOSING_PHASE_WAIT,  // Wait for the answer or for the script's event to end
};

/// Phases of the sliding prop's slide in either direction, held in
/// `_DryfieldWaterTowerPropSceneWork::phase`.
enum {
    DRYFIELD_WATER_TOWER_PROP_SLIDE_MOVING,   // Advancing in Z towards the end placement, raising dust
    DRYFIELD_WATER_TOWER_PROP_SLIDE_SETTLING, // Held at the end placement until the settle count runs out
};

/// Settling updates before either slide direction applies its final placement.
///
/// Prop commands reset the signed-halfword counter to zero. It increments before
/// comparison, so final placement occurs on settling call 61 in either direction.
#define DRYFIELD_WATER_TOWER_PROP_SLIDE_SETTLE_FRAMES 61

/// Phases of the falling prop's fall to its hanging height, held in
/// `_DryfieldWaterTowerPropSceneWork::phase`. The free-falling copy has no
/// phases of its own: it leaves the first one on the frame it is placed.
enum {
    DRYFIELD_WATER_TOWER_PROP_FALL_SPAWN_COPY, // Spawn the free-falling copy
    DRYFIELD_WATER_TOWER_PROP_FALL_START_COPY, // Start the copy falling
    DRYFIELD_WATER_TOWER_PROP_FALL_FALLING,    // Accelerating downwards until the hanging height is passed
    DRYFIELD_WATER_TOWER_PROP_FALL_HANGING,    // At rest at the hanging height
};

/// Phases of the falling prop's drop from its hanging height to the ground,
/// held in `_DryfieldWaterTowerPropSceneWork::phase`.
enum {
    DRYFIELD_WATER_TOWER_PROP_DROP_START,    // Remove the free-falling copy and take the drop's start placement
    DRYFIELD_WATER_TOWER_PROP_DROP_DROPPING, // Descending at a constant rate until the ground placement is passed
    DRYFIELD_WATER_TOWER_PROP_DROP_SETTLING, // Shaking at the ground placement until the settle count runs out
};

/// Settling updates until the dropped prop installs collision and reports arrival.
///
/// The command clears the signed-halfword counter. Settling increments it
/// before testing >=, so arrival is reported on update 11; the first ten
/// settling updates apply the ground shake.
#define DRYFIELD_WATER_TOWER_PROP_DROP_SETTLE_FRAMES 11

/// Work block of each task in the water tower room's prop scene, kept at `Task::work`.
///
/// The scene is three tasks spawned from one table: a driver without a model,
/// which sequences the room's mechanism and carries out the requests its event
/// scripts post, and two prop tasks, one whose model slides across the room
/// and one whose model falls from above. Each allocates its own zeroed copy of
/// this block and stores the player task in it. Beyond that the driver uses the
/// task pointers, the request and the run fields, and the props use their
/// matrices and motion fields; `phase` and `fallingPropTask` serve both.
///
/// The mechanism is operated in timed runs. A run slides the prop out and
/// counts frames against a time limit; it ends when the limit passes or when
/// the player takes the room's second action, and either way the prop slides
/// back. A completed run goes on to the falling prop's fall and drop.
typedef struct {
    MATRIX lightMtx;            // Props: light matrix the model is drawn with
    MATRIX colorMtx;            // Props: colour matrix the model is drawn with
    Task*  playerTask;          // Player task at allocation; the driver shows, hides and places its model
    Task*  slidingPropTask;     // Driver: the sliding prop's task, which it spawns
    Task*  fallingPropTask;     // Driver: the falling prop's task, which it spawns. Falling prop: the free-falling copy of itself it spawns
    Task*  actorSceneTask;      // Driver: the actor scene's task, which it spawns and waits for
    Task*  padScriptTask;       // Driver: task of the vibration script last started; never read
    byte   field_54[4];         // Never accessed; role unproven
    u16    phase;               // Phase of the motion or driver step in progress; each numbers its own from 0
    s16    settleFrames;        // Props: frames spent settling at the end of a slide or drop
    u16    request;             // Driver: request to carry out this frame, then cleared (DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_*)
    s16    field_5E;            // Cleared whenever a request is posted and never read; role unproven
    s16    field_60;            // Cleared whenever a prop is commanded into a new state and never read; role unproven
    byte   field_62[2];         // Never accessed; role unproven
    u16    mechanismState;      // Driver: the mechanism-state flag as read when the props were placed; 2 once a run has been timed
    u16    runResult;           // Driver: how the last run ended (DRYFIELD_WATER_TOWER_RUN_TIMED_OUT, DRYFIELD_WATER_TOWER_RUN_COMPLETED)
    s16    nextView;            // Driver: 1-based view slot the next view change switches the room to
    s16    fallSpeed;           // Falling prop: distance fallen each frame, growing by 4 a frame
    u16    actorEventReceived;  // Driver: an actor has reported an event to the room (0 no, 1 yes)
    u16    runRequested;        // Driver: the room has asked for a run; cleared as the run starts (0 no, 1 yes)
    u16    shadowEnabled;       // Falling prop: draw its ground shadow (0 no, 1 yes)
    u16    timeouts;            // Driver: runs that have timed out; selects the next run's time limit
    s16    currentView;         // Driver: the session's view slot on the last frame the driver ran
    u16    reequipRequested;    // Driver: the weapon re-equip has been requested, which happens once (0 no, 1 yes)
    u16    runningSoundStarted; // Driver: the run's sound loop has been started (0 no, 1 yes)
    byte   field_7A[2];         // Never accessed; role unproven
} _DryfieldWaterTowerPropSceneWork;
STATIC_ASSERT_SIZEOF(_DryfieldWaterTowerPropSceneWork, 0x7C);

/// `gPlayerStatus.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on and
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two weapon-id bases that record uses; the
/// alternate block is indexed by `gPlayerStatus.weapon` plus 1 against the base block's
/// plus 0x22.

/// The lowered-cap collision patch: two normals, two faces and eight vertices.
///
/// Installed into grid normals 2..3, faces 2..3 and vertices 8..15 when the cap
/// reaches its lower position. The live grid and these template arrays belong
/// to this room overlay.
static SVECTOR                _gDryfieldWaterTowerCollision04550[2];
static WorldCollisionGridFace _gDryfieldWaterTowerCollision045E0[2];
static SVECTOR                _gDryfieldWaterTowerCollision04560[8];

/// Collision patches for the raised and lowered cap positions.
///
/// Each patch supplies two normals, two faces and eight vertices for a section
/// of the room's live grid. The first section starts at normal 0, face 0 and
/// vertex 0; the second starts at normal 2, face 2 and vertex 8.
static SVECTOR                _gDryfieldWaterTowerCollision045A0[8];
static SVECTOR                _gDryfieldWaterTowerCollision045F8[2];
static SVECTOR                _gDryfieldWaterTowerCollision04608[8];
static SVECTOR                _gDryfieldWaterTowerCollision04648[8];
static WorldCollisionGridFace _gDryfieldWaterTowerCollision04688[2];

static u16 _dryfieldWaterTowerStepTimedRun(Task* task);
static s32 _dryfieldWaterTowerStepPropDrop(Task* task);

static WorldCoordRoomLights _gDryfieldWaterTowerRoomLights;

static WorldCollisionGrid    _gDryfieldWaterTowerRoomCollisionGrid;
static WorldCollisionTrigger _gDryfieldWaterTowerViewBoundaryTriggers[14];
static WorldCollisionTrigger _gDryfieldWaterTowerActionTriggers[24];
static void                  _dryfieldWaterTowerActorSceneTask(Task* task);
static void                  _dryfieldWaterTowerStageActorSceneAudioStart(void);
static void                  _dryfieldWaterTowerStartActorScenePlayback(void);
static void                  _dryfieldWaterTowerFinishActorScenePlayback(void);
static void                  _dryfieldWaterTowerPostActorSceneRequest(s16 request);
static void                  _dryfieldWaterTowerStartChaserBattleOnce(void);
static void                  _dryfieldWaterTowerSkipActorScene(void);

static TmdSource _gDryfieldWaterTowerDryfieldWaterTankModel020D4;
static TmdSource _gDryfieldWaterTowerModel03D14;
static void      _dryfieldWaterTowerFallingPropTask(Task* task);
static void      _dryfieldWaterTowerSlidingPropTask(Task* task);
static void      _dryfieldWaterTowerPropSceneTask(Task* task);
static void      _dryfieldWaterTowerRestorePlayerWeaponAnimation(s32 blendFrames);
static void      _dryfieldWaterTowerRequestWeaponReequipOnce(void);
static void      _dryfieldWaterTowerLockAttachmentsForPropScene(void);
static void      _dryfieldWaterTowerPostPropSceneRequest(s16 request);
static void      _dryfieldWaterTowerSkipRunOpening(void);
static void      _dryfieldWaterTowerSkipRunEnding(void);
static void      _dryfieldWaterTowerSkipPropClosingScene(void);
static void      _dryfieldWaterTowerLatchActorEvent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
static void      _dryfieldWaterTowerRequestRun(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Resting state of both prop tasks after a skip placement stops their motion.
enum { DRYFIELD_WATER_TOWER_PROP_STATE_IDLE = 1 };

/// Task states shared by commands to the sliding and falling props.
enum {
    DRYFIELD_WATER_TOWER_PROP_STATE_INITIALIZE         = 0,
    DRYFIELD_WATER_TOWER_SLIDING_PROP_STATE_SLIDE_OUT  = 2,
    DRYFIELD_WATER_TOWER_SLIDING_PROP_STATE_SLIDE_BACK = 3,
    DRYFIELD_WATER_TOWER_FALLING_PROP_STATE_FALL       = 2,
    DRYFIELD_WATER_TOWER_FALLING_PROP_STATE_DROP       = 3,
};

/// States of the prop-scene driver, in execution order.
enum {
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_INITIALIZE,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_SPAWN_PROPS,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_RESTORE_PLACEMENTS,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_FIRST_ACTION,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_ACTOR_SCENE,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_RUN_REQUEST,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_TIMED_RUN,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_CHASERS,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_CLOSING_SCENE,
    DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_FINISHED,
};

/// Saved progression through the room's first action and completed prop scene.
enum {
    DRYFIELD_WATER_TOWER_PROGRESS_INITIAL,
    DRYFIELD_WATER_TOWER_PROGRESS_ACTOR_SCENE_STARTED,
    DRYFIELD_WATER_TOWER_PROGRESS_COMPLETE,
};

/// Action payloads from the room's collision triggers.
enum {
    DRYFIELD_WATER_TOWER_ACTION_START_ACTOR_SCENE = 1,
    DRYFIELD_WATER_TOWER_ACTION_COMPLETE_RUN      = 2,
};

/// Live trigger indices and mapped views used by the mechanism scene.
enum {
    DRYFIELD_WATER_TOWER_FIRST_ACTION_TRIGGER = 6,
    DRYFIELD_WATER_TOWER_MECHANISM_TRIGGER    = 9,
    DRYFIELD_WATER_TOWER_RUN_ACTION_TRIGGER   = 20,
    DRYFIELD_WATER_TOWER_RUN_MAPPED_VIEW      = 7,
    DRYFIELD_WATER_TOWER_CLOSING_MAPPED_VIEW  = 9,
};

/// Task-table slots and room-bank sound entries selected by the scene.
enum {
    DRYFIELD_WATER_TOWER_PROP_TASK_SLOT_SLIDING           = 1,
    DRYFIELD_WATER_TOWER_PROP_TASK_SLOT_FALLING           = 2,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_TASK_SLOT            = 0,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_FADE_IN_TASK_SLOT    = 2,
    DRYFIELD_WATER_TOWER_OPENING_SOUND_ENTRY              = 6,
    DRYFIELD_WATER_TOWER_ENDING_SOUND_ENTRY               = 7,
    DRYFIELD_WATER_TOWER_REPEATED_RUN_OPENING_ATTENUATION = 32,
};

/// Full script volume, and the game-tick bit used to alternate prop shake offsets.
enum {
    DRYFIELD_WATER_TOWER_FULL_SOUND_VOLUME   = 127,
    DRYFIELD_WATER_TOWER_PROP_SHAKE_TICK_BIT = 1 << 2,
};

/// Audio updates used to fade the ending and drop sounds when their scenes are skipped.
enum { DRYFIELD_WATER_TOWER_SKIP_SOUND_FADE_TICKS = 10 };

static s32 _dryfieldWaterTowerApplyPropCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedSecondArg);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

TaskMessageEntry gDryfieldWaterTowerRoomMessages[7] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantWaterTowerMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldWaterTowerUseKeyItemMsg },
    { DIRECTION_MESSAGE_ROOM_ACTION, dryfieldWaterTowerRoomActionMsg },
    { ROOM_MESSAGE_COMMAND, dryfieldWaterTowerCommandMessage },
    { ROOM_MESSAGE_SOUND, waterTowerSoundMsg },
    { ROOM_MESSAGE_ACTOR_EVENT, dryfieldWaterTowerActorEventMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gDryfieldWaterTowerMechanismPromptTasks[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldWaterTowerMechanismPromptTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static TmdBone _gDryfieldWaterTowerDryfieldWaterTankModel020D4Skeleton[1] = {
#include "assets/dryfield_water_tank_model_020D4_skeleton.inc"
};

static u32 _gDryfieldWaterTowerDryfieldWaterTankModel020D4PartVerts[1] = {
#include "assets/dryfield_water_tank_model_020D4_partVerts.inc"
};

static SVECTOR _gDryfieldWaterTowerDryfieldWaterTankModel020D4Verts[97] = {
#include "assets/dryfield_water_tank_model_020D4_verts.inc"
};

static u32 _gDryfieldWaterTowerDryfieldWaterTankModel020D4Stream[426] = {
#include "assets/dryfield_water_tank_model_020D4_stream.inc"
};

static TmdSource _gDryfieldWaterTowerDryfieldWaterTankModel020D4 = {
    0,
    3360,
    0,
    1,
    _gDryfieldWaterTowerDryfieldWaterTankModel020D4PartVerts,
    _gDryfieldWaterTowerDryfieldWaterTankModel020D4Verts,
    &_gDryfieldWaterTowerDryfieldWaterTankModel020D4Verts[97],
    _gDryfieldWaterTowerDryfieldWaterTankModel020D4Skeleton,
    _gDryfieldWaterTowerDryfieldWaterTankModel020D4Stream,
};

static TmdBone _gDryfieldWaterTowerModel03D14Skeleton[1] = {
#include "assets/dryfield_water_tower_model_03D14_skeleton.inc"
};

static u32 _gDryfieldWaterTowerModel03D14PartVerts[1] = {
#include "assets/dryfield_water_tower_model_03D14_partVerts.inc"
};

static SVECTOR _gDryfieldWaterTowerModel03D14Verts[152] = {
#include "assets/dryfield_water_tower_model_03D14_verts.inc"
};

static u32 _gDryfieldWaterTowerModel03D14Stream[466] = {
#include "assets/dryfield_water_tower_model_03D14_stream.inc"
};

static TmdSource _gDryfieldWaterTowerModel03D14 = {
    0,
    3680,
    0,
    1,
    _gDryfieldWaterTowerModel03D14PartVerts,
    _gDryfieldWaterTowerModel03D14Verts,
    &_gDryfieldWaterTowerModel03D14Verts[152],
    _gDryfieldWaterTowerModel03D14Skeleton,
    _gDryfieldWaterTowerModel03D14Stream,
};

/// Endpoints of the sliding prop's motion, indexed by its placement role.
enum {
    DRYFIELD_WATER_TOWER_SLIDING_PROP_HOME,
    DRYFIELD_WATER_TOWER_SLIDING_PROP_EXTENDED,
};

/// Home and extended placements of the mechanism's sliding prop.
///
/// Positions use room-coordinate units; yaw is a quarter turn in both records.
/// Motion and skip handlers share these endpoints. The room owns the records
/// and placement messages borrow them only for synchronous dispatch.
static ActorTransform _gDryfieldWaterTowerSlidingPropPlacements[2] = {
    { { 3100, 0, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { { 3100, 0, 1800, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
};

/// Placement roles used by the falling prop and its free-falling copy.
enum {
    DRYFIELD_WATER_TOWER_FALLING_PROP_UPPER,
    DRYFIELD_WATER_TOWER_FALLING_PROP_HANGING,
    DRYFIELD_WATER_TOWER_FALLING_PROP_GROUND,
};

/// Upper, hanging and ground placements for the falling prop.
///
/// Positions use room units; every yaw is three quarters of a turn. The upper
/// mark also starts the free-falling copy. The hanging and ground heights stop
/// the two motion steps, and restoration/skip paths share the ground record.
/// Placement messages borrow a single record synchronously.
static ActorTransform _gDryfieldWaterTowerFallingPropPlacements[3] = {
    { { -2900, -8000, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 } },
    { { -2900, -4000, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 } },
    { { -2500, -200, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 } },
};

/// Start of the prop's straight drop, shifted 400 room units from its hanging X.
///
/// Keeps the hanging height and three-quarter-turn yaw, directly above the
/// ground mark. The drop command borrows this record during placement dispatch.
static ActorTransform _gDryfieldWaterTowerPropDropStartPlacement = { { -2500, -4000, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 } };

/// Player placement roles in the completed run's closing sequence.
enum {
    DRYFIELD_WATER_TOWER_PLAYER_CLOSING_MARK,
    DRYFIELD_WATER_TOWER_PLAYER_CLOSING_WALK_START,
};

/// Closing mark and walk-start placement for the player after a completed run.
///
/// The player starts 600 room units to the right of the mark, then approaches
/// it as the prop returns. Skip and drop requests place the player at the mark.
/// Both records face three quarters of a turn; messages borrow them during dispatch.
static ActorTransform _gDryfieldWaterTowerClosingPlayerPlacements[2] = {
    { { 1900, 0, -270, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 } },
    { { 2500, 0, -270, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 } },
};

/// Placement and motion-command receivers shared by the sliding and falling props.
///
/// No terminator is stored: dispatch only `ACTOR_MESSAGE_PLACE` with a borrowed
/// `ActorTransform`, or `ACTOR_COMMAND_MESSAGE_APPLY` with an `ActorCommand`.
/// Commands 2/3 select each prop's two motions. Placement defines no reply;
/// the command handler returns the selected state. Keep this room loaded.
static TaskMessageEntry _gDryfieldWaterTowerPropMessages[2] = {
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
    { ACTOR_COMMAND_MESSAGE_APPLY, _dryfieldWaterTowerApplyPropCommand },
};

static SVECTOR _gDryfieldWaterTowerCollision04550[2] = {
#include "assets/dryfield_water_tower_collision_04550.inc"
};

static SVECTOR _gDryfieldWaterTowerCollision04560[8] = {
#include "assets/dryfield_water_tower_collision_04560.inc"
};

static SVECTOR _gDryfieldWaterTowerCollision045A0[8] = {
#include "assets/dryfield_water_tower_collision_045A0.inc"
};

static WorldCollisionGridFace _gDryfieldWaterTowerCollision045E0[2] = {
#include "assets/dryfield_water_tower_collision_045E0.inc"
};

static SVECTOR _gDryfieldWaterTowerCollision045F8[2] = {
#include "assets/dryfield_water_tower_collision_045F8.inc"
};

static SVECTOR _gDryfieldWaterTowerCollision04608[8] = {
#include "assets/dryfield_water_tower_collision_04608.inc"
};

static SVECTOR _gDryfieldWaterTowerCollision04648[8] = {
#include "assets/dryfield_water_tower_collision_04648.inc"
};

static WorldCollisionGridFace _gDryfieldWaterTowerCollision04688[2] = {
#include "assets/dryfield_water_tower_collision_04688.inc"
};

/// Signed local X offsets for the sliding prop's cyclic dust puffs.
///
/// The cursor advances before spawning and wraps through entries 0..10.
/// Offsets use model-local game units. The final stored zero is never selected;
/// its role is unproven, and the full twelve-halfword extent is retained.
static s16 _gDryfieldWaterTowerPropSlideDustXOffsets[12] = {
    400,
    1000,
    -400,
    -200,
    0,
    -600,
    200,
    800,
    -1000,
    -800,
    600,
    0,
};

/// CAP slots and frame waits shared by this room's event streams.
enum {
    DRYFIELD_WATER_TOWER_CAP_ACTOR_SCENE            = 1,
    DRYFIELD_WATER_TOWER_CAP_RUN                    = 2,
    DRYFIELD_WATER_TOWER_CAP_PROP_CLOSING           = 3,
    DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES = 3,
    DRYFIELD_WATER_TOWER_SCRIPT_SKIP_FADE_FRAMES    = 8,
    DRYFIELD_WATER_TOWER_SCRIPT_WEAPON_BLEND_FRAMES = 10,
    DRYFIELD_WATER_TOWER_SCRIPT_SKIP_KEEP_SOUND     = 1,
};

/// First-run opening: slide the mechanism out, start rumble/sound and apply the run view.
///
/// CAP command 2 supplies the cues. Skipping keeps sound for the paired
/// `_gDryfieldWaterTowerRunOpeningSkipScript`; the last message releases player
/// scripted control. The interpreter borrows this END-terminated stream and
/// requires the loaded room and initialized prop-scene driver through playback.
static EvsCommand _gDryfieldWaterTowerRunOpeningScript[14] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = DRYFIELD_WATER_TOWER_SCRIPT_SKIP_KEEP_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldWaterTowerRestorePlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_WATER_TOWER_CAP_RUN }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_RUN_RUMBLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_APPLY_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip stream that restores the open mechanism and run view behind a screen fade.
///
/// Completes the opening through its skip callback, cleans up the event and
/// releases player scripted control. Borrowed alongside the normal opening
/// stream until interpreter completion; the live driver and sliding prop are required.
static EvsCommand _gDryfieldWaterTowerRunOpeningSkipScript[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerSkipRunOpening }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Run ending that slides the mechanism home after a timeout or successful action.
///
/// CAP command 2 supplies cues; the driver result chooses player placement or
/// hiding and the next view. Locks attachments, restores the weapon pose and
/// starts slide-back rumble before releasing player control. Sound survives
/// input skipping. The END-terminated stream borrows the room and live driver.
static EvsCommand _gDryfieldWaterTowerRunEndingScript[15] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = DRYFIELD_WATER_TOWER_SCRIPT_SKIP_KEEP_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_WATER_TOWER_CAP_RUN }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_BACK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerLockAttachmentsForPropScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldWaterTowerRestorePlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_BACK_RUMBLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SHOW_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip stream that restores the closed mechanism and the selected post-run view.
///
/// Its callback stops rumble/run sounds and places the player only for a
/// completed run. Fade and cleanup precede control release. Borrowed with the
/// normal ending stream; player, driver and sliding prop must remain live.
static EvsCommand _gDryfieldWaterTowerRunEndingSkipScript[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerSkipRunEnding }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Final prop scene: cue the fall, drop and weapon re-equip after the chaser battle.
///
/// CAP command 3 supplies cues; request 5 is retained although the driver has
/// no handler for it. Drop rumble accompanies the fall/drop sequence. The
/// END-terminated stream requires the loaded room and live driver, props and player.
static EvsCommand _gDryfieldWaterTowerPropClosingScript[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_WATER_TOWER_CAP_PROP_CLOSING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerLockAttachmentsForPropScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldWaterTowerRestorePlayerWeaponAnimation }, { .value = DRYFIELD_WATER_TOWER_SCRIPT_WEAPON_BLEND_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_UNHANDLED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_FALL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_DROP_RUMBLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostPropSceneRequest }, { .value = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_DROP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerRequestWeaponReequipOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip stream that restores the final ground placement, collision and player mark.
///
/// Finishes behind a fade, performs event cleanup and the once-only weapon
/// re-equip/battle release, then releases player control. Borrowed with the
/// normal closing stream; its driver, player, falling prop and grid must be live.
static EvsCommand _gDryfieldWaterTowerPropClosingSkipScript[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerSkipPropClosingScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerRequestWeaponReequipOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Running-sound volume percentages for selected logical water-tower views.
///
/// Eight entries precede `DRYFIELD_WATER_TOWER_VIEW_VOLUME_END`; unlisted views
/// use full volume (127). Listed percentages scale 127 with truncating integer
/// division. The sentinel's percentage word is not consumed.
static _DryfieldWaterTowerViewVolume _gDryfieldWaterTowerRunningSoundViewVolumes[9] = {
    { 3, 60 },
    { 4, 75 },
    { 5, 50 },
    { 6, 50 },
    { 7, 75 },
    { 8, 90 },
    { 19, 75 },
    { 20, 100 },
    { DRYFIELD_WATER_TOWER_VIEW_VOLUME_END, 0xFFFF },
};

/// Actor-event and timed-run request receivers for the persistent prop-scene driver.
///
/// No terminator is stored: only `ROOM_MESSAGE_ACTOR_EVENT` (forwarded by the
/// room) and `DRYFIELD_WATER_TOWER_MESSAGE_REQUEST_RUN` (from the prompt) are valid.
/// Both ignore their integer payloads and define no reply; senders must discard
/// the result. Installed after driver work allocation and borrowed while the room is loaded.
static TaskMessageEntry _gDryfieldWaterTowerPropSceneMessages[2] = {
    { ROOM_MESSAGE_ACTOR_EVENT, _dryfieldWaterTowerLatchActorEvent },
    { DRYFIELD_WATER_TOWER_MESSAGE_REQUEST_RUN, _dryfieldWaterTowerRequestRun },
};

TaskDesc gDryfieldWaterTowerPropSceneTasks[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _dryfieldWaterTowerPropSceneTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _dryfieldWaterTowerSlidingPropTask, { .model = &_gDryfieldWaterTowerDryfieldWaterTankModel020D4 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _dryfieldWaterTowerFallingPropTask, { .model = &_gDryfieldWaterTowerModel03D14 } },
};

/// Player mark at the introductory actor scene's end, shared by playback and skipping.
///
/// Position uses room units and yaw is three quarters of a turn. Placement
/// messages borrow this record synchronously after making the player visible.
static ActorTransform _gDryfieldWaterTowerActorScenePlayerPlacement = { { -700, -1, -4500, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0 } };

/// Actor marks and animation-request slots for the introductory scene.
enum {
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_START              = 0,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_END               = 1,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_END                = 2,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_START             = 3,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_OPENING_CLIP       = 0,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_OPENING_CLIP      = 1,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_CLOSING_CLIP       = 2,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_OPENING_ANIMATION  = 13,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_OPENING_ANIMATION = 14,
    DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_CLOSING_ANIMATION  = 15,
};

/// Start/end marks of the two introductory actors, in room-coordinate units.
///
/// Slots 0/2 are the first actor's start/end; 3/1 are the second actor's
/// start/skip-end. The latter pair shares position and yaw but changes pitch
/// from turn-minus-1/32 to turn-minus-1/16. Dispatch borrows one record at a time.
static ActorTransform _gDryfieldWaterTowerActorScenePlacements[4] = {
    { { -4465, -1, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { { -4000, 100, -1200, 0 }, { ACTOR_TRANSFORM_ANGLE_TURN - ACTOR_TRANSFORM_ANGLE_TURN / 16, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { { -4465, -1, 1000, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { { -4000, 100, -1200, 0 }, { ACTOR_TRANSFORM_ANGLE_TURN - ACTOR_TRANSFORM_ANGLE_TURN / 32, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
};

/// Opening clips for the first/second chasers and the first chaser's closing clip.
///
/// IDs 13..15 belong to actor_421600's animation bank. The receiver copies
/// only `animationId` into its scripted-animation state and ignores the other
/// request fields. Each request is borrowed synchronously; the room owns the array.
static AnimationPlayRequest _gDryfieldWaterTowerActorSceneAnimations[3] = {
    { { .index = 0 }, DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_OPENING_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_OPENING_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_CLOSING_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Scene/audio key for the introductory water-tower actor playback.
///
/// Group 2, stream 16 and qualifier 11 select the current folder's stream;
/// the absent second qualifier is zero. Borrowed until event-script completion.
static EvsSceneKey _gDryfieldWaterTowerActorSceneStreamKey = { 2, 16, 11 };

/// Introductory actor event: select the stream, stage both chasers and start their battle.
///
/// CAP command 1 supplies the cues. Scene selection precedes deferred audio and
/// playback; later requests change clips and show/place the player. The paired
/// input skip stream restores final marks and starts the same battle once.
/// END-terminated and borrowed while this room, the driver and both actors remain live.
static EvsCommand _gDryfieldWaterTowerActorSceneScript[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_WATER_TOWER_CAP_ACTOR_SCENE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gDryfieldWaterTowerActorSceneStreamKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerStageActorSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostActorSceneRequest }, { .value = DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_SET_UP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerStartActorScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostActorSceneRequest }, { .value = DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_SECOND_ACTOR_CLIP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostActorSceneRequest }, { .value = DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_FIRST_ACTOR_CLIP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostActorSceneRequest }, { .value = DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_MOVE_FIRST_ACTOR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerFinishActorScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerStartChaserBattleOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWaterTowerPostActorSceneRequest }, { .value = DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_PLACE_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_SCRIPT_REQUEST_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum { DRYFIELD_WATER_TOWER_ACTOR_SCENE_SKIP_FADE_WAIT_FRAMES = 8 };

/// Skips the introductory actor scene to its final marks and starts the chaser battle.
///
/// Under the primary fade, places both actors and the player, cancels streaming
/// and refreshes mapped view 4. Returns the fade before starting the battle once
/// and releasing player control. Eleven records include END; the paired normal
/// stream's actor-scene work and all three actors must remain live until completion.
static EvsCommand _gDryfieldWaterTowerActorSceneSkipScript[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_ACTOR_SCENE_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerSkipActorScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TOWER_ACTOR_SCENE_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTowerStartChaserBattleOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Introductory actor-scene task and its fade-out/fade-in tasks, in slots 0..2.
///
/// All are bodyless, priority 192. Spawn only an in-bounds slot: no TASK_DESC_END
/// is stored. The scene spawns slot 2 with an eight-frame fade; slot 1 has no
/// recovered spawn. Tasks borrow this room's callbacks through teardown.
static TaskDesc _gDryfieldWaterTowerActorSceneTasks[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _dryfieldWaterTowerActorSceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenFadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenFadeInTask, { .value = 0 } },
};

/// Ambient-effect gates for mapped views 1..21, indexed by view minus one.
///
/// Enabled views admit the room's dust effects; disabled views suppress them.
/// The following stored halfword has an unproven role and is retained in this
/// declaration until its object boundary is established. It is not a view mode.
static u16 _gDryfieldWaterTowerViewEffectModes[22] = {
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    0x0EFE,
};

WorldCollisionRoomResources gDryfieldWaterTowerRoomCollisionResources[1] = {
    { &_gDryfieldWaterTowerRoomCollisionGrid, _gDryfieldWaterTowerViewBoundaryTriggers, _gDryfieldWaterTowerActionTriggers, NULL },
};

u8* gDryfieldWaterTowerRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldWaterTowerRoomViewCounts[1] = { 21 };

WorldCoordRoomLighting gDryfieldWaterTowerRoomLighting[1] = {
    { &_gDryfieldWaterTowerRoomLights, NULL },
};

/// Room-local map-marker flags and arrival views for the four warp endpoints.
enum {
    DRYFIELD_WATER_TOWER_KITCHEN_MAP_MARKER_FLAG   = 477,
    DRYFIELD_WATER_TOWER_BREEZEWAY_MAP_MARKER_FLAG = 476,
    DRYFIELD_WATER_TOWER_KITCHEN_ARRIVAL_VIEW      = 4,
    DRYFIELD_WATER_TOWER_BREEZEWAY_ARRIVAL_VIEW    = 20,
    DRYFIELD_WATER_TOWER_TANK_ARRIVAL_VIEW         = 9,
    DRYFIELD_WATER_TOWER_MOTEL_ROOM_6_ARRIVAL_VIEW = 3,
};

DirectionWarpEntry gDryfieldWaterTowerWarpEntries[4] = {
    { { { .word = 0 }, -2437, 0, -5662 }, { 0, 0, 0, 0 }, { { .word = 0 }, -2437, 0, -5662 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 4), SOUND_WATER_TOWER_KITCHEN_DOOR_OPEN, DIRECTION_WARP_SOUND_NONE, DRYFIELD_WATER_TOWER_KITCHEN_ARRIVAL_VIEW, DIRECTION_WARP_FLAG_NONE, DRYFIELD_WATER_TOWER_KITCHEN_MAP_MARKER_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 5646, 0, 1800 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 5646, 0, 1800 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 2), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 1), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 8), DRYFIELD_WATER_TOWER_BREEZEWAY_ARRIVAL_VIEW, DIRECTION_WARP_FLAG_NONE, DRYFIELD_WATER_TOWER_BREEZEWAY_MAP_MARKER_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -2186, 1, 170 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -2186, 1, 170 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 5), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 5), DIRECTION_WARP_SOUND_NONE, DRYFIELD_WATER_TOWER_TANK_ARRIVAL_VIEW, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 5500, 1, -5310 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 5500, 1, -5310 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 5), DIRECTION_WARP_SOUND_NONE, DRYFIELD_WATER_TOWER_MOTEL_ROOM_6_ARRIVAL_VIEW, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Writable room-space collision normals, with 4096 representing unit length.
static SVECTOR _gDryfieldWaterTowerRoomCollisionGridNormals[29] = {
#include "assets/dryfield_water_tower_collision_06004_normals.inc"
};

/// Writable room-space mesh vertices, including the two prop collision patches.
static SVECTOR _gDryfieldWaterTowerRoomCollisionGridVerts[175] = {
#include "assets/dryfield_water_tower_collision_06004_verts.inc"
};

/// Indexed room mesh faces, using surface classes 0..2.
///
/// Prop motion replaces faces 0..3 in place while preserving their cell membership.
static WorldCollisionGridFace _gDryfieldWaterTowerRoomCollisionGridFaces[73] = {
#include "assets/dryfield_water_tower_collision_06004_faces.inc"
};

/// Sixteen packed cell face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gDryfieldWaterTowerRoomCollisionGridCells[374] = {
#include "assets/dryfield_water_tower_collision_06004_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gDryfieldWaterTowerRoomCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define DRYFIELD_WATER_TOWER_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldWaterTowerRoomCollisionGridCells[(halfwordOffset)])
/// X-major cell directory for the room's four-by-four collision grid.
static s16* _gDryfieldWaterTowerRoomCollisionGridTable[16] = {
#include "assets/dryfield_water_tower_collision_06004_table.inc"
};
#undef DRYFIELD_WATER_TOWER_CELL_FACE_LIST_AT_OFFSET

/// Live collision mesh and four-by-four spatial index for the water tower room.
///
/// Cells are 4000 room units wide, with X/Z origins at -7000. Room binding
/// supplies the view coordinate. The descriptor and its writable pools remain
/// owned by the loaded overlay; prop tasks replace normals, faces and vertices
/// in the leading two-face/eight-vertex sections without rebuilding cell lists.
static WorldCollisionGrid _gDryfieldWaterTowerRoomCollisionGrid = {
    NULL,
    _gDryfieldWaterTowerRoomCollisionGridNormals,
    _gDryfieldWaterTowerRoomCollisionGridVerts,
    _gDryfieldWaterTowerRoomCollisionGridFaces,
    _gDryfieldWaterTowerRoomCollisionGridTable,
    7000,
    7000,
    4,
    4,
    4000,
    ARRAY_SIZE(_gDryfieldWaterTowerRoomCollisionGridFaces),
};

ViewCamera gDryfieldWaterTowerViewCameras[20] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x5DC0, 0 } }, 322 },
    { { { { 2851, 0, -2940 }, { -2508, 2135, -2433 }, { 1532, 3495, 1486 } }, { -4690, 2700, 5660 } }, 257 },
    { { { { 0, 0, -ONE }, { 23, 4095, 0 }, { 4095, -23, 0 } }, { 2600, 1169, 4500 } }, 257 },
    { { { { -119, 0, 4094 }, { -19, 4095, 0 }, { -4094, -19, -119 } }, { -2900, 1170, 4250 } }, 257 },
    { { { { -3811, 0, -1500 }, { -121, 4082, 307 }, { 1495, 330, -3798 } }, { 5810, 1779, -4040 } }, 257 },
    { { { { 4000, 0, -878 }, { 74, 4081, 338 }, { 875, -346, 3986 } }, { 5550, 1170, 2970 } }, 257 },
    { { { { 1168, 0, 3925 }, { 3159, 2431, -939 }, { -2330, 3296, 693 } }, { -4290, 6370, -3260 } }, 257 },
    { { { { 4001, 0, 876 }, { 118, 4058, -539 }, { -868, 552, 3964 } }, { -5510, 1500, 5900 } }, 257 },
    { { { { 4002, 0, -870 }, { -738, 2168, -3395 }, { 460, 3475, 2118 } }, { 700, 5770, 3420 } }, 230 },
    { { { { 4044, 0, 649 }, { 244, 3795, -1519 }, { -602, 1539, 3747 } }, { -190, 2050, 3580 } }, 257 },
    { { { { 4095, 0, -54 }, { 3, 4087, 255 }, { 54, -256, 4087 } }, { 4140, 1170, 5700 } }, 230 },
    { { { { 3988, 0, -930 }, { -333, 3823, -1430 }, { 868, 1469, 3723 } }, { 4400, 1470, 3720 } }, 271 },
    { { { { 4094, 0, -112 }, { 4, 4091, 181 }, { 112, -181, 4090 } }, { 4140, 1570, 700 } }, 230 },
    { { { { 3261, 0, 2478 }, { 0, ONE, 0 }, { -2478, 0, 3261 } }, { -4900, 1150, 2500 } }, 257 },
    { { { { 0, 0, ONE }, { -1074, 3952, 0 }, { -3952, -1074, 0 } }, { -5900, 750, 1000 } }, 257 },
    { { { { -3562, 0, 2021 }, { 462, 3987, 815 }, { -1968, 937, -3467 } }, { -4750, 1750, 40 } }, 289 },
    { { { { 3429, 0, 2240 }, { -689, 3896, 1056 }, { -2131, -1261, 3262 } }, { -4790, 1110, 1250 } }, 289 },
    { { { { 2272, 0, -3408 }, { 2302, 3019, 1535 }, { 2512, -2767, 1674 } }, { 6000, 1890, 4000 } }, 221 },
    { { { { -3657, 0, 1843 }, { 0, ONE, 0 }, { -1843, 0, -3657 } }, { -1430, 1250, -4770 } }, 257 },
    { { { { 1485, 0, -3816 }, { -3580, 1417, -1394 }, { 1321, 3842, 514 } }, { -3517, 4818, -1078 } }, 257 },
};

/// Empty foreground list for mapped view 1, retaining the decoded background image.
///
/// The source slot retains this list's address but supplies no sprite elements.
static SpriteBatch _gDryfieldWaterTowerView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 2, retaining the decoded background image.
///
/// The source slot retains this list's address but supplies no sprite elements.
static SpriteBatch _gDryfieldWaterTowerView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 3, in pixels relative to the view centre.
///
/// Batches cover all 38 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView3SpriteSources[38] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, -80, 24, 1250, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -88, -40, 1250, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -88, 24, 1250, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -96, 0, 1250, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -96, -104, 1250, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -112, -120, 1250, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -104, -64, 1250, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, -112, 24, 1250, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, -56, 1375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, 0, 1375, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -144, -48, 1375, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -144, 0, 1375, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -128, -56, 1375, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -128, 0, 1375, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -112, -48, 1375, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, -112, 0, 1375, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, -96, -56, 1375, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -96, 0, 1375, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 56 } }, -80, -56, 1375, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 56 } }, -80, 0, 1375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -72, -56, 1375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -72, 0, 1375, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -80, 0, 1250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -80, -64, 1250, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -88, -72, 1125, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -88, 0, 1125, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -96, 0, 1000, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -96, -80, 1000, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -112, -88, 875, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -112, 0, 875, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, -128, 0, 750, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 104 } }, -128, -104, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 120 } }, -144, -120, 687, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 120 } }, -144, 0, 687, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 120 } }, -160, -120, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 120 } }, -160, 0, 625, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -80, -120, 1375, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, -80, -40, 1375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 3, covering 38 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView3SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 12, 0, 0, { 0, 0 } },
    { 20, 16, 0, 0, { 2, 0 } },
    { 36, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 4, in pixels relative to the view centre.
///
/// Batches cover all 38 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView4SpriteSources[38] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 64, 24, 1250, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 72, -40, 1250, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 72, 8, 1250, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 80, -120, 1250, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 80, -72, 1250, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 80, 0, 1250, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 96, 32, 1250, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 96, -120, 1250, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 56, -48, 1500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 56, 8, 1500, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 72, -48, 1500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 72, 0, 1500, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 88, -56, 1500, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 88, 0, 1500, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 104, -56, 1500, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 104, 0, 1500, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 120, -56, 1500, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 120, 0, 1500, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 136, -56, 1500, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, 136, 0, 1500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 56, -56, 1375, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 56, 0, 1375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 64, -64, 1250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 64, 0, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 72, -64, 1125, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 72, 0, 1125, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 80, -88, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 80, 0, 1000, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 104 } }, 96, -104, 875, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 96, 0, 875, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 136, -120, 625, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 136, -64, 625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 136, 0, 625, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 136, 64, 625, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 112, 0, 750, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 112, 56, 750, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 112, -120, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 112, -64, 750, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 4, covering 38 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView4SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 12, 0, 0, { 2, 0 } },
    { 20, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 5, in pixels relative to the view centre.
///
/// Batches cover all 47 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView5SpriteSources[47] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -8, -40, 2000, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -24, -40, 2000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -40, -40, 2000, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -56, -40, 2000, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -72, -40, 2000, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -88, -40, 2000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -104, -40, 2000, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -120, -40, 2000, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -136, -40, 2000, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 72 } }, -160, -40, 2000, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -40, -120, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -32, -96, 2000, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -8, 16, 2000, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -24, -32, 2000, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -32, 16, 2000, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -144, 0, 2000, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 72 } }, -160, -120, 2000, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -160, -48, 2000, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 80, 64, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, 88, 48, 1250, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 96, 40, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 104, 24, 1250, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 112, -16, 1250, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 112, 24, 1250, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 112, 56, 1250, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 112, 88, 1250, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 128, -40, 1250, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 128, 8, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 128, 48, 1250, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 128, 88, 1250, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -8, -40, 1844, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 48 } }, -16, -48, 1705, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, -16, 0, 1693, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, -32, 0, 1599, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -32, -48, 1556, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -48, -48, 1334, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 64 } }, -48, 0, 1339, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 64 } }, -64, -56, 1208, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 72 } }, -64, 8, 1248, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 72 } }, -88, -56, 1100, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 72 } }, -88, 16, 1102, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 80 } }, -112, -64, 963, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, -112, 16, 975, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, -136, -64, 857, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, -136, 24, 864, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, -160, -72, 774, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, -160, 24, 790, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 5, covering 47 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView5SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 3, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { 10, 5, 0, 0, { 5, 0 } },
    { 15, 3, 0, 0, { 1, 0 } },
    { 18, 12, 0, 0, { 4, 0 } },
    { 30, 17, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 6, in pixels relative to the view centre.
///
/// Batches cover all 30 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView6SpriteSources[30] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 48, -24, 2000, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 64, -24, 2000, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 80, -24, 2000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 96, -24, 2000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 112, -24, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 128, -24, 2000, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 144, -24, 2000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 48, -32, 1500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 48, 16, 1500, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 40, -24, 2500, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 40, 16, 2500, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 56, -40, 1375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 56, 16, 1375, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 72, -40, 1250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 72, 24, 1250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 72 } }, 88, -48, 1125, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 72 } }, 88, 24, 1125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 104, -56, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 104, 24, 1000, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 120, -64, 937, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 120, 24, 937, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 104 } }, 136, -80, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, 136, 24, 875, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 48, 40, 1500, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, 56, 8, 1500, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 64, -72, 1500, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, 64, 8, 1500, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 72, -120, 1500, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 72, -72, 1500, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 72, 8, 1500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 6, covering 30 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView6SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 16, 0, 0, { 2, 0 } },
    { 23, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 7, in pixels relative to the view centre.
///
/// Batches cover all 63 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView7SpriteSources[63] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -160, 24, 409, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -144, 40, 455, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -128, 56, 518, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -112, 72, 594, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -96, 88, 699, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -80, 104, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -120, 1125, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -8, -120, 1125, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -16, -96, 1125, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -24, -72, 1125, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -32, -48, 1125, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -40, -24, 1125, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, 0, 1125, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -80, 88, 1125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -72, 72, 1125, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -64, 48, 1125, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -56, 24, 1125, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -160, 48, 1250, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, 48, 1250, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -160, -64, 1250, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -144, -48, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -128, -24, 1250, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -128, 48, 1250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -112, 48, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -96, 48, 1250, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -80, 48, 983, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -64, 72, 1183, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -112, -24, 1250, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -96, -24, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -80, 8, 1250, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -136, -120, 1250, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -120, -120, 1250, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -112, -48, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -104, -120, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -88, -96, 2000, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -80, -72, 2250, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -112, -80, 1250, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -120, -80, 1250, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -96, -80, 1250, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -88, -80, 2250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -96, -48, 1250, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -88, -48, 2000, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -48, 112, 1013, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -80, -104, 2602, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -72, -80, 1544, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -64, -48, 1526, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -56, -8, 1391, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -48, 24, 1326, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -40, 64, 1484, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -160, -104, 1917, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -144, -104, 1979, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -128, -104, 2035, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -112, -104, 2058, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -96, -112, 2008, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -80, -112, 2044, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -112, -56, 2500, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -96, -64, 2500, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -56, -8, 1500, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -48, 0, 1500, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -40, -8, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -32, -16, 1625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -24, -24, 1625, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -32, 24, 1875, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 7, covering 63 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView7SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 11, 0, 0, { 4, 0 } },
    { 17, 25, 0, 0, { 1, 0 } },
    { 42, 7, 0, 0, { 5, 0 } },
    { 49, 6, 0, 0, { 0, 0 } },
    { 55, 2, 0, 0, { 6, 0 } },
    { 57, 6, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 8, in pixels relative to the view centre.
///
/// Batches cover all 111 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView8SpriteSources[111] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, -48, 2375, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 64 } }, -144, -56, 2375, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 64 } }, -128, -56, 2375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 64 } }, -112, -56, 2375, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -96, -64, 2375, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -80, -64, 2375, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -64, -64, 2375, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -48, -24, 2375, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -32, -24, 2375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -48, -56, 2375, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -24, -64, 2375, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, -16, -48, 2375, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, -32, -72, 1794, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -24, -64, 1978, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -16, -64, 2140, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 0, 831, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, 56, 884, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 56, 876, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 56, 869, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 64, 918, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, 64, 910, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, 64, 884, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 64, 878, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 64, 860, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -160, -104, 829, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -160, -24, 871, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -144, -104, 819, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -144, -24, 857, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 40, 866, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 40, 868, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 40, 887, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 48, 887, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 48, 871, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 48, 889, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 56, 905, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 56, 874, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 56, 896, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 64, 894, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 64, 878, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 64, 897, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 72, 870, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 72, 877, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 72, 892, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -128, -120, 812, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -120, -120, 812, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -104, 0, 875, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -120, -48, 875, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -120, 0, 875, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 40, 889, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, 40, 882, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, 40, 874, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 40, 853, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 40, 847, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 40, 864, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 40, 869, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 48, 893, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, 48, 886, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, 48, 879, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 48, 872, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 48, 854, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 48, 893, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 48, 871, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 56, 898, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, 56, 890, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, 56, 883, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 56, 876, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 56, 859, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 56, 888, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 56, 874, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 64, 918, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, 64, 911, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, 64, 904, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 64, 878, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 64, 879, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 64, 866, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, 72, 901, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 72, 908, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -160, -120, 875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -160, -48, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -144, -120, 875, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -144, -48, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -128, 0, 875, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 48, 869, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 48, 854, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 48, 883, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 48, 870, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 48, 894, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -96, 48, 1036, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -88, 48, 1143, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 56, 868, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 56, 859, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 56, 886, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 56, 875, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 56, 890, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 64, 860, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 64, 891, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 64, 879, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 72, 866, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -120, 72, 865, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -144, -112, 812, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -136, -112, 812, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -136, -24, 812, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -120, -104, 875, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -120, -24, 875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -104, -96, 1000, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -104, -24, 1000, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -88, -96, 1125, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -88, -24, 1125, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -72, -88, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 64 } }, -72, -16, 1250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -56, 8, 1250, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 8, covering 111 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView8SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 3, 0, 0, { 0, 0 } },
    { 15, 13, 0, 0, { 5, 0 } },
    { 28, 20, 0, 0, { 1, 0 } },
    { 48, 34, 0, 0, { 4, 0 } },
    { 82, 29, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 9, in pixels relative to the view centre.
///
/// Batches cover all 102 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView9SpriteSources[102] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, 64, 947, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 64, 874, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, 72, 976, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 72, 1129, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, 80, 1384, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 80, 1359, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -48, 112, 836, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -16, 104, 916, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, 16, 96, 930, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, 40, 88, 903, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, 72, 80, 997, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 104, 72, 960, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 120, 64, 973, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, -80, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -144, -88, 750, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -128, -104, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -112, -120, 750, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -96, -120, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -80, -120, 750, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, -64, -120, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, -160, 88, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, -128, 80, 750, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, -88, 72, 750, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, -56, 64, 750, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, -16, 56, 750, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, 24, 48, 750, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, 56, 40, 750, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, 96, 32, 750, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 136, 24, 750, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, -120, 1337, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -128, -112, 1750, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -128, -80, 1875, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -104, -64, 1750, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -160, -40, 1125, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, -40, 1375, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -128, -40, 1875, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -160, 0, 1125, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -144, 0, 1337, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -128, 0, 1875, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -160, 48, 1125, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, 48, 1337, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -128, 48, 1350, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -160, 88, 1125, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -144, 88, 1337, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -128, 88, 1750, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, -80, 1337, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -152, -80, 1125, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, -120, 1750, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -136, -120, 1750, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -120, -120, 1750, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -104, -120, 1750, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -24, -88, 2000, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -24, -120, 1875, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -8, -120, 1937, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -8, -88, 2000, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 8, -120, 1937, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 8, -88, 2000, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 24, -88, 2000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 24, -104, 1937, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 40, -120, 1922, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 40, -96, 1060, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, 56, -96, 2072, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, 56, -120, 1912, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -40, -120, 1717, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -40, -88, 2039, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -56, -120, 1718, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -56, -88, 2058, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -72, -120, 215, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -72, -88, 2005, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -88, -120, 226, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -88, -88, 1874, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 40, -64, 1215, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, -24, 1512, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 24, -8, 1675, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 24, -16, 1600, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, -32, 1450, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, -40, 1381, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, -48, 1320, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, -56, 1273, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, 40, -104, 1017, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 48, -112, 983, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 48, -120, 953, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 40, -96, 1055, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 40, -88, 1087, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 40, -80, 1127, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 40, -72, 1175, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 64, -88, 1875, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 72, -120, 1875, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 80, -120, 1875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 88, -120, 1875, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 104, -120, 1875, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 112, 0, 1500, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 120, -16, 1500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 136, -32, 1500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 136, 16, 1500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, 152, 64, 1500, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, 152, 16, 1500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 152, -48, 1500, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, -120, 2000, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -128, -120, 2000, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -112, -104, 2000, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -96, -72, 2000, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 9, covering 102 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView9SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 16, 0, 0, { 4, 0 } },
    { 29, 18, 0, 0, { 1, 0 } },
    { 47, 24, 0, 0, { 5, 0 } },
    { 71, 15, 0, 0, { 0, 0 } },
    { 86, 12, 0, 0, { 6, 0 } },
    { 98, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 10, retaining the decoded background image.
///
/// The source slot retains this list's address but supplies no sprite elements.
static SpriteBatch _gDryfieldWaterTowerView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 11, in pixels relative to the view centre.
///
/// Batches cover all 17 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView11SpriteSources[17] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 80, 750, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 40, 80, 750, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 80, 750, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 80, 750, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -16, 80, 750, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 8, 80, 750, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 56, 80, 750, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -40, 88, 750, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -16, 88, 750, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 8, 88, 750, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 32, 88, 750, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 56, 88, 750, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -40, 104, 750, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -16, 104, 750, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 8, 104, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 32, 104, 750, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 56, 104, 750, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 11, covering 17 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView11SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterTowerView11SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground sprite rectangles for mapped view 12, in pixels relative to the view centre.
///
/// Batches cover all 20 elements. Depths are unscaled sorting values; RGB 128
/// is neutral and code flags zero select opaque modulation. Storage stays writable.
static SpriteSource _gDryfieldWaterTowerView12SpriteSources[20] = {
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 40 } }, -48, 40, 500, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -32, 56, 500, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 64, 500, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, 64, 500, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 16, 64, 500, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 32, 64, 500, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 48, 64, 500, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 64, 64, 500, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 80, 64, 500, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 96, 56, 500, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 24 } }, -48, 80, 500, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -32, 80, 500, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -16, 80, 500, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 0, 80, 500, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 16, 80, 500, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 32, 80, 500, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 48, 80, 500, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, 80, 500, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 80, 80, 500, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 96, 80, 500, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
};

/// Foreground ranges for mapped view 12, covering 20 source sprites.
///
/// The zero-count first record keeps the decoded background image enabled.
/// The final record ends the list; intermediate zero counts do not end it.
static SpriteBatch _gDryfieldWaterTowerView12SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterTowerView12SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 13, retaining the decoded background image.
///
/// The source slot retains this list's address but supplies no sprite elements.
static SpriteBatch _gDryfieldWaterTowerView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles for mapped view 14, in view-centred pixels.
///
/// Batch ranges cover all 47 sources; depth is the unscaled OT sorting value.
/// The room owns these records for the lifetime of its sprite views.
static SpriteSource _gDryfieldWaterTowerView14SpriteSources[47] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 24, 875, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 40, -88, 625, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 40, 40, 625, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 48, -16, 750, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 48, 32, 750, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 56, -80, 750, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, 0, 750, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 56, 32, 750, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 72, 40, 875, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, 0, 875, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 72, -72, 875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 80, -24, 875, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, -48, 1250, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 88, -48, 1250, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, -16, 1250, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 88, -16, 1250, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, 16, 1250, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 88, 16, 1250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 80, 48, 1222, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 88, 48, 1250, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -8, 0, 1625, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 8, 0, 1625, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 24, 0, 1625, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 40, 0, 1625, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 40, -24, 1625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 24, -24, 1625, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, -24, 1625, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -8, -24, 1625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 128 } }, -56, -120, 250, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 112 } }, -56, 8, 250, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 128 } }, -72, -120, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 112 } }, -72, 8, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -88, 8, 475, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -104, 8, 450, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -120, 8, 425, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -136, 8, 412, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -152, 8, 400, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -160, 8, 387, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -160, -64, 387, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -152, -64, 400, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -136, -64, 412, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -120, -64, 425, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -104, -64, 450, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -88, -64, 475, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -104, -56, 1125, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -104, -16, 1125, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -104, 32, 1125, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
};

/// Five foreground ranges covering mapped view 14's 47 source sprites.
///
/// The zero-count first record retains the decoded background. Serialized
/// trailing bytes are preserved; their role is unproven. The last record ends the list.
static SpriteBatch _gDryfieldWaterTowerView14SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { 12, 8, 0, 0, { 3, 0 } },
    { 20, 8, 0, 0, { 2, 0 } },
    { 28, 16, 0, 0, { 4, 0 } },
    { 44, 3, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 15, retaining the decoded background.
///
/// The source slot retains this list's address but supplies no sprite elements.
static SpriteBatch _gDryfieldWaterTowerView15SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 16, retaining the decoded background.
///
/// The source slot retains this list's address but supplies no sprite elements.
static SpriteBatch _gDryfieldWaterTowerView16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 17, retaining the decoded background.
///
/// The source slot retains this list's address but supplies no sprite elements.
static SpriteBatch _gDryfieldWaterTowerView17SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles for mapped view 18, in view-centred pixels.
///
/// Batch ranges cover all 16 sources; depth is the unscaled OT sorting value.
static SpriteSource _gDryfieldWaterTowerView18SpriteSources[16] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -56, -56, 2500, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -48, -88, 250, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -40, -104, 250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -32, -104, 250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -120, -120, 250, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -104, -120, 250, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -88, -112, 250, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -72, -96, 250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -56, -80, 250, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -40, -64, 250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -40, -24, 250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -24, -48, 250, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -24, -16, 250, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -8, -40, 250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -8, -8, 250, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 8, -16, 250, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
};

/// Two foreground ranges covering mapped view 18's 16 source sprites.
///
/// The zero-count first record retains the decoded background. The first
/// range's serialized trailing bytes have an unproven role; the last record ends the list.
static SpriteBatch _gDryfieldWaterTowerView18SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mechanism foreground sprite in mapped view 19, in view-centred pixels.
///
/// Batch visibility selects its presence; depth is the unscaled OT sorting value.
static SpriteSource _gDryfieldWaterTowerView19SpriteSources[1] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 80, 136 } }, -64, -88, 367, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

/// Foreground range for mapped view 19's single mechanism sprite.
///
/// The zero-count first record retains the decoded background. Room commands
/// change the second record's hidden byte; the last record ends the list.
static SpriteBatch _gDryfieldWaterTowerView19SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterTowerView19SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted clip for mapped view 19, followed by its draw-area terminator.
///
/// Restricts early drawing to a 3-by-2 pixel rectangle at (316, 1), then restores
/// full-screen drawing at unscaled depth 500. Coordinates are draw-buffer pixels.
static SpriteDrawArea _gDryfieldWaterTowerView19SpriteDrawAreas[2] = {
    { { 316, 1, 3, 2 }, 500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Foreground texture rectangles for mapped view 20, in view-centred pixels.
///
/// Its single batch covers all five sources at unscaled sorting depth 375.
static SpriteSource _gDryfieldWaterTowerView20SpriteSources[5] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, -48, 375, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -80, -56, 375, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -72, -80, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -56, -72, 375, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -40, -64, 375, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
};

/// Foreground range covering mapped view 20's five source sprites.
///
/// The zero-count first record retains the decoded background; the last record ends the list.
static SpriteBatch _gDryfieldWaterTowerView20SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterTowerView20SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for the retained mapped-view-21 slot.
///
/// Retains the decoded background and this list's address in the source slot.
/// No source elements or camera record accompany this slot.
static SpriteBatch _gDryfieldWaterTowerView21SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldWaterTowerSpriteViews[21] = {
    { { .empty = _gDryfieldWaterTowerView1SpriteBatches }, _gDryfieldWaterTowerView1SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTowerView2SpriteBatches }, _gDryfieldWaterTowerView2SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView3SpriteSources }, _gDryfieldWaterTowerView3SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView4SpriteSources }, _gDryfieldWaterTowerView4SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView5SpriteSources }, _gDryfieldWaterTowerView5SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView6SpriteSources }, _gDryfieldWaterTowerView6SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView7SpriteSources }, _gDryfieldWaterTowerView7SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView8SpriteSources }, _gDryfieldWaterTowerView8SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView9SpriteSources }, _gDryfieldWaterTowerView9SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTowerView10SpriteBatches }, _gDryfieldWaterTowerView10SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView11SpriteSources }, _gDryfieldWaterTowerView11SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView12SpriteSources }, _gDryfieldWaterTowerView12SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTowerView13SpriteBatches }, _gDryfieldWaterTowerView13SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView14SpriteSources }, _gDryfieldWaterTowerView14SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTowerView15SpriteBatches }, _gDryfieldWaterTowerView15SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTowerView16SpriteBatches }, _gDryfieldWaterTowerView16SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTowerView17SpriteBatches }, _gDryfieldWaterTowerView17SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView18SpriteSources }, _gDryfieldWaterTowerView18SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTowerView19SpriteSources }, _gDryfieldWaterTowerView19SpriteBatches, _gDryfieldWaterTowerView19SpriteDrawAreas },
    { { .elements = _gDryfieldWaterTowerView20SpriteSources }, _gDryfieldWaterTowerView20SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTowerView21SpriteBatches }, _gDryfieldWaterTowerView21SpriteBatches, NULL },
};

/// Directed view-change quads joining views 3..9 and 20 in the water tower room.
///
/// Room binding supplies coordinates and mutable list links. The final quad is
/// included and carries LAST; positions/radii use room units and normals use Q12.
static WorldCollisionTrigger _gDryfieldWaterTowerViewBoundaryTriggers[14] = {
    { NULL, NULL, NULL, { 447, -1888, -4529, 0 }, { { -602, -2912, -2368, 0 }, { 595, -2912, 2364, 0 }, { -602, 2912, -2368, 0 }, { 595, 2912, 2364, 0 } }, { 3978, 0, -1007, 0 }, { 0, 0, ONE, 0 }, 3797, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 607, -1984, -4417, 0 }, { { 594, -3008, 2362, 0 }, { -603, -3008, -2369, 0 }, { 594, 3008, 2362, 0 }, { -603, 3008, -2369, 0 } }, { -3981, 0, 1006, 0 }, { 0, 0, ONE, 0 }, 3865, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4449, -1712, -4673, 0 }, { { 1730, -2736, 1722, 0 }, { -1730, -2736, -1722, 0 }, { 1730, 2736, 1722, 0 }, { -1730, 2736, -1722, 0 } }, { -2892, 0, 2904, 0 }, { 0, 0, ONE, 0 }, 3665, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4640, -1632, -4544, 0 }, { { -1730, -2656, -1722, 0 }, { 1730, -2656, 1722, 0 }, { -1730, 2656, -1722, 0 }, { 1730, 2656, 1722, 0 } }, { 2897, 0, -2912, 0 }, { 0, 0, ONE, 0 }, 3602, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4865, -1680, 575, 0 }, { { -2441, -2704, 6, 0 }, { 2441, -2704, -6, 0 }, { -2441, 2704, 6, 0 }, { 2441, 2704, -6, 0 } }, { -11, 0, -4102, 0 }, { 0, 0, ONE, 0 }, 3638, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4770, -1776, 446, 0 }, { { 2441, -2800, -17, 0 }, { -2440, -2800, 17, 0 }, { 2441, 2800, -17, 0 }, { -2440, 2800, 17, 0 } }, { 28, 0, 4105, 0 }, { 0, 0, ONE, 0 }, 3709, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3138, -1696, 3709, 0 }, { { 279, -2720, -1510, 0 }, { -278, -2720, 1511, 0 }, { 279, 2720, -1510, 0 }, { -278, 2720, 1511, 0 } }, { 4035, 0, 743, 0 }, { 0, 0, ONE, 0 }, 3114, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2816, -1792, 3518, 0 }, { { -238, -2816, 1330, 0 }, { 238, -2816, -1330, 0 }, { -238, 2816, 1330, 0 }, { 238, 2816, -1330, 0 } }, { -4039, 0, -724, 0 }, { 0, 0, ONE, 0 }, 3114, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5343, -1824, -2755, 0 }, { { 2712, -2848, 15, 0 }, { -2711, -2848, -14, 0 }, { 2712, 2848, 15, 0 }, { -2711, 2848, -14, 0 } }, { -23, 0, 4103, 0 }, { 0, 0, ONE, 0 }, 3924, 0, 8, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5280, -1904, -2561, 0 }, { { -2757, -2928, -12, 0 }, { 2756, -2928, 11, 0 }, { -2757, 2928, -12, 0 }, { 2756, 2928, 11, 0 } }, { 16, 0, -4105, 0 }, { 0, 0, ONE, 0 }, 4015, 0, 3, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3023, -2016, -65, 0 }, { { 4, -3040, 1207, 0 }, { -3, -3040, -1206, 0 }, { 4, 3040, 1207, 0 }, { -3, 3040, -1206, 0 } }, { -4104, 0, 11, 0 }, { 0, 0, ONE, 0 }, 3268, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2847, -2128, -33, 0 }, { { -3, -3152, -1206, 0 }, { 4, -3152, 1207, 0 }, { -3, 3152, -1206, 0 }, { 4, 3152, 1207, 0 } }, { 4099, 0, -13, 0 }, { 0, 0, ONE, 0 }, 3367, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4431, -2048, 511, 0 }, { { -1504, -2928, 554, 0 }, { 1490, -2928, -570, 0 }, { -1504, 2928, 554, 0 }, { 1490, 2928, -570, 0 } }, { -1447, 0, -3853, 0 }, { 0, 0, ONE, 0 }, 3328, 0, 8, 20, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4383, -1888, 479, 0 }, { { 1495, -2928, -564, 0 }, { -1499, -2928, 560, 0 }, { 1495, 2928, -564, 0 }, { -1499, 2928, 560, 0 } }, { 1445, 0, 3851, 0 }, { 0, 0, ONE, 0 }, 3328, 0, 20, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Water tower interaction quads for warps, CAP commands and automatic room actions.
///
/// Room binding owns their live list links while the overlay stays loaded.
/// Prop-scene progress changes selected enables and removes a completed action.
/// The final quad is included and carries LAST. Geometry uses room units and Q12 normals.
static WorldCollisionTrigger _gDryfieldWaterTowerActionTriggers[24] = {
    { NULL, NULL, NULL, { -2832, -48, -5760, 0 }, { { -624, 0, -416, 0 }, { 624, 0, -416, 0 }, { -624, 0, 416, 0 }, { 624, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 749, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_G_R_KITCHEN, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5760, -48, 1696, 0 }, { { 416, 0, -736, 0 }, { 416, 0, 736, 0 }, { -416, 0, -736, 0 }, { -416, 0, 736, 0 } }, { 0, 4103, 0, 0 }, { -ONE, 0, 0, 0 }, 844, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_BREEZEWAY, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2400, -48, 112, 0 }, { { -768, 0, -624, 0 }, { 769, 0, -624, 0 }, { -768, 0, 624, 0 }, { 769, 0, 624, 0 } }, { 0, 4104, 0, 0 }, { 4090, 0, -201, 0 }, 989, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_TANK, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1008, -64, 3664, 0 }, { { -400, 0, -336, 0 }, { 528, 0, -592, 0 }, { -848, 0, 464, 0 }, { 720, 0, 464, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, 4091, 0 }, 966, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 688, -64, -4416, 0 }, { { -435, 0, -2947, 0 }, { 862, 0, 2233, 0 }, { -863, 0, -2234, 0 }, { 434, 0, 2946, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, ONE, 0 }, 2974, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_WATER_TOWER_ACTION_START_ACTOR_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 5568, -48, -5248, 0 }, { { 416, 0, -624, 0 }, { 416, 0, 624, 0 }, { -416, 0, -624, 0 }, { -416, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 749, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_ROOM_6, 66, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2842, -64, -128, 0 }, { { 611, 0, -1392, 0 }, { 611, 0, 1392, 0 }, { -611, 0, -1392, 0 }, { -611, 0, 1392, 0 } }, { 0, 4098, 0, 0 }, { ONE, 0, 0, 0 }, 1519, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_WATER_TOWER_ACTION_COMPLETE_RUN, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -3456, -64, -1280, 0 }, { { -1616, 0, -1504, 0 }, { 464, 0, -1504, 0 }, { -1616, 0, 1536, 0 }, { 464, 0, 1536, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, ONE, 0 }, 2217, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3456, -192, -1280, 0 }, { { -1616, 0, -1504, 0 }, { 464, 0, -1504, 0 }, { -1616, 0, 1536, 0 }, { 464, 0, 1536, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, ONE, 0 }, 2217, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2816, -64, -128, 0 }, { { 611, 0, -1392, 0 }, { 611, 0, 1392, 0 }, { -611, 0, -1392, 0 }, { -611, 0, 1392, 0 } }, { 0, 4098, 0, 0 }, { -ONE, 0, 0, 0 }, 1519, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 64, -64, -3296, 0 }, { { -3728, 0, -416, 0 }, { 3728, 0, -416, 0 }, { -3728, 0, 416, 0 }, { 3728, 0, 416, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -ONE, 0 }, 3744, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1856, -64, 3232, 0 }, { { -1872, 0, -416, 0 }, { 1872, 0, -416, 0 }, { -1872, 0, 416, 0 }, { 1872, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1915, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 112, -64, -2528, 0 }, { { -3136, 0, -416, 0 }, { 3136, 0, -416, 0 }, { -3136, 0, 416, 0 }, { 3136, 0, 416, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, ONE, 0 }, 3156, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, 2432, 0 }, { { -2832, 0, -416, 0 }, { 2832, 0, -416, 0 }, { -2832, 0, 416, 0 }, { 2832, 0, 416, 0 } }, { 0, 4113, 0, 0 }, { 0, 0, -ONE, 0 }, 2862, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3552, -64, 32, 0 }, { { -416, 0, 3312, 0 }, { -416, 0, -3312, 0 }, { 416, 0, 3312, 0 }, { 416, 0, -3312, 0 } }, { 0, 4103, 0, 0 }, { -ONE, 0, 0, 0 }, 3337, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2688, -64, -2224, 0 }, { { -416, 0, 704, 0 }, { -416, 0, -704, 0 }, { 416, 0, 704, 0 }, { 416, 0, -704, 0 } }, { 0, 4117, 0, 0 }, { -ONE, 0, 0, 0 }, 817, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2624, -64, 2048, 0 }, { { -416, 0, 704, 0 }, { -416, 0, -704, 0 }, { 416, 0, 704, 0 }, { 416, 0, -704, 0 } }, { 0, 4117, 0, 0 }, { -ONE, 0, 0, 0 }, 817, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3712, -64, 2208, 0 }, { { -416, 0, 1104, 0 }, { -416, 0, -1104, 0 }, { 416, 0, 1104, 0 }, { 416, 0, -1104, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3776, -64, -2368, 0 }, { { -416, 0, 1104, 0 }, { -416, 0, -1104, 0 }, { 416, 0, 1104, 0 }, { 416, 0, -1104, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2560, -64, 1856, 0 }, { { -416, 0, 1104, 0 }, { -416, 0, -1104, 0 }, { 416, 0, 1104, 0 }, { 416, 0, -1104, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3840, -64, 0, 0 }, { { 355, 0, -1392, 0 }, { 355, 0, 1392, 0 }, { -355, 0, -1392, 0 }, { -355, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { 4052, 0, -601, 0 }, 1431, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1679, 0, -1440, 0 }, { { 594, 0, -1104, 0 }, { 594, 0, 688, 0 }, { -594, 0, -1103, 0 }, { -594, 0, 1521, 0 } }, { 0, 4119, 0, 0 }, { 4091, 0, 201, 0 }, 1629, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2240, -64, -2272, 0 }, { { 722, 0, -496, 0 }, { 722, 0, 496, 0 }, { -722, 0, -495, 0 }, { -722, 0, 497, 0 } }, { 0, 4111, 0, 0 }, { 201, 0, -4091, 0 }, 875, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2304, -64, -352, 0 }, { { 722, 0, -496, 0 }, { 722, 0, 496, 0 }, { -722, 0, -495, 0 }, { -722, 0, 497, 0 } }, { 0, 4111, 0, 0 }, { -201, 0, 4091, 0 }, 875, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Four mutable directional lights, admitted in every room view.
///
/// Translation supplies a direction; RGB intensity uses 12 fractional bits.
/// Gameplay parents and composes the coordinates and overwrites attenuation.
static WorldCoordLight _gDryfieldWaterTowerDirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
};

/// Five mutable point lights, admitted in every room view.
///
/// Positions and falloff radii use world units; RGB intensity is Q12.
/// Gameplay parents and composes the coordinates and overwrites attenuation.
static WorldCoordPointLight _gDryfieldWaterTowerPointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4500, -2620, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1339, 2360 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3200, -2159, -922 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 2220, 3442 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -766, -2000, -4618 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1319, 2059 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1240, -5082, 2801 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 7000, 7001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3966, -2581, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1800, 3001 },
};

/// Mutable light collection for the room, with no cone-light array.
static WorldCoordRoomLights _gDryfieldWaterTowerRoomLights = {
    ARRAY_SIZE(_gDryfieldWaterTowerDirectionalLights),
    _gDryfieldWaterTowerDirectionalLights,
    ARRAY_SIZE(_gDryfieldWaterTowerPointLights),
    _gDryfieldWaterTowerPointLights,
    0,
    NULL,
};

/// Resource-entry keys and decimal file numbers in the water tower's actor layouts.
enum {
    DRYFIELD_WATER_TOWER_CHASER_RESOURCE_ENTRY       = 1,
    DRYFIELD_WATER_TOWER_SCORPION_RESOURCE_ENTRY     = 25,
    DRYFIELD_WATER_TOWER_SCRIPTED_CHASER_FILE_NUMBER = 216,
    DRYFIELD_WATER_TOWER_SCORPION_FILE_NUMBER        = 25,
    DRYFIELD_WATER_TOWER_REGULAR_CHASER_FILE_NUMBER  = 1,
};

/// Resource recipe for variant 1's two scripted Desert Chasers.
///
/// Entry 1 loads file 421600 and borrows its sole task descriptor at index zero.
/// The second record terminates the resource list; placements belong to the map overlay.
static AreaResource _gDryfieldWaterTowerVariant1ChaserResources[2] = {
    { DRYFIELD_WATER_TOWER_CHASER_RESOURCE_ENTRY, DRYFIELD_WATER_TOWER_SCRIPTED_CHASER_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor421600DesertChaserTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Resource recipe for variant 2's seven scorpions.
///
/// Entry 25 loads file 102500 and uses task descriptor zero. The second record
/// terminates the list; the paired placements belong to the map overlay.
static AreaResource _gDryfieldWaterTowerVariant2ScorpionResources[2] = {
    { DRYFIELD_WATER_TOWER_SCORPION_RESOURCE_ENTRY, DRYFIELD_WATER_TOWER_SCORPION_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Resource recipe for variant 3's five scorpions.
///
/// Entry 25 loads file 102500 and uses task descriptor zero. This variant owns
/// a separate terminated resource list paired with its map-overlay placements.
static AreaResource _gDryfieldWaterTowerVariant3ScorpionResources[2] = {
    { DRYFIELD_WATER_TOWER_SCORPION_RESOURCE_ENTRY, DRYFIELD_WATER_TOWER_SCORPION_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Resource recipe for variant 10's regular Desert Chaser.
///
/// Entry 1 loads file 400100 and borrows its sole task descriptor at index zero.
/// The second record terminates the list; the paired placement is room-owned.
static AreaResource _gDryfieldWaterTowerVariant10ChaserResources[2] = {
    { DRYFIELD_WATER_TOWER_CHASER_RESOURCE_ENTRY, DRYFIELD_WATER_TOWER_REGULAR_CHASER_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 10's regular Desert Chaser placement, followed by the placement terminator.
///
/// Uses resource entry 1 with zero spawn mode/variant and tuning row zero.
/// Position is in room units, yaw is three quarters of a turn, and texture
/// offsets are page zero and CLUT row two. The room owns the borrowed table.
static AreaPlacement _gDryfieldWaterTowerVariant10ChaserPlacements[2] = {
    { DRYFIELD_WATER_TOWER_CHASER_RESOURCE_ENTRY, 0, 0, -1568, 0, 2432, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gDryfieldWaterTowerAreaVariants[13] = {
    { NULL, NULL },
    { gMapDryfieldWaterTowerVariant1Placements, _gDryfieldWaterTowerVariant1ChaserResources },
    { gMapDryfieldWaterTowerVariant2Placements, _gDryfieldWaterTowerVariant2ScorpionResources },
    { gMapDryfieldWaterTowerVariant3Placements, _gDryfieldWaterTowerVariant3ScorpionResources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { _gDryfieldWaterTowerVariant10ChaserPlacements, _gDryfieldWaterTowerVariant10ChaserResources },
    { NULL, NULL },
    { NULL, NULL },
};

/// Cue-2 bases in the current sound bank; cue 1 adds one and companions add 100.
enum {
    DRYFIELD_WATER_TOWER_FOOTSTEP_WALK_BASE = 0x10000049,
    DRYFIELD_WATER_TOWER_FOOTSTEP_RUN_BASE  = 0x1000004B,
};

/// Footstep cue bases shared by the room's blocking surfaces.
///
/// Scripted jumps reuse the walking pair; running selects its own pair.
static WorldCollisionFootstepSounds _gDryfieldWaterTowerFootstepSounds = {
    DRYFIELD_WATER_TOWER_FOOTSTEP_WALK_BASE,
    DRYFIELD_WATER_TOWER_FOOTSTEP_RUN_BASE,
    DRYFIELD_WATER_TOWER_FOOTSTEP_WALK_BASE,
};

/// Blocking surface policy and footstep cues for classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gDryfieldWaterTowerDefaultSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldWaterTowerFootstepSounds,
};

/// Class-1 policy: probes pass, weapon impacts are disabled and footsteps are silent.
///
/// Grid contacts still apply pushback independently of probe passage.
static WorldCollisionSurfaceProperties _gDryfieldWaterTowerClass1SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class-2 blocking policy, retaining a separate record with the default footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldWaterTowerClass2SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldWaterTowerFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldWaterTowerSurfacePropertiesByClass[8] = {
    &_gDryfieldWaterTowerDefaultSurfaceProperties,
    &_gDryfieldWaterTowerClass1SurfaceProperties,
    &_gDryfieldWaterTowerClass2SurfaceProperties,
    &_gDryfieldWaterTowerDefaultSurfaceProperties,
    &_gDryfieldWaterTowerDefaultSurfaceProperties,
    &_gDryfieldWaterTowerDefaultSurfaceProperties,
    &_gDryfieldWaterTowerDefaultSurfaceProperties,
    &_gDryfieldWaterTowerDefaultSurfaceProperties,
};

/// Two-lane vibration commands for the run opening.
///
/// The binary lane plays segment 0, waits one frame and loops to step 0 with
/// count 29 before stopping at step 3. The variable lane plays segments 1..3
/// then stops. Step 4 is retained but unreachable. Both arrays remain borrowed
/// until the vibration task ends or a scene skip halts it.
static PadScriptCmd _gDryfieldWaterTowerRunPadCommands[5] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 29), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 3) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Timed segments of `_gDryfieldWaterTowerRunPadCommands`.
///
/// Segment 0 supplies a one-frame binary pulse. Variable segments ramp
/// 90..120 for 20 frames, hold 120 for 58 and ramp 90..190 for 15.
/// Intensities are bytes before attenuation; the fourth byte has no interpreter read.
static PadScriptVibrationSegment _gDryfieldWaterTowerRunPadSegments[4] = {
    { 0, 0, 1, 0 },
    { 90, 120, 20, 1 },
    { 120, 120, 58, 1 },
    { 90, 190, 15, 1 },
};

/// Two-lane vibration commands for the mechanism's slide back.
///
/// Binary steps 0..3 pulse segment 0 with a one-frame wait and loop count 31;
/// variable steps play segments 1..3 then stop. Step 4 is unreachable but retained.
/// Commands and segments are borrowed until script completion or a skip halt.
static PadScriptCmd _gDryfieldWaterTowerSlideBackPadCommands[5] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 31), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 3) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Timed segments of `_gDryfieldWaterTowerSlideBackPadCommands`.
///
/// Segment 0 supplies a one-frame binary pulse. Variable segments ramp
/// 90..120 for 20 frames, hold 120 for 65 and ramp 90..190 for 15.
/// Byte intensities precede attenuation; the unread fourth byte is retained.
static PadScriptVibrationSegment _gDryfieldWaterTowerSlideBackPadSegments[4] = {
    { 0, 0, 1, 0 },
    { 90, 120, 20, 1 },
    { 120, 120, 65, 1 },
    { 90, 190, 15, 1 },
};

/// Drop vibration: keep the binary motor stopped and play one variable-motor segment.
///
/// Steps 0/1 select segment 0 then STOP. The final STOP operand 5 is ignored.
/// Commands and `_gDryfieldWaterTowerDropPadSegment` are borrowed until the
/// spawned vibration task ends or the closing-scene skip halts it.
static PadScriptCmd _gDryfieldWaterTowerDropPadCommands[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 5) }
};

/// Drop's variable-motor ramp from intensity 90 to 180 over 60 script frames.
///
/// Borrowed as segment 0 by `_gDryfieldWaterTowerDropPadCommands`; intensities
/// are bytes before attenuation, and the fourth byte has no interpreter read.
static PadScriptVibrationSegment _gDryfieldWaterTowerDropPadSegment = { 90, 180, 60, 1 };

/// Run durations selected by the number of earlier timeouts.
///
/// Ascending inclusive thresholds 0, 1, 2 and 65535 cover every u16 count.
/// Durations are 25/30/35/40 units of 30 frames (750/900/1050/1200 frames).
/// The last record is an all-remaining-counts entry and is consumed normally.
static _DryfieldWaterTowerTimeLimit _gDryfieldWaterTowerRunTimeLimits[4] = {
    { 0, 25 },
    { 1, 30 },
    { 2, 35 },
    { DRYFIELD_WATER_TOWER_TIME_LIMIT_ALL_TIMEOUTS, 40 },
};

static void       _dryfieldWaterTowerStepPropFall(Task* task);
static s32        _dryfieldWaterTowerStepPropSlideOut(Task* task);
static s32        _dryfieldWaterTowerStepPropSlideBack(Task* task);
static void       _dryfieldWaterTowerDispatchPropSceneRequest(Task* task);
static inline u16 _dryfieldWaterTowerStepPropClosingScene(Task* task);
static void       _dryfieldWaterTowerDispatchActorSceneRequest(Task* task);

/// Advances the falling prop to its hanging height, or advances its free-falling copy.
///
/// Requires initialized work and a live model root. With spawn argument 1 zero,
/// spawns and starts a copy, then stops only after passing the hanging placement.
/// With a nonzero argument, places the copy at the upper mark on its first call
/// and keeps falling until the owning prop kills it. The signed halfword speed
/// wraps as it grows by four parent-coordinate units per call; Y advances by it.
static void _dryfieldWaterTowerStepPropFall(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work  = task->work;
    GfxCoord*                         coord = task->extra.tmd->coords;
    ActorCommand                      command;

    // The original prop hangs; its spawned copy keeps accelerating below it.
    if (task->spawnArg1.value == 0) {
        switch (work->phase) {
            case DRYFIELD_WATER_TOWER_PROP_FALL_SPAWN_COPY:
                work->fallingPropTask = taskSpawnFromTable(gDryfieldWaterTowerPropSceneTasks, DRYFIELD_WATER_TOWER_PROP_TASK_SLOT_FALLING, 1, 0);
                work->phase++;
                return;

            case DRYFIELD_WATER_TOWER_PROP_FALL_START_COPY:
                command.command = DRYFIELD_WATER_TOWER_FALLING_PROP_STATE_FALL;
                TASK_MESSAGE_DISPATCH_POINTER(work->fallingPropTask, ACTOR_COMMAND_MESSAGE_APPLY, &command, 0);
                work->phase++;
                /* fallthrough */

            case DRYFIELD_WATER_TOWER_PROP_FALL_FALLING:
                if (coord->coord.t[1] > _gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_FALLING_PROP_HANGING].pos.vy) {
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_FALLING_PROP_HANGING], 0);
                    work->phase++;
                    return;
                }
                break;

            case DRYFIELD_WATER_TOWER_PROP_FALL_HANGING:
                return;
        }
    } else if (work->phase == DRYFIELD_WATER_TOWER_PROP_FALL_SPAWN_COPY) {
        TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_FALLING_PROP_UPPER], 0);
        work->phase++;
    }

    work->fallSpeed    += 4;
    coord->coord.t[1]  += work->fallSpeed;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Installs the fallen prop's ground collision in the live grid's reserved faces.
///
/// Copies the ground templates into normals 2..3, faces 2..3 and vertices 8..15
/// of the live grid, in that order, without changing its cell lists. The two
/// face records occupy 24 bytes, the two normals 16 and the eight vertices 64.
/// Source and destination arrays are disjoint and word-aligned; the overlay's
/// grid and template storage must remain loaded while collision uses the patch.
static inline void _dryfieldWaterTowerInstallDroppedPropCollision(void)
{
    enum {
        DRYFIELD_WATER_TOWER_DROPPED_PROP_NORMAL_FIRST = 2,
        DRYFIELD_WATER_TOWER_DROPPED_PROP_FACE_FIRST   = 2,
        DRYFIELD_WATER_TOWER_DROPPED_PROP_VERTEX_FIRST = 8
    };

    memCopyBytes(_gDryfieldWaterTowerCollision04550, &_gDryfieldWaterTowerRoomCollisionGridNormals[DRYFIELD_WATER_TOWER_DROPPED_PROP_NORMAL_FIRST], sizeof(_gDryfieldWaterTowerCollision04550));
    memCopyBytes(_gDryfieldWaterTowerCollision045E0, &_gDryfieldWaterTowerRoomCollisionGridFaces[DRYFIELD_WATER_TOWER_DROPPED_PROP_FACE_FIRST], sizeof(_gDryfieldWaterTowerCollision045E0));
    memCopyBytes(_gDryfieldWaterTowerCollision04560, &_gDryfieldWaterTowerRoomCollisionGridVerts[DRYFIELD_WATER_TOWER_DROPPED_PROP_VERTEX_FIRST], sizeof(_gDryfieldWaterTowerCollision04560));
}

/// Advances the falling prop's drop to the ground, returning 1 after settling.
///
/// Requires initialized work and a live model root. Removes the free-falling
/// copy, descends 300 parent-coordinate units per call, then snaps to the ground
/// and raises two dust puffs. Eleven settling calls precede the ground collision
/// patch and final placement. Returns 0 while unfinished; tick bit 2 adds a
/// 10-unit Z shake during settling. The motion leaves the root dirty.
static s32 _dryfieldWaterTowerStepPropDrop(Task* task)
{
    enum { DRYFIELD_WATER_TOWER_DROP_DUST_ARGUMENT = (s32)0x80000000 | (2 << 12) | 0x700 };

    _DryfieldWaterTowerPropSceneWork* work;
    GfxCoord*                         coord;
    GfxCoord*                         dustCoord;
    SVECTOR                           dustOffset;
    s32                               puffIndex;

    work                = task->work;
    coord               = task->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    switch (work->phase) {
        case DRYFIELD_WATER_TOWER_PROP_DROP_START:
            work->shadowEnabled = 1;
            if (work->fallingPropTask != NULL) {
                taskKill(work->fallingPropTask);
                work->fallingPropTask = NULL;
            }
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerPropDropStartPlacement, 0);
            work->phase++;
            break;

        case DRYFIELD_WATER_TOWER_PROP_DROP_DROPPING:
            coord->coord.t[1] += 0x12C;
            if (_gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_FALLING_PROP_GROUND].pos.vy < coord->coord.t[1]) {
                sndEvtRequestScriptStop(SOUND_WATER_TOWER_CAP_DROP, SOUND_SCRIPT_STOP_NO_FADE);
                sndEvtRequestScriptStart(SOUND_WATER_TOWER_CAP_LAND, 0, 0);
                dustCoord     = task->extra.tmd->coords;
                dustOffset.vz = 0;
                dustOffset.vy = 0;
                dustOffset.vx = -0xC8;
                puffIndex     = 0;
                do {
                    effectSpawn(EFFECT_DUST_PUFF, dustCoord, DRYFIELD_WATER_TOWER_DROP_DUST_ARGUMENT, &dustOffset);
                    puffIndex++;
                    dustOffset.vx += 0x190;
                } while ((u32)(puffIndex & 0xFFFF) < 2U);
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_FALLING_PROP_GROUND], 0);
                work->phase++;
            }
            break;

        case DRYFIELD_WATER_TOWER_PROP_DROP_SETTLING:
            work->settleFrames++;
            if (work->settleFrames >= DRYFIELD_WATER_TOWER_PROP_DROP_SETTLE_FRAMES) {
                // Enable the landed surface only after the ground shake finishes.
                _dryfieldWaterTowerInstallDroppedPropCollision();
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_FALLING_PROP_GROUND], 0);
                return 1;
            }
            {
                s32 groundZ       = _gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_FALLING_PROP_GROUND].pos.vz;
                coord->coord.t[2] = groundZ;
                if (gDisplayState.gameTick & DRYFIELD_WATER_TOWER_PROP_SHAKE_TICK_BIT) {
                    coord->coord.t[2] = groundZ + 0xA;
                }
            }
            break;
    }
    return 0;
}

/// Updates the falling prop's light and colour matrices at its frame's composed origin.
///
/// The live model must have writable lighting matrices and a current `workm`.
/// Position is in composed coordinate units; all three available light slots are queried.
static inline void _dryfieldWaterTowerLightFallingProp(const Task* task)
{
    enum { DRYFIELD_WATER_TOWER_PROP_LIGHT_COUNT = 3 };

    VECTOR3          worldPosition;
    const TmdObject* model;

    model            = task->extra.tmd;
    worldPosition.vx = model->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(model, &worldPosition, 0, DRYFIELD_WATER_TOWER_PROP_LIGHT_COUNT);
}

/// Draws the falling prop's 768-unit square shadow on its parent's Y = -200 plane.
///
/// `coord` supplies the live model frame's height. The local Y offset wraps to
/// a signed halfword; yaw preserves the plane, while pitch or roll tilts it.
/// Drawing requires the current frame's primitive arena and ordering table.
static inline void _dryfieldWaterTowerDrawFallingPropShadow(const Task* task, const GfxCoord* coord)
{
    enum {
        DRYFIELD_WATER_TOWER_PROP_SHADOW_SIDE    = 768,
        DRYFIELD_WATER_TOWER_PROP_SHADOW_PLANE_Y = -200
    };

    SVECTOR offset;

    offset.vx = 0;
    offset.vy = -((u16)coord->coord.t[1]) + DRYFIELD_WATER_TOWER_PROP_SHADOW_PLANE_Y;
    offset.vz = 0;
    actorRenderDrawGroundShadow(task->extra.tmd->coords, DRYFIELD_WATER_TOWER_PROP_SHADOW_SIDE, &offset);
}

/// Owns the falling prop and its optional free-falling copy through fall and drop commands.
///
/// Allocates task-owned scene work and lends its two leading matrices to the
/// model. Scene pauses suppress active drawing; attachment-menu activity pauses
/// motion and lighting. The shadow latch is tested even while that menu is open.
/// The coordinate parent and model resources must remain live for the task.
static void _dryfieldWaterTowerFallingPropTask(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work;
    TmdObject*                        model;
    GfxCoord*                         coord;
    _DryfieldWaterTowerPropSceneWork* newWork;

    model = task->extra.tmd;
    work  = task->work;
    coord = model->coords;
    if (gGameSession->sceneUpdatesPaused != 0) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    if (gAttachmentState.menuOpen == ATTACHMENT_MENU_CLOSED) {
        switch (task->state) {
            case DRYFIELD_WATER_TOWER_PROP_STATE_INITIALIZE: {
                TmdObject* initialModel;
                GfxCoord*  initialCoord;

                initialModel = task->extra.tmd;
                initialCoord = initialModel->coords;
                newWork      = memMalloc(sizeof(*newWork), false);
                task->work   = newWork;
                if (newWork == 0) {
                    taskKill(task);
                } else {
                    memFillBytes(newWork, 0, sizeof(*newWork));
                    newWork->playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    initialCoord->parent = &gGfxViewCoord;
                    initialModel->flags  = 0;
                    tmdAllocPrimitiveBuffer(initialModel);
                    initialModel->colorMtx = &newWork->colorMtx;
                    initialModel->lightMtx = &newWork->lightMtx;
                    task->msgTable         = _gDryfieldWaterTowerPropMessages;
                }
                task->state++;
                break;
            }

            case DRYFIELD_WATER_TOWER_PROP_STATE_IDLE:
                if (work->fallingPropTask != 0) {
                    taskKill(work->fallingPropTask);
                    work->fallingPropTask = 0;
                }
                break;

            case DRYFIELD_WATER_TOWER_FALLING_PROP_STATE_FALL:
                _dryfieldWaterTowerStepPropFall(task);
                break;

            case DRYFIELD_WATER_TOWER_FALLING_PROP_STATE_DROP:
                if (_dryfieldWaterTowerStepPropDrop(task) & 0xFFFF) {
                    task->state = DRYFIELD_WATER_TOWER_PROP_STATE_IDLE;
                }
                break;
        }
        _dryfieldWaterTowerLightFallingProp(task);
    }
    // Retain the entry work pointer for the shadow, including the initialization frame.
    if (work->shadowEnabled != 0) {
        _dryfieldWaterTowerDrawFallingPropShadow(task, coord);
    }
}

/// Advances the prop's dust cursor through entries 0..10 and spawns a local puff.
///
/// Requires a live model root and the loaded twelve-word room offset table.
/// `killCountdown` must be initialized in 0..10; it is this prop's cyclic index,
/// not a teardown countdown. Only the table's first eleven entries participate;
/// X is a signed halfword in model-local game units.
/// The packed argument enables child puffs, two updates per texture frame,
/// and an initial scale of 0x300. The effect consumes the stack offset on spawn.
static inline void _dryfieldWaterTowerSpawnPropSlideDust(Task* task)
{
    enum {
        DRYFIELD_WATER_TOWER_PROP_DUST_LAST_OFFSET = 10,
        DRYFIELD_WATER_TOWER_SLIDE_DUST_ARGUMENT   = (s32)0x80000000 | (2 << 12) | 0x300,
    };
    GfxCoord* dustCoord;
    SVECTOR   dustOffset;

    dustCoord = task->extra.tmd->coords;
    if (task->killCountdown >= DRYFIELD_WATER_TOWER_PROP_DUST_LAST_OFFSET) {
        task->killCountdown = 0;
    } else {
        task->killCountdown = (u16)task->killCountdown + 1;
    }
    dustOffset.vy = 0;
    dustOffset.vz = 0;
    dustOffset.vx = _gDryfieldWaterTowerPropSlideDustXOffsets[task->killCountdown];
    effectSpawn(EFFECT_DUST_PUFF, dustCoord, DRYFIELD_WATER_TOWER_SLIDE_DUST_ARGUMENT, &dustOffset);
}

/// Advances the sliding prop out to the far mark, returning 1 after settling.
///
/// Requires initialized work and a live model root. Z advances 20 parent-coordinate
/// units per call until it passes the endpoint; 61 settling calls precede final
/// placement. Returns 0 while unfinished and marks the root dirty. Tick bit 2
/// adds five units in Y while moving and X while settling. The dust cursor uses
/// the first eleven table entries and narrows each X offset to a signed halfword.
static s32 _dryfieldWaterTowerStepPropSlideOut(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work  = task->work;
    GfxCoord*                         coord = task->extra.tmd->coords;

    switch (work->phase) {
        case DRYFIELD_WATER_TOWER_PROP_SLIDE_MOVING:
            coord->coord.t[2] += 0x14;
            coord->coord.t[1]  = _gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_EXTENDED].pos.vy;
            if (gDisplayState.gameTick & DRYFIELD_WATER_TOWER_PROP_SHAKE_TICK_BIT) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] > _gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_EXTENDED].pos.vz) {
                work->phase++;
            }
            _dryfieldWaterTowerSpawnPropSlideDust(task);
            break;

        case DRYFIELD_WATER_TOWER_PROP_SLIDE_SETTLING:
            work->settleFrames++;
            if (work->settleFrames >= DRYFIELD_WATER_TOWER_PROP_SLIDE_SETTLE_FRAMES) {
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_EXTENDED], 0);
                return 1;
            }
            coord->coord.t[0] = _gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_EXTENDED].pos.vx;
            if (gDisplayState.gameTick & DRYFIELD_WATER_TOWER_PROP_SHAKE_TICK_BIT) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Advances the sliding prop back to its home mark, returning 1 after settling.
///
/// Requires initialized work and a live model root. Z advances 20 parent-coordinate
/// units per call until it passes the endpoint; 61 settling calls precede final
/// placement. Returns 0 while unfinished and marks the root dirty. Tick bit 2
/// adds five units in Y while moving and X while settling. The dust cursor uses
/// the first eleven table entries and narrows each X offset to a signed halfword.
static s32 _dryfieldWaterTowerStepPropSlideBack(Task* task)
{
    enum { DRYFIELD_WATER_TOWER_SLIDE_BACK_SOUND_FADE_TICKS = 10 };

    _DryfieldWaterTowerPropSceneWork* work  = task->work;
    GfxCoord*                         coord = task->extra.tmd->coords;

    switch (work->phase) {
        case DRYFIELD_WATER_TOWER_PROP_SLIDE_MOVING:
            sndEvtRequestScriptVolume(SOUND_WATER_TOWER_CAP_RUNNING, DRYFIELD_WATER_TOWER_FULL_SOUND_VOLUME);
            coord->coord.t[2] -= 0x14;
            coord->coord.t[1]  = _gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_HOME].pos.vy;
            if (gDisplayState.gameTick & DRYFIELD_WATER_TOWER_PROP_SHAKE_TICK_BIT) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] < _gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_HOME].pos.vz) {
                sndEvtRequestScriptStop(SOUND_WATER_TOWER_CAP_RUNNING, DRYFIELD_WATER_TOWER_SLIDE_BACK_SOUND_FADE_TICKS);
                work->phase++;
            }
            _dryfieldWaterTowerSpawnPropSlideDust(task);
            break;

        case DRYFIELD_WATER_TOWER_PROP_SLIDE_SETTLING:
            work->settleFrames++;
            if (work->settleFrames >= DRYFIELD_WATER_TOWER_PROP_SLIDE_SETTLE_FRAMES) {
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_HOME], 0);
                return 1;
            }
            coord->coord.t[0] = _gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_HOME].pos.vx;
            if (gDisplayState.gameTick & DRYFIELD_WATER_TOWER_PROP_SHAKE_TICK_BIT) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Updates the sliding prop's light and colour matrices at its composed origin.
///
/// Requires a live model with writable borrowed matrices and a current workm.
/// The three signed 32-bit composed translation words supply the lighting
/// position in game units, without recomposing the root. Requests the three
/// strongest room/transient lights, reduced to the available count by the query.
static inline void _dryfieldWaterTowerLightSlidingProp(const Task* task)
{
    enum { DRYFIELD_WATER_TOWER_SLIDING_PROP_LIGHT_COUNT = 3 };
    VECTOR3          worldPosition;
    const TmdObject* model;

    model            = task->extra.tmd;
    worldPosition.vx = model->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(model, &worldPosition, 0, DRYFIELD_WATER_TOWER_SLIDING_PROP_LIGHT_COUNT);
}

/// Owns the sliding prop's work, model lighting and commanded slide in either direction.
///
/// Allocates task-owned work and lends its light and colour matrices to the model.
/// Scene pauses suppress active drawing; an open attachment menu pauses motion
/// and lighting. Completed slides return to idle. The model root is parented to
/// the live view coordinate and lighting is queried at its composed origin.
static void _dryfieldWaterTowerSlidingPropTask(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work;
    TmdObject*                        entryModel;
    TmdObject*                        initialModel;
    GfxCoord*                         coord;

    entryModel = task->extra.tmd;
    if (gGameSession->sceneUpdatesPaused != 0) {
        entryModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    entryModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    if ((s8)gAttachmentState.menuOpen != ATTACHMENT_MENU_CLOSED) {
        return;
    }
    switch (task->state) {
        case DRYFIELD_WATER_TOWER_PROP_STATE_INITIALIZE:
            initialModel = task->extra.tmd;
            coord        = initialModel->coords;
            work         = memMalloc(sizeof(*work), false);
            task->work   = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                coord->parent       = &gGfxViewCoord;
                initialModel->flags = 0;
                tmdAllocPrimitiveBuffer(initialModel);
                initialModel->colorMtx = &work->colorMtx;
                initialModel->lightMtx = &work->lightMtx;
                task->msgTable         = _gDryfieldWaterTowerPropMessages;
            }
            task->state++;
            break;

        case DRYFIELD_WATER_TOWER_PROP_STATE_IDLE:
            break;

        case DRYFIELD_WATER_TOWER_SLIDING_PROP_STATE_SLIDE_OUT:
            if ((_dryfieldWaterTowerStepPropSlideOut(task) & 0xFFFF) != 0) {
                task->state = DRYFIELD_WATER_TOWER_PROP_STATE_IDLE;
            }
            break;

        case DRYFIELD_WATER_TOWER_SLIDING_PROP_STATE_SLIDE_BACK:
            if ((_dryfieldWaterTowerStepPropSlideBack(task) & 0xFFFF) != 0) {
                task->state = DRYFIELD_WATER_TOWER_PROP_STATE_IDLE;
            }
            break;
    }
    _dryfieldWaterTowerLightSlidingProp(task);
}

/// Consumes one event-script request on the prop-scene driver, then clears the latch.
///
/// Requires initialized driver work, live player and prop tasks, and loaded room
/// scripts and placements. Requests show or place the player, select the saved
/// view, command a prop motion, or start its vibration and sound. Request 5 has
/// no handler. A completed run walks the player to the closing mark while the
/// prop slides back; a timeout hides the player for that slide.
static void _dryfieldWaterTowerDispatchPropSceneRequest(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work = task->work;

    switch (work->request) {
        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_NONE:
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_OUT: {
            ActorCommand command;

            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
            command.command = DRYFIELD_WATER_TOWER_SLIDING_PROP_STATE_SLIDE_OUT;
            TASK_MESSAGE_DISPATCH_POINTER(work->slidingPropTask, ACTOR_COMMAND_MESSAGE_APPLY, &command, 0);
            break;
        }

        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SHOW_PLAYER:
            if (work->runResult == DRYFIELD_WATER_TOWER_RUN_COMPLETED) {
                TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerClosingPlayerPlacements[DRYFIELD_WATER_TOWER_PLAYER_CLOSING_MARK], 0);
            }
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);

            // Showing the player also applies the view chosen by the run.
            /* fallthrough */
        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_APPLY_VIEW:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = work->nextView;
            gGameSession->viewDirty                                = 1;
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_BACK: {
            ActorCommand command;

            {
                WorldCollisionTrigger* runActionTrigger = &_gDryfieldWaterTowerActionTriggers[DRYFIELD_WATER_TOWER_RUN_ACTION_TRIGGER];
                runActionTrigger->flags                |= WORLD_COLLISION_TRIGGER_ENABLED;
            }
            if (work->runResult == DRYFIELD_WATER_TOWER_RUN_COMPLETED) {
                TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerClosingPlayerPlacements[DRYFIELD_WATER_TOWER_PLAYER_CLOSING_WALK_START], 0);
                TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &_gDryfieldWaterTowerClosingPlayerPlacements[DRYFIELD_WATER_TOWER_PLAYER_CLOSING_MARK], 0);
            } else {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE, 0);
            }
            command.command = DRYFIELD_WATER_TOWER_SLIDING_PROP_STATE_SLIDE_BACK;
            TASK_MESSAGE_DISPATCH_POINTER(work->slidingPropTask, ACTOR_COMMAND_MESSAGE_APPLY, &command, 0);
            break;
        }

        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_FALL: {
            ActorCommand command;

            command.command = DRYFIELD_WATER_TOWER_FALLING_PROP_STATE_FALL;
            TASK_MESSAGE_DISPATCH_POINTER(work->fallingPropTask, ACTOR_COMMAND_MESSAGE_APPLY, &command, 0);
            break;
        }

        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_DROP: {
            ActorCommand command;

            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerClosingPlayerPlacements[DRYFIELD_WATER_TOWER_PLAYER_CLOSING_MARK], 0);
            command.command = DRYFIELD_WATER_TOWER_FALLING_PROP_STATE_DROP;
            TASK_MESSAGE_DISPATCH_POINTER(work->fallingPropTask, ACTOR_COMMAND_MESSAGE_APPLY, &command, 0);
            break;
        }

        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_RUN_RUMBLE:
            work->runningSoundStarted = 1;
            work->padScriptTask       = padScriptSpawn(_gDryfieldWaterTowerRunPadCommands,
                                                       _gDryfieldWaterTowerRunPadSegments);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, DRYFIELD_WATER_TOWER_OPENING_SOUND_ENTRY), 0, 0);
            sndEvtRequestScriptStart(SOUND_WATER_TOWER_CAP_RUNNING, 0, 0);
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_SLIDE_BACK_RUMBLE:
            work->padScriptTask = padScriptSpawn(_gDryfieldWaterTowerSlideBackPadCommands,
                                                 _gDryfieldWaterTowerSlideBackPadSegments);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, DRYFIELD_WATER_TOWER_ENDING_SOUND_ENTRY), 0, 0);
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_DROP_RUMBLE:
            work->padScriptTask = padScriptSpawn(_gDryfieldWaterTowerDropPadCommands,
                                                 &_gDryfieldWaterTowerDropPadSegment);
            sndEvtRequestScriptStart(SOUND_WATER_TOWER_CAP_DROP, 0, 0);
            break;
    }
    work->request = DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_NONE;
}

/// Returns the next mechanism run's time limit in frames, based on earlier timeouts.
///
/// `task` must carry the initialized prop-scene driver's work. The ascending
/// thresholds end at `DRYFIELD_WATER_TOWER_TIME_LIMIT_ALL_TIMEOUTS`, covering every
/// 16-bit timeout count. The four entries yield 750, 900, 1050 or 1200 frames.
static inline u16 _dryfieldWaterTowerGetRunTimeLimitFrames(const Task* task)
{
    const _DryfieldWaterTowerPropSceneWork* work = task->work;
    u16                                     limitIndex;

    limitIndex = 0;
    if (_gDryfieldWaterTowerRunTimeLimits[0].maxTimeouts < work->timeouts) {
        do {
            limitIndex += 1;
        } while (_gDryfieldWaterTowerRunTimeLimits[limitIndex].maxTimeouts < work->timeouts);
    }
    return _gDryfieldWaterTowerRunTimeLimits[limitIndex].duration * DRYFIELD_WATER_TOWER_TIME_LIMIT_UNIT_FRAMES;
}

/// Returns the running sound's per-view percentage of full script volume (127).
///
/// Requires initialized driver work, the loaded sentinel-ended volume table and
/// the current room's view map. The cached logical slot is passed
/// through the mapped-to-logical lookup again, preserving the room's behavior.
/// An unlisted lookup result uses full volume; integer division rounds down.
static inline s32 _dryfieldWaterTowerGetRunningSoundVolume(const Task* task)
{
    const _DryfieldWaterTowerPropSceneWork* work = task->work;
    u16                                     volumeIndex;

    for (volumeIndex = 0; _gDryfieldWaterTowerRunningSoundViewVolumes[volumeIndex].viewIndex != DRYFIELD_WATER_TOWER_VIEW_VOLUME_END; volumeIndex++) {
        if (_gDryfieldWaterTowerRunningSoundViewVolumes[volumeIndex].viewIndex == viewFindLogicalIndex((u8)work->currentView)) {
            return _gDryfieldWaterTowerRunningSoundViewVolumes[volumeIndex].percent * DRYFIELD_WATER_TOWER_FULL_SOUND_VOLUME / 100;
        }
    }
    return DRYFIELD_WATER_TOWER_FULL_SOUND_VOLUME;
}

/// Runs the mechanism's opening, timed player action and ending, returning its result.
///
/// Called once per driver frame with initialized work and phase START. The first
/// run plays its opening script; later runs restore its open placement directly.
/// The frame limit grows with earlier timeouts. Timing ends on the second room
/// action or only after the unsigned 16-bit frame count exceeds the limit.
/// An active direction action suppresses timeout and volume updates while the count advances.
/// Returns UNDER_WAY until the ending script is idle, then TIMED_OUT or COMPLETED.
/// Live player/prop tasks, scene actors, scripts and collision grids are required.
static u16 _dryfieldWaterTowerStepTimedRun(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work = task->work;
    GameSession*                      session;
    ActorCommand                      hideCommand;
    ActorCommand                      startRunCommand;
    ActorCommand                      endRunCommand;
    u16                               actionId;
    u8                                actionReason;
    u8                                actionArg;
    s32                               completionReason;

    switch (work->phase) {
        case DRYFIELD_WATER_TOWER_RUN_PHASE_START:
            // Opening and repeat runs share the opened collision patch.
            if (work->mechanismState == GAME_FLAG_WATER_TOWER_MECHANISM_INITIAL) {
                hideCommand.context.loc.stage = gGameSession->location.stage;
                hideCommand.context.loc.area  = gGameSession->location.area;
                hideCommand.command           = DRYFIELD_WATER_TOWER_CHASER_COMMAND_HIDE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &hideCommand, ACTOR_COMMAND_MESSAGE_APPLY);
                work->nextView            = viewFindLogicalIndex(DRYFIELD_WATER_TOWER_RUN_MAPPED_VIEW);
                work->runningSoundStarted = 0;
                evsStartScriptWithSkip(_gDryfieldWaterTowerRunOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldWaterTowerRunOpeningSkipScript);
                work->phase++;
            } else {
                TASK_MESSAGE_DISPATCH_POINTER(work->slidingPropTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_EXTENDED], 0);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewFindLogicalIndex(DRYFIELD_WATER_TOWER_RUN_MAPPED_VIEW);
                session                                                = gGameSession;
                session->viewDirty                                     = 1;
                session->hideHud                                       = 0;
                session->eventState                                    = 0;
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, DRYFIELD_WATER_TOWER_OPENING_SOUND_ENTRY), 0, DRYFIELD_WATER_TOWER_REPEATED_RUN_OPENING_ATTENUATION);
                sndEvtRequestScriptStart(SOUND_WATER_TOWER_CAP_RUNNING, 0, 0);
                work->phase = DRYFIELD_WATER_TOWER_RUN_PHASE_START_TIMER;
            }
            {
                WorldCollisionTrigger* runActionTrigger = &_gDryfieldWaterTowerActionTriggers[DRYFIELD_WATER_TOWER_RUN_ACTION_TRIGGER];
                runActionTrigger->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
            }
            gDryfieldWaterTowerRunTimeLimitFrames = _dryfieldWaterTowerGetRunTimeLimitFrames(task);
            memCopyBytes(_gDryfieldWaterTowerCollision04608, _gDryfieldWaterTowerRoomCollisionGridVerts, sizeof(_gDryfieldWaterTowerCollision04608));
            memCopyBytes(_gDryfieldWaterTowerCollision045F8, _gDryfieldWaterTowerRoomCollisionGridNormals, sizeof(_gDryfieldWaterTowerCollision045F8));
            memCopyBytes(_gDryfieldWaterTowerCollision04688, _gDryfieldWaterTowerRoomCollisionGridFaces, sizeof(_gDryfieldWaterTowerCollision04688));
            return DRYFIELD_WATER_TOWER_RUN_UNDER_WAY;

        case DRYFIELD_WATER_TOWER_RUN_PHASE_OPENING_SCRIPT:
            if (gGameSession->eventState != 0) {
                return DRYFIELD_WATER_TOWER_RUN_UNDER_WAY;
            }
            work->phase++;
            break;

        case DRYFIELD_WATER_TOWER_RUN_PHASE_START_TIMER:
            startRunCommand.context.loc.stage = gGameSession->location.stage;
            startRunCommand.context.loc.area  = gGameSession->location.area;
            startRunCommand.command           = DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_RUN;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &startRunCommand, ACTOR_COMMAND_MESSAGE_APPLY);
            gDryfieldWaterTowerRunElapsedFrames = 0;
            work->mechanismState                = GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_OPERATED;
            gameFlagSetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE, GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_OPERATED);
            work->phase++;

            // Count the first timed frame in the same call that starts the run.
            /* fallthrough */
        case DRYFIELD_WATER_TOWER_RUN_PHASE_TIMING:
            if (worldCollisionReadActionHit(&actionId, &actionReason, &actionArg) != 0 && gAttachmentState.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE &&
                (actionId & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM && (completionReason = (s8)actionReason) == DRYFIELD_WATER_TOWER_ACTION_COMPLETE_RUN) {
                worldCollisionUnlinkTrigger(0, &_gDryfieldWaterTowerActionTriggers[DRYFIELD_WATER_TOWER_FIRST_ACTION_TRIGGER]);
                work->nextView = viewFindLogicalIndex(DRYFIELD_WATER_TOWER_CLOSING_MAPPED_VIEW);
                evsStartScriptWithSkip(_gDryfieldWaterTowerRunEndingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldWaterTowerRunEndingSkipScript);
                work->runResult = completionReason;
                work->phase++;
                break;
            }
            gDryfieldWaterTowerRunElapsedFrames++;
            if (gDirectionActionActive != 0) {
                break;
            }
            if (gDryfieldWaterTowerRunTimeLimitFrames < gDryfieldWaterTowerRunElapsedFrames) {
                work->timeouts++;
                work->nextView = gGameSession->location.view;
                evsStartScriptWithSkip(_gDryfieldWaterTowerRunEndingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldWaterTowerRunEndingSkipScript);
                work->runResult = DRYFIELD_WATER_TOWER_RUN_TIMED_OUT;
                work->phase++;
            }
            sndEvtRequestScriptVolume(SOUND_WATER_TOWER_CAP_RUNNING, _dryfieldWaterTowerGetRunningSoundVolume(task) & 0xFF);
            return DRYFIELD_WATER_TOWER_RUN_UNDER_WAY;

        case DRYFIELD_WATER_TOWER_RUN_PHASE_ENDING_SCRIPT:
            // Finish retreat and restore closed collision after either ending script.
            if (gGameSession->eventState != 0) {
                return DRYFIELD_WATER_TOWER_RUN_UNDER_WAY;
            }
            if (work->runResult != DRYFIELD_WATER_TOWER_RUN_COMPLETED) {
                endRunCommand.context.loc.stage = gGameSession->location.stage;
                endRunCommand.context.loc.area  = gGameSession->location.area;
                endRunCommand.command           = DRYFIELD_WATER_TOWER_CHASER_COMMAND_END_RUN;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &endRunCommand, ACTOR_COMMAND_MESSAGE_APPLY);
            }
            memCopyBytes(_gDryfieldWaterTowerCollision04648, _gDryfieldWaterTowerRoomCollisionGridVerts, sizeof(_gDryfieldWaterTowerCollision04648));
            memCopyBytes(_gDryfieldWaterTowerCollision045F8, _gDryfieldWaterTowerRoomCollisionGridNormals, sizeof(_gDryfieldWaterTowerCollision045F8));
            memCopyBytes(_gDryfieldWaterTowerCollision04688, _gDryfieldWaterTowerRoomCollisionGridFaces, sizeof(_gDryfieldWaterTowerCollision04688));
            gameFlagSetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE, GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_RESTORED);
            return work->runResult;
    }
    return DRYFIELD_WATER_TOWER_RUN_UNDER_WAY;
}

/// Starts the final chaser battle and waits for the actors' completion event.
///
/// Called once per driver frame after a completed mechanism run, with `phase`
/// initially `DRYFIELD_WATER_TOWER_CLOSING_PHASE_START`. The borrowed work must
/// remain live. Returns 1 when the event latch is set in the wait phase, else 0;
/// this step never clears the latch, which belongs to the driver's lifetime.
static inline u16 _dryfieldWaterTowerWaitForChasersStep(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work = task->work;
    ActorCommand                      command;

    switch (work->phase) {
        case DRYFIELD_WATER_TOWER_CLOSING_PHASE_START:
            command.context.loc.stage = gGameSession->location.stage;
            command.context.loc.area  = gGameSession->location.area;
            command.command           = DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_FINAL_BATTLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &command, ACTOR_COMMAND_MESSAGE_APPLY);
            work->phase++;
            break;
        case DRYFIELD_WATER_TOWER_CLOSING_PHASE_WAIT:
            if (work->actorEventReceived != 0) {
                return 1;
            }
            break;
        default:
            return 0;
    }
    return 0;
}

/// Runs the prop-closing scene and records completed room progress when it ends.
///
/// Called once per driver frame with initialized work and phase START. An active
/// attachment wheel or pending display change delays script startup. Returns 0
/// while waiting and 1 once the normal or skipped script has ended.
static inline u16 _dryfieldWaterTowerStepPropClosingScene(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work = task->work;

    switch (work->phase) {
        case DRYFIELD_WATER_TOWER_CLOSING_PHASE_START:
            if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL) {
                break;
            }
            if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return 0;
            }
            evsStartScriptWithSkip(_gDryfieldWaterTowerPropClosingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldWaterTowerPropClosingSkipScript);
            work->phase++;
            break;
        case DRYFIELD_WATER_TOWER_CLOSING_PHASE_WAIT:
            if (gGameSession->eventState != 0) {
                return 0;
            }
            gameFlagSetNibble(GAME_FLAG_WATER_TOWER_PROGRESS, DRYFIELD_WATER_TOWER_PROGRESS_COMPLETE);
            return 1;
        default:
            return 0;
    }
    return 0;
}

/// Sequences the room's actor scene, timed mechanism runs and final prop closing scene.
///
/// Owns a zeroed work block and spawns the sliding and falling props. Saved
/// progress chooses whether to wait for the first action, resume timed runs, or
/// restore completed placements and collision. Pauses while scene updates are
/// blocked, the attachment menu is open, or the player has no HP. Each active
/// frame caches the logical view and consumes a posted script request.
/// The room overlay, live player, scene actors and collision storage must survive.
static void _dryfieldWaterTowerPropSceneTask(Task* task)
{
    _DryfieldWaterTowerPropSceneWork* work = task->work;
    _DryfieldWaterTowerPropSceneWork* newWork;
    _DryfieldWaterTowerPropSceneWork* fallingPropWork;
    u16                               actionId;
    u8                                actionReason;
    u8                                actionArg;
    s32                               actorSceneExitValue;
    s32                               triggerDisableMask;
    WorldCollisionTrigger*            firstActionTrigger;
    WorldCollisionTrigger*            mechanismTrigger;
    WorldCollisionTrigger*            runActionTrigger;

    if (gGameSession->sceneUpdatesPaused != 0 || gAttachmentState.menuOpen != ATTACHMENT_MENU_CLOSED || gPlayerStatus.hp == 0) {
        return;
    }

    switch (task->state) {
        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_INITIALIZE:
            // Keep allocation and the later task-work reload separate.
            newWork    = memMalloc(sizeof(*newWork), false);
            task->work = newWork;
            if (newWork == NULL) {
                taskKill(task);
            } else {
                memFillBytes(newWork, 0, sizeof(*newWork));
                newWork->playerTask              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                gDryfieldWaterTowerPropSceneTask = task;
                task->msgTable                   = _gDryfieldWaterTowerPropSceneMessages;
            }
            memCopyBytes(_gDryfieldWaterTowerCollision04648, _gDryfieldWaterTowerRoomCollisionGridVerts, sizeof(_gDryfieldWaterTowerCollision04648));
            memCopyBytes(_gDryfieldWaterTowerCollision045F8, _gDryfieldWaterTowerRoomCollisionGridNormals, sizeof(_gDryfieldWaterTowerCollision045F8));
            memCopyBytes(_gDryfieldWaterTowerCollision04688, _gDryfieldWaterTowerRoomCollisionGridFaces, sizeof(_gDryfieldWaterTowerCollision04688));
            // This initial patch uses raised vertices; the landed patch is different.
            memCopyBytes(_gDryfieldWaterTowerCollision04550, _gDryfieldWaterTowerRoomCollisionGridNormals + 2, sizeof(_gDryfieldWaterTowerCollision04550));
            memCopyBytes(_gDryfieldWaterTowerCollision045E0, _gDryfieldWaterTowerRoomCollisionGridFaces + 2, sizeof(_gDryfieldWaterTowerCollision045E0));
            memCopyBytes(_gDryfieldWaterTowerCollision045A0, _gDryfieldWaterTowerRoomCollisionGridVerts + 8, sizeof(_gDryfieldWaterTowerCollision045A0));
            work = task->work;
            task->state++;
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_SPAWN_PROPS:
            work->slidingPropTask = taskSpawnFromTable(gDryfieldWaterTowerPropSceneTasks, DRYFIELD_WATER_TOWER_PROP_TASK_SLOT_SLIDING, 0, 0);
            work->fallingPropTask = taskSpawnFromTable(gDryfieldWaterTowerPropSceneTasks, DRYFIELD_WATER_TOWER_PROP_TASK_SLOT_FALLING, 0, 0);
            task->state++;
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_RESTORE_PLACEMENTS:
            TASK_MESSAGE_DISPATCH_POINTER(work->slidingPropTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_HOME], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->fallingPropTask, ACTOR_MESSAGE_PLACE, _gDryfieldWaterTowerFallingPropPlacements, 0);
            work->mechanismState = gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE);
            if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_PROGRESS) == DRYFIELD_WATER_TOWER_PROGRESS_INITIAL) {
                task->state++;
            } else if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_PROGRESS) == DRYFIELD_WATER_TOWER_PROGRESS_ACTOR_SCENE_STARTED) {
                task->state = DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_RUN_REQUEST;
            } else if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_PROGRESS) == DRYFIELD_WATER_TOWER_PROGRESS_COMPLETE) {
                // Rebuild the completed scene from saved state without replaying it.
                triggerDisableMask         = ~WORLD_COLLISION_TRIGGER_ENABLED;
                firstActionTrigger         = &_gDryfieldWaterTowerActionTriggers[DRYFIELD_WATER_TOWER_FIRST_ACTION_TRIGGER];
                firstActionTrigger->flags &= triggerDisableMask;
                runActionTrigger           = &_gDryfieldWaterTowerActionTriggers[DRYFIELD_WATER_TOWER_RUN_ACTION_TRIGGER];
                runActionTrigger->flags   &= triggerDisableMask;
                TASK_MESSAGE_DISPATCH_POINTER(work->fallingPropTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_FALLING_PROP_GROUND], 0);
                fallingPropWork                = work->fallingPropTask->work;
                fallingPropWork->shadowEnabled = 1;
                _dryfieldWaterTowerInstallDroppedPropCollision();
                if (work->mechanismState == GAME_FLAG_WATER_TOWER_MECHANISM_TANK_OPERATED) {
                    mechanismTrigger         = &_gDryfieldWaterTowerActionTriggers[DRYFIELD_WATER_TOWER_MECHANISM_TRIGGER];
                    mechanismTrigger->flags &= triggerDisableMask;
                    TASK_MESSAGE_DISPATCH_POINTER(work->slidingPropTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_EXTENDED], 0);
                    memCopyBytes(_gDryfieldWaterTowerCollision04608, _gDryfieldWaterTowerRoomCollisionGridVerts, sizeof(_gDryfieldWaterTowerCollision04608));
                    memCopyBytes(_gDryfieldWaterTowerCollision045F8, _gDryfieldWaterTowerRoomCollisionGridNormals, sizeof(_gDryfieldWaterTowerCollision045F8));
                    memCopyBytes(_gDryfieldWaterTowerCollision04688, _gDryfieldWaterTowerRoomCollisionGridFaces, sizeof(_gDryfieldWaterTowerCollision04688));
                    task->state = DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_FINISHED;
                }
            }
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_FIRST_ACTION:
            if (worldCollisionReadActionHit(&actionId, &actionReason, &actionArg) != 0 && (actionId & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM && (s8)actionReason == DRYFIELD_WATER_TOWER_ACTION_START_ACTOR_SCENE) {
                work->actorSceneTask = taskSpawnFromTable(_gDryfieldWaterTowerActorSceneTasks, DRYFIELD_WATER_TOWER_ACTOR_SCENE_TASK_SLOT, 0, 0);
                gameFlagSetNibble(GAME_FLAG_WATER_TOWER_PROGRESS, DRYFIELD_WATER_TOWER_PROGRESS_ACTOR_SCENE_STARTED);
                task->state++;
            }
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_ACTOR_SCENE:
            if (taskPollKill(work->actorSceneTask, &actorSceneExitValue) != 0) {
                task->state++;
            }
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_RUN_REQUEST:
            if (work->runRequested != 0) {
                work->runRequested = 0;
                work->phase        = 0;
                task->state++;
            }
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_TIMED_RUN:
            switch (_dryfieldWaterTowerStepTimedRun(task)) {
                case DRYFIELD_WATER_TOWER_RUN_UNDER_WAY:
                    break;
                case DRYFIELD_WATER_TOWER_RUN_TIMED_OUT:
                    work->phase = 0;
                    task->state--;
                    break;
                case DRYFIELD_WATER_TOWER_RUN_COMPLETED:
                    work->phase = 0;
                    task->state++;
                    break;
            }
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_WAIT_CHASERS:
            if (_dryfieldWaterTowerWaitForChasersStep(task)) {
                work->phase = 0;
                task->state++;
            }
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_CLOSING_SCENE:
            if (_dryfieldWaterTowerStepPropClosingScene(task)) {
                work->phase = 0;
                task->state++;
            }
            break;

        case DRYFIELD_WATER_TOWER_PROP_SCENE_STATE_FINISHED:
            break;
    }
    work->currentView = gGameSession->location.view;
    _dryfieldWaterTowerDispatchPropSceneRequest(task);
}

/// Restores animation set 1 from the player's equipped-weapon bank for a prop scene.
///
/// The low 16 bits of `blendFrames` are a duration in normal-rate animation
/// frames: zero resets the pose; nonzero blends from the current pose. With
/// character 1, weapon indices 0..32 select bank indices 1..33. The retained
/// alternate-character branch's bank extent is unproven. The player's model
/// and selected bank must be loaded. Playback disables world collision and
/// synchronously consumes the stack request.
static void _dryfieldWaterTowerRestorePlayerWeaponAnimation(s32 blendFrames)
{
    AnimationPlayRequest request;
    s32                  weaponId;
    s32                  bankIndex;
    s32                  durationFrames;

    weaponId                     = gPlayerStatus.weapon;
    bankIndex                    = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
    durationFrames               = blendFrames & 0xFFFF;
    request.source.index         = bankIndex;
    request.animationId          = PLAYER_ANIMATION_IDLE;
    request.blend                = durationFrames != 0 ? ANIMATION_BLEND_INTERPOLATE : ANIMATION_BLEND_RESET;
    request.blendFrames          = durationFrames;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

#include "../../shared/actor/actor_messages_place_ypr.inc.c"

/// Restarts a sliding or falling prop in the state selected by an actor command.
///
/// Receives `ACTOR_COMMAND_MESSAGE_APPLY` with a borrowed command and a live
/// prop-scene work block. Only `command` is read: 2 starts sliding out or falling,
/// and 3 starts sliding back or dropping. Motion phase, settle time, the unused
/// reset slot and kill countdown are cleared. Returns the selected state.
static s32 _dryfieldWaterTowerApplyPropCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedSecondArg)
{
    _DryfieldWaterTowerPropSceneWork* work = task->work;
    s32                               nextState;

    work->phase         = 0;
    work->field_60      = 0;
    work->settleFrames  = 0;
    nextState           = command->command;
    task->state         = nextState;
    task->killCountdown = 0;
    return nextState;
}

/// Requests a visible weapon re-equip and releases the scene's battle hold once.
///
/// Used by both the normal and skipped prop-closing scripts. The published
/// prop-scene driver and actor at placement index 0 must be live. Its lifetime
/// latch prevents duplicate re-equip requests and battle releases; a final
/// release awards that actor's rewards through the scene combat API.
static void _dryfieldWaterTowerRequestWeaponReequipOnce(void)
{
    enum { DRYFIELD_WATER_TOWER_FIRST_CHASER_PLACEMENT_INDEX = 0 };

    _DryfieldWaterTowerPropSceneWork* work = gDryfieldWaterTowerPropSceneTask->work;

    if (work->reequipRequested == 0) {
        gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gGameSession->flowFlags &= (0xFF ^ GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON);
        sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(DRYFIELD_WATER_TOWER_FIRST_CHASER_PLACEMENT_INDEX), 1);
        work->reequipRequested = 1;
    }
}

/// Cancels room effects and locks attachment controls for the prop scene.
///
/// The event script calls this before restoring the player's weapon animation.
/// Requires the live gameplay attachment state. The attachment/HUD update
/// clears the lock after the event has ended and HUD suppression has lifted.
static void _dryfieldWaterTowerLockAttachmentsForPropScene(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Posts a prop-scene request for the driver to consume on its next frame.
///
/// The published driver must be live. `request` is a
/// `DRYFIELD_WATER_TOWER_PROP_SCENE_REQUEST_*` value, stored in a 16-bit latch;
/// a later post replaces it. The adjacent, unread reset slot is also cleared.
static void _dryfieldWaterTowerPostPropSceneRequest(s16 request)
{
    _DryfieldWaterTowerPropSceneWork* work = gDryfieldWaterTowerPropSceneTask->work;

    work->request  = request;
    work->field_5E = 0;
}

/// Applies the timed run's opening-script end state when that script is skipped.
///
/// Requires the live prop-scene driver and sliding prop. Selects mapped view 7,
/// stops the prop at its far placement and stops vibration. Starts the running
/// sound only if the normal script has not already done so, then fades the
/// opening sound. This callback does not set the running-sound latch itself.
static void _dryfieldWaterTowerSkipRunOpening(void)
{
    enum {
        DRYFIELD_WATER_TOWER_RUN_MAPPED_VIEW          = 7,
        DRYFIELD_WATER_TOWER_OPENING_SOUND_ENTRY      = 6,
        DRYFIELD_WATER_TOWER_OPENING_SOUND_FADE_TICKS = 30
    };

    _DryfieldWaterTowerPropSceneWork* work = gDryfieldWaterTowerPropSceneTask->work;

    if (work->runningSoundStarted == 0) {
        sndEvtRequestScriptStart(SOUND_WATER_TOWER_CAP_RUNNING, 0, 0);
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewFindLogicalIndex(DRYFIELD_WATER_TOWER_RUN_MAPPED_VIEW);
    gGameSession->viewDirty                                = 1;
    TASK_MESSAGE_DISPATCH_POINTER(work->slidingPropTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_EXTENDED], 0);
    work->slidingPropTask->state = DRYFIELD_WATER_TOWER_PROP_STATE_IDLE;
    padScriptHalt();
    sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, DRYFIELD_WATER_TOWER_OPENING_SOUND_ENTRY), DRYFIELD_WATER_TOWER_OPENING_SOUND_FADE_TICKS);
}

/// Applies the timed run's ending-script end state when that script is skipped.
///
/// Requires the live prop-scene driver, sliding prop and player. Restores the
/// driver's selected logical view, stops the prop at its home placement and
/// stops vibration and run sounds. A completed run also places the player at
/// the closing scene's mark; a timeout retains the player's position.
static void _dryfieldWaterTowerSkipRunEnding(void)
{
    enum { DRYFIELD_WATER_TOWER_ENDING_SOUND_ENTRY = 7 };

    _DryfieldWaterTowerPropSceneWork* work = gDryfieldWaterTowerPropSceneTask->work;

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = work->nextView;
    gGameSession->viewDirty                                = 1;
    TASK_MESSAGE_DISPATCH_POINTER(work->slidingPropTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerSlidingPropPlacements[DRYFIELD_WATER_TOWER_SLIDING_PROP_HOME], 0);
    work->slidingPropTask->state = DRYFIELD_WATER_TOWER_PROP_STATE_IDLE;
    padScriptHalt();
    sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, DRYFIELD_WATER_TOWER_ENDING_SOUND_ENTRY), DRYFIELD_WATER_TOWER_SKIP_SOUND_FADE_TICKS);
    sndEvtRequestScriptStop(SOUND_WATER_TOWER_CAP_RUNNING, DRYFIELD_WATER_TOWER_SKIP_SOUND_FADE_TICKS);
    if (work->runResult == DRYFIELD_WATER_TOWER_RUN_COMPLETED) {
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerClosingPlayerPlacements[DRYFIELD_WATER_TOWER_PLAYER_CLOSING_MARK], 0);
    }
}

/// Applies the prop-closing scene's end state when its event script is skipped.
///
/// Requires the live driver, falling prop, player and collision grid. Selects
/// mapped view 9, stops the prop at ground level, places the player and halts
/// vibration and the drop sound. Leaves the session's view-redraw flag unchanged.
static void _dryfieldWaterTowerSkipPropClosingScene(void)
{
    enum {
        DRYFIELD_WATER_TOWER_CLOSING_MAPPED_VIEW   = 9,
        DRYFIELD_WATER_TOWER_PROP_GROUND_PLACEMENT = DRYFIELD_WATER_TOWER_FALLING_PROP_GROUND
    };

    _DryfieldWaterTowerPropSceneWork* work = gDryfieldWaterTowerPropSceneTask->work;
    _DryfieldWaterTowerPropSceneWork* fallingPropWork;

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewFindLogicalIndex(DRYFIELD_WATER_TOWER_CLOSING_MAPPED_VIEW);
    TASK_MESSAGE_DISPATCH_POINTER(work->fallingPropTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerFallingPropPlacements[DRYFIELD_WATER_TOWER_PROP_GROUND_PLACEMENT], 0);
    work->fallingPropTask->state = DRYFIELD_WATER_TOWER_PROP_STATE_IDLE;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerClosingPlayerPlacements[DRYFIELD_WATER_TOWER_PLAYER_CLOSING_MARK], 0);
    padScriptHalt();
    sndEvtRequestScriptStop(SOUND_WATER_TOWER_CAP_DROP, DRYFIELD_WATER_TOWER_SKIP_SOUND_FADE_TICKS);
    // Make collision and the ground shadow agree with the completed placement.
    _dryfieldWaterTowerInstallDroppedPropCollision();
    fallingPropWork                = work->fallingPropTask->work;
    fallingPropWork->shadowEnabled = 1;
}

/// Returns the next run's frame limit narrowed to 16 bits and rounded down to even.
///
/// Retained standalone entry with no recovered caller. Requires initialized
/// driver work as does `_dryfieldWaterTowerGetRunTimeLimitFrames`; its extra mask
/// leaves all four current table durations unchanged.
static s32 _dryfieldWaterTowerGetEvenRunTimeLimitFrames(const Task* task)
{
    enum { DRYFIELD_WATER_TOWER_EVEN_FRAME_MASK = 0xFFFE };

    return _dryfieldWaterTowerGetRunTimeLimitFrames(task) & DRYFIELD_WATER_TOWER_EVEN_FRAME_MASK;
}

/// Latches an actor's room event so the mechanism's closing step can finish.
///
/// Receives `ROOM_MESSAGE_ACTOR_EVENT` on the initialized prop-scene driver.
/// Both payload words are ignored; the latch stays set for this driver's lifetime.
/// No sender reads a reply value.
static void _dryfieldWaterTowerLatchActorEvent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    _DryfieldWaterTowerPropSceneWork* work = task->work;

    work->actorEventReceived = 1;
}

/// Requests a timed mechanism run for the driver to start when it is waiting.
///
/// Receives `DRYFIELD_WATER_TOWER_MESSAGE_REQUEST_RUN` on the initialized
/// prop-scene driver. Both payload words are ignored. Repeated requests coalesce
/// until the driver consumes the latch. No sender reads a reply value.
static void _dryfieldWaterTowerRequestRun(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    _DryfieldWaterTowerPropSceneWork* work = task->work;

    work->runRequested = 1;
}

/// Consumes one actor-scene request, then clears the latch even for an unhandled value.
///
/// Requires initialized work, live player and both placed actors, and loaded
/// placement and animation records. Sets up the two actor clips and placements,
/// restarts individual clips, or shows and places the player. Setup releases the
/// player's model buffer and starts an eight-frame fade-in task.
static void _dryfieldWaterTowerDispatchActorSceneRequest(Task* task)
{
    enum { DRYFIELD_WATER_TOWER_ACTOR_SCENE_FADE_IN_FRAMES = 8 };

    _DryfieldWaterTowerActorSceneWork* work = task->work;

    switch (work->request) {
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_NONE:
            break;
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_SET_UP:
            TASK_MESSAGE_DISPATCH_POINTER(work->firstActorTask, ACTOR_MESSAGE_PLAY_ANIMATION, &_gDryfieldWaterTowerActorSceneAnimations[DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_OPENING_CLIP], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->firstActorTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerActorScenePlacements[DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_START], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->secondActorTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerActorScenePlacements[DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_START], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->secondActorTask, ACTOR_MESSAGE_PLAY_ANIMATION, &_gDryfieldWaterTowerActorSceneAnimations[DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_OPENING_CLIP], 0);
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE, 0);
            taskSpawnFromTable(_gDryfieldWaterTowerActorSceneTasks, DRYFIELD_WATER_TOWER_ACTOR_SCENE_FADE_IN_TASK_SLOT, DRYFIELD_WATER_TOWER_ACTOR_SCENE_FADE_IN_FRAMES, 0);
            break;
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_MOVE_FIRST_ACTOR:
            TASK_MESSAGE_DISPATCH_POINTER(work->firstActorTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerActorScenePlacements[DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_END], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->firstActorTask, ACTOR_MESSAGE_PLAY_ANIMATION, &_gDryfieldWaterTowerActorSceneAnimations[DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_CLOSING_CLIP], 0);
            break;
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_PLACE_PLAYER:
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerActorScenePlayerPlacement, 0);
            break;
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_FIRST_ACTOR_CLIP:
            TASK_MESSAGE_DISPATCH_POINTER(work->firstActorTask, ACTOR_MESSAGE_PLAY_ANIMATION, &_gDryfieldWaterTowerActorSceneAnimations[DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_OPENING_CLIP], 0);
            break;
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_SECOND_ACTOR_CLIP:
            TASK_MESSAGE_DISPATCH_POINTER(work->secondActorTask, ACTOR_MESSAGE_PLAY_ANIMATION, &_gDryfieldWaterTowerActorSceneAnimations[DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_OPENING_CLIP], 0);
            break;
    }
    work->request = DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_NONE;
}

/// Starts the actor scene's weapon pose with a ten-frame interpolation.
///
/// The player and selected bank must be live. Primary weapon indices 0..32
/// select banks 1..33 and set 1. The retained alternate branch selects weapon
/// + 34 beyond the known bank table; its valid domain is unproven. The dispatch
/// borrows the stack request synchronously, requests disabled grid collision
/// and uses ten whole normal-rate animation frames for interpolation.
static inline void _dryfieldWaterTowerStartActorSceneWeaponPose(void)
{
    enum {
        DRYFIELD_WATER_TOWER_ACTOR_SCENE_WEAPON_BLEND_FRAMES = 10,
    };
    AnimationPlayRequest animationRequest;
    s32                  weaponIndex;
    s32                  bankIndex;

    weaponIndex                           = gPlayerStatus.weapon;
    bankIndex                             = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponIndex);
    animationRequest.source.index         = bankIndex;
    animationRequest.animationId          = PLAYER_ANIMATION_IDLE;
    animationRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    animationRequest.blendFrames          = DRYFIELD_WATER_TOWER_ACTOR_SCENE_WEAPON_BLEND_FRAMES;
    animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
}

/// Owns the first-action actor scene, its request work and the player's weapon pose.
///
/// Waits for scene updates, the attachment menu and pending display changes before
/// setup. Selects the equipped weapon's animation bank (primary banks 1..33 for
/// weapon indices 0..32) and blends set 1 over ten animation frames; the retained
/// alternate-character bank extent is unproven. Both placed actors must exist.
/// Runs the normal or skip event script and requests task teardown when it ends.
/// Active ticks require unpaused scene updates and a closed attachment menu;
/// each surviving state dispatches the scene's pending actor request.
/// The borrowed player and actor tasks and room records must remain live.
static void _dryfieldWaterTowerActorSceneTask(Task* task)
{
    enum {
        DRYFIELD_WATER_TOWER_ACTOR_SCENE_STATE_INITIALIZE,
        DRYFIELD_WATER_TOWER_ACTOR_SCENE_STATE_START_SCRIPT,
        DRYFIELD_WATER_TOWER_ACTOR_SCENE_STATE_WAIT_SCRIPT,
        DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_ACTOR_KEY = 1 << 12
    };

    _DryfieldWaterTowerActorSceneWork* work;
    s32                                actorPlaceKey;

    if (gGameSession->sceneUpdatesPaused != 0) {
        return;
    }
    if ((s8)gAttachmentState.menuOpen != ATTACHMENT_MENU_CLOSED) {
        return;
    }

    switch (task->state) {
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_STATE_INITIALIZE:
            // Prepare the player pose before publishing the script request work.
            if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            _dryfieldWaterTowerStartActorSceneWeaponPose();
            work       = memMalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                gDryfieldWaterTowerActorSceneTask = task;
                actorPlaceKey                     = gGameSession->location.area | (gGameSession->location.stage << 8);
                work->firstActorTask              = sceneFindEnemyByPlaceKey(actorPlaceKey)->task;
                actorPlaceKey                     = ((gGameSession->location.stage << 8) | DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_ACTOR_KEY) | gGameSession->location.area;
                work->secondActorTask             = sceneFindEnemyByPlaceKey(actorPlaceKey)->task;
            }
            task->state++;
            break;
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_STATE_START_SCRIPT:
            evsStartScriptWithSkip(_gDryfieldWaterTowerActorSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldWaterTowerActorSceneSkipScript);
            task->state++;
            break;
        case DRYFIELD_WATER_TOWER_ACTOR_SCENE_STATE_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
                return;
            }
            break;
    }
    _dryfieldWaterTowerDispatchActorSceneRequest(task);
}

#include "../../shared/screen/screen_fade_in.inc.c"
#undef SCREEN_FADE_IN_TASK

#include "../../shared/screen/screen_fade_out.inc.c"

/// Stages the selected actor scene's audio-start request for later CD dispatch.
///
/// The script must have selected the scene first; its buffers must survive
/// consumption of the deferred request.
static void _dryfieldWaterTowerStageActorSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Enqueues playback of the actor scene after its audio-start cue.
///
/// The selected scene and prepared playback buffers must remain live through
/// CD dispatch, with room in the resident command queue.
static void _dryfieldWaterTowerStartActorScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes the actor scene's streaming session and restores its saved random state.
///
/// Called after the final playback cue; the selected scene session must be live.
/// The script continues with actor and player commands after this callback.
static void _dryfieldWaterTowerFinishActorScenePlayback(void)
{
    streamFinishScene();
}

/// Posts an actor-scene request for the scene task to consume on its next frame.
///
/// The published actor-scene task must be live. `request` is a
/// `DRYFIELD_WATER_TOWER_ACTOR_SCENE_REQUEST_*` value, stored in a 16-bit latch;
/// a later post replaces it. The adjacent, unread reset slot is also cleared.
static void _dryfieldWaterTowerPostActorSceneRequest(s16 request)
{
    _DryfieldWaterTowerActorSceneWork* work = gDryfieldWaterTowerActorSceneTask->work;

    work->request = request;
    work->field_E = 0;
}

/// Engages the chaser battle and releases the first chaser's scripted pose once.
///
/// Used by the normal and skipped introductory actor scene. Requires its live
/// published task and the room's actors. The actor-scene lifetime latch prevents
/// repeat broadcasts; engaging combat does not acquire a battle reference.
static void _dryfieldWaterTowerStartChaserBattleOnce(void)
{
    _DryfieldWaterTowerActorSceneWork* work = gDryfieldWaterTowerActorSceneTask->work;
    ActorCommand                       command;

    if (work->actorCommandSent == 0) {
        sceneEngageBattle(1);
        command.context.loc.stage = gGameSession->location.stage;
        command.context.loc.area  = gGameSession->location.area;
        command.command           = DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_BATTLE;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &command, ACTOR_COMMAND_MESSAGE_APPLY);
        work->actorCommandSent = 1;
    }
}

/// Applies the introductory actor scene's end placements when its script is skipped.
///
/// Requires the live actor-scene task, both actors, player and selected stream
/// session. Shows and places the player, selects mapped view 4 and requests
/// its redraw, then cancels playback and restores scene streaming/random state.
static void _dryfieldWaterTowerSkipActorScene(void)
{
    enum {
        DRYFIELD_WATER_TOWER_ACTOR_SCENE_END_MAPPED_VIEW = 4,
        DRYFIELD_WATER_TOWER_SECOND_ACTOR_END_PLACEMENT  = DRYFIELD_WATER_TOWER_ACTOR_SCENE_SECOND_END,
        DRYFIELD_WATER_TOWER_FIRST_ACTOR_END_PLACEMENT   = DRYFIELD_WATER_TOWER_ACTOR_SCENE_FIRST_END
    };

    _DryfieldWaterTowerActorSceneWork* work = gDryfieldWaterTowerActorSceneTask->work;

    // Preserve the placements' actor order before releasing the player's scene pose.
    TASK_MESSAGE_DISPATCH_POINTER(work->secondActorTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerActorScenePlacements[DRYFIELD_WATER_TOWER_SECOND_ACTOR_END_PLACEMENT], 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->firstActorTask, ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerActorScenePlacements[DRYFIELD_WATER_TOWER_FIRST_ACTOR_END_PLACEMENT], 0);
    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWaterTowerActorScenePlayerPlacement, 0);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewFindLogicalIndex(DRYFIELD_WATER_TOWER_ACTOR_SCENE_END_MAPPED_VIEW);
    gGameSession->viewDirty                                = 1;
    // Cancellation finishes the stream; retain the callback's repeated finish.
    cdCmdCancelScene();
    streamFinishScene();
}

void dryfieldWaterTowerSetMechanismSpriteVisible(u8 visible)
{
    enum {
        DRYFIELD_WATER_TOWER_MECHANISM_SPRITE_VIEW_INDEX  = 19,
        DRYFIELD_WATER_TOWER_MECHANISM_SPRITE_BATCH_INDEX = 1
    };

    GameLocationKey* location;
    SpriteBatch*     batches;

    location = &gGameSession->location;
    if (location->stage == GAME_STAGE_DRYFIELD) {
        batches = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1][DRYFIELD_WATER_TOWER_MECHANISM_SPRITE_VIEW_INDEX - 1].batches;
        if (!visible) {
            batches[DRYFIELD_WATER_TOWER_MECHANISM_SPRITE_BATCH_INDEX].hidden = 1;
            return;
        }
        batches[DRYFIELD_WATER_TOWER_MECHANISM_SPRITE_BATCH_INDEX].hidden = 0;
    }
}

void dryfieldWaterTowerUpdateViewEffectGateTask(Task* unused)
{
    gRoomEffectState->roomEffectMode = _gDryfieldWaterTowerViewEffectModes[(u8)viewGetMappedIndex() - 1];
}
