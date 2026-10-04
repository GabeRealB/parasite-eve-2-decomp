/* Part of the Rat library; see rat.h. */

/// Runs the handler of the rat's current behaviour mode (`RatWork::mode`).
void ratBehavior(Task* arg0)
{
    switch (((RatWork*)arg0->work)->mode) {
        case RAT_MODE_IDLE:
            ratIdle(arg0);
            break;
        case RAT_MODE_ATTACK:
            ratAttack(arg0);
            break;
        case RAT_MODE_STAGGER:
            ratStagger(arg0);
            break;
        case RAT_MODE_BUILDUP:
            ratBuildup(arg0);
            break;
        case RAT_MODE_HURT:
            ratHurt(arg0);
            break;
        case RAT_MODE_DEAD:
            break;
    }
}
