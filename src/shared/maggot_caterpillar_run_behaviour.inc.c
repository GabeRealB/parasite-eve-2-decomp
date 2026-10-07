/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Runs the handler of the work's current `behaviour`;
/// `MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD`, entered when the hit points run out, runs nothing.
void maggotCaterpillarRunBehaviour(Task* arg0)
{
    MaggotCaterpillarWork* work;

    work = arg0->work;
    switch (work->behaviour) {
        case MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT:
            maggotCaterpillarWaitState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_AIM:
            _maggotCaterpillarAimState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH:
            _maggotCaterpillarAmbushState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM:
            _maggotCaterpillarRoamState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY:
            maggotCaterpillarSprayState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE:
            _maggotCaterpillarPounceState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_HURT:
            _maggotCaterpillarHurtState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_STUN:
            _maggotCaterpillarStunState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE:
            maggotCaterpillarEntranceState(arg0);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD:
            break;
    }
}
