#include "common.h"

#include "actors/actor_146300.h"
#include "actors/actors_shared_801366fc.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 D_actor_146300_80137AAC;
extern s32 D_actor_146300_80137B10;
extern s32 D_actor_146300_80137B38;
extern s32 D_actor_146300_80137B60;
extern s32 D_actor_146300_80137C10;
extern s32 D_actor_146300_801386C0;
extern s32 D_actor_146300_80138810;
extern s32 D_actor_146300_801388D0;
extern s32 D_actor_146300_80138A38;
extern s32 D_actor_146300_80138AC8;
extern s32 D_actor_146300_80142824;

void func_actor_146300_80131ECC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_AgeFlag119();
            switch (GameFlag_GetNibble(0x7B)) {
                case 2:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        Gp_RunCapCmd1(0x12);
                        task->state++;
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        D_actor_146300_80142824 = 0x13;
                        GameFlag_SetNibble(0x7B, 3);
                        task->state = 0xA;
                    }
                    break;
                case 3:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        if (Gp_GetCurBit2Flag(0x1F) == 1) {
                            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
                            D_actor_146300_80142824 = 0x13;
                            task->state             = 0x14;
                        } else {
                            Gp_RunCapCmd1(0x12);
                            task->state++;
                        }
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        D_actor_146300_80142824 = 0x14;
                        GameFlag_SetNibble(0x7B, 4);
                        task->state = 0xA;
                    }
                    break;
                case 4:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        if (Gp_GetCurBit2Flag(0x20) == 1) {
                            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
                            D_actor_146300_80142824 = 0x14;
                            task->state             = 0x14;
                        } else {
                            Gp_RunCapCmd1(0x12);
                            task->state++;
                        }
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        GameFlag_SetNibble(0x7B, 5);
                        task->state = 0x1E;
                    }
                    break;
                case 5:
                    if (Gp_GetCurBit2Flag(0x21) == 1) {
                        Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
                        task->state = 0x28;
                    } else {
                        func_800E8614((s32)&D_actor_146300_80138AC8, 0);
                        Task_Kill(task);
                    }
                    break;
                default:
                    task->state++;
                    break;
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(1);
            Task_Kill(task);
            break;
        case 10:
            Gp_StartCapSlot((s16)D_actor_146300_80142824, 0, 0);
            func_800E8614((s32)&D_actor_146300_801386C0, 1);
            task->state++;
            break;
        case 11:
            if (gGameSession->eventState == 0) {
                task->state = 0x14;
            }
            break;
        case 20:
            Gp_StartCapSlot((s16)D_actor_146300_80142824, 0, 1);
            func_800E8614((s32)&D_actor_146300_80138810, 1);
            task->state++;
            break;
        case 30:
            func_800E8614((s32)&D_actor_146300_801388D0, 1);
            task->state++;
            break;
        case 31:
            if (gGameSession->eventState == 0) {
                task->state = 0x28;
            }
            break;
        case 40:
            Gp_StartCapSlot(0x15, 0, 1);
            func_800E8614((s32)&D_actor_146300_80138A38, 1);
            task->state++;
            break;
        case 21:
        case 41:
            if (gGameSession->eventState == 0) {
                task->state = 1;
            }
            break;
    }
}

void func_actor_146300_8013224C(void)
{
    switch (GameFlag_GetNibble(0x7B)) {
        case 2:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
            break;
        case 3:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x1F) == 1) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
                }
            } else {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
            }
            break;
        case 4:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x20) == 1) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
                }
                break;
            }
            /* fallthrough */
        case 5:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_actor_146300_80137C10, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B60, 0);
            break;
    }
}

void func_actor_146300_801323E0(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_actor_146300_80132418(s32 arg0)
{
    switch (arg0) {
        case 0:
            if (Gp_GetCapEventKey() == 1) {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B10, 0);
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 2) {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
            }
            break;
    }
}

/// State-0 handler of the `ActorsShared80131f9c` dispatcher: allocates the
/// 0x4EC work block, publishes it in `ActorsShared80131f9cWork` and the task's
/// `work` slot, binds the model's coordinate frame to the view and hands the
/// object the block's light and colour matrices, then points it at the part's
/// world translation dropped by 0x320 in y.
///
/// The companion task from `D_actor_146300_801427C8` carries the model whose
/// texture page and CLUT row come out of the current area record - the session
/// location key is copied onto the stack, `Gp_SyncAreaKeyIndex` fills in its
/// nested index and the actor's own `field_8 >> 12` selects the 0x10-byte
/// record. The actor's task is then reparented under that companion before the
/// animation state is reset to mode 2 / id 0xB and the shared tick runs.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR              vec;
    GpAreaKey           key;
    Actor146300Work*    work;
    TmdObject*          obj;
    TmdObject*          model;
    GsCOORDINATE2*      coord;
    GpAreaKey*          sessionKey;
    Actor146300AreaRec* entry;
    Task*               helper;
    GpAreaKey*          keyPtr;
    u8                  areaByte0;
    u32                 idx;

    obj                      = task->extra;
    coord                    = obj->field_8;
    work                     = Mem_Calloc(0x4EC, 0);
    ActorsShared80131f9cWork = work;
    task->work               = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = ActorsShared801366fc;
    coord->sub              = &Gfx_ViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.field_5     = 0;
    enemy->node.field_4     = 1;
    obj->field_E            = 1;
    obj->field_C            = 0;
    D_actor_146300_8014282C = task;
    helper                  = Task_SpawnFromTable(D_actor_146300_801427C8, 1, 0, 0);
    sessionKey              = (GpAreaKey*)&gGameSession->loc;
    idx                     = enemy->field_8 >> 0xC;
    model                   = helper->extra;
    D_actor_146300_80142830 = helper;
    key.field_3             = sessionKey->field_3;
    key.field_2             = sessionKey->field_2;
    key.field_1             = sessionKey->field_1;
    areaByte0               = sessionKey->field_0;
    /* Both calls take `&key`. Left alone, GCC 2.8.1 CSEs that address into one
       pseudo that is live across the first call, costing a callee-saved
       register; the ROM rematerializes `addiu a0, sp, key` for each call. The
       barrier keeps the address materialization next to the call and the
       `+r` touch makes the second one a fresh computation. */
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry           = (Actor146300AreaRec*)((idx << 4) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
    model->field_24 = entry->field_D;
    model->field_25 = entry->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    Task_Reparent(task, D_actor_146300_80142830);
    obj->field_1C = &ActorsShared80131f9cWork->light;
    obj->field_20 = &ActorsShared80131f9cWork->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_146300_801427E0, (GpAnimObj*)obj,
                  &ActorsShared80131f9cWork->pad_374[0], ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->field_4B8 = 0xB;
    ActorsShared80131f9cWork->field_4B4 = 2;
    task->field_24                      = D_actor_146300_801427A0;
    func_actor_146300_801327CC(task);
    SOFT_BARRIER();
    task->state++;
}
