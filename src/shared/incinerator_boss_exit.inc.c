/* Part of the incinerator boss library; see incinerator_boss.h. */

/// Exit callback of the boss task: when its work block exists, send each of the
/// seven escorts to state 2, unlink every collision group but the third, and
/// detach the enemy's contact records, then tear the enemy down.
void incinBossExit(Task* arg0)
{
    Actor403200Work* work;
    Enemy*           enemy;
    s16              i;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work != NULL) {
        for (i = 0; i < 7; i++) {
            if (work->field_ECC[i] != NULL) {
                work->field_ECC[i]->task->state = 2;
            }
        }
        Gp_UnlinkObj(&work->hits[0].obj);
        Gp_UnlinkObj(&work->hits[1].obj);
        Gp_UnlinkObj(&work->hits[3].obj);
        Gp_UnlinkObj(&work->hits[4].obj);
        Gp_UnlinkObj(&work->hits[5].obj);
        Gp_UnlinkObj(&work->hits[6].obj);
        Gp_UnlinkObj(&work->hits[7].obj);
        Gp_UnlinkObj(&work->hits[8].obj);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, arg0);
}
