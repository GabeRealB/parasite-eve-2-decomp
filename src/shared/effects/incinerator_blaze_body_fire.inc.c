#include "main/random.h"

#include "gameplay/room_effects.h"

/* Part of the incinerator blaze library; see incinerator_blaze.h. */

/// Spawns a body-fire blast attached to a selected player-model coordinate.
///
/// `playerTask` is a live TMD task, evaluated twice; `partIndex` is 0..15 and
/// `fireSize` is a spawn magnitude, each evaluated once. All must be stable
/// and independent of the writable `_gBlazePlayerFireArg`. The carrier supplies
/// that record and `_gBlazePlayerFireParts[16]`; the model must cover its indices
/// (up to 18). The size narrows to s16 and the record's high half stays intact.
/// The blast uses the record's selected part even though the call passes the
/// root. Spawning snapshots the record but borrows the part for effect lifetime.
/// No storage is allocated by the macro itself. Expands to one statement.
#define BLAZE_SPAWN_PLAYER_FIRE(playerTask, partIndex, fireSize)                                                 \
    do {                                                                                                         \
        _gBlazePlayerFireArg.spawnArgLo = (fireSize);                                                            \
        _gBlazePlayerFireArg.coord      = &(playerTask)->extra.tmd->coords[_gBlazePlayerFireParts[(partIndex)]]; \
        effectSpawnHit(EFFECT_HIT_KIND_BLAST, (playerTask)->extra.tmd->coords, NULL, &_gBlazePlayerFireArg);     \
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
