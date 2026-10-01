/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Runs the handler of the work's current behaviour state (`field_39A`, 0-8);
/// state 9, entered when the hit points run out, runs nothing.
void maggotCaterpillarRunBehaviour(Task* arg0)
{
    switch (((MaggotCaterpillarWork*)arg0->work)->field_39A) {
        case 0:
            maggotCaterpillarWaitState(arg0);
            break;
        case 1:
            maggotCaterpillarAimState(arg0);
            break;
        case 2:
            maggotCaterpillarAmbushState(arg0);
            break;
        case 3:
            maggotCaterpillarRoamState(arg0);
            break;
        case 4:
            maggotCaterpillarSprayState(arg0);
            break;
        case 5:
            maggotCaterpillarPounceState(arg0);
            break;
        case 6:
            maggotCaterpillarHurtState(arg0);
            break;
        case 7:
            maggotCaterpillarStunState(arg0);
            break;
        case 8:
            maggotCaterpillarEntranceState(arg0);
            break;
        case 9:
            break;
    }
}
