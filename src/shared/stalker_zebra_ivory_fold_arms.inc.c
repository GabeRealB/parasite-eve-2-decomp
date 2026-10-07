/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Requests both arm models to fold back by clearing their strike-extension flags.
static void _stalkerZebraIvoryFoldArms(Task* task)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)task->work;

    work->leftArmOut  = 0;
    work->rightArmOut = 0;
}
