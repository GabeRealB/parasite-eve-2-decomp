/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Projectile task: runs the puff's state handler from `gMaggotCaterpillarPuffStates`.
void maggotCaterpillarPuffTask(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = gMaggotCaterpillarPuffStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
