/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Turns the root heading by a fixed step toward the tracked player.
///
/// Uses the XZ offset last stored by player tracking, in the root's parent frame.
/// Heading and turnStep use 4096 units per turn; a signed 12-bit error within
/// 256 units leaves the heading unchanged. Walk callers supply positive steps
/// 16..32. The step is not clamped to the error, and the stored halfword may wrap;
/// the rotation update normalizes it later. Requires live work and a tracked offset.
static void _madChaserTurnToPlayer(Task* task, s32 turnStep)
{
    enum {
        MAD_CHASER_HEADING_BITS   = 12,
        MAD_CHASER_TURN_DEAD_ZONE = ACTOR_TRANSFORM_ANGLE_TURN / 16,
    };
    MadChaserWork* work = task->work;
    SVECTOR        horizontalOffset;
    s32            yawError;
    u16            currentYaw;
    s32            targetYaw;

    horizontalOffset.vx = work->toPlayer.vx;
    horizontalOffset.vy = 0;
    horizontalOffset.vz = work->toPlayer.vz;
    VectorNormalSS(&horizontalOffset, &horizontalOffset);
    targetYaw  = ratan2(-horizontalOffset.vx, -horizontalOffset.vz);
    currentYaw = work->rotation.vy;
    // Wrap the error to a signed turn before choosing the rotation direction.
    yawError = ((currentYaw - targetYaw) << (32 - MAD_CHASER_HEADING_BITS)) >> (32 - MAD_CHASER_HEADING_BITS);
    if (yawError > MAD_CHASER_TURN_DEAD_ZONE) {
        work->rotation.vy = currentYaw - turnStep;
    } else if (yawError < -MAD_CHASER_TURN_DEAD_ZONE) {
        work->rotation.vy = currentYaw + turnStep;
    }
}
