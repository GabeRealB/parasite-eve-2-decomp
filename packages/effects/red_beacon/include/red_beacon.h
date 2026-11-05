/* A pulsing red indicator light in the Acropolis elevator halls: a one-frame
 * room-effect task that draws an additive vertical diamond glowing red at its
 * coordinate's origin, its brightness a triangle wave of the frame counter and
 * its size shrinking with depth.
 *
 * Include this header in the prologue and task.inc.c at its function's
 * position, with RED_BEACON_TASK defined to the room's callback name.
 */

#ifndef EFFECTS_RED_BEACON_RED_BEACON_H
#define EFFECTS_RED_BEACON_RED_BEACON_H

#include <core/pe2.h>

/// The red beacon's spawn argument, as the bytes of `Task::spawnArg1`; build it with `RED_BEACON_ARG`.
typedef struct {
    u8   pulseRate; // The light pulses dark to bright and back every 256 / pulseRate frames
    u8   size;      // Half extent before the depth divide, in units of 0x200
    byte unused[2];
} RedBeaconArg;

/// Packs a red beacon's pulse rate and size, each 0..255, into its spawn argument.
#define RED_BEACON_ARG(pulseRate, size) (((size) << 8) | (pulseRate))

#endif /* EFFECTS_RED_BEACON_RED_BEACON_H */
