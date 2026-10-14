/* The Shelter elevator halls' floor-selection task. It waits for the
 * elevator's caption menu, maps the chosen key to the destination floor's area
 * and warp, lets the lift's sound play out, then resolves the destination room
 * through the Shelter map and changes room.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_SHELTER_ELEVATOR_H
#define SRC_SHARED_SHELTER_ELEVATOR_H

#include "types.h"

#include "gameplay/companion_load.h"

/// Waits for floor selection and the lift sound, then reloads the selected hall.
///
/// Requires the hall's CAP menu and live player/session state. spawnArg1 is
/// the resolved sound-script ID to wait for; spawnArg2 is unused. States 0..4
/// hold player control, wait for the menu, select B1/B2/B3 from variant keys
/// 11/12/13 and wait for the voice. Any other key resumes control and kills
/// the task. Success resolves the chosen area/warp from room 1 through the
/// loaded Shelter map, commits warp/room, requests reload and kills the task.
void shelterElevatorTask(Task* task);

#endif /* SRC_SHARED_SHELTER_ELEVATOR_H */
