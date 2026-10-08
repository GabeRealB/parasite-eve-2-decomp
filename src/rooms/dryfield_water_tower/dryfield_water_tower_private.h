#ifndef SRC_ROOMS_DRYFIELD_WATER_TOWER_DRYFIELD_WATER_TOWER_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WATER_TOWER_DRYFIELD_WATER_TOWER_PRIVATE_H

#include "common.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// View slot the water-tower mechanism scene started on, plus the zero word
/// that follows it.
///
/// A CAP command can move the room to another view. The scene task copies the
/// live save's `location.loc.view` here before starting its command, and
/// writes it back when the command ends without the event key that advances
/// the mechanism; with that key the view the command left is kept. The slot is
/// a byte stored as a word, so only the low byte carries it. The following
/// word is zero in the room's image and has no recovered access; its role, and
/// whether it belongs to this object at all, is unproven.
typedef struct {
    u32 view;       // 1-based view slot saved at the scene's start (a `u8` stored as a word)
    u8  field_4[4]; // Zero bytes with no accesses; role unproven
} DryfieldWaterTowerSavedView;
STATIC_ASSERT_SIZEOF(DryfieldWaterTowerSavedView, 8);

/// Prop-scene tasks: the driver, sliding prop and falling prop, in slots 0..2.
///
/// The room entry spawns `_dryfieldWaterTowerPropSceneTask`; that driver spawns
/// `_dryfieldWaterTowerSlidingPropTask` and `_dryfieldWaterTowerFallingPropTask`.
extern TaskDesc D_dryfield_water_tower_80182384[];

extern Task* D_dryfield_water_tower_801876A4;

extern Task* D_dryfield_water_tower_801876AC;

extern TaskDesc gRoomEventTaskDesc;

extern TaskDesc D_dryfield_water_tower_801803D8[2];

extern DryfieldWaterTowerSavedView D_dryfield_water_tower_8018768C;

/// The room's message table, `(messageId, handler)` pairs ending at
/// `TASK_MESSAGE_TABLE_END`, which the entry task installs as its own
/// `Task::msgTable`.
extern TaskMessageEntry D_dryfield_water_tower_801803A0[7];

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
/// `DryfieldWaterTowerSavedView::view`; reply key 10 marks the tower operated,
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
