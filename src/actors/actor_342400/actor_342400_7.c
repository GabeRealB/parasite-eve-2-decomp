#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/mem.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_80163354.h"
#include "actors/actors_shared_80166180.h"
#include "actors/actors_shared_801695a0.h"

/// Nine state handlers, indexed by `Actor342400Work::field_420`; copied to the
/// stack before dispatch. splat migrates this table into
/// `func_actor_342400_80165FC0`'s own `.s`, so there is no standalone rodata
/// file to `INCLUDE_RODATA`; it is defined here, as `D_actor_342400_80161E54`
/// is in `actor_342400.c`.
const TaskFuncTable9 D_actor_342400_80161F50 = { {
    func_actor_342400_8016A664,
    func_actor_342400_8016A724,
    func_actor_342400_8016A804,
    ActorsShared801695a0,
    func_actor_342400_8016A950,
    ActorsShared80166180,
    func_actor_342400_8016A9AC,
    func_actor_342400_8016A9C4,
    func_actor_342400_8016AA08,
} };

/// Colours `enemy` from `coord`'s world position through a 0x10-byte `VECTOR`
/// taken off `G_SCRATCH_HEAD`. Inlined so each scratch-head access keeps its
/// own `lui` instead of sharing a CSE'd register.
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

/// Per-frame callback of the main enemy. `D_801153F4` 2 hides the model,
/// 0 runs the current state handler (then colours it), 1 only colours it.
/// Unless `field_451` is set, it then runs `ActorsShared80163354` for
/// three part pairs.
void func_actor_342400_80165FC0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable9   sp    = D_actor_342400_80161F50;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
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

/// Closes this unit's `.rodata` so `actor_342400_8`'s rodata starts at
/// 0x80161F78; gas does not pad the section out. Nothing reads it.
const u32 D_actor_342400_80161F74 = 0;
