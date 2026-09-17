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

extern u8 D_actor_341700_80175F5C[];

void func_actor_341700_8016D130(GpEnemy* arg0, Task* arg1)
{
    Actor341700SubWork* work;
    TaskIdMap*          idMap;
    TmdObject*          model;
    GsCOORDINATE2*      coord;
    VECTOR              block;

    model       = (TmdObject*)arg1->extra;
    coord       = model->field_8;
    idMap       = Mem_Calloc(0x80U, false);
    work        = (Actor341700SubWork*)idMap;
    arg1->idMap = idMap;
    if (idMap == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    coord->sub                                     = &Gfx_ViewCoord;
    ((TmdObject*)arg1->extra)->field_8->coord.t[1] = 0;
    ((TmdObject*)arg1->extra)->field_8->coord.t[0] = 0x1388;
    ((TmdObject*)arg1->extra)->field_8->coord.t[2] = -0x1770;
    ((TmdObject*)arg1->extra)->field_8->flg        = 0;
    arg1->field_24                                 = D_actor_341700_80175F5C;
    arg0->field_4                                  = &coord->coord;
    arg0->field_48                                 = 0;
    arg0->field_1C.vx                              = 0;
    arg0->field_1C.vy                              = 0;
    arg0->field_1C.vz                              = 0;
    arg0->field_18                                 = &((TmdObject*)arg1->extra)->field_8[2];
    arg0->node.field_4                             = 1;
    arg0->field_4C                                 = 0;
    arg0->field_42                                 = 0;
    arg0->field_40                                 = 0;
    model->field_1C                                = &work->light;
    model->field_20                                = &work->color;
    coord->flg                                     = 0;
    Gp_UpdateCoord(coord);
    block.vx = coord->workm.t[0];
    block.vy = coord->workm.t[1];
    block.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &block, 0, 0);
    work->field_2 = -1;
    work->field_0 = 1;
    arg1->state  += 1;
}

void func_actor_341700_8016D2B8(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor341700SubWork*)arg1->idMap)->field_4 != 0) {
        model              = (TmdObject*)arg1->extra;
        arg0->node.field_4 = 1;
        model->field_C     = 0x84;
    }
}

void func_actor_341700_8016D2E8(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor341700SubWork*)arg1->idMap)->field_4 != 0) {
        model              = (TmdObject*)arg1->extra;
        arg0->node.field_4 = 1;
        model->field_C     = 0;
        Tmd_AllocBuffers(model);
    }
}
