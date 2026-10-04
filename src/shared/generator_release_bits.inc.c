/* Part of the Generator library; see generator.h. */

/// Message handler in the main task's message table: ORs the bit the payload's
/// selector names into the work block's `releaseBits` (1, 2 or both for
/// selector 3; 0 is a no-op). Bit 1 releases the death handler from its wait,
/// bit 2 lets it run its `Gp_ReleaseStateF0Add` call.
s32 generatorSetReleaseBits(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    GeneratorWork* work;

    work = task->work;
    switch (msg->command) {
        case 0:
            break;
        case 1:
            work->releaseBits |= GENERATOR_RELEASE_DEATH;
            break;
        case 2:
            work->releaseBits |= GENERATOR_RELEASE_BATTLE_EXIT;
            break;
        case 3:
            work->releaseBits |= GENERATOR_RELEASE_DEATH | GENERATOR_RELEASE_BATTLE_EXIT;
            break;
    }
    return 0;
}
