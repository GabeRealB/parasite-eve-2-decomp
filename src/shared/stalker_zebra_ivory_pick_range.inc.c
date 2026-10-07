/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Takes the capsule body out of the grid, then goes to state 0xD when the
/// player is more than 2000 away, otherwise to state 8.
void stalkerZebraIvoryPickRange(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    StalkerZebraIvoryWork* work2;
    StalkerZebraIvoryWork* work3;

    work = (StalkerZebraIvoryWork*)arg0->work;
    _stalkerZebraIvoryDisableCapsuleGrid(arg0);
    if (work->playerDistance > 2000) {
        work2           = (StalkerZebraIvoryWork*)arg0->work;
        work2->state    = 0xD;
        work2->subState = 0;
    } else {
        work3           = (StalkerZebraIvoryWork*)arg0->work;
        work3->state    = 8;
        work3->subState = 0;
    }
}
