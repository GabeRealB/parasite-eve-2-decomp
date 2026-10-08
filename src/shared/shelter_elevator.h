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

void shelterElevatorTask(Task* task);

#endif /* SRC_SHARED_SHELTER_ELEVATOR_H */
