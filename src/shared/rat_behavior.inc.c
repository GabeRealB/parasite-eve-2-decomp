/* Part of the Rat library; see rat.h. */

/// Runs the handler of the rat's current behaviour state (`field_37A`).
void ratBehavior(Task* arg0)
{
    switch (((RatWork*)arg0->work)->field_37A) {
        case 0:
            ratIdle(arg0);
            break;
        case 1:
            ratAttack(arg0);
            break;
        case 2:
            ratStagger(arg0);
            break;
        case 3:
            ratBuildup(arg0);
            break;
        case 4:
            ratHurt(arg0);
            break;
        case 5:
            break;
    }
}
