/* Part of the glow pod library; see glow_pod.h. */

/// Consumes the pending bits of the second enemy's `reactionFlags`: bit 0x1 is
/// dropped on its own, bit 0x2 puts the work into reaction state 3 with its
/// frame count cleared, and bits 0xC are dropped last, after re-reading the
/// byte.
void glowPodReactionFlags(Task* arg0)
{
    GpEnemy*     enemy;
    GlowPodWork* work;
    u8           flags;

    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    work  = arg0->work;
    flags = enemy->reactionFlags;
    if (flags != 0) {
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags = enemy->reactionFlags & 0xFD;
            work->field_286      = 3;
            work->field_28A      = 0;
        }
        flags = enemy->reactionFlags;
        if (flags & 0xC) {
            enemy->reactionFlags = flags & 0xF3;
        }
    }
}
