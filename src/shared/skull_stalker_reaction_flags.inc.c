/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Consumes pending enemy reactions and enters the buildup hold when requested.
///
/// Stagger and damage-over-time requests are discarded; buildup selects the
/// status hold and clears its phase counter. Reads the byte again between
/// stages so each clear preserves the other pending bits.
static void _skullStalkerConsumeReactions(Task* task)
{
    Enemy*            enemy;
    SkullStalkerWork* work;
    u8                pendingReactions;

    enemy            = task->spawnArg2.pointer;
    work             = task->work;
    pendingReactions = enemy->reactionFlags;
    if (pendingReactions != 0) {
        if (pendingReactions & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = pendingReactions & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags = enemy->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->state          = SKULL_STALKER_STATE_STATUS_HOLD;
            work->phaseFrames    = 0;
        }
        pendingReactions = enemy->reactionFlags;
        if (pendingReactions & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            enemy->reactionFlags = pendingReactions & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}
