/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Consumes the pending bits of the second enemy's `reactionFlags`: bit 0x1 is
/// dropped on its own, bit 0x2 puts the work into the status hold with its
/// frame count cleared, and bits 0xC are dropped last, after re-reading the
/// byte.
void skullStalkerReactionFlags(Task* arg0)
{
    Enemy*            enemy;
    SkullStalkerWork* work;
    u8                flags;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = arg0->work;
    flags = enemy->reactionFlags;
    if (flags != 0) {
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags = enemy->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->state          = SKULL_STALKER_STATE_STATUS_HOLD;
            work->phaseFrames    = 0;
        }
        flags = enemy->reactionFlags;
        if (flags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            enemy->reactionFlags = flags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}
