/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Applies room-grid contact corrections to a falling Sucklerceph.
///
/// Requires live root/work storage and the four initialized body contacts.
/// The first grid hit applies signed 16.16 whole-unit XYZ corrections, rebounds
/// Y at -100 units per frame and reduces forward speed by one quarter with
/// signed truncation. Later hits correct only X/Z. Opposed grid normals restore
/// the translation saved before the fall step. Consumes the contacts and
/// releases the full `ActorContactDeltaWideScratch` reservation before return;
/// composition is refreshed by the caller.
static void _sucklercephDropCollide(Task* task)
{
    enum { SUCKLERCEPH_DROP_REBOUND_SPEED = -100 };

    ActorContactDeltaWideScratch* scratch;
    SucklercephWork*              work;
    GfxCoord*                     rootCoord;
    s32                           pushbackResult;

    work           = task->work;
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaWideScratch);
    rootCoord      = task->extra.tmd->coords;
    pushbackResult = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL);
    switch (pushbackResult) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            if (work->dropCollided == 0) {
                rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                work->fallSpeed        = SUCKLERCEPH_DROP_REBOUND_SPEED;
                work->forwardSpeed    -= work->forwardSpeed / 4;
                work->dropCollided     = pushbackResult;
            }
            rootCoord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevRootPos.vx;
            rootCoord->coord.t[1] = work->prevRootPos.vy;
            rootCoord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaWideScratch);
}
