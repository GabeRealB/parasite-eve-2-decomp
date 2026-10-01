/* Part of the Generator library; see generator.h. */

/// Regeneration step: while the part object is alive (`field_336` clear)
/// and the enemy's hit points are below the ceiling `field_33C`, one point
/// comes back every five frames (`field_33E` counts them down), reported
/// through the lock-on node as a damage of -1.
void generatorRegenerate(Task* arg0)
{
    GeneratorWork* work;
    Enemy*         enemy;
    s16            timer;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if ((work->field_336 == 0) && (enemy->hp < work->field_33C)) {
        timer           = work->field_33E - 1;
        work->field_33E = timer;
        if ((timer << 0x10) <= 0) {
            enemy->hp = enemy->hp + 1;
            func_800DA6E8(&enemy->node, -1, 0);
            work->field_33E = 5;
        }
    }
}
