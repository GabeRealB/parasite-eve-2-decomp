/* Part of the burster library; see burster.h. */

/// Collision response of the dropping first enemy: the contact table at
/// `rec154` is run through `func_800E0C10` with a 0x48-byte scratch. Response 1
/// adds the returned X and Z offsets to the root; only the first one also adds
/// Y, latches `field_2E0`, sets the fall speed to -0x64 and takes a quarter off
/// the step length. Response 2 puts the root back where the last step started.
/// The table is released either way.
void bursterDropCollide(Task* arg0)
{
    ActorDeltaFrame48* scratch;
    Actor104600Work*   work;
    GfxCoord*          coord;
    s32                movement;

    work     = (Actor104600Work*)arg0->work;
    scratch  = (ActorDeltaFrame48*)SCRATCH_PUSH_BYTES(0x48);
    coord    = arg0->extra.tmd->coords;
    movement = func_800E0C10(&work->rec154[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_2E0 == 0) {
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                work->field_2DE    = -0x64;
                work->field_2BE    = (u16)work->field_2BE - work->field_2BE / 4;
                work->field_2E0    = movement;
            }
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_274.vx;
            coord->coord.t[1] = work->field_274.vy;
            coord->coord.t[2] = work->field_274.vz;
            break;
    }
    Gp_ClearRec18Occupied(&work->rec154[0]);
    SCRATCH_STACK_RELEASE_BYTES(0x48);
}
