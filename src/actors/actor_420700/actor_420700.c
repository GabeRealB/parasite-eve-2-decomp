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

/// Step 1 of the `ActorsShared80131f9c` dispatcher: refresh the model's third
/// coordinate and colour the actor from its world translation, run
/// `func_actor_420700_80132478`, then step the `field_4BC` ramp by the mode in
/// `field_4BA` and pass it as the weight of `func_800B0928` aimed at the
/// slot-3 task (modes 0, 1 and 2) or of `func_800B0CF4` aimed at a fixed
/// world point (mode 3).
///
/// Mode 0 chooses its own step each frame: +0x40 while the actor lies behind
/// the slot-3 actor's `field_52` heading, -0x80 otherwise or while an event
/// is running. In that mode the animation slots after the first are held
/// (rate 0) once the ramp is off zero; otherwise they run at one frame per
/// tick.
void ActorsShared80131f9cSub1(GpEnemy* enemy, Task* task)
{
    VECTOR         pos;
    GsCOORDINATE2  target[2];
    GsCOORDINATE2* coords;
    GsCOORDINATE2* player;
    GsCOORDINATE2* part;
    GameActor*     actor;
    s32            dx;
    s32            dz;
    s32            c;
    s32            i;
    u8             rate;

    coords = ((TmdObject*)task->extra)->coords;
    part   = &coords[2];
    player = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords;
    Gp_UpdateCoord(part);
    pos.vx = part->workm.t[0];
    pos.vy = part->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    func_actor_420700_80132478(task);
    rate = 0x10;
    if (ActorsShared80131f9cWork->field_4BA != 0) {
        if (ActorsShared80131f9cWork->field_4BA == 1 || ActorsShared80131f9cWork->field_4BA == 3) {
            ActorsShared80131f9cWork->field_4BC += 0x80;
            if (ActorsShared80131f9cWork->field_4BC > 0x1000) {
                ActorsShared80131f9cWork->field_4BC = 0x1000;
            }
        } else {
            ActorsShared80131f9cWork->field_4BC -= 0x80;
            if (ActorsShared80131f9cWork->field_4BC < 0) {
                ActorsShared80131f9cWork->field_4BC = 0;
            }
        }
        if (ActorsShared80131f9cWork->field_4BA == 3) {
            target[0].coord.t[0] = 0x1173;
            target[0].coord.t[1] = 0;
            target[0].coord.t[2] = -0x733;
            func_800B0CF4(task, target, 0x200, 0x100, ActorsShared80131f9cWork->field_4BC);
        } else {
            func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, ActorsShared80131f9cWork->field_4BC);
        }
    } else {
        if (gGameSession->eventState == 0) {
            dx    = coords->coord.t[0] - player->coord.t[0];
            dz    = coords->coord.t[2] - player->coord.t[2];
            actor = (GameActor*)gameGetPtrSlot(3)->work;
            c     = rcos(actor->field_52);
            if (dx * rsin(actor->field_52) + dz * c < 0) {
                D_actor_420700_8013EFF0 = 0x40;
            } else {
                D_actor_420700_8013EFF0 = -0x80;
            }
            if (ActorsShared80131f9cWork->field_4BC != 0) {
                rate = 0;
            }
        } else {
            D_actor_420700_8013EFF0 = -0x80;
        }
        ActorsShared80131f9cWork->field_4BC += D_actor_420700_8013EFF0;
        if (ActorsShared80131f9cWork->field_4BC > 0x1000) {
            ActorsShared80131f9cWork->field_4BC = 0x1000;
        }
        if (ActorsShared80131f9cWork->field_4BC < 0) {
            ActorsShared80131f9cWork->field_4BC = 0;
        }
        func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, ActorsShared80131f9cWork->field_4BC);
    }
    for (i = 1; i < 0x14; i++) {
        ActorsShared80131f9cWork->slots[i].rate = rate;
    }
}
