/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Resolves the node the route cursor names and writes its position to pos. On
/// arrival it sets route->arrived, clears the turn counters and advances the
/// cursor, wrapping at OVERLAY_WALKER_ROUTE_END.
void bossStrangerFollowRoute(BossStrangerWalker* work, SVECTOR3* pos)
{
    BossStrangerRoute* route;
    BossStrangerRoute* routeAdvance;
    BossStrangerRoute* routeWrap;
    BossStrangerRoute* routeNext;
    u8                 node;

    route      = work->route;
    work->node = route->nodeIndices[route->cursor];
    if (bossStrangerArrived(work) == 0) {
        pos->vx              = work->nav->nodes[work->node].x;
        pos->vy              = work->nav->nodes[work->node].y;
        pos->vz              = work->nav->nodes[work->node].z;
        work->route->arrived = 0;
        return;
    }

    // Reload the route for the cursor advance, the end-marker wrap and the
    // index the cursor then names. Each of those phases keeps its own pointer.
    work->route->arrived = 1;
    routeAdvance         = work->route;
    work->turnRun        = 0;
    work->turnBonus      = 0;
    routeAdvance->cursor++;

    routeWrap = work->route;
    if (routeWrap->nodeIndices[routeWrap->cursor] == OVERLAY_WALKER_ROUTE_END) {
        routeWrap->cursor = 0;
    }

    routeNext  = work->route;
    node       = routeNext->nodeIndices[routeNext->cursor];
    work->node = node;
    pos->vx    = work->nav->nodes[node].x;
    pos->vy    = work->nav->nodes[work->node].y;
    pos->vz    = work->nav->nodes[work->node].z;
}
