/* Part of the Glutton library; see glutton.h. */

void GLUTTON_SET_SHAKE_LEVEL(s8 level)
{
    GluttonWork* work = GLUTTON_HOST_TASK->work;

    work->shakeLevel = level;
}
