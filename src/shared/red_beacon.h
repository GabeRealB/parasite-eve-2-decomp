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

#include "common.h"

#include "main/task_types.h"

/// The red beacon's spawn argument, as the bytes of `Task::spawnArg1`.
///
/// A spawner packs it with `RED_BEACON_ARG`. `pulseRate` multiplies the
/// display's frame counter before the product is folded into the 0..0x80
/// triangle that sets the red level, so the light goes dark, bright and dark
/// again every 256 / `pulseRate` frames. `size` is the diamond's half extent
/// before the depth divide: on screen it is `size * 0x200 / otz` pixels, so
/// the light shrinks with distance.
typedef struct {
    u8   pulseRate; // Frame-counter multiplier of the brightness pulse
    u8   size;      // Half extent before the depth divide, in units of 0x200
    byte unused[2]; // High half of the argument word; zero from every spawner and never read
} RedBeaconArg;
STATIC_ASSERT_SIZEOF(RedBeaconArg, 0x4);

/// Packs a `RedBeaconArg` into the argument word a spawner passes.
#define RED_BEACON_ARG(pulseRate, size) (((size) << 8) | (pulseRate))

void redBeaconTask(Task* arg0);

#endif /* SRC_SHARED_RED_BEACON_H */
