/* Part of the Glutton library; see glutton.h. */

/// Exit callback of the boss task: when its work block exists, send each of the
/// seven escorts to state 2, unlink every collision group but the third, and
/// detach the enemy's contact records, then tear the enemy down.
void gluttonExit(Task* arg0)
{
    GluttonWork* work;
    Enemy*       enemy;
    s16          i;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work != NULL) {
        for (i = 0; i < ARRAY_SIZE(work->escorts); i++) {
            if (work->escorts[i] != NULL) {
                work->escorts[i]->task->state = 2;
            }
        }
        worldCollisionUnlinkBody(&work->hits[0].body);
        worldCollisionUnlinkBody(&work->hits[1].body);
        worldCollisionUnlinkBody(&work->hits[3].body);
        worldCollisionUnlinkBody(&work->hits[4].body);
        worldCollisionUnlinkBody(&work->hits[5].body);
        worldCollisionUnlinkBody(&work->hits[6].body);
        worldCollisionUnlinkBody(&work->hits[7].body);
        worldCollisionUnlinkBody(&work->hits[8].body);
        enemy->recs = 0;
    }
    enemyDestroy(enemy, arg0);
}
