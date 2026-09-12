#include "common.h"

#include "actors/actors_shared_801433b8.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

void ActorsShared801433b8(Task* arg0)
{
    ActorsShared801433b8Work* work;
    GpEnemy*                  enemy;
    s16                       i;

    work  = (ActorsShared801433b8Work*)arg0->idMap;
    enemy = arg0->spawnArg2;
    if (work != NULL) {
        for (i = 0; i < 7; i++) {
            if (work->parts[i] != NULL) {
                work->parts[i]->task->state = 2;
            }
        }
        Gp_UnlinkObj(&work->nodes[0].obj);
        Gp_UnlinkObj(&work->nodes[1].obj);
        Gp_UnlinkObj(&work->nodes[3].obj);
        Gp_UnlinkObj(&work->nodes[4].obj);
        Gp_UnlinkObj(&work->nodes[5].obj);
        Gp_UnlinkObj(&work->nodes[6].obj);
        Gp_UnlinkObj(&work->nodes[7].obj);
        Gp_UnlinkObj(&work->nodes[8].obj);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, arg0);
}
