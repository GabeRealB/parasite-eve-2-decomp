/* Part of the Zebra and Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Rebuilds the root rotation from the Stalker's wrapped pitch, yaw and roll.
///
/// Uses the same scratch-stack and cache contract as
/// `_stalkerZebraIvoryApplyRotationInline`; preserves root translation.
static void _stalkerZebraIvoryApplyRotation(Task* task)
{
    STALKER_ZEBRA_IVORY_REBUILD_ROOT_ROTATION(task)
}

#undef STALKER_ZEBRA_IVORY_REBUILD_ROOT_ROTATION
