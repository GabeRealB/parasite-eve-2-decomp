#include <engine/random.h>

/* Part of the fireball library; see fireball.h. */

/// While room effects run, spawns a room mote drifting outward in a random direction one call in four;
/// `spawnArgBits` are added to its packed spawn argument. Unused.
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
        RANDOM_LCG_ADVANCE();
        if ((RANDOM_LCG_DRAW() & FIREBALL_EMBER_SPAWN_CHANCE_MASK) == 0) {
            RANDOM_LCG_ADVANCE();
            heading = RANDOM_LCG_DRAW() & FIREBALL_EMBER_HEADING_MASK;
            memset(&horizontalOffset, 0, sizeof(horizontalOffset));
            // Preserve logical shifts and halfword narrowing of the Q12 trig results.
            horizontalOffset.vx = (u32)(rcos(heading) * 5) >> 5;
            horizontalOffset.vz = (u32)(rsin(heading) * 5) >> 5;
            spawnOffset         = horizontalOffset;
            effectSpawn(gRoomEffectMoteId, coord, spawnArgBits | FIREBALL_EMBER_SPAWN_ARGUMENT, &spawnOffset);
        }
    }
}
