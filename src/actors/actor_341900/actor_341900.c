#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "actors/actor_341900.h"

extern GpMsgEntry D_actor_341900_80163A38[];

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80161E58);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80161FD0);

/// Turns the model's world translation into the light/colour matrix pair the
/// actor draws with, allocating that pair on the first frame.
void func_actor_341900_80162200(Task* arg0)
{
    TmdObject*           extra;
    TmdObject*           mdl;
    Actor341900ColorMtx* mtx;
    VECTOR               pos;

    if (arg0->state == 0) {
        extra       = (TmdObject*)arg0->extra;
        mtx         = (Actor341900ColorMtx*)Mem_Malloc(0x44, 0);
        arg0->idMap = (TaskIdMap*)mtx;
        if (mtx == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(mtx, 0, 0x44);
            mtx->field_40                           = (Task*)arg0->spawnArg2;
            extra->field_C                          = 0;
            ((TmdObject*)arg0->extra)->field_8->sub = &Gfx_ViewCoord;
            extra->field_1C                         = &mtx->light;
            extra->field_20                         = &mtx->color;
            extra->field_E                          = 0x1F;
            arg0->field_24                          = D_actor_341900_80163A38;
            Task_Reparent(mtx->field_40, arg0);
        }
        arg0->state++;
    }

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80162330);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_801625B4);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80162708);

INCLUDE_RODATA("actors/nonmatchings/actor_341900/actor_341900", D_actor_341900_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_801628B8);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80162AD4);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80162EFC);
