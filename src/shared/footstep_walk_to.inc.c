/* Part of the footstep walk library; see footstep_walk.h. */

/// "Walk to" opcode: records `mode` in `gFootstepWalkMode`, turns the
/// model to face `target` (away from it in mode 1) caching the yaw in the work
/// block, and leaves in `travel` the planar distance divided by the walk's
/// frame count: 0x3C in mode 0, 0xF in mode 1 and 0x19 in mode 2.
s32 footstepWalkTo(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*              coord;
    FootstepWalkQuietWork* work; // The head either walker's block opens with
    s32                    dx;
    s32                    dz;
    s32                    steps;
    s32                    dist;
    s32                    angle;

    coord             = task->extra.tmd->coords;
    work              = task->work;
    gFootstepWalkMode = mode;
    dx                = target->vx - coord->coord.t[0];
    dz                = target->vz - coord->coord.t[2];
    angle             = ratan2(dx, dz);
    work->st.yaw      = angle;
    if (gFootstepWalkMode == 1) {
        work->st.yaw = angle + 0x800;
    }
    gfxRotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (gFootstepWalkMode) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}
