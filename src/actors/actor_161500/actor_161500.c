#include "common.h"

#include "actors/actor_161500.h"
#include "actors/actors_shared_80132404.h"
#include "actors/actors_shared_801324c8.h"
#include "actors/actors_shared_80132514.h"
#include "actors/actors_shared_801366fc.h"
#include "actors/actors_shared_8014c874.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_161500_801401B0[];
extern u8       D_actor_161500_801401C8[];
extern u8       D_actor_161500_80140180[];

extern s32 D_actor_161500_80134920[8];
extern s32 D_actor_161500_80135288[8];
extern s32 D_actor_161500_801352A8;
extern s32 D_actor_161500_801354B8;
extern s32 D_actor_161500_80135668;
extern s32 D_actor_161500_801357E8;
extern s32 D_actor_161500_80135968;
extern s32 D_actor_161500_80135AE8;
extern s32 D_actor_161500_80135C68;
extern s32 D_actor_161500_80136E88;
extern s32 D_actor_161500_80137080;
extern s32 D_actor_161500_80137650;
extern s16 D_actor_161500_801376F2;
extern s32 D_actor_161500_801376F8;
extern s32 D_actor_161500_801378D8;
extern s32 D_actor_161500_80137AB8;

void func_actor_161500_80131E38(void)
{
    if ((GameFlag_GetNibble(0x116) != 1) && (GameFlag_GetNibble(0x116) != 2) && (GameFlag_GetNibble(0x113) == 4)) {
        GameFlag_SetNibble(0x113, 5);
        GameFlag_SetNibble(0x116, 4);
    }

    switch (GameFlag_GetNibble(0x116)) {
        case 0:
            func_800E8614((s32)&D_actor_161500_80135668, 0);
            break;
        case 1:
            func_800E8614((s32)&D_actor_161500_801357E8, 0);
            GameFlag_SetNibble(0x116, 3);
            break;
        case 2:
            func_800E8614((s32)&D_actor_161500_80135968, 0);
            GameFlag_SetNibble(0x116, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_161500_80135AE8, 0);
            GameFlag_SetNibble(0x116, 0);
            break;
        case 4:
            func_800E8614((s32)&D_actor_161500_80135C68, 0);
            GameFlag_SetNibble(0x116, 0);
            break;
    }
}

void func_actor_161500_80131F50(s32 arg0)
{
    s8 capFile;

    if (arg0 != 0) {
        Gp_CapFile = 0;
        if (arg0 <= 0) {
            capFile = 1;
            if (gGameSession->at4.loc.place == 1) {
                capFile = 2;
            }
            arg0 = capFile;
        }
        Gp_LoadCapFile(arg0);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_161500_80131FBC(void)
{
    s32 temp_s0;
    s32 temp_v0;

    temp_s0 = (gGameSession->at4.loc.place == 1) * 4;
    temp_v0 = GameFlag_GetNibble(0x103);
    func_800E8614(D_actor_161500_80134920[temp_v0 + temp_s0], 0);
    if (temp_v0 < 3) {
        GameFlag_SetNibble(0x103, temp_v0 + 1);
    }
}

void func_actor_161500_80132038(void)
{
    s32 temp_s0;
    s32 temp_v0;

    temp_s0 = (gGameSession->at4.loc.place == 1) * 4;
    temp_v0 = GameFlag_GetNibble(0x104);
    func_800E8614(D_actor_161500_80135288[temp_v0 + temp_s0], 0);
    if (temp_v0 < 3) {
        GameFlag_SetNibble(0x104, temp_v0 + 1);
    }
}

void func_actor_161500_801320B4(void)
{
    GameSession* session;

    session = gGameSession;
    do {
        func_800D4D2C((session->at4.loc.place == 1) ? 0x31 : 0x30);
    } while (0);
}

void func_actor_161500_801320F0(s32 arg0)
{
    Gp_RunCapCmd(arg0, 0);
}

void func_actor_161500_80132110(void)
{
    if (GameFlag_GetNibble(0x105) == 0) {
        func_800E8614((s32)&D_actor_161500_801352A8, 0);
    } else {
        func_800E8614((s32)&D_actor_161500_801354B8, 0);
    }
}

void func_actor_161500_80132150(void)
{
    if (GameFlag_GetNibble(0x112) != 0) {
        func_800D4D2C((GameFlag_GetNibble(0xEA) != 2) ? 0x31 : 0x33);
    } else {
        func_800D4D2C((GameFlag_GetNibble(0xEA) == 2) ? 0x32 : 0x30);
    }
}

void func_actor_161500_801321B4(Task* arg0)
{
    D_80115768 = 1;
    Gp_SetItemSeenBit(0x124, 1);
    GameFlag_SetNibble(0xE4, 2);
    func_800E8614((s32)&D_actor_161500_80137AB8, 0);
    taskKill(arg0);
}

void func_actor_161500_80132210(void)
{
    GsCOORDINATE2* target;
    GsCOORDINATE2* player;

    target = ((TmdObject*)((Task*)Game_GetPtrSlot(0xA))->extra)->coords;
    player = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords;
    Gp_UpdateCoord(target);
    Gp_UpdateCoord(player);
    D_actor_161500_801376F2 =
        ratan2(target->coord.t[0] - player->coord.t[0], target->coord.t[2] - player->coord.t[2]) & 0xFFF;
}

void func_actor_161500_80132294(u8 arg0)
{
    D_80115768 = arg0;
}

void func_actor_161500_801322A0(void)
{
    s32 temp_v0;

    if (Game_GetPtrSlot(0xA) != NULL) {
        temp_v0 = GameFlag_GetNibble(0xE4);
        if (temp_v0 == 1) {
            if (Gp_GetCurBit2Flag(3) == temp_v0) {
                func_800E8614((s32)&D_actor_161500_801378D8, 0);
            } else {
                func_800E8614((s32)&D_actor_161500_801376F8, 0);
            }
        }
    }
}

void func_actor_161500_8013230C(void)
{
    s32 temp_v0;

    if (Game_GetPtrSlot(0xA) != NULL) {
        temp_v0 = GameFlag_GetNibble(0xE4);
        switch (temp_v0) {
            case 0:
                func_800E8634((s32)&D_actor_161500_80137080, 0, (s32)&D_actor_161500_80136E88);
                GameFlag_SetNibble(0xE4, 1);
                break;
            case 1:
                func_800E8614((s32)&D_actor_161500_80137650, 1);
                break;
            case 2:
                break;
        }
    }
}

void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor161500Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    coord      = ((TmdObject*)task->extra)->coords;
    obj        = task->extra;
    work       = (Actor161500Work*)memCalloc(0x4FC, false);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = ActorsShared801366fc;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->otOffset        = 1;
    work->enemy          = enemy;
    if (task->spawnArg1 != 0) {
        spawned = Gp_SpawnEnemyFromTable(D_actor_161500_801401B0, 1, 0, enemy);
        Task_Reparent(task, spawned->task);
        work->field_4F4 = spawned->task;
        work->animId    = 2;
    } else {
        work->animId = 1;
    }
    work->field_4EE = 0;
    work->field_4F0 = 0;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_161500_801401C8, obj,
                  &work->slots[0x14], work->slots);
    work->state    = 2;
    task->msgTable = D_actor_161500_80140180;
    func_actor_161500_8013252C(task);
    task->state += 1;
}

/// The actor's step body, the same shape `ActorsShared8014c874` runs: state 1
/// and state 2 reseed the animation slots and advance to 3, the step state
/// walks the attach coordinate 0x1E units per frame while the walk clip has
/// `travel` left, then ticks the slots.
void func_actor_161500_8013252C(Task* task)
{
    Actor161500Work* work;
    s16              animId;

    work = (Actor161500Work*)task->work;
    if (work->state == 1) {
        ActorsShared801324c8(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        ActorsShared80132514(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            ActorsShared8014c874_MoveForward(((TmdObject*)task->extra)->coords, 0x1E);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->state   = 1;
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        ActorsShared80132404(task);
        return;
    }
}
