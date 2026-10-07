/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Marks patrol arrival and selects the next node, wrapping at the route end marker.
///
/// Requires the walker's live, nonempty patrol route; resets the turn counters
/// and returns the new navigation index after storing it in `walker->node`.
static inline u8 _bossStrangerAdvancePatrolNode(BossStrangerWalker* walker)
{
    BossStrangerRoute* routeAdvance;
    BossStrangerRoute* routeWrap;
    BossStrangerRoute* routeNext;
    u8                 node;

    // Reload the route for the cursor advance, the end-marker wrap and the
    // index the cursor then names. Each of those phases keeps its own pointer.
    walker->route->arrived = 1;
    routeAdvance           = walker->route;
    walker->turnRun        = 0;
    walker->turnBonus      = 0;
    routeAdvance->cursor++;

    routeWrap = walker->route;
    if (routeWrap->nodeIndices[routeWrap->cursor] == OVERLAY_WALKER_ROUTE_END) {
        routeWrap->cursor = 0;
    }

    routeNext    = walker->route;
    node         = routeNext->nodeIndices[routeNext->cursor];
    walker->node = node;
    return node;
}

/// Selects the patrol route's current node and writes the next steering goal.
///
/// On arrival, raises `route->arrived`, clears the turn counters and advances
/// the route cursor, wrapping at `OVERLAY_WALKER_ROUTE_END`; the output then
/// names the new node. Otherwise clears `arrived` and writes the current node.
/// Requires a nonempty, end-marked route with a valid cursor and node indices
/// into `nav->nodes`, a live coordinate, and initialized scratch storage for
/// the arrival test. `goal` supplies writable packed halfword XYZ storage
/// disjoint from the walker and navigation records. No pointer is retained.
static void _bossStrangerFollowRoute(BossStrangerWalker* walker, SVECTOR3* goal)
{
    BossStrangerRoute* route;
    u8                 node;

    route        = walker->route;
    walker->node = route->nodeIndices[route->cursor];
    if (_bossStrangerArrived(walker) == 0) {
        goal->vx               = walker->nav->nodes[walker->node].x;
        goal->vy               = walker->nav->nodes[walker->node].y;
        goal->vz               = walker->nav->nodes[walker->node].z;
        walker->route->arrived = 0;
        return;
    }

    node     = _bossStrangerAdvancePatrolNode(walker);
    goal->vx = walker->nav->nodes[node].x;
    goal->vy = walker->nav->nodes[walker->node].y;
    goal->vz = walker->nav->nodes[walker->node].z;
}
