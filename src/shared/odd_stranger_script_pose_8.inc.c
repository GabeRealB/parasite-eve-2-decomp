/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 15: restarts clip 8 with the head yaw cleared and the blend rate copied from field_8A4, and moves to state 7 at the clip boundary.
void oddStrangerScriptPose8(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = ODD_STRANGER_BODY_RADIUS;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 8;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        work->field_8A2               = work->field_8A4;
    }
    oddStrangerDrive(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 7;
    }
}
