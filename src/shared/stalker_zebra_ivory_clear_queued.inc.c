/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Clears `leftArmOut` and `rightArmOut`, so both arm models fold away.
void stalkerZebraIvoryClearQueued(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    work->leftArmOut  = 0;
    work->rightArmOut = 0;
}
