/* Part of the pair walk library; see pair_walk.h. */

/// Message handler 0x7DD of `gPairWalkMessages`, the enemy's "walk to"
/// opcode: turns its root coordinate to face `target`, caching the yaw in
/// `Actor150400Work::yaw`, and leaves the horizontal distance to it, in
/// twelfths, in `travel` for the walk state to count down.
s32 pairWalkTo(Task* task, s32 arg1, VECTOR* target)
{
    GfxCoord*        coord;
    Actor150400Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    dx           = target->vx - coord->coord.t[0];
    dz           = target->vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    gfxRotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
