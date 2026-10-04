/* Part of the scripted walk library; see scripted_walk.h. */

/// Message 0x7DD (approach): turns the model to face `target` -- away from it
/// in mode 1, where the update then walks it backwards -- keeps the mode in
/// `SCRIPTED_WALK_MODE`, and stores the number of steps the walk takes:
/// the planar distance over the mode's step length, 60 in mode 0, 15 in mode 1
/// and 25 otherwise.
s32 scriptedWalkTo(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*             coord;
    SCRIPTED_WALK_WORK_T* work;
    s32                   dx;
    s32                   dz;
    s32                   steps;
    s32                   dist;
    s32                   angle;

    coord              = task->extra.tmd->coords;
    work               = task->work;
    SCRIPTED_WALK_MODE = mode;
    dx                 = target->vx - coord->coord.t[0];
    dz                 = target->vz - coord->coord.t[2];
    angle              = ratan2(dx, dz);
    work->st.yaw       = angle;
    if (SCRIPTED_WALK_MODE == 1) {
        work->st.yaw = angle + 0x800;
    }
    gfxRotMatrixY(&coord->coord, work->st.yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (SCRIPTED_WALK_MODE) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}
