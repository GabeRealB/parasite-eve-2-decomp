/* Part of the reversing walker library; see reversing_walker.h. */

/// State handler at index 1 of `D_actor_350700_80161E30`, the move body that
/// mirrors the parent's `func_actor_350700_80163528`: rotates the constant local-space offset
/// `_gReverseWalkForward` through the root part's matrix into `work->walk.velocity`,
/// seeds `walk.lastDistance` with `ACTOR_WALK_DISTANCE_NONE` and advances
/// `walk.motionStep` so the dispatcher runs the next handler. Where the parent's step rotates its offset unchanged, this one
/// shrinks it to -0.4 of its length whenever `field_4C4` is clear.
void reverseWalkBeginMove(Task* arg0)
{
    Actor350500Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    coord = arg0->extra.tmd->coords;
    work  = (Actor350500Work*)arg0->work;

    vec = _gReverseWalkForward;
    if (work->field_4C4 == 0) {
        vec.vx = vec.vx * -0.4;
        vec.vy = vec.vy * -0.4;
        vec.vz = vec.vz * -0.4;
    }
    ApplyMatrixLV(&coord->coord, &vec, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}
