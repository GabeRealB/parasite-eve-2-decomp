#include "common.h"

#include "actors/actor_335800.h"

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
    Game_Session->field_5 = D_8007216D = arg0;
    Game_Session->field_76             = 1;
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
        D_8007216C             = areaId;
        Game_Session->field_4  = areaId;
        Game_Session->field_52 = 1;
        Game_Session->field_76 = 1;
    }
}

void func_actor_335800_801622C0(s32 arg0)
{
    GameSession*         g;
    GameSessionFrom4*    sess;
    Actor335800SprtRec*  rec;
    Actor335800SprtView* view;

    g    = Game_Session;
    sess = (GameSessionFrom4*)&g->field_4;
    rec  = (Actor335800SprtRec*)Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1];
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

    if (Game_Session->field_126 != 0) {
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

    if ((Game_Session->field_1 != 0) && (Game_Session->field_5F == 0)) {
        temp_v1 = Game_Session->field_13B;
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
    arg0->idMap     = (TaskIdMap*)work;
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
        sessionKey      = (GpAreaKey*)&Game_Session->field_4;
        key.field_3     = sessionKey->field_3;
        key.field_2     = sessionKey->field_2;
        key.field_1     = sessionKey->field_1;
        key.field_0     = sessionKey->field_0;
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
        sessionKey      = (GpAreaKey*)(keyAddr = (u8*)&Game_Session->field_4);
        key.field_3     = sessionKey->field_3;
        key.field_2     = sessionKey->field_2;
        key.field_1     = ((GpAreaKey*)keyAddr)->field_1;
        key.field_0     = ((GpAreaKey*)(&Game_Session->field_4))->field_0;
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

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162844);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162B3C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162C80);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared80138404Table);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E30);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared80132920Offset);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E5C);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E68);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E78);
