/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Requests clip `arg1` at step `arg2`, restarting it (`animRequest` 2).
void stalkerZebraIvoryPlayClip(Task* arg0, s16 arg1, s16 arg2)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    work->animStep    = arg2;
    work->animClip    = arg1;
    work->animRequest = 2;
}
