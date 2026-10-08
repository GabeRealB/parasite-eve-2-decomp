#ifndef SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

/// The room's two-entry task descriptor table: entry 0 starts the streamed
/// sequence, entry 1 is the task that plays it.
extern TaskDesc D_dryfield_night_dilapidated_house_801872B4[];

extern WorldCoordRoomLights D_dryfield_night_dilapidated_house_80189B60[1];

extern WorldCollisionTrigger D_dryfield_night_dilapidated_house_80189B78[12];

extern WorldCollisionOccluder D_dryfield_night_dilapidated_house_80189F08[1];

extern WorldCoordRoomAmbientEntry D_dryfield_night_dilapidated_house_8018A054[12];

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_dryfield_night_dilapidated_house_8017E700[5];

extern EvsCommand D_dryfield_night_dilapidated_house_801868F4[88];

extern EvsCommand D_dryfield_night_dilapidated_house_80187134[16];

extern WorldCoordPointLight D_dryfield_night_dilapidated_house_80189500[8];

extern WorldCoordSpotLight D_dryfield_night_dilapidated_house_80189800[1];

// Callbacks referenced by the overlay's shared data tables.

/// Refuses `ROOM_MESSAGE_USE_KEY_ITEM` without consuming the selected collected item.
///
/// Ignores both payload words and returns `ROOM_KEY_ITEM_USE_REFUSED`.
s32 dryfieldNightDilapidatedHouseRefuseKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArgument);

/// Gates the first back-street departure with the house's door event.
///
/// Borrows an eight-byte request and writable reply, which may be the same
/// record, and copies the whole request. Returns 1 for other destinations or
/// an unlocked door, and 2 while the door event is eligible. Queries preserve
/// progress; execution latches the request, unlocks the door and requests the
/// deferred CAP/sound/transition task, without rollback if allocation fails.
/// Keep the room and its event records loaded through that task's completion.
/// The receiver and message ID are unused.
s32 dryfieldNightDilapidatedHouseGateBackStreetExit(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Ignores `ROOM_MESSAGE_COMMAND` and returns zero without changing room state.
///
/// The integer command ID and its command-specific argument are unused.
s32 dryfieldNightDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArgument);

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION` and returns zero without changing room state.
///
/// The borrowed trigger request is neither read nor retained; the second
/// payload word is unused.
s32 dryfieldNightDilapidatedHouseIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArgument);

/// Stages audio start for the scene selected by the first-visit event script.
///
/// The selected scene and its prepared buffers must survive CD consumption.
/// A later CD dispatch commits this deferred request; this callback does not
/// change the scene/audio mode.
void dryfieldNightDilapidatedHouseStageSceneAudioStart(void);

/// Enqueues playback of the first-visit script's selected scene/audio session.
///
/// Requires free CD request capacity and prepared playback buffers that remain
/// live through consumption. Without a selected slot, enters playing mode directly.
void dryfieldNightDilapidatedHouseEnqueueScenePlayback(void);

/// Ends first-visit scene streaming and restores the random values saved at selection.
///
/// Requires prior successful scene selection. Leaves CD cancellation, buffers
/// and task teardown to their owners.
void dryfieldNightDilapidatedHouseFinishScene(void);

/// Cancels the first-visit scene when its event script is skipped.
///
/// Discards the deferred CD replacement, requests asynchronous cancellation and
/// immediately ends streaming and restores saved random values. Requires prior
/// successful scene selection; buffer and task teardown remain with their owners.
void dryfieldNightDilapidatedHouseCancelScene(void);

/// Requests the house's movie-display launcher from the first-visit event script.
///
/// Borrows the room's two-entry descriptor table; keep the room loaded until
/// movie playback and game-display restoration finish. Allocation failure is
/// unchecked. This callback has no arguments or result.
void dryfieldNightDilapidatedHouseStartMovie(void);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H
