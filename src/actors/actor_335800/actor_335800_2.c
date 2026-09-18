#include "common.h"

#include "actors/actor_335800.h"

#include "actors/actors_shared_801327f8.h"

#include "gameplay/1A8.h"

#include "gameplay/1BC.h"

#include "gameplay/3CD8.h"

#include "gameplay/D4.h"

#include "gameplay/gameplay.h"

#include "main/gameflag.h"

#include "main/sound.h"

#include "main/task.h"

#include "main/unknown_syms.h"

extern TaskDesc D_actor_335800_80164DE0;

extern TaskDesc D_80182834;

extern s8 D_8007272D;

extern u8 D_8007216D;

extern u8 D_8007216C;

extern GpRec14 D_actor_335800_80164E7C;

extern s32 D_80165FC0;

extern s32 D_80166098;

/// The warp-payload table the two dispatchers above reach by entry:
/// `func_actor_335800_801621B4` selects `n * 3` 8-byte units of it.
extern GpMsg3EE D_actor_335800_80164EA4[];

extern s32 D_actor_335800_80164EBC;

extern s32 D_actor_335800_80164ED4;

/// The two child tasks the parent actor spawns, and the message table its
/// `field_24` is pointed at; both live in this overlay's trailing data.
extern TaskDesc D_actor_335800_8016EADC;

extern GpMsgEntry D_actor_335800_8016EB00[];

void func_actor_335800_80162F7C(Task* arg0);

void func_actor_335800_80162F9C(Task* arg0);

void func_actor_335800_801620C0(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 0, 0, 0);
}

void func_actor_335800_801620F0(u8 arg0)
{
    gGameSession->at4.loc.room = D_8007216D = arg0;
    gGameSession->roomObjsDirty             = 1;
}

void func_actor_335800_80162114(void)
{
    Task*          slot;
    TmdObject*     extra;
    GsCOORDINATE2* coord;

    slot = Game_GetPtrSlot(3);
    if (slot != NULL) {
        extra = slot->extra;
        coord = extra->field_8;
        if ((u32)(coord->coord.t[2] - 0xC53) < 0x96F) {
            Gp_DispatchMsg(slot, 0x3E9, (s32)&D_actor_335800_80164ED4, 0);
        }
        if ((u32)(coord->coord.t[2] - 0x3E9) < 0x86A) {
            Gp_DispatchMsg(slot, 0x3E9, (s32)&D_actor_335800_80164EBC, 0);
        }
    }
}

void func_actor_335800_801621B4(s32 arg0)
{
    Task*          slot;
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    s32            msgId;
    s32            lowIdx;
    s32            highIdx;
    s32            unit;

    slot = Game_GetPtrSlot(3);
    if (slot != NULL) {
        extra  = slot->extra;
        coord  = extra->field_8;
        lowIdx = 1;
        if (arg0 != 0) {
            highIdx = 2;
        } else {
            lowIdx  = 3;
            highIdx = 4;
        }
        msgId = 0x3E9;
        if (coord->coord.t[2] >= 0xC53) {
            unit = highIdx * 3;
        } else {
            unit = lowIdx * 3;
            SOFT_USE_REG(msgId);
        }
        Gp_DispatchMsg(slot, msgId, (s32)((unit * 8) + (s32)D_actor_335800_80164EA4), 0);
    }
}

void func_actor_335800_8016224C(void)
{
    register u8    areaId asm("a0");
    Task*          slot;
    TmdObject*     extra;
    GsCOORDINATE2* coord;

    slot = Game_GetPtrSlot(3);
    if (slot != NULL) {
        areaId = 6;
        extra  = slot->extra;
        coord  = extra->field_8;
        if (coord->coord.t[2] >= 0xC53) {
            areaId = 5;
        }
        D_8007216C                  = areaId;
        gGameSession->at4.loc.view  = areaId;
        gGameSession->viewDirty     = 1;
        gGameSession->roomObjsDirty = 1;
    }
}

void func_actor_335800_801622C0(s32 arg0)
{
    GameSession*         g;
    GpAreaKey*           sess;
    Actor335800SprtRec*  rec;
    Actor335800SprtView* view;

    g    = gGameSession;
    sess = &g->at4.loc;
    rec  = (Actor335800SprtRec*)Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1];
    switch (arg0) {
        case 0:
            view           = rec->field_1CC;
            view->field_14 = 0;
            view->field_1C = 0;
            break;
        case 1:
            view           = rec->field_1CC;
            view->field_14 = arg0;
            view->field_1C = arg0;
            GameFlag_SetNibble(0x7F, 1);
            break;
    }
}

void func_actor_335800_80162364(Task* arg0)
{
    if (arg0->state == 0) {
        if (arg0->spawnArg1 != 0) {
            func_800E8614((s32)&D_80166098, 0);
        } else {
            func_800E8614((s32)&D_80165FC0, 0);
        }
        arg0->state += 1;
        return;
    }
    Task_Kill(arg0);
}

void func_actor_335800_801623D8(void)
{
    Task_SpawnFromTable(&D_80182834, 0, 0, 0);
}

void func_actor_335800_80162408(void)
{
    SetDispMask(1);
}

void func_actor_335800_80162428(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_335800_80162434(s32 arg0)
{
    SndEvt_EnqueueType2(D_80062735, arg0 & 0xFFFF);
}

void func_actor_335800_80162460(void)
{
    func_800E3FAC(0xA2, 0x18);
}

void func_actor_335800_80162484(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_actor_335800_801624B8(s32 arg0)
{
    GameFlag_SetNibble(0x108, arg0);
}

void func_actor_335800_801624DC(Task* arg0)
{
    Task* slot;

    if (gGameSession->field_126 != 0) {
        slot = Game_GetPtrSlot(3);
        Gp_PlayerWeaponId(&D_actor_335800_80164E7C.field_0);
        Gp_DispatchMsg(slot, 0x3E8, (s32)&D_actor_335800_80164E7C, 0);
        Task_Kill(arg0);
    }
}

void func_actor_335800_80162558(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 3, 0, 0);
}

void func_actor_335800_80162588(Task* arg0)
{
    s8  var_a0;
    u8  temp_v1;
    s32 count;

    if ((gGameSession->eventState != 0) && (gGameSession->evtSkipped == 0)) {
        temp_v1 = gGameSession->padScriptFlags;
        var_a0  = 0;
        if (temp_v1 & 1) {
            count  = (u16)arg0->killCountdown;
            var_a0 = count & 1;
        }
        if ((temp_v1 & 2) && !(arg0->killCountdown & 1)) {
            var_a0 = -1;
        }
        Display_ClampField126(var_a0);
        arg0->killCountdown += 1;
        return;
    }
    Display_ClampField126(0);
    Task_Kill(arg0);
}

void func_actor_335800_80162640(Task* arg0)
{
    Actor335800MainWork* work;
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    u8*                  keyAddr;
    Task*                spawned;

    work = (Actor335800MainWork*)Mem_Calloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_506 = -1;
    work->field_4D8 = 0;
    work->field_4DC = 0;
    work->field_4E0 = 0;
    spawned         = Task_SpawnFromTable(&D_actor_335800_8016EADC, 1, 4, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->field_4FC = spawned;
        model           = (TmdObject*)spawned->extra;
        idx             = ((GpEnemy*)arg0->spawnArg2)->field_8 >> 12;
        sessionKey      = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage       = sessionKey->stage;
        key.area        = sessionKey->area;
        key.room        = sessionKey->room;
        key.view        = sessionKey->view;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = place->field_D;
        model->field_25 = place->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_335800_8016EADC, 2, 8, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->field_500 = spawned;
        model           = (TmdObject*)spawned->extra;
        idx             = ((GpEnemy*)arg0->spawnArg2)->field_8 >> 12;
        sessionKey      = (GpAreaKey*)(keyAddr = (u8*)&gGameSession->at4.loc.view);
        key.stage       = sessionKey->stage;
        key.area        = sessionKey->area;
        key.room        = ((GpAreaKey*)keyAddr)->room;
        key.view        = ((GpAreaKey*)(&gGameSession->at4.loc.view))->view;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = place->field_D;
        model->field_25 = place->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
    func_actor_335800_80162F9C(arg0);
    arg0->field_24     = D_actor_335800_8016EB00;
    arg0->exitCallback = func_actor_335800_80162F7C;
    arg0->state       += 1;
}

void func_actor_335800_80162FF4(void);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_335800_80162844(Task* task)
{
    TmdObject*           ext      = task->extra;
    Actor335800MainWork* work     = (Actor335800MainWork*)task->work;
    TaskFunc             funcs[2] = { (TaskFunc)func_actor_335800_80162FF4, ActorsShared801327f8 };
    VECTOR3              pos;
    GsCOORDINATE2*       coord;
    GpAnimRec*           rec;
    s32                  i;
    s32                  j;

    funcs[work->field_4F8](task);
    coord              = ((TmdObject*)task->extra)->field_8;
    work->field_4D8   += work->field_4C8;
    work->field_4DC   += work->field_4CC;
    work->field_4E0   += work->field_4D0;
    coord->coord.t[0] += (s16)(work->field_4D8 >> 16);
    coord->coord.t[1] += (s16)(work->field_4DC >> 16);
    coord->coord.t[2] += (s16)(work->field_4E0 >> 16);
    coord->flg         = 0;
    work->field_4D8    = (u16)work->field_4D8;
    work->field_4DC    = (u16)work->field_4DC;
    work->field_4E0    = (u16)work->field_4E0;
    if (!(ext->field_C & 0x80)) {
        if (work->field_474 != 0) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex(&work->anim, i);
            }
            rec = Gp_AnimGetRec(&work->anim, &work->slots[1]);
            if (rec != NULL) {
                if (!(rec->field_3 & 0x20) && (work->field_508 & 0x20)) {
                    Gp_SpawnEff(0x600A1, &((TmdObject*)task->extra)->field_8[8], 0xD, NULL);
                }
                work->field_508 = rec->field_3 & 0x30;
            }
        }
        if (work->field_504 == 0) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    work->color.m[i][j] >>= 1;
                    work->light.m[i][j] >>= 1;
                }
                work->color.t[i] >>= 1;
                work->light.t[i] >>= 1;
            }
            work->field_504 = -1;
        }
        if (work->field_504 > 0) {
            if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
                Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->field_8);
            }
        }
    }
    if (gGameSession->viewReady != 0) {
        work->field_504 = 1;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->field_8[1].workm.t, 0, 3);
    }
    if (work->field_506 >= 0) {
        if (work->field_506 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_506--;
    }
}

s32 func_actor_335800_801632A4(Task* task, s32 arg1, Actor335800AnimPreset* msg, s32 arg3);

/// Arrival check for the parent block: once both planar distances to the
/// root coordinate stop shrinking, plays anim 0x7D3 and advances `field_4FA`;
/// otherwise records the new distances.
void func_actor_335800_80162B3C(Task* arg0)
{
    Actor335800MainWork*  work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor335800AnimPreset preset;

    work  = (Actor335800MainWork*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (work->field_4B8 - coord->coord.t[0] >= 0) {
        dx = (u16)work->field_4B8 - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->field_4B8;
    }
    d.vx = dx;
    if (work->field_4C0 - coord->coord.t[2] >= 0) {
        dz = (u16)work->field_4C0 - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->field_4C0;
    }
    d.vz = dz;
    if (d.vx >= work->field_4E8 && d.vz >= work->field_4EC) {
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_335800_801632A4(arg0, 0x7D3, &preset, 0);
        work->field_4C8 = 0;
        work->field_4CC = 0;
        work->field_4D0 = 0;
        work->field_4FA++;
        return;
    }
    work->field_4E8 = d.vx < 0 ? -d.vx : d.vx;
    work->field_4EC = d.vz < 0 ? -d.vz : d.vz;
}

void         func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern void* D_actor_335800_8016EAD8[];

/// Placement handler for the parent block: stores the spawn position and
/// rotation, then applies a start preset exactly as `func_actor_335800_801632A4`
/// does (inlined here).
s32 func_actor_335800_80162C80(Task* task, s32 arg1, Actor335800Placement* place, Actor335800SpawnAnim* anim)
{
    Actor335800MainWork*   work;
    Actor335800MainWork*   w;
    Actor335800AnimPreset  preset;
    Actor335800AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor335800MainWork*)task->work;
    w->field_4F8   = 1;
    w->field_4FA   = 0;
    w->field_4B8   = place->pos.vx;
    w->field_4BC   = place->pos.vy;
    w->field_4C0   = place->pos.vz;
    w->field_4F0   = place->rot.vx;
    w->field_4F2   = place->rot.vy;
    w->field_4F4   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_477   = anim->field_4;
    } else {
        preset.field_4 = 0xD;
        w->field_477   = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor335800MainWork*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_335800_8016EAD8[work->field_476], (GpAnimObj*)ext, work->field_334,
                      work->slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0 && work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared80138404Table);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E30);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared80132920Offset);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E5C);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E68);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E78);
