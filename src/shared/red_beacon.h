/* A pulsing red indicator light in the Acropolis elevator halls: a one-frame
 * room-effect task that draws an additive vertical diamond glowing red at its
 * coordinate's origin, its brightness a triangle wave of the frame counter and
 * its size shrinking with depth.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_RED_BEACON_H
#define SRC_SHARED_RED_BEACON_H

#include "types.h"

#include "main/task_types.h"

void redBeaconTask(Task* arg0);

#endif /* SRC_SHARED_RED_BEACON_H */
