#include "main/random.h"

/* Part of the fireball library; see fireball.h. */

/// Occasionally spawns a drifting room mote while room effects are running.
///
/// Consumes one shared LCG draw per running call and a second on a one-in-four
/// spawn. spawnArgBits is ORed with half-extent 512, speed 16 and lifetime 32
/// in the room mote's packed argument; it may select palette or motion bits.
/// The spawn copies the signed-halfword horizontal offset synchronously and
/// borrows the live coordinate hierarchy. Requires an installed mote callback.
/// The standalone body is retained in each fireball image although unused.
static void _fireballSpawnEmber(GfxCoord* coord, s32 spawnArgBits)
{
    enum {
        FIREBALL_EMBER_SPAWN_CHANCE_MASK = 3,
        FIREBALL_EMBER_HEADING_MASK      = 0xF80,
        FIREBALL_EMBER_SPAWN_ARGUMENT    = 0x20100200
    };
    SVECTOR spawnOffset;
    SVECTOR horizontalOffset;
    s32     heading;

    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & FIREBALL_EMBER_SPAWN_CHANCE_MASK) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            heading         = (gRandomLcgState >> 16) & FIREBALL_EMBER_HEADING_MASK;
            memset(&horizontalOffset, 0, sizeof(horizontalOffset));
            // Preserve logical shifts and halfword narrowing of the Q12 trig results.
            horizontalOffset.vx = (u32)(rcos(heading) * 5) >> 5;
            horizontalOffset.vz = (u32)(rsin(heading) * 5) >> 5;
            spawnOffset         = horizontalOffset;
            effectSpawn(gRoomEffectMoteId, coord, spawnArgBits | FIREBALL_EMBER_SPAWN_ARGUMENT, &spawnOffset);
        }
    }
}
