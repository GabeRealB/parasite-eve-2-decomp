/* Part of the Generator library; see generator.h. */

/// Message handler in the main task's message table: ORs the bit the payload's
/// selector names into the work block's `field_33A` (1, 2 or both for
/// selector 3; 0 is a no-op). Bit 1 releases the death handler from its wait,
/// bit 2 lets it run its `Gp_ReleaseStateF0Add` call.
s32 generatorSetReleaseBits(Task* task, s32 msgId, ActorCommand* msg)
{
    GeneratorWork* work;

    work = (GeneratorWork*)task->work;
    switch (msg->command) {
        case 0:
            break;
        case 1:
            work->field_33A |= 1;
            break;
        case 2:
            work->field_33A |= 2;
            break;
        case 3:
            work->field_33A |= 3;
            break;
    }
    return 0;
}
