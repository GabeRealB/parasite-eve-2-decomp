#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_80163354.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_13", func_actor_342400_80168B74);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_13", func_actor_342400_80168F14);

/// Seven state handlers, indexed by `Actor342400Work::field_420`; copied to
/// the stack before dispatch.
extern TaskFuncTable7 D_actor_342400_8016203C;

/// Same helper as in `actor_342400_6.c`: colours `enemy` from `coord`'s world
/// position through a 0x10-byte `VECTOR` taken off `G_SCRATCH_HEAD`.
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

/// Per-frame callback, the seven-state counterpart of
/// `func_actor_342400_80165FC0`: in mode 0 it also spawns effect 3 on the
/// model's second coord part every 32 frames.
void func_actor_342400_801690FC(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable7   sp    = D_actor_342400_8016203C;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
            }
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                ActorsShared80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
                ActorsShared80163354(arg0, 1, 7, 0x80, 0, 0xFF);
                ActorsShared80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}
