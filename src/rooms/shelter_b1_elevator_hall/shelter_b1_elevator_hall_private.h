#ifndef SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room_common.h"

extern TaskDesc D_shelter_b1_elevator_hall_80182CAC;

extern TaskMessageEntry D_shelter_b1_elevator_hall_80182CB8[6];

extern TaskDesc D_shelter_b1_elevator_hall_80182CE8;

extern RoomFadeStorage D_shelter_b1_elevator_hall_801849F0;

// Callbacks referenced by the overlay's shared data tables.

s32 func_shelter_b1_elevator_hall_8017D810(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_shelter_b1_elevator_hall_8017D99C(Task*);

s32 func_shelter_b1_elevator_hall_8017DB54(Task*, s32, s32, s32);

/// Ignores the hall's room commands and returns zero.
///
/// Handles `ROOM_MESSAGE_COMMAND`; all arguments are unused.
s32 shelterB1ElevatorHallIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandMode);

/// Ignores the hall's direction actions and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`; the borrowed request is neither
/// read nor retained, and the other arguments are unused.
s32 shelterB1ElevatorHallIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 secondArg);

/// Queues the hall's confirmation or elevator-ride sound for a room cue.
///
/// Handles `ROOM_MESSAGE_SOUND`: cue 6 plays the system confirmation sound,
/// cue 8 plays the hall's elevator-ride script, and other cues do nothing.
/// Always returns zero; the other arguments are unused. Requires the current
/// stage's sound scripts and the sound event queue.
s32 shelterB1ElevatorHallPlaySoundCueMessage(Task* task, s32 messageId, s32 cueId, s32 secondArg);

#endif // SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H
