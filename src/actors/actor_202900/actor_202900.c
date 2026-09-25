#include "common.h"

#include "actors/actor_202900.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/areaplace.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8       D_actor_202900_80156E0C[];
extern TaskDesc D_actor_202900_80156E24[];
extern u8       D_actor_202900_80156E3C[];

/// Step 0 of the `func_actor_202900_8014A02C` dispatcher: allocate and publish the
/// work block, start the second task and texture its model from the area record
/// the actor was placed from, then seed the animation context and run the
/// first step body.
void func_actor_202900_80149E24(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GpAreaKey      key;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    TmdObject*     model;
    Task*          spawned;
    GpAreaRec*     rec;
    GpAreaPlace*   place;
    GpAreaKey*     sessionKey;
    GpAreaKey*     keyPtr;
    s32            idx;
    u32            raw;
    u8             areaByte0;

    obj        = task->extra;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_202900_80156E54 = memCalloc(0x564, false));
    if (D_actor_202900_80156E54 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_202900_8014A158;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    D_actor_202900_80156E58 = (GpActorWork*)task;
    spawned                 = Task_SpawnFromTable(D_actor_202900_80156E24, 1, 0, 0);
    D_actor_202900_80156E5C = spawned;
    sessionKey              = (GpAreaKey*)&gGameSession->at4.loc;
    raw                     = enemy->placeKey;
    model                   = spawned->extra;
    key.stage               = sessionKey->stage;
    key.area                = sessionKey->area;
    key.room                = sessionKey->room;
    areaByte0               = sessionKey->view;
    idx                     = raw >> 12;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec          = Gp_GetNestedAreaRec(&key);
    place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    obj->lightMtx = &D_actor_202900_80156E54->light;
    obj->colorMtx = &D_actor_202900_80156E54->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gp_AnimInitCtx(&D_actor_202900_80156E54->anim, D_actor_202900_80156E3C, obj,
                   D_actor_202900_80156E54->poses);
    D_actor_202900_80156E54->animId    = 4;
    D_actor_202900_80156E54->field_47C = 2;
    D_actor_202900_80156E54->field_484 = 0;
    task->msgTable                     = D_actor_202900_80156E0C;
    func_actor_202900_8014A194((GpActorWork*)task);
    task->state++;
}
