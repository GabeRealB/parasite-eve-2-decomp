#include "common.h"

#include "actors/actor_420700.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8       D_actor_420700_8013EF48[];
extern TaskDesc D_actor_420700_8013EF68[];
extern u8       D_actor_420700_8013EF8C[];
extern s32      D_actor_420700_8013EFF0;
extern s32      D_actor_420700_8013EFF4;

/// Step 0 of the `ActorsShared80131f9c` dispatcher: allocate and publish the
/// work block, spawn the two model tasks, texture the first from the placement
/// the actor was spawned from, then seed the model's matrices and animation
/// context before running the first step body.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GpAreaKey      key;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    TmdObject*     model;
    GpAreaPlace*   place;
    s32            idx;
    u32            raw;
    GpAreaKey*     keyPtr;
    u8             view;
    GpAreaKey*     sessionKey;
    void*          work;

    obj                      = task->extra;
    coord                    = obj->coords;
    work                     = memCalloc(0x5A0, 0);
    ActorsShared80131f9cWork = work;
    task->work               = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_420700_8013239C;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    D_actor_420700_8013EFE4 = task;
    D_actor_420700_8013EFE8 = Task_SpawnFromTable(D_actor_420700_8013EF68, 1, 0, 0);
    D_actor_420700_8013EFEC = Task_SpawnFromTable(D_actor_420700_8013EF68, 2, 0, 0);
    sessionKey              = (GpAreaKey*)&gGameSession->at4.loc;
    raw                     = enemy->placeKey;
    model                   = D_actor_420700_8013EFE8->extra;
    key.stage               = sessionKey->stage;
    key.area                = sessionKey->area;
    key.room                = sessionKey->room;
    view                    = sessionKey->view;
    idx                     = raw >> 12;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = view;
    Gp_SyncAreaKeyIndex(keyPtr);
    place        = (GpAreaPlace*)((idx << 4) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    obj->lightMtx           = &ActorsShared80131f9cWork->light;
    obj->colorMtx           = &ActorsShared80131f9cWork->color;
    D_actor_420700_8013EFF0 = 0;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_420700_8013EFF4 = 0x96;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_420700_8013EF8C, obj,
                  ActorsShared80131f9cWork->poses, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->field_4B8 = 5;
    ActorsShared80131f9cWork->field_4B4 = 2;
    task->msgTable                      = D_actor_420700_8013EF48;
    func_actor_420700_80132478(task);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", ActorsShared80131f9cSub1);
