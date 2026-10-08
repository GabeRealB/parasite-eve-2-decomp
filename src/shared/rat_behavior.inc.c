/* Part of the Rat library; see rat.h. */

/// Runs the rat's current living behavior mode.
///
/// Requires live `RatWork`. Dead and unrecognized modes do nothing; task-state
/// dispatch owns the death sequence.
static void _ratBehavior(Task* actor)
{
    RatWork* work;

    work = actor->work;
    switch (work->mode) {
        case RAT_MODE_IDLE:
            _ratIdle(actor);
            break;
        case RAT_MODE_ATTACK:
            _ratAttack(actor);
            break;
        case RAT_MODE_STAGGER:
            _ratStagger(actor);
            break;
        case RAT_MODE_BUILDUP:
            _ratBuildup(actor);
            break;
        case RAT_MODE_HURT:
            _ratHurt(actor);
            break;
        case RAT_MODE_DEAD:
            break;
    }
}
