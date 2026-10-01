/* Part of the Generator library; see generator.h. */

/// Message 2006 handler: returns the work block's field_338, which the spawn
/// sets to 1 and generatorBodyHit clears when the killing hit lands.
s16 generatorIsAlive(Task* arg0)
{
    return ((Actor05300Work*)arg0->work)->field_338;
}
