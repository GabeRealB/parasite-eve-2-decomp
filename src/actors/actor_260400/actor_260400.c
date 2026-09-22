#include "common.h"

#include "actors/actor_260400.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/areaplace.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32      D_actor_260400_8014C6C0;
extern s32      D_actor_260400_8014C788;
extern s32      D_actor_260400_8014CF38;
extern s32      D_actor_260400_8014D118;
extern s32      D_actor_260400_8014D208;
extern s32      D_actor_260400_8014D340;
extern s32      D_actor_260400_8014D4A8;
extern s32      D_actor_260400_8014D610;
extern u8       D_actor_260400_80154BE8[];
extern TaskDesc D_actor_260400_80154C18[];
extern u8       D_actor_260400_80154C30[];

void func_actor_260400_80149E38(void)
{
    switch (GameFlag_GetNibble(0xE3)) {
        case 0:
            func_800E8634((s32)&D_actor_260400_8014C788, 0, (s32)&D_actor_260400_8014CF38);
            GameFlag_SetNibble(0xE3, 1);
            break;
        case 1:
            if ((Gp_GetCurBit2Flag(4) == 1) || (Gp_GetCurBit2Flag(5) == 1)) {
                func_800E8614((s32)&D_actor_260400_8014D118, 0);
            } else {
                func_800E8614((s32)&D_actor_260400_8014D208, 0);
                GameFlag_SetNibble(0xE3, 2);
            }
            break;
        case 2:
            func_800E8614((s32)&D_actor_260400_8014D340, 0);
            GameFlag_SetNibble(0xE3, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_260400_8014D4A8, 0);
            GameFlag_SetNibble(0xE3, 4);
            break;
        case 4:
            func_800E8614((s32)&D_actor_260400_8014D610, 0);
            break;
    }
}

void func_actor_260400_80149F5C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_260400_80149FA4(void)
{
    s32 slot;

    slot = Gp_LookupSlot4(0);
    if (slot != 0) {
        Gp_DispatchMsg((Task*)slot, 0x7D4, (s32)&D_actor_260400_8014C6C0, 0);
    }
}

/// Step 0 of the `ActorsShared80131f9c` dispatcher: allocate and publish the
/// work block, seed the model's matrices and animation context, then start the
/// helper task and texture its model from the placement the actor was spawned
/// from before running the first step body.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GpAreaKey      key;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    TmdObject*     model;
    Task*          spawned;
    GpAreaPlace*   place;
    s32            idx;
    u32            raw;
    GpAreaKey*     sessionKey;
    GpAreaKey*     keyPtr;
    void*          work;

    obj                      = task->extra;
    coord                    = obj->coords;
    work                     = memCalloc(0x4F8, 0);
    ActorsShared80131f9cWork = work;
    task->work               = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_260400_8014A630;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    obj->lightMtx           = &ActorsShared80131f9cWork->light;
    obj->colorMtx           = &ActorsShared80131f9cWork->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_260400_80154C74 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_260400_80154C30, obj,
                  ActorsShared80131f9cWork->poses, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->field_4B8 = 1;
    ActorsShared80131f9cWork->field_4B4 = 2;
    spawned                             = Task_SpawnFromTable(D_actor_260400_80154C18, 1, 8, 0);
    if (spawned != NULL) {
        ActorsShared80131f9cWork->field_4F0 = spawned;
        sessionKey                          = (GpAreaKey*)&gGameSession->at4.loc;
        model                               = spawned->extra;
        raw                                 = ((GpEnemy*)task->spawnArg2)->placeKey;
        key.stage                           = sessionKey->stage;
        key.area                            = sessionKey->area;
        key.room                            = sessionKey->room;
        keyPtr                              = &key;
        TOUCH_REG(keyPtr);
        key.view = sessionKey->view;
        idx      = raw >> 12;
        Gp_SyncAreaKeyIndex(keyPtr);
        place        = (GpAreaPlace*)((idx << 4) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    ActorsShared80131f9cWork->field_4EA = 0;
    ActorsShared80131f9cWork->field_4EC = 0;
    ActorsShared80131f9cWork->field_4F4 = 0;
    task->msgTable                      = D_actor_260400_80154BE8;
    func_actor_260400_8014A200(task);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400", func_actor_260400_8014A200);
