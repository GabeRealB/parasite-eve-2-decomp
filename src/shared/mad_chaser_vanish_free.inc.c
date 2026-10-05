/* Part of the Mad Chaser library; see mad_chaser.h. */

/// On frame 3 frees the model's buffers and sets model flag 4; after 0x24
/// frames destroys the enemy.
void madChaserVanishFree(Task* arg0)
{
    MadChaserWork* work;
    TmdObject*     model;
    u16            ticks;

    work              = (MadChaserWork*)arg0->work;
    model             = arg0->extra.tmd;
    ticks             = work->stateFrames + 1;
    work->stateFrames = ticks;
    if ((s16)ticks == 3) {
        tmdFreePrimitiveBuffer(model);
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    if ((s16)work->stateFrames >= 0x24) {
        enemyDestroy(arg0->spawnArg2.pointer, arg0);
    }
}
