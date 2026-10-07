/* Part of the Mad Chaser waves library; see mad_chaser_waves.h. */

/// Pair spawner state 0: allocates its work, spawns two Mad Chasers (killing the
/// task if neither appears), numbers them, gives them texture page 3 / CLUT row
/// 5 and 1 HP, marks the slot live and advances.
void madChaserWavePairSpawn(Task* arg0)
{
    OverlayEncounterPairWork* work;
    Enemy*                    enemy;
    Task*                     task;
    TmdObject*                obj;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work   = work;
    work->enemy0 = enemySpawnFromTable(&D_actor_207000_80151E60, 1, 1, 0);
    work->enemy1 = enemySpawnFromTable(&D_actor_207000_80151E60, 1, 1, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
        taskKill(arg0);
        return;
    }
    if (work->enemy0 != NULL) {
        enemy           = work->enemy0;
        enemy->placeKey = gMadChaserWaveEnemyCount << 12;
        gMadChaserWaveEnemyCount++;
        task                   = enemy->task;
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 3;
        obj->clutRowOffset     = 5;
        enemy->hp              = 1;
    }
    if (work->enemy1 != NULL) {
        enemy           = work->enemy1;
        enemy->placeKey = gMadChaserWaveEnemyCount << 12;
        gMadChaserWaveEnemyCount++;
        task                   = enemy->task;
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 3;
        obj->clutRowOffset     = 5;
        enemy->hp              = 1;
    }
    gMadChaserWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
    arg0->state++;
}
