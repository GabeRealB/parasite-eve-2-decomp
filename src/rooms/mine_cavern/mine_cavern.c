#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 D_mine_cavern_8018EB50;

#include "rooms/mine_cavern.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "mine_cavern_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pairsrc.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

// Message-table callbacks use the argument views required by this TU.

extern TaskDesc D_mine_cavern_8018E3F4;

u16 D_mine_cavern_8018E360 = 6016;

u16 D_mine_cavern_8018E362 = 2048;

u16 D_mine_cavern_8018E364 = 2048;

u16 D_mine_cavern_8018E366 = 950;

u16 D_mine_cavern_8018E368 = 2710;

SVECTOR D_mine_cavern_8018E36C[6] = {
    { 200, -2060, 5160, 0 },
    { 9000, -2040, 8710, 0 },
    { 0x4560, -2060, 5000, 0 },
    { 0x36B0, -2060, 290, 0 },
    { 4550, -2090, 280, 0 },
    { 6910, -2780, 3700, 0 },
};

SVECTOR D_mine_cavern_8018E39C[4] = {
    { 4550, -700, 340, 0 },
    { 0x3566, -800, 330, 0 },
    { 4340, -800, 8710, 0 },
    { 0x3552, -800, 8710, 0 },
};

u8 D_mine_cavern_8018E3BC[4][8] = {
    { 4, 22, 5, 23, 24, 25, 18, 19 },
    { 3, 2, 0, 0, 0, 0, 0, 0 },
    { 6, 21, 16, 0, 0, 0, 0, 0 },
    { 7, 8, 20, 17, 0, 0, 0, 0 },
};

s16 D_mine_cavern_8018E3DC = 0;

MineCavernTint D_mine_cavern_8018E3E0[5] = {
    { 30, 30, 30, 0 },
    { 25, 25, 25, 0 },
    { 17, 21, 22, 0 },
    { 7, 15, 16, 0 },
    { 0, 9, 11, 0 },
};

TaskDesc D_mine_cavern_8018E3F4 = { 0, 96, func_mine_cavern_80182DC8, { .model = NULL } };

TmdBone D_mine_cavern_8018E400[1] = {
#include "assets/mine_cavern_model_11100_skeleton.inc"
};

u32 D_mine_cavern_8018E424[1] = {
#include "assets/mine_cavern_model_11100_partVerts.inc"
};

SVECTOR D_mine_cavern_8018E428[20] = {
#include "assets/mine_cavern_model_11100_verts.inc"
};

SVECTOR D_mine_cavern_8018E4C8[11] = {
#include "assets/mine_cavern_model_11100_normals.inc"
};

u32 D_mine_cavern_8018E520[104] = {
#include "assets/mine_cavern_model_11100_stream.inc"
};

TmdSource D_mine_cavern_8018E6C0 = {
    0,
    728,
    0,
    1,
    D_mine_cavern_8018E424,
    D_mine_cavern_8018E428,
    D_mine_cavern_8018E4C8,
    D_mine_cavern_8018E400,
    D_mine_cavern_8018E520,
};

TmdBone D_mine_cavern_8018E6E4[1] = {
#include "assets/mine_cavern_model_114FC_skeleton.inc"
};

u32 D_mine_cavern_8018E708[1] = {
#include "assets/mine_cavern_model_114FC_partVerts.inc"
};

SVECTOR D_mine_cavern_8018E70C[20] = {
#include "assets/mine_cavern_model_114FC_verts.inc"
};

SVECTOR D_mine_cavern_8018E7AC[11] = {
#include "assets/mine_cavern_model_114FC_normals.inc"
};

u32 D_mine_cavern_8018E804[174] = {
#include "assets/mine_cavern_model_114FC_stream.inc"
};

TmdSource D_mine_cavern_8018EABC = {
    0,
    1248,
    0,
    1,
    D_mine_cavern_8018E708,
    D_mine_cavern_8018E70C,
    D_mine_cavern_8018E7AC,
    D_mine_cavern_8018E6E4,
    D_mine_cavern_8018E804,
};

GpU16Pair D_mine_cavern_8018EAE0[1] = {
    { 18, 0 },
};

GpPairSrcE D_mine_cavern_8018EAE4 = { D_mine_cavern_8018EAE0, 30, 0, 0, 0, 0, 0, 0, 0, 0 };

u8 D_mine_cavern_8018EAF4[36] = {
    0,
    2,
    2,
    2,
    0,
    0,
    0,
    10,
    10,
    10,
    30,
    30,
    30,
    10,
    30,
    15,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    30,
    30,
    30,
    30,
    10,
    1,
    30,
    30,
    30,
    0,
    0,
    0,
};

SVECTOR D_mine_cavern_8018EB18[4] = {
    { 4550, 0, 310, 0 },
    { 0x3566, 0, 330, 0 },
    { 4340, 0, 8710, 0 },
    { 0x3552, 0, 8710, 0 },
};

TaskDesc D_mine_cavern_8018EB38[2] = {
    { 1, 96, func_mine_cavern_80183A68, { .model = &D_mine_cavern_8018E6C0 } },
    { 1, 96, func_mine_cavern_80183C10, { .model = &D_mine_cavern_8018EABC } },
};

s32 D_mine_cavern_8018EB54;

s32 D_mine_cavern_8018EB58;

u16 D_mine_cavern_8018EB5C;

/// One byte of gameplay state. Read back with `lb` elsewhere, so it is signed.

static void func_mine_cavern_8017DDFC(Task* arg0);
static void func_mine_cavern_8017DEE4(Task* task);

s32 func_mine_cavern_8017D908(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);

    if (in->prefix.packed == 8) {
        if (GameFlag_GetNibble(0xBB) != 1) {
            if (in->field_5 != 0) {
                return 0;
            }
            Gp_SetNibbleIf(in->field_6, 2);
            if (Gp_StateF0.prefix.bytes.field_0 == 1 && gGameSession->at4.loc.variant == Gp_StateF0.prefix.bytes.field_0) {
                Gp_RunCapCmd1(9);
                return 0;
            }
            Gp_RunCapCmd1(0xD);
            if (GameFlag_GetNibble(0x11A) != 0) {
                return 0;
            }
            GameFlag_SetNibble(0x11A, 1);
            return 0;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0x11A) != 2) {
            GameFlag_SetNibble(0x11A, 2);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0);
        }
    }

    if (in->prefix.packed == 5) {
        if (gGameSession->at4.loc.variant == 1 || gGameSession->at4.loc.variant == 4) {
            if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                if (in->field_5 == 0) {
                    Gp_RunCapCmd1(0xB);
                }
                return 0;
            }
        }
    }
    return 1;
}

s32 func_mine_cavern_8017DAA0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8  temp;
    s32 flag;
    s32 cmd;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xC7) != 0) {
            return 0;
        }
        if (Gp_StateF0.prefix.bytes.field_0 == arg2) {
            temp = gGameSession->at4.loc.variant;
            if (temp == arg2 || temp == 4) {
                cmd = 0xA;
                goto cap_only;
            }
        }
        flag = GameFlag_GetNibble(0xBB);
        if (flag == 1) {
            cmd = 0x11;
            goto cap_only;
        }
        flag = GameFlag_GetNibble(0xBB);
        if (flag == 3) {
            cmd = 0x12;
            goto spawn;
        }
        flag = GameFlag_GetNibble(0xBE);
        cmd  = 5;
        if (flag == 2) {
            goto cap_only;
        }
    spawn:
        Gp_RunCapCmd1(cmd);
        Task_SpawnFromTable(D_mine_cavern_80183CA4, 0, 0, 0);
        goto rest;
    cap_only:
        Gp_RunCapCmd1(cmd);
    }
rest:
    temp = gGameSession->at4.loc.variant;
    if (temp == 1 || temp == 4) {
        switch (arg2) {
            case 8:
                Gp_StartCapSlot(8, 1, GameFlag_GetNibble(0xE2) & 1);
                break;
            case 14:
                Gp_StartCapSlot(0xE, 1, ((u32)GameFlag_GetNibble(0xE2) >> 1) & 1);
                break;
            case 15:
                Gp_StartCapSlot(0xF, 1, ((u32)GameFlag_GetNibble(0xE2) >> 2) & 1);
                break;
            case 16:
                Gp_StartCapSlot(0x10, 1, ((u32)GameFlag_GetNibble(0xE2) >> 3) & 1);
                break;
        }
    }
    return 0;
}

/// Room script callback that does nothing and reports 0.
s32 func_mine_cavern_8017DC50(void)
{
    return 0;
}

s32 func_mine_cavern_8017DC58(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 6) && (GameFlag_GetNibble(0xC4) == 1)) {
        Gp_RunCapCmd1(6);
    }
    return 0;
}

/// Advances the cavern's collapse sequence one step: flag 0xE6 goes 0 -> 1
/// (bit 0 of `Gp_StateC08.field_6` set) and 1 -> 2 (quake shake, then camera
/// pan), each step writing `D_mine_cavern_8018EB50` to the step number.
s32 func_mine_cavern_8017DC9C(void)
{
    if (GameFlag_GetNibble(0xE6) == 0) {
        Gp_StateC08.field_6 |= 1;
        Gp_PulseState1C();
        GameFlag_SetNibble(0xE6, 1);
        D_mine_cavern_8018EB50 = 1;
    } else if (GameFlag_GetNibble(0xE6) == 1) {
        func_800E3FAC(0xA2, 0x3D);
        func_800E8634(D_mine_cavern_80188A3C, 0, D_mine_cavern_80188D24);
        GameFlag_SetNibble(0xE6, 2);
    }
    return 0;
}

s32 func_mine_cavern_8017DD38(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xD) {
        SndEvt_EnqueueType6(0x54020000 | 0xD, 0, 0);
    }
    return 0;
}

void func_mine_cavern_8017DD6C(Task* task)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 0xB) {
            GameFlag_SetNibble(0xC4, 1);
            GameFlag_SetNibble(0xBE, 2);
            GameFlag_SetNibble(0xC3, 0);
        }
        if (Gp_GetCapEventKey() == 0x15) {
            GameFlag_SetNibble(0xBB, 1);
            GameFlag_SetNibble(0x1B9, 0);
        }
        taskKill(task);
    }
}

static void func_mine_cavern_8017DDFC(Task* arg0)
{
    arg0->msgTable = D_mine_cavern_80183C6C;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.variant == 1) && (GameFlag_GetNibble(0x10F) == 0)) {
        func_800E8634(D_mine_cavern_80187C74, 0, D_mine_cavern_8018804C);
        func_mine_cavern_8017E394();
        GameFlag_SetNibble(0x10F, 1);
    } else {
        gStageSceneMusicEntry = 1;
    }
    Task_SpawnFromTable(&D_mine_cavern_8018E3F4, 0, 0, 0);
    if (GameFlag_GetNibble(0xC7) != 0) {
        func_mine_cavern_8017E3A0(1);
    } else {
        func_mine_cavern_8017E3A0(0);
    }
    arg0->state            = arg0->state + 1;
    D_mine_cavern_8018EB50 = 0;
}

static void func_mine_cavern_8017DEE4(Task* task)
{
    s32 flag;

    flag = GameFlag_GetNibble(0xE6);
    if ((flag == 1) && (D_mine_cavern_8018EB50 == flag) && (Gp_StateC08.field_A != D_mine_cavern_8018EB50)) {
        func_800E8634(D_mine_cavern_80188214, 0, D_mine_cavern_801887B4);
        D_mine_cavern_8018EB50 = 2;
    }
}

/// The room task's state handlers, run by `func_mine_cavern_8017DF54`.
static const TaskFuncTable3 D_mine_cavern_8017D5C4 = {
    { func_mine_cavern_8017DDFC, func_mine_cavern_8017DEE4, taskKill },
};

/// Runs the room task's current state handler from the room's three-entry
/// table, copying the table onto the stack before the call.
void func_mine_cavern_8017DF54(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_cavern_8017D5C4;
    sp.funcs[task->state](task);
}

void func_mine_cavern_8017DFAC(s32 arg0)
{
    if ((GameFlag_GetNibble(0xE6) == 1 && D_mine_cavern_8018EB54 == 0) ||
        (GameFlag_GetNibble(0xE6) == 2 && D_mine_cavern_8018EB54 == 1)) {
        Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x1E);
        Gp_StateF0.prefix.bytes.field_1 = arg0;
        gGameSession->flowFlags        |= 0x80;
        D_mine_cavern_8018EB54         += 1;
        return;
    }
    if (arg0 < Gp_StateF0.prefix.bytes.field_1) {
        Gp_StateF0.prefix.bytes.field_1 = arg0;
    }
}

void func_mine_cavern_8017E088(s16 arg0)
{
    Gp_StartCapSlot(arg0, 1, 1);
}

void func_mine_cavern_8017E0B4(void)
{
    Gp_StateF0.prefix.bytes.field_0 = 0;
    if (Gp_StateF0.field_6 == 0) {
        (Gp_IncStateF0Ref)(0);
    }
    Gp_ArmStateF0(1);
}

void func_mine_cavern_8017E0F4(s32 arg0)
{
    if (arg0 != 0) {
        gGameSession->flowFlags &= 0xFD;
        return;
    }
    gGameSession->flowFlags |= 2;
    gGameSession->flowFlags |= 8;
}

/// Room script callback: stores its argument into `Mc_SaveData[0].state.sceneEvent`.
void func_mine_cavern_8017E150(s8 arg0)
{
    Mc_SaveData[0].state.sceneEvent = arg0;
}

void func_mine_cavern_8017E15C(void)
{
    Gp_ApplyAreaRecs(D_mine_cavern_8018E32C);
}

/// Room script callback: selects its argument as the scene music entry
/// (`gStageSceneMusicEntry`).
void func_mine_cavern_8017E180(u8 arg0)
{
    gStageSceneMusicEntry = arg0;
}

void func_mine_cavern_8017E18C(Task* task)
{
    task->killCountdown++;
    switch (task->killCountdown) {
        case 0x21:
        case 0x6:
        case 0x40:
        case 0x7C:
        case 0x60:
        case 0x8C:
            SndEvt_EnqueueType6(0x1000003A, 0, 0x30);
            break;
        case 0x50:
        case 0x12:
        case 0x30:
        case 0x70:
        case 0x87:
        case 0x218:
            SndEvt_EnqueueType6(0x10000039, 0, 0x30);
            break;
    }
    if ((gGameSession->evtSkipped != 0) || (task->killCountdown >= 0x219)) {
        taskKill(task);
    }
}

void func_mine_cavern_8017E2D8(void)
{
    SndEvt_EnqueueType2(0, 0x64);
}

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_mine_cavern_8017E2FC(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}
