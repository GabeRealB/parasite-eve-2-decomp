/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Script-event hook: events 8 and 10 each queue their stage sound; every
/// event returns 0.
s32 drivewayScriptSound(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52190008, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x5219000A, 0, 0);
            break;
    }
    return 0;
}
