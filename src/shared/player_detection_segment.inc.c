/* Part of the player detection library; see player_detection.h. */

/// Tests the segment from `arg0` to `arg1` against every occluder in
/// `D_80115550` with ENABLED set, stopping at the first node that reports a hit
/// (1); returns the last node's result, 0 when none was tested.
s32 detectSegmentHitsWall(SVECTOR* arg0, SVECTOR* arg1)
{
    VECTOR*                 vec;
    WorldCollisionOccluder* node;
    s32                     ret;

    ret     = 0;
    node    = D_80115550;
    vec     = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    vec->vx = arg1->vx - arg0->vx;
    vec->vy = arg1->vy - arg0->vy;
    vec->vz = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->flags & WORLD_COLLISION_OCCLUDER_ENABLED) {
            ret = worldCollisionTestOccluderSegment(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
    return ret;
}
