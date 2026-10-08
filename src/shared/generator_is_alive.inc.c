/* Part of the Generator library; see generator.h. */

/// Answers ACTOR_MESSAGE_IS_PRESENT with the generator body's alive latch.
///
/// Requires initialized GeneratorWork. Returns 1 before the killing hit and
/// 0 throughout the held death sequence; both payload words are ignored.
static s32 _generatorIsAlive(Task* task, s32 messageId, s32 unusedArg2, s32 unusedArg3)
{
    return ((GeneratorWork*)task->work)->alive;
}
