#include "common.h"

#include "actors/actor_403200.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Set the heights of collision grid quads `arg1` and `arg1 + 1`: 500 for each
/// quad's first two vertices, 800 for the other two. Nothing in the actor calls
/// it.
void func_actor_403200_80140FD4(s32 arg0, s16 arg1)
{
    SVECTOR* verts;

    verts                  = Gp_GridParams->field_8;
    verts[arg1 * 4].vy     = 500;
    verts[arg1 * 4 + 1].vy = 500;
    verts[arg1 * 4 + 2].vy = 800;
    verts[arg1 * 4 + 3].vy = 800;
    verts[arg1 * 4 + 4].vy = 500;
    verts[arg1 * 4 + 5].vy = 500;
    verts[arg1 * 4 + 6].vy = 800;
    verts[arg1 * 4 + 7].vy = 800;
}

/// Exit callback of the boss task: when its work block exists, send each of the
/// seven escorts to state 2, unlink every collision group but the third, and
/// detach the enemy's contact records, then tear the enemy down.
void func_actor_403200_80141018(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         enemy;
    s16              i;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
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
