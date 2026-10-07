/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Tests whether the walker has reached its current navigation node on the XZ plane.
///
/// Returns 1 strictly inside either a 300-unit radius or the signed-halfword
/// radius obtained from `speedTarget` times four; equality is outside.
/// Offsets wrap to signed halfwords, and negative radii are squared as supplied.
/// Thus the 0xFFFE speed target supplies radius -8 and the 300-unit test decides.
/// Requires a live coordinate, a valid `node` in `nav->nodes`, and initialized
/// scratch storage for one `SVECTOR` plus the nested range test. Borrows the
/// walker read-only and restores the scratch cursor before returning.
static s16 _bossStrangerArrived(const BossStrangerWalker* walker)
{
    enum {
        BOSS_STRANGER_ARRIVAL_MIN_RADIUS   = 300, // Game-coordinate units
        BOSS_STRANGER_ARRIVAL_SPEED_FRAMES = 4    // Scales the target step into a radius
    };
    SVECTOR* toNode;

    toNode = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);

    // Stage the node's position, then subtract the walker's translation in
    // place. Only the low halfword of each coordinate takes part, and the
    // height is dropped because the test measures on the XZ plane.
    toNode->vx = walker->nav->nodes[walker->node].x;
    toNode->vy = walker->nav->nodes[walker->node].y;
    toNode->vz = walker->nav->nodes[walker->node].z;
    toNode->vx = toNode->vx - walker->coord->coord.t[0];
    toNode->vy = 0;
    toNode->vz = toNode->vz - walker->coord->coord.t[2];

    if (!actorOutsideRadius(toNode, walker->speedTarget * BOSS_STRANGER_ARRIVAL_SPEED_FRAMES) ||
        !actorOutsideRadius(toNode, BOSS_STRANGER_ARRIVAL_MIN_RADIUS)) {
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
        return 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return 0;
}
