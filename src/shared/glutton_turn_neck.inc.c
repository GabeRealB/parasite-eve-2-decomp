/* Part of the Glutton library; see glutton.h. */

/// Walk the yaw `field_7C8` toward `arg1` (clamped to +/-0x200) by at most 0x71
/// per call, turn model part 3 by it through `ActorContact_TurnJoint`, and
/// refresh part 3, the root of the fifth escort's model and part 4.
void gluttonTurnNeck(Task* task, s16 arg1)
{
    Actor403200Work* work = task->work;
    s16              value;

    value = arg1;
    if (arg1 > 0x200) {
        value = 0x200;
    }
    if (arg1 < -0x200) {
        value = -0x200;
    }

    if (work->field_7C8 < value) {
        if (value - work->field_7C8 >= 0x72) {
            work->field_7C8 = work->field_7C8 + 0x71;
        } else {
            work->field_7C8 = value;
        }
    } else if (value < work->field_7C8) {
        if (abs(work->field_7C8 - value) >= 0x72) {
            work->field_7C8 = work->field_7C8 - 0x71;
        } else {
            work->field_7C8 = value;
        }
    }

    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&task->extra.tmd->coords[3]);
    ActorContact_TurnJoint(&task->extra.tmd->coords[3], work->field_7C8);
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&task->extra.tmd->coords[3]);
    work->field_ECC[4]->task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&work->field_ECC[4]->task->extra.tmd->coords[0]);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&task->extra.tmd->coords[4]);
}
