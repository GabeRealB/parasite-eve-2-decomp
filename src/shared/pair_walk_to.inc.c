/* Part of the pair walk library; see pair_walk.h. */

/// Message handler 0x7DD of `gPairWalkMessages`, the enemy's "walk to"
/// opcode: turns its root coordinate to face `target`, caching the yaw in
/// `PairWalkWork::st`, and leaves the horizontal distance to it, in
/// twelfths, in `travel` for the walk state to count down.
s32 pairWalkTo(Task* task, s32 arg1, VECTOR* target, s32 arg3)
{
    GfxCoord*     coord;
    PairWalkWork* work;
    s32           dx;
    s32           dz;
    u16           yaw;

    coord        = task->extra.tmd->coords;
    work         = task->work;
    dx           = target->vx - coord->coord.t[0];
    dz           = target->vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    gfxRotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
