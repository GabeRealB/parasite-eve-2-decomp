/* Part of the Generator library; see generator.h. */

/// Regeneration step: while the part object is alive (`lifeSupportDestroyed` clear)
/// and the enemy's hit points are below the ceiling `hpCeiling`, one point
/// comes back every five frames (`regenTimer` counts them down), reported
/// through the lock-on node as a damage of -1.
void generatorRegenerate(Task* arg0)
{
    GeneratorWork* work;
    Enemy*         enemy;
    s16            timer;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if ((work->lifeSupportDestroyed == 0) && (enemy->hp < work->hpCeiling)) {
        timer            = work->regenTimer - 1;
        work->regenTimer = timer;
        if ((timer << 0x10) <= 0) {
            enemy->hp = enemy->hp + 1;
            worldTargetAddReadoutAmount(&enemy->node, -1, 0);
            work->regenTimer = 5;
        }
    }
}
