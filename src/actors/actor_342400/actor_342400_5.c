#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_80165cc0.h"
#include "actors/actors_shared_80163354.h"

/// Colours `enemy` from `coord`'s world position through a 0x10-byte `VECTOR`
/// taken off `G_SCRATCH_HEAD`; the same helper as `actor_342400_7.c`.
static __inline__ void update_color(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Per-frame callback shaped like `func_actor_342400_80165FC0`, with a
/// one-entry handler table. `D_801153F4` 2 hides the model; 0 runs the state
/// handler and the follow-up steps, then moves the task to state 4 when
/// `field_448` requests it and the enemy is out of HP; 0 and 1 both colour
/// it, run `ActorsShared80163354` for three part pairs and unhide it. The work block is reloaded through its own local
/// for the state reset, as the original does.
void func_actor_342400_80164F3C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFunc         sp[1] = { func_actor_342400_8016A494 };

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp[(s16)work->field_420](arg0);
            func_actor_342400_801653DC(arg0, 1);
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->field_40 <= 0) {
                Actor342400Work* w = (Actor342400Work*)arg0->idMap;

                arg0->state  = work->field_448;
                w->field_420 = 0;
                w->field_422 = 0;
            }
            ActorsShared80165cc0(arg0);
            if (work->field_432 == 1) {
                func_actor_342400_80169654(arg0, 6, &work->field_80);
            }
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            ActorsShared80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
            ActorsShared80163354(arg0, 1, 7, 0x80, 0, 0xFF);
            ActorsShared80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->field_C &= ~0x80;
            return;
    }
}
