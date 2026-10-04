/* Part of the Generator library; see generator.h. */

/// Message 2006 handler: returns the work block's `alive`, which the spawn
/// sets to 1 and generatorBodyHit clears when the killing hit lands.
s32 generatorIsAlive(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    return ((GeneratorWork*)arg0->work)->alive;
}
