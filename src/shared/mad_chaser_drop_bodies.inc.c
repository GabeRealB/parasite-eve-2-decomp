/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Detaches the enemy's records and unlinks its three hit bodies, clears the
/// frame counter and advances the state.
void madChaserDropBodies(Task* arg0)
{
    Actor341700Work* work2;
    Actor341700Work* work;

    work                                    = (Actor341700Work*)arg0->work;
    ((Enemy*)arg0->spawnArg2.pointer)->recs = 0;
    work2                                   = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}
