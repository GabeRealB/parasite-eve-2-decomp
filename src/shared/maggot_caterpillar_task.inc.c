/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Actor task: runs the enemy's state handler from `gMaggotCaterpillarStates`.
void maggotCaterpillarTask(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = gMaggotCaterpillarStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
