#ifndef SRC_ROOMS_DRYFIELD_WATER_TOWER_DRYFIELD_WATER_TOWER_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WATER_TOWER_DRYFIELD_WATER_TOWER_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Prop-scene tasks: the driver, sliding prop and falling prop, in slots 0..2.
///
/// All use priority `TASK_PRIORITY_CONTROL`; the props use TMD bodies and allocate their primitive
/// buffers themselves. Slot 2 also spawns the free-falling copy with argument 1.
/// No TASK_DESC_END is stored: callers must select a slot in 0..2. The room
/// owns the recipes and models and must remain loaded through task teardown.
extern TaskDesc gDryfieldWaterTowerPropSceneTasks[3];

/// Borrowed prop-scene driver published after its work allocation succeeds.
///
/// Normal and skip script callbacks require this task and its work to be live.
/// The task system owns both; this room-visit handle is not cleared at teardown.
extern Task* gDryfieldWaterTowerPropSceneTask;

/// Borrowed controller for the introductory actor scene and its pending requests.
///
/// Published after its work allocation succeeds. Script callbacks require the
/// live controller and work; scene completion requests teardown without
/// clearing this handle. The pointer does not retain the task or its work.
extern Task* gDryfieldWaterTowerActorSceneTask;

/// Bodyless transition-task recipe shared by this overlay's gate and event task.
///
/// Runs at priority `TASK_PRIORITY_SCENE` with zero spawn payloads and unused metadata. The gate
/// reads the recipe synchronously after latching the request and destination;
/// keep those snapshots and the room callback code live through session reload.
extern TaskDesc gRoomEventTaskDesc;

/// Bodyless mechanism-prompt task at priority `TASK_PRIORITY_SCENE`, followed by TASK_DESC_END.
///
/// CAP room command 7 spawns slot 0 with zero arguments. The room owns the
/// descriptor and callback and must remain loaded until the prompt task ends.
extern TaskDesc gDryfieldWaterTowerMechanismPromptTasks[2];

/// Water-tower room receivers for transitions, key items, actions, CAP commands and events.
///
/// Six live entries precede TASK_MESSAGE_TABLE_END. Installed on the room task
/// and borrowed for its lifetime. Actor events forward to the prop-scene driver
/// and define no usable reply; never dispatch the end marker itself.
extern TaskMessageEntry gDryfieldWaterTowerRoomMessages[7];

/// Requests a timed mechanism run from the room's prop-scene driver.
///
/// Both payload words are ignored; the driver consumes the request when waiting.
/// The sender must discard the handler's reply word.
enum { DRYFIELD_WATER_TOWER_MESSAGE_REQUEST_RUN = 5100 };

/// Shows or hides the mechanism's single background sprite in mapped view 19.
///
/// `visible` is zero to hide, nonzero to draw. During Dryfield, the water-tower
/// area's sprite directory and its three-record view-19 batch list must be live.
/// The selected batch contains source sprite 0. Other stages are left alone.
void dryfieldWaterTowerSetMechanismSpriteVisible(u8 visible);

// Callbacks referenced by the overlay's shared data tables.
/// Prompts for a tower mechanism run and transfers an accepted run to its driver.
///
/// Start in state 0 with the player, prop-scene driver and room CAP resources
/// live. Initial/restored mechanism states hold player presentation and save
/// the starting view; reply key 10 marks the tower operated,
/// requests the timed run and plays its sound. Other replies restore the view,
/// HUD and player control. Operated states only start transition CAP playback.
/// Finished paths release the task; the accepted run owns subsequent restoration.
void dryfieldWaterTowerMechanismPromptTask(Task* task);

/// Refuses every collected key-item use at the water tower.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; no payload or task storage is read.
/// Returns `ROOM_KEY_ITEM_USE_REFUSED` without consuming the item.
s32 dryfieldWaterTowerUseKeyItemMsg(Task* task, s32 messageId, s32 itemId, s32 secondArg);

/// Starts the mechanism prompt for CAP room command 7.
///
/// Other commands do nothing. Requires loaded room descriptors; always returns
/// zero, including spawn failure, and retains no payload.
s32 dryfieldWaterTowerCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

/// Ignores room direction-action requests and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`. The request is borrowed through
/// dispatch, but this handler does not dereference it or read either other word.
s32 dryfieldWaterTowerRoomActionMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg);

/// Forwards room actor events to the mechanism's scene driver.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT`, preserving its ID and both integer
/// payload words, and forwards the driver's reply word. The sender must discard
/// that word: the driver's handler defines no return value. Requires the room's
/// published driver task to be live; no absent-task guard is provided here.
s32 dryfieldWaterTowerActorEventMsg(Task* task, s32 messageId, s32 eventId, s32 eventArg);

#endif // SRC_ROOMS_DRYFIELD_WATER_TOWER_DRYFIELD_WATER_TOWER_PRIVATE_H
