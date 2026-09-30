/* Part of the patrol walker library; see patrol_walker.h. */

/// Resolves the node the route cursor names and writes its position to pos. On
/// arrival it sets route->arrived, clears the turn counters and advances the
/// cursor, wrapping at the 0xFF terminator.
void patrolFollowRoute(OverlayWalker* work, SVECTOR3* pos)
{
    OverlayWalkerRoute* route;
    OverlayWalkerRoute* step;
    OverlayWalkerRoute* wrap;
    OverlayWalkerRoute* next;
    u8                  node;

    route      = work->route;
    work->node = route->nodes[route->cursor];
    if (patrolArrived(work) == 0) {
        pos->vx              = work->nav->nodes[work->node].x;
        pos->vy              = work->nav->nodes[work->node].y;
        pos->vz              = work->nav->nodes[work->node].z;
        work->route->arrived = 0;
        return;
    }

    work->route->arrived = 1;
    step                 = work->route;
    work->field_62       = 0;
    work->field_64       = 0;
    step->cursor++;

    wrap = work->route;
    if (wrap->nodes[wrap->cursor] == 0xFF) {
        wrap->cursor = 0;
    }

    next       = work->route;
    node       = next->nodes[next->cursor];
    work->node = node;
    pos->vx    = work->nav->nodes[node].x;
    pos->vy    = work->nav->nodes[work->node].y;
    pos->vz    = work->nav->nodes[work->node].z;
}
