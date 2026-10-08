/* Part of the Generator library; see generator.h. */

/// Restores one body HP every five active updates while Life Support survives.
///
/// Requires initialized body work and Enemy. Healing stops at hpCeiling and
/// reports -1 to the target readout. The signed halfword countdown is retained
/// while healing is disabled; its initial zero makes the first missing HP due.
static void _generatorRegenerate(Task* task)
{
    enum { GENERATOR_REGEN_INTERVAL_FRAMES = 5 };
    GeneratorWork* work;
    Enemy*         enemy;
    s16            regenFrames;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if ((work->lifeSupportDestroyed == 0) && (enemy->hp < work->hpCeiling)) {
        regenFrames      = work->regenTimer - 1;
        work->regenTimer = regenFrames;
        if (regenFrames <= 0) {
            enemy->hp = enemy->hp + 1;
            worldTargetAddReadoutAmount(&enemy->node, -1, 0);
            work->regenTimer = GENERATOR_REGEN_INTERVAL_FRAMES;
        }
    }
}
