/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Runs the current behavior after this frame's contact and status reactions.
///
/// Requires initialized enemy work and model state. Handlers update movement,
/// turn and animation requests for the rest of the frame. DEAD and values
/// outside the ten defined behaviors run no handler; task death is dispatched
/// separately through `Task::state`.
static void _maggotCaterpillarRunBehaviour(Task* actor)
{
    MaggotCaterpillarWork* work;

    work = actor->work;
    switch (work->behaviour) {
        case MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT:
            _maggotCaterpillarWaitState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_AIM:
            _maggotCaterpillarAimState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH:
            _maggotCaterpillarAmbushState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM:
            _maggotCaterpillarRoamState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY:
            _maggotCaterpillarSprayState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE:
            _maggotCaterpillarPounceState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_HURT:
            _maggotCaterpillarHurtState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_STUN:
            _maggotCaterpillarStunState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE:
            _maggotCaterpillarEntranceState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD:
            break;
    }
}
