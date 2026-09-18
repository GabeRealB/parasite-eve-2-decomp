#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_160700.h"
#include "actors/actors_shared_801366fc.h"
#include "actors/actors_shared_8014c874.h"

extern TaskDesc D_actor_160700_801416A8[];
extern u8       D_actor_160700_801416C0[];
extern u8       D_actor_160700_80141678[];

extern s32 D_actor_160700_801354CC;
extern s32 D_actor_160700_80135664;
extern s32 D_actor_160700_80135ACC;
extern s32 D_actor_160700_80135BD4;
extern s32 D_actor_160700_801362F4;
extern s32 D_actor_160700_80136414;

void func_actor_160700_80131E24(void)
{
    s32 slot;

    if (GameFlag_GetNibble(0x113) != 0) {
        slot = Gp_LookupSlot4(0);
        if (slot != 0) {
            Gp_DispatchMsg((Task*)slot, 0x7D3, (s32)&D_actor_160700_801354CC, 0);
        }
    }
}

void func_actor_160700_80131E70(void)
{
    switch (GameFlag_GetNibble(0x113)) {
        case 0:
            func_800E8634((s32)&D_actor_160700_80135664, 0, (s32)&D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0xC);
            break;
        case 1:
            func_800E8634((s32)&D_actor_160700_80135BD4, 0, (s32)&D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_160700_801362F4, 0);
            GameFlag_SetNibble(0x113, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_160700_80136414, 0);
            break;
    }
}

void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GpAreaKey        key;
    Actor160700Work* work;
    Actor160700Work* mem;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    GpEnemy*         spawned;
    TmdObject*       model;
    GpAreaKey*       sessionKey;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    GpAreaRec*       rec;
    GpAreaPlace*     place;
    s32              idx;
    u32              raw;

    obj        = task->extra;
    coord      = obj->field_8;
    mem        = (Actor160700Work*)Mem_Calloc(0x4F8, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback  = ActorsShared801366fc;
    coord->sub          = &Gfx_ViewCoord;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    enemy->node.field_5 = 0;
    enemy->node.field_4 = 1;
    obj->field_C        = 0;
    obj->field_E        = 1;
    work->enemy         = enemy;
    spawned             = Gp_SpawnEnemyFromTable(D_actor_160700_801416A8, 1, 0, enemy);
    model               = (TmdObject*)spawned->task->extra;
    raw                 = enemy->field_8;
    sessionKey          = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage           = sessionKey->stage;
    key.area            = sessionKey->area;
    key.room            = sessionKey->room;
    idx                 = raw >> 12;
    areaByte0           = sessionKey->view;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec             = Gp_GetNestedAreaRec(&key);
    place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = place->field_D;
    model->field_25 = place->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    Task_Reparent(task, spawned->task);
    work->field_4F0 = spawned->task;
    work->field_4B8 = 1;
    obj->field_1C   = &work->light;
    obj->field_20   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_160700_801416C0, (GpAnimObj*)obj,
                  &work->slots[0x14], work->slots);
    work->state    = 2;
    task->msgTable = D_actor_160700_80141678;
    ActorsShared8014c874(task);
    task->state += 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_160700/actor_160700", D_actor_160700_80131E20);
