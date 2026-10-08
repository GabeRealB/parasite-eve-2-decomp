#ifndef SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Requests the prop scene's event script posts to the scene's driver task.
///
/// The driver carries a request out on its next frame and clears it, so each
/// one lasts a single frame.
enum {
    DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_NONE        = 0, // Nothing posted
    DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_START_SLIDE = 1, // Hide the player's model, show the prop and start its slide
    DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_SHOW_PLAYER = 2, // Switch to the room's view 3 and show the player's model again
    DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_PLAY_SOUNDS = 3, // Queue the scene's two sound events
};

/// Prop task states also sent in `ActorCommand::command` to restart its slide.
enum {
    DRYFIELD_WATER_TANK_PROP_STATE_INIT    = 0,
    DRYFIELD_WATER_TANK_PROP_STATE_IDLE    = 1,
    DRYFIELD_WATER_TANK_PROP_STATE_SLIDING = 2,
};

extern TaskDesc D_dryfield_water_tank_80184DF4[2];

extern u16 D_dryfield_water_tank_801868CC[10];

extern Task* D_dryfield_water_tank_80188D50;

extern EvsCommand D_dryfield_water_tank_8017F114[11];

extern EvsCommand D_dryfield_water_tank_8017F21C[11];

extern TaskMessageEntry D_dryfield_water_tank_8017F324[5];

extern TaskDesc D_dryfield_water_tank_8017F34C[2];

extern ActorTransform D_dryfield_water_tank_8017FD60[2];

extern u16 D_dryfield_water_tank_8017FDA8[12];

extern EvsCommand D_dryfield_water_tank_8017FDC0[11];

extern EvsCommand D_dryfield_water_tank_8017FEC8[8];

extern TaskDesc D_dryfield_water_tank_8017FF88[2];

extern TaskDesc D_dryfield_water_tank_80180794;

extern AnimationSet gDryfieldWaterTankAnimation034BC;

extern AnimationSet gDryfieldWaterTankAnimation0377C;

extern AnimationSet gDryfieldWaterTankAnimation03A60;

extern AnimationSet gDryfieldWaterTankAnimation03CB4;

extern AnimationSet gDryfieldWaterTankAnimation04000;

extern AnimationSet gDryfieldWaterTankAnimation047BC;

extern AnimationSet gDryfieldWaterTankAnimation04AA0;

extern AnimationSet gDryfieldWaterTankAnimation04C98;

extern AnimationSet gDryfieldWaterTankAnimation04FEC;

extern AnimationSet gDryfieldWaterTankAnimation051E4;

extern AnimationSet gDryfieldWaterTankAnimation05704;

extern AnimationSet gDryfieldWaterTankAnimation059E4;

extern AnimationSet gDryfieldWaterTankAnimation05DB8;

extern AnimationSet gDryfieldWaterTankAnimation061A8;

extern AnimationSet gDryfieldWaterTankAnimation06514;

extern AnimationSet gDryfieldWaterTankAnimation06840;

extern AnimationSet gDryfieldWaterTankAnimation06C68;

extern AnimationSet gDryfieldWaterTankAnimation06F48;

extern TaskMessageEntry D_dryfield_water_tank_8017FD90[3];

/// Selects the room sprites shown before or after operating the tank mechanism.
///
/// Nonzero shows the pre-operation sprite in mapped view 8 and hides the
/// post-operation sprite in mapped view 3; zero reverses them. The caller uses
/// nonzero for mechanism states 0..2 and zero for state 3. Only the argument's
/// low byte is significant. No-op outside `GAME_STAGE_DRYFIELD`.
///
/// In that stage, the active area's loaded sprite directory must be the water
/// tank's ten-view array, with at least four batches in view 3 and two in view 8.
/// Mutates the room-owned batches without allocating or releasing resources.
void dryfieldWaterTankSetPreOperationSprites(u8 beforeOperation);

/// Initializes and lights the scene prop, advancing its requested slide each frame.
///
/// Spawned by `dryfieldWaterTankPropSceneTask` with a TMD body whose primitive
/// buffer has not been allocated. Owns a zeroed work block and model buffer,
/// borrows the room's model data, and joins the driver's teardown tree. The
/// driver must be published before initialization. `DRYFIELD_WATER_TANK_PROP_STATE_*`
/// selects initialization, idle or sliding; arrival returns to idle. Lighting
/// samples the root's composed world translation every frame, including idle.
/// Requires successful work allocation and composed coordinates for that query.
void dryfieldWaterTankPropTask(Task* task);

/// Runs the sliding prop scene and consumes requests posted by its event script.
///
/// Owns its zeroed work and the spawned prop task; publishes the driver before
/// spawning the prop. Initialization requires successful work and child-task
/// allocation. The next frame places the prop and starts the normal/skip scripts.
/// Requests hide/show the player, start the slide or queue sounds, then clear.
/// On script completion exits its child and suspends this driver with a stop
/// request; the driver's own work remains allocated until external teardown.
/// Room resources, the player and published driver must outlive script callbacks.
void dryfieldWaterTankPropSceneTask(Task* task);

/// Posts a single-frame prop-scene request for the driver's next update.
///
/// `request` is one of `DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_*`; a later post
/// overwrites an earlier pending request. Requires the live scene driver and
/// its allocated work. Also clears an unread work halfword of unproven purpose.
void dryfieldWaterTankPostPropSceneRequest(s16 request);

/// Restores the player and final view when the prop scene is skipped.
///
/// Requires the live driver and its player task. Selects logical view 3, shows
/// the player's model and requests a view reload, then queues a fade over ten
/// audio updates of scene sound entry 2. Scene-task cleanup belongs to the skip script.
void dryfieldWaterTankSkipPropScene(void);

/// Spawns one of the room's two scripted player-path tasks from a packed word.
///
/// Bits 0..15 select leg 0 or 1; bits 16..31 are an unsigned spawnArg1 value
/// in 0..65535, and spawnArg2 is zero. Both current script calls pass zero in
/// the high half. The room's descriptor table and player must remain loaded
/// through path playback. No selector bounds check or allocation result is returned.
void dryfieldWaterTankSpawnPlayerPathTask(u32 packedPath);

/// Places the player at one entry of the first scripted path per callback tick.
///
/// A fresh bodyless task starts `killCountdown` at zero; entries 0..51 are
/// consumed, then the task kills itself. The loaded room table has 82 entries,
/// but its final 30 are not part of this leg. Positions use world-coordinate
/// units and yaw is -2047 in 4096 units per turn. Requires the live registered
/// player; placement is consumed synchronously and animation is controlled by
/// the scene script. Negative counters are outside this callback's domain.
void dryfieldWaterTankMovePlayerFirstLegTask(Task* task);

/// Places the player along the second scripted path, one entry per callback tick.
///
/// Follows `dryfieldWaterTankMovePlayerFirstLegTask` with a separate fresh
/// bodyless task and zero `killCountdown`. Consumes all 52 entries, then kills
/// itself. Positions use world-coordinate units and yaw is 1024 in 4096 units
/// per turn. Requires the live registered player; dispatch consumes placement
/// synchronously. Negative counters are outside this callback's domain.
void dryfieldWaterTankMovePlayerSecondLegTask(Task* task);

/// Offers the tank mechanism's CAP prompt and starts its prop scene on acceptance.
///
/// CAP command 14, variant 0 prompts; retained choice key 10 operates the tank.
/// Already-operated state 3 instead plays variant 1 with a display transition
/// and immediately ends this task. A fresh prompt holds/hides the player, saves
/// the current saved-view byte, waits for CAP to finish, then hides actors until
/// resolving the choice. Declining restores the player, HUD, event flag and view;
/// accepting sets mechanism state 3 and hands presentation to the prop scene.
/// Requires idle CAP playback, the room's CAP resources and live player/session.
void dryfieldWaterTankMechanismPromptTask(Task* task);

/// Refuses every `ROOM_MESSAGE_USE_KEY_ITEM` request without consuming an item.
///
/// All arguments are unused; returns `ROOM_KEY_ITEM_USE_REFUSED`.
s32 dryfieldWaterTankRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg);

/// Accepts a room-transition request unchanged, returning 1.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows an initialized eight-byte request and
/// a writable reply, which may be the same object. Copies the complete record
/// even in query mode; retains neither pointer and performs no departure effects.
s32 dryfieldWaterTankResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Handles the water tank's direction-trigger actions, returning 1 for every ID.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request synchronously;
/// only actionId is read. ID 1 latches and starts the movie event once, ID 2
/// latches the player-path scene and updates story progress once, and IDs 3/4
/// start the two player-placement scripts. Other IDs have no effect. The room
/// resources and player must be live; task, messageId and secondArg are unused.
s32 dryfieldWaterTankHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg);

/// Routes CAP room command 14 to the mechanism prompt task, returning 0.
///
/// `ROOM_MESSAGE_COMMAND` carries the CAP command index as its first word.
/// Other indices do nothing; task, messageId and secondArg are unused.
s32 dryfieldWaterTankHandleRoomCommand(Task* task, s32 messageId, s32 commandIndex, s32 secondArg);

/// Spawns the movie-event task, then polls its requested teardown before ending.
///
/// A fresh bodyless task starts at state 0; state 1 polls the published child
/// handle and kills this waiter when its exit handler has run. Requires a
/// successful child spawn and that handle to remain live until polling succeeds.
/// This waiter performs the movie-event task's requested cleanup.
void dryfieldWaterTankWaitMovieEventTask(Task* task);

/// Applies `ACTOR_MESSAGE_SET_MODEL_DRAW` to the scene prop's live model.
///
/// Zero `visible` sets `TMD_OBJECT_SKIP_ACTIVE_DRAW`; nonzero clears it, leaving
/// other flags intact. `messageId` and `secondArg` are unused. No reply is defined.
void dryfieldWaterTankSetPropModelDraw(Task* task, s32 messageId, s32 visible, s32 secondArg);

/// Restarts the prop's movement phase and applies the command's task state.
///
/// `ACTOR_COMMAND_MESSAGE_APPLY` borrows `command` through dispatch and reads
/// only its command word: `DRYFIELD_WATER_TANK_PROP_STATE_*` selects initialization,
/// idle or sliding. The scene driver sends the sliding state. Requires allocated
/// prop work; resets the dust index and an unread halfword
/// of unproven purpose, retaining the settle count and current model position.
/// `messageId` and `secondArg` are unused. No reply is defined.
void dryfieldWaterTankRestartPropSlide(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg);

#endif // SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
