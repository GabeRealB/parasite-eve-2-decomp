/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Steps root yaw toward a target's X/Z with a one-sixteenth-turn dead zone.
///
/// Target and root translation must share the same parent coordinate frame;
/// only target X/Z are read and no pointer is retained. The displacement
/// narrows to signed halfwords before normalization. `turnStep` is a positive
/// increment in 4096ths of a turn (callers use 24..36); it is not clamped to
/// the heading error. Wraps the error to [-2048, 2047], changes yaw only beyond
/// +/-256, and narrows stored yaw to s16. Marks the root dirty without rebuilding
/// its matrix; the caller applies the updated rotation.
static void _stalkerZebraIvoryTurnToward(Task* task, const SVECTOR* target, s32 turnStep)
{
    enum { STALKER_ZEBRA_IVORY_TURN_DEAD_ZONE = 256,
           STALKER_ZEBRA_IVORY_YAW_SIGN_SHIFT = 20 };
    StalkerZebraIvoryWork* work = task->work;
    GfxCoord*              rootCoord;
    SVECTOR                targetDirection;
    s32                    yawError;
    s32                    targetYaw;
    u16                    currentYaw;

    rootCoord               = task->extra.tmd->coords;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    targetDirection.vx      = target->vx - rootCoord->coord.t[0];
    targetDirection.vy      = 0;
    targetDirection.vz      = target->vz - rootCoord->coord.t[2];
    VectorNormalSS(&targetDirection, &targetDirection);
    targetYaw  = ratan2(targetDirection.vx, targetDirection.vz);
    currentYaw = work->yaw;
    // Sign-extend the 12-bit wrapped heading error before choosing turn direction.
    yawError = ((currentYaw - targetYaw) << STALKER_ZEBRA_IVORY_YAW_SIGN_SHIFT) >> STALKER_ZEBRA_IVORY_YAW_SIGN_SHIFT;
    if (yawError > STALKER_ZEBRA_IVORY_TURN_DEAD_ZONE) {
        work->yaw = currentYaw - turnStep;
    } else if (yawError < -STALKER_ZEBRA_IVORY_TURN_DEAD_ZONE) {
        work->yaw = currentYaw + turnStep;
    }
}
