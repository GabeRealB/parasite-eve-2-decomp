/* Part of the Glutton library; see glutton.h. */

/// Moves the neck toward a target yaw and refreshes its dependent coordinates.
///
/// `yawTarget` uses 4096 angle units per turn, clamped to +/-0x200. The stored
/// yaw closes by at most 0x71 per call and turns host part 3 in world space.
/// Requires live host parts 3 and 4 and escort 4's root. Their compositions are
/// refreshed after the turn so the pitched neck and attached limb follow it.
static void _gluttonTurnNeck(Task* task, s16 yawTarget)
{
    enum { GLUTTON_NECK_YAW_LIMIT = 0x200,
           GLUTTON_NECK_YAW_STEP  = 0x71 };
    GluttonWork* work = task->work;
    s16          clampedYaw;

    clampedYaw = yawTarget;
    if (yawTarget > GLUTTON_NECK_YAW_LIMIT) {
        clampedYaw = GLUTTON_NECK_YAW_LIMIT;
    }
    if (yawTarget < -GLUTTON_NECK_YAW_LIMIT) {
        clampedYaw = -GLUTTON_NECK_YAW_LIMIT;
    }

    if (work->neckYaw < clampedYaw) {
        if (clampedYaw - work->neckYaw >= GLUTTON_NECK_YAW_STEP + 1) {
            work->neckYaw = work->neckYaw + GLUTTON_NECK_YAW_STEP;
        } else {
            work->neckYaw = clampedYaw;
        }
    } else if (clampedYaw < work->neckYaw) {
        if (abs(work->neckYaw - clampedYaw) >= GLUTTON_NECK_YAW_STEP + 1) {
            work->neckYaw = work->neckYaw - GLUTTON_NECK_YAW_STEP;
        } else {
            work->neckYaw = clampedYaw;
        }
    }

    // Compose the animated joint before applying yaw in world space.
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
    _actorRenderYawJointInWorld(&task->extra.tmd->coords[3], work->neckYaw);
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
    work->escorts[4]->task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->escorts[4]->task->extra.tmd->coords[0]);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[4]);
}
