#ifndef SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

/// Room-controller routes for transitions, key items, interactions and CAP commands.
///
/// Four handlers and a TASK_MESSAGE_TABLE_END marker. The live room task borrows
/// this table and its callbacks while the overlay remains loaded.
extern TaskMessageEntry gNeoArkPowerPlant1RoomMessages[5];

/// Generator-clear presentation, including CAP cues, vibration and the two release waits.
///
/// Starts after placed actor 0 reports defeat. Twenty-four instructions include
/// END; the interpreter borrows this table and its payloads through completion.
/// Skipping is disabled before the final view and battle-exit restoration.
extern EvsCommand gNeoArkPowerPlant1GeneratorClearScript[24];

/// Skip path that restores the view and player, stops vibration and releases both generator waits.
///
/// Ten instructions include END. Used only while the generator-clear scene's
/// skip target remains armed; table and payloads belong to the loaded room.
extern EvsCommand gNeoArkPowerPlant1GeneratorClearSkipScript[10];

/// One-shot view-3 battle entry: place the player, play the room clip and engage combat.
///
/// Thirteen instructions include END. Installs extended clip 47 in the equipped
/// bank, plays the room clip, then resumes player control. The retained half-rate
/// request precedes playback, which resets the player rate to normal.
/// The interpreter and player borrow the room's tables and animation resources.
extern EvsCommand gNeoArkPowerPlant1BattleEntryScript[13];

/// Room ticks remaining before changing the view-7 sound (0 unarmed).
///
/// Saved/active view mismatch arms four after this plant clears and while the
/// second remains uncleared. Predecrement to zero starts or stops area script 10.
extern s32 gNeoArkPowerPlant1ViewSoundDelayFrames;

/// Surface class 4's walking, running and scripted-jump footstep cue bases.
///
/// Type-1 requests select the loaded bank; cue 1 adds one to each base, and
/// companion playback adds 100. The surface table borrows this room-owned record.
extern const WorldCollisionFootstepSounds gNeoArkPowerPlant1Surface4FootstepSounds;

/// Surface class 5's walking, running and scripted-jump footstep cue bases.
///
/// Type-1 requests select the loaded bank; cue 1 adds one to each base, and
/// companion playback adds 100. The surface table borrows this room-owned record.
extern const WorldCollisionFootstepSounds gNeoArkPowerPlant1Surface5FootstepSounds;

// Callbacks referenced by the overlay's shared data tables.

/// Refuses key-item use in Power Plant 1 without consuming the item or starting an event.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED` for the item menu's unavailable-use notice.
s32 neoArkPowerPlant1RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);

/// Resolves a Neo Ark destination room for a Power Plant 1 transition request.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Copies the complete eight-byte request
/// into writable reply storage before resolving its room from game progress.
/// Query mode preserves the copied record. The pointers may alias and are
/// borrowed only for this call; the Neo Ark map overlay must be loaded.
/// Always returns 1 to allow the transition; task and message ID are unused.
s32 neoArkPowerPlant1ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Selects Power Plant 1 CAP dialogue from generator and plant progress.
///
/// Handles `ROOM_MESSAGE_COMMAND`: command 2 tests this plant's clearance;
/// command 3 tests battle completion, then generator-part defeat. Commands 9
/// and 12 select two dialogue pairs by Power Plant 2 clearance. Other commands
/// do nothing. Starts display-transition CAP playback and always returns zero.
/// The task, message ID and final word are unused; keep the plant CAP resources
/// loaded through playback.
s32 neoArkPowerPlant1HandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg);

/// Ignores trigger-driven room actions in Power Plant 1 and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`. The borrowed request is neither
/// read nor retained; the zero second payload and receiver are also unused.
s32 neoArkPowerPlant1IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

/// Cancels active effects and blocks new ability requests for the generator-clear scene.
///
/// Called at the scene script's start with live room-effect and attachment state.
/// Cancellation is deferred to the effect update; the attachment event lock
/// remains set for the ordinary attachment controller to release.
void neoArkPowerPlant1PrepareGeneratorClearScene(void);

/// Stops scene vibration when the generator-clear event is skipped.
///
/// Requests vibration-script and motor-task teardown and clears port 0's pending
/// vibration requests. Requires a live game session; teardown can be deferred
/// while actor updates are frozen.
void neoArkPowerPlant1StopSkippedSceneVibration(void);

#endif // SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H
