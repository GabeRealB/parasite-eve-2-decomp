#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "actors/actor_342000.h"

extern GpMsgEntry D_actor_342000_801648A8[];

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80161EA4);

void func_actor_342000_8016201C(Task* arg0)
{
    TmdObject*           extra;
    TmdObject*           mdl;
    Actor342000ColorMtx* mtx;
    VECTOR               pos;

    if (arg0->state == 0) {
        extra       = (TmdObject*)arg0->extra;
        mtx         = (Actor342000ColorMtx*)Mem_Malloc(0x44, 0);
        arg0->idMap = (TaskIdMap*)mtx;
        if (mtx == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(mtx, 0, 0x44);
            mtx->field_40  = (Task*)arg0->spawnArg2;
            extra->field_C = 0;
            if (arg0->spawnArg1 != 0) {
                extra->field_E = 0x1F;
            }
            ((TmdObject*)arg0->extra)->field_8->sub = &Gfx_ViewCoord;
            extra->field_20                         = &mtx->color;
            extra->field_1C                         = &mtx->light;
            arg0->field_24                          = D_actor_342000_801648A8;
            Task_Reparent(mtx->field_40, arg0);
        }
        arg0->state += 1;
    }

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162158);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_801625D8);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_801628C8);

INCLUDE_RODATA("actors/nonmatchings/actor_342000/actor_342000", D_actor_342000_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162BBC);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162F28);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_8016382C);
