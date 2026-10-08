#include "main/random.h"

#include "gameplay/room_effects.h"

/* Part of the incinerator blaze library; see incinerator_blaze.h. */

/// Binds the shared fire argument to a selected player part and emits one blast.
///
/// Requires the carrier's sixteen-entry gBlazePlayerParts and reusable
/// gBlazeFireSpawn, plus a live player model containing the selected part.
/// playerTask is evaluated twice; partIndex and fireSize once. Arguments must
/// be stable, side-effect-free expressions. fireSize narrows to the signed low
/// half of the spawn word. The stored coordinate is borrowed by the effect;
/// the macro owns no storage and expands to one statement.
#define BLAZE_SPAWN_PLAYER_FIRE(playerTask, partIndex, fireSize)                                        \
    do {                                                                                                \
        gBlazeFireSpawn.spawnArgLo = (fireSize);                                                        \
        gBlazeFireSpawn.coord      = &(playerTask)->extra.tmd->coords[gBlazePlayerParts[(partIndex)]];  \
        effectSpawnHit(EFFECT_HIT_KIND_BLAST, (playerTask)->extra.tmd->coords, NULL, &gBlazeFireSpawn); \
    } while (0)

/// Spawns incinerator fire on randomly selected parts of the live player model.
///
/// The carrier supplies sixteen part indices and a reusable EffectSpawnArg;
/// the player TMD must contain every selected coordinate. Each tick advances the
/// LCG, including ticks that emit no fire. Entry emits on one of the first four
/// parts; spawnArg1 zero then emits small fire every sixteen display frames on
/// those parts, while nonzero emits large fire every eight frames on all sixteen.
/// The scene owns the task's lifetime; this callback does not end it.
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

    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    partIndex       = gRandomLcgState >> 16;

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
            partIndex &= ARRAY_SIZE(gBlazePlayerParts) - 1;
            BLAZE_SPAWN_PLAYER_FIRE(playerTask, partIndex, BLAZE_BODY_FIRE_LARGE_SIZE);
            return;
    }
}

#undef BLAZE_SPAWN_PLAYER_FIRE
