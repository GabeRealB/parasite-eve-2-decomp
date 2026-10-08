/* Part of the Generator library; see generator.h. */

/// Latches script commands that release the body's death and battle-exit waits.
///
/// ACTOR_COMMAND_MESSAGE_APPLY borrows a readable ActorCommand for this call.
/// Commands 0..3 select none, death, battle exit or both; other values do
/// nothing. Existing bits remain set. The second payload is ignored; returns 0.
static s32 _generatorSetReleaseBits(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg3)
{
    GeneratorWork* work;

    work = task->work;
    switch (command->command) {
        case 0:
            break;
        case GENERATOR_RELEASE_DEATH:
            work->releaseBits |= GENERATOR_RELEASE_DEATH;
            break;
        case GENERATOR_RELEASE_BATTLE_EXIT:
            work->releaseBits |= GENERATOR_RELEASE_BATTLE_EXIT;
            break;
        case GENERATOR_RELEASE_DEATH | GENERATOR_RELEASE_BATTLE_EXIT:
            work->releaseBits |= GENERATOR_RELEASE_DEATH | GENERATOR_RELEASE_BATTLE_EXIT;
            break;
    }
    return 0;
}
