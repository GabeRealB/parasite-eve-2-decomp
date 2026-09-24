#include "common.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "actors/actor_341700.h"

/// Colours `enemy` from `coord`'s world position through a 0x10-byte
/// `VECTOR` taken off `G_SCRATCH_HEAD`. Inlined so each scratch-head access
/// keeps its own `lui` instead of sharing a CSE'd register.
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

/// Per-frame callback shaped like `func_actor_341700_80164CDC`, with a
/// one-entry handler table. `D_801153F4` 2 hides the model; 0 runs the state
/// handler and the follow-up steps, then moves the task to state 4 when
/// `field_448` requests it and the enemy is out of HP; 0 and 1 both colour
/// it, run `func_actor_341700_80162070` for three part pairs and unhide it. The work block is reloaded through its own local
/// for the state reset, as the original does.
void func_actor_341700_80163C58(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFunc         sp[1] = { func_actor_341700_801691B0 };

    switch (D_801153F4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp[(s16)work->field_420](arg0);
            func_actor_341700_801640F8(arg0, 1);
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                Actor341700Work* w = (Actor341700Work*)arg0->work;

                arg0->state  = work->field_448;
                w->field_420 = 0;
                w->field_422 = 0;
            }
            func_actor_341700_801649DC(arg0);
            if (work->field_432 == 1) {
                func_actor_341700_80168370(arg0, 6, (SVECTOR3*)&work->field_80);
            }
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            func_actor_341700_80162070(arg0, 2, 6, 0xC8, 0, 0xFF);
            func_actor_341700_80162070(arg0, 1, 7, 0x80, 0, 0xFF);
            func_actor_341700_80162070(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~0x80;
            return;
    }
}
