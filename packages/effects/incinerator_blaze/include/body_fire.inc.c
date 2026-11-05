#include <engine/random.h>

#include <gameplay/room_effects.h>

/* Part of the incinerator blaze library; see incinerator_blaze.h. */

/// Spawns a blast of fire `fireSize` big on the player's model part `_gBlazePlayerFireParts[partIndex]`, through
/// `_gBlazePlayerFireArg`; the package defines both.
#define BLAZE_SPAWN_PLAYER_FIRE(playerTask, partIndex, fireSize)                                                 \
    do {                                                                                                         \
        _gBlazePlayerFireArg.spawnArgLo = (fireSize);                                                            \
        _gBlazePlayerFireArg.coord      = &(playerTask)->extra.tmd->coords[_gBlazePlayerFireParts[(partIndex)]]; \
        effectSpawnHit(EFFECT_HIT_KIND_BLAST, (playerTask)->extra.tmd->coords, NULL, &_gBlazePlayerFireArg);     \
    } while (0)

/// Keeps setting random parts of the player's model on fire: small fires on the first four parts every 16 frames,
/// or with a nonzero `spawnArg1`, large ones on all sixteen every 8; the scene ends it.
static void _blazeBodyFireTask(Task* task)
{
    enum {
        BLAZE_BODY_FIRE_INITIAL            = 0,
        BLAZE_BODY_FIRE_EMIT               = 1,
        BLAZE_BODY_FIRE_PRIMARY_PART_COUNT = 4,
        BLAZE_BODY_FIRE_LARGE_SIZE         = 0x100,
        BLAZE_BODY_FIRE_SMALL_SIZE         = 0x10,
        BLAZE_BODY_FIRE_SMALL_FRAME_PERIOD = 16,
        BLAZE_BODY_FIRE_LARGE_FRAME_PERIOD = 8,
    };
    Task* playerTask;
    s32   partIndex;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    RANDOM_LCG_ADVANCE();
    partIndex = RANDOM_LCG_DRAW();

    switch (task->state) {
        case BLAZE_BODY_FIRE_INITIAL:
            partIndex &= BLAZE_BODY_FIRE_PRIMARY_PART_COUNT - 1;
            BLAZE_SPAWN_PLAYER_FIRE(playerTask, partIndex, BLAZE_BODY_FIRE_LARGE_SIZE);
            task->state++;
            return;
        case BLAZE_BODY_FIRE_EMIT:
            if (task->spawnArg1.value == 0) {
                if (gDisplayState.animFrame & (BLAZE_BODY_FIRE_SMALL_FRAME_PERIOD - 1)) {
                    return;
                }
                partIndex &= BLAZE_BODY_FIRE_PRIMARY_PART_COUNT - 1;
                BLAZE_SPAWN_PLAYER_FIRE(playerTask, partIndex, BLAZE_BODY_FIRE_SMALL_SIZE);
                return;
            }
            if (gDisplayState.animFrame & (BLAZE_BODY_FIRE_LARGE_FRAME_PERIOD - 1)) {
                return;
            }
            partIndex &= ARRAY_SIZE(_gBlazePlayerFireParts) - 1;
            BLAZE_SPAWN_PLAYER_FIRE(playerTask, partIndex, BLAZE_BODY_FIRE_LARGE_SIZE);
            return;
    }
}

#undef BLAZE_SPAWN_PLAYER_FIRE
