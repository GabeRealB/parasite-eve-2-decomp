/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Collision response of the dropping first enemy: the contact table at
/// `contacts` is run through `func_800E0C10` with a 0x48-byte scratch. Response 1
/// adds the returned X and Z offsets to the root; only the first one also adds
/// Y, latches `dropCollided`, sets the fall speed to -0x64 and takes a quarter off
/// the step length. Response 2 puts the root back where the last step started.
/// The table is released either way.
void sucklercephDropCollide(Task* arg0)
{
    ActorContactDeltaWideScratch* scratch;
    SucklercephWork*              work;
    GfxCoord*                     coord;
    s32                           movement;

    work     = arg0->work;
    scratch  = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaWideScratch);
    coord    = arg0->extra.tmd->coords;
    movement = func_800E0C10(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->dropCollided == 0) {
                coord->coord.t[1]  += scratch->delta.fixed.vy.halves.integer;
                work->fallSpeed     = -0x64;
                work->forwardSpeed -= work->forwardSpeed / 4;
                work->dropCollided  = movement;
            }
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    Gp_ClearRec18Occupied(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaWideScratch);
}
