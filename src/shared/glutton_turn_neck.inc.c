/* Part of the Glutton library; see glutton.h. */

/// Walk the yaw `neckYaw` toward `arg1` (clamped to +/-0x200) by at most 0x71
/// per call, turn model part 3 by it through `ActorContact_TurnJoint`, and
/// refresh part 3, the root of the fifth escort's model and part 4.
void gluttonTurnNeck(Task* task, s16 arg1)
{
    GluttonWork* work = task->work;
    s16          value;

    value = arg1;
    if (arg1 > 0x200) {
        value = 0x200;
    }
    if (arg1 < -0x200) {
        value = -0x200;
    }

    if (work->neckYaw < value) {
        if (value - work->neckYaw >= 0x72) {
            work->neckYaw = work->neckYaw + 0x71;
        } else {
            work->neckYaw = value;
        }
    } else if (value < work->neckYaw) {
        if (abs(work->neckYaw - value) >= 0x72) {
            work->neckYaw = work->neckYaw - 0x71;
        } else {
            work->neckYaw = value;
        }
    }

    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
    ActorContact_TurnJoint(&task->extra.tmd->coords[3], work->neckYaw);
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
    work->escorts[4]->task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->escorts[4]->task->extra.tmd->coords[0]);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[4]);
}
