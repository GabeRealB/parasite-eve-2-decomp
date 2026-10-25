#ifndef SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Bodyless transition-task recipe shared by this overlay's gate and event task.
///
/// Runs at priority `TASK_PRIORITY_SCENE` with zero spawn payloads and unused metadata. The gate
/// reads the recipe synchronously after latching the request and destination;
/// keep those snapshots and the room callback code live through session reload.
extern TaskDesc gRoomEventTaskDesc;

/// Parking-lot receiver table for transitions, key use, scavenger actions and CAP/sound cues.
///
/// The room task borrows six ordered records, including `TASK_MESSAGE_TABLE_END`.
/// Keep this table and its callbacks loaded while the room task receives messages.
extern const TaskMessageEntry gDryfieldNightParkingLotRoomMessages[6];

/// Stages the scavengers' entrance, then engages battle and releases player control.
///
/// Eleven commands, ending with `EVENT_SCRIPT_OPCODE_END`. Starts CAP command 7,
/// installs the player's entrance clip and selects scavenger wave 1. Two CAP
/// cues bracket animation playback and battle start. A zero-frame wait yields
/// before player control is released; a three-frame wait precedes termination.
/// The event task borrows the commands and animation payloads until it ends;
/// keep this room, gameplay and the required actors loaded throughout.
extern EvsCommand gDryfieldNightParkingLotScavengerEntranceScript[11];

// Callbacks referenced by the overlay's shared data tables.

/// Queues parking-lot bank entry 9 or 10 for the corresponding room sound cue.
///
/// Handles `ROOM_MESSAGE_SOUND` using the current stage's loaded bank. Other
/// cues do nothing; returns zero and ignores the other arguments.
s32 parkingLotSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

/// Refuses every key-item use without consuming the selected inventory item.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED` so the inventory menu displays its refusal notice.
s32 dryfieldNightParkingLotRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);

/// Plays parking-lot CAP command 4 for room command 4.
///
/// Handles `ROOM_MESSAGE_COMMAND` with queued display-transition playback.
/// Other commands are ignored; returns zero regardless of playback starting.
/// Requires the loaded CAP command table. Receiver, ID and second payload
/// are unused, and no payload is retained.
s32 dryfieldNightParkingLotCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedSecondArg);

/// Starts the scavengers' entrance and battle script once for room action 1.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` on variant 3 while the entrance
/// flag is clear. Latches the flag and holds player control before starting
/// the script. Other actions or visits do nothing. Borrows the four-byte
/// request until synchronous dispatch returns; no pointer is retained.
/// Returns zero and ignores the receiver, message ID and second payload word.
s32 dryfieldNightParkingLotStartScavengerEncounterMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Sets the scripted activation wave of the parking lot's scavengers.
///
/// The event-script word is narrowed to a signed byte (0 hold, 1 entrance,
/// 2 engage, 3 and above later waves). The encounter script passes 1 to begin
/// their entrance; this callback does not itself start combat or spawn actors.
void dryfieldNightParkingLotSetScavengerWave(s32 wave);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H
