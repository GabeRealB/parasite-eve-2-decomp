#ifndef ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
#define ROOMS_SHELTER_B1_STERILIZATION_ROOM_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"
#include "rooms/room.h"

/// Panel titles drawn by the room's menu tasks.
extern const char D_shelter_b1_sterilization_room_8017D610[];

/// The "100.0%" string drawn for a full-share usage row.
extern const u8 D_shelter_b1_sterilization_room_8017D61C[];

/// State handlers of the tasks run by `func_shelter_b1_sterilization_room_80180518`
/// and `func_shelter_b1_sterilization_room_801811E0`.
extern const TaskFuncTable3 D_shelter_b1_sterilization_room_8017D6A4;

/// The "%" suffix appended to a formatted percentage.
extern u8 D_shelter_b1_sterilization_room_80184594[];

/// UI descriptor of the help-line box the "Play Data" panels open beside their
/// lists.
extern UiObjectDesc D_shelter_b1_sterilization_room_801847AC;

/// Task descriptor table used by the cutscene runner
/// `func_shelter_b1_sterilization_room_8017F550`, which spawns entry 1 and
/// waits on it while the cutscene plays. The room's event handler spawns
/// entry 0 with a `RoomCutsceneRec` as its argument.
extern TaskDesc D_shelter_b1_sterilization_room_80184E1C;

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_shelter_b1_sterilization_room_80188504[];

/// One bit per entry of `D_shelter_b1_sterilization_room_80188504` already
/// spawned, so each is spawned only once until the mask is cleared.
extern s32 D_shelter_b1_sterilization_room_8018C340;

/// Draws the room's two backdrop halves as opaque sprites tinted by `shade`.
void func_shelter_b1_sterilization_room_80180A2C(s32 shade);

/// Draws the room's two backdrop halves as semi-transparent sprites tinted by
/// `shade`.
void func_shelter_b1_sterilization_room_80180BF0(s32 shade);

#endif // ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
