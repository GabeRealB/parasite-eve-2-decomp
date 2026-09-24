#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/mc.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "actors/actors_shared_80149e54.h"
#include "rooms/shelter_b4_reservoir.h"
extern s16 D_shelter_b4_reservoir_80184F82;

extern s32 D_shelter_b4_reservoir_8018492C;
extern s16 D_shelter_b4_reservoir_80184F80;

extern u8             D_shelter_b4_reservoir_80184F78;
extern u8             D_shelter_b4_reservoir_80184F79;
extern u8             D_shelter_b4_reservoir_80184F7A;
extern s16            D_shelter_b4_reservoir_80184F7C;
extern s32            D_shelter_b4_reservoir_80187510;
extern Task*          D_shelter_b4_reservoir_80184930;
extern u8             D_8007216C;
extern u8             D_801153F4;
extern u8             D_8007216D;
extern s16            D_80114D08;
extern u8             D_80115680;
extern u8             D_80115690;
extern u8             D_shelter_b4_reservoir_80184948;
extern u8             D_shelter_b4_reservoir_80184DC8;
extern GpAreaApplyRec D_shelter_b4_reservoir_801874A0;
extern TaskDesc       D_shelter_b4_reservoir_801848EC;
extern GpSaveLoc      D_shelter_b4_reservoir_80187508;
extern GpStateBD8     D_shelter_b4_reservoir_80187500;
extern s16            D_80071076;

/// Spawn table of the screen-wave task, and the context it is spawned with.
/// The context's mode word is written through its own symbol, which is how the
/// original reached it.
extern TaskDesc     D_shelter_b4_reservoir_80184724;
extern ActorWaveCtx D_shelter_b4_reservoir_80187624;
extern s16          D_shelter_b4_reservoir_80187628;

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", D_shelter_b4_reservoir_8017D5C4);

void func_shelter_b4_reservoir_8017DE8C(Task* task)
{
    switch (task->state) {
        case 0:
            gGameSession->eventState = 1;
            gGameSession->hideHud    = 1;
            task->state++;
            break;
        case 1:
            Gp_RunCapCmd(3, 0);
            D_80115690 = 1;
            D_80115680 = 5;
            task->state++;
            break;
        case 2:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            D_801153F4 = 0;
            if (Gp_GetCapEventKey() == 0xC) {
                taskKill(task);
                D_8007216C = 5;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Gp_MsgAllyWeapon(1);
                Gp_MsgAlly3F3(1);
                gGameSession->eventState = 0;
                gGameSession->hideHud    = 0;
                D_80114D08               = 0xA;
                break;
            }
            Gp_MsgSlot4Chain(0, 0);
            func_800E8634((s32)&D_shelter_b4_reservoir_80184948, 0, (s32)&D_shelter_b4_reservoir_80184DC8);
            task->state++;
            break;
        case 4:
            if (gGameSession->eventState == 0) {
                D_8007216D                  = 2;
                gGameSession->at4.loc.room  = 2;
                gGameSession->roomObjsDirty = 1;
                GameFlag_SetNibble(0xB7, 1);
                GameFlag_SetNibble(0x1BF, 2);
                GameFlag_SetNibble(0xB6, 1);
                GameFlag_SetNibble(0x1BE, 2);
                Gp_ApplyAreaRecs(&D_shelter_b4_reservoir_801874A0);
                D_80114D08 = 0xA;
                taskKill(task);
            }
            break;
    }
}

void func_shelter_b4_reservoir_8017E068(void)
{
    D_shelter_b4_reservoir_80187510 = (D_shelter_b4_reservoir_80184F78 << 0x18) | (D_shelter_b4_reservoir_80184F7A << 0xC) | (D_shelter_b4_reservoir_80184F79 << 0x10) | D_shelter_b4_reservoir_80184F7C;
}

void func_shelter_b4_reservoir_8017E0AC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(arg0->spawnArg1, 0);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
                D_80114D08 = 0xA;
                break;
            }
            D_801153F4 = 1;
            Gp_TriggerPeIfArmed();
            D_shelter_b4_reservoir_80187500.field_0 = 0;
            D_shelter_b4_reservoir_80187500.field_1 = 0;
            D_shelter_b4_reservoir_80187500.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b4_reservoir_80187500);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(0x542D0001, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(0x542D0001) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b4_reservoir_80187508.field_2;
            Mc_SaveData.at4.loc.warp = D_shelter_b4_reservoir_80187508.field_4;
            Mc_SaveData.at4.loc.room = D_shelter_b4_reservoir_80187508.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_reservoir_8017E25C(void)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E264(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 0x2C) {
        if (GameFlag_GetNibble(0xB7) == 1) {
            if (src->field_5 == 0) {
                Gp_SetNibbleIf(src->field_6, 2);
                Gp_RunCapCmd1(2);
            }
        } else {
            if (src->field_5 == 0) {
                D_shelter_b4_reservoir_80187508.field_2 = dst->field_0;
                D_shelter_b4_reservoir_80187508.field_4 = dst->field_2;
                D_shelter_b4_reservoir_80187508.field_1 = dst->field_3;
                Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 3, 0xB, 0);
            }
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_reservoir_8017E354(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        Gp_MsgPlayer3F3(0);
        Gp_MsgAlly3F3(0);
        Gp_MsgPlayerWeapon(0);
        Gp_MsgAllyWeapon(0);
        D_8007216C = 6;
        D_801153F4 = 2;
        Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3C4(void)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3CC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        SndEvt_EnqueueType6(0x542D0000 | 2, 0, 0);
    }
    return 0;
}

void func_shelter_b4_reservoir_8017E400(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32)(arg0->killCountdown * 0x5DC) / (s32)arg0->spawnArg1;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F80 = temp_v0 - 0x7D0;
    if (arg0->spawnArg1 < temp_v1) {
        taskKill(arg0);
        D_shelter_b4_reservoir_8018492C = 0;
    }
}

void func_shelter_b4_reservoir_8017E4B0(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32) - (arg0->killCountdown * 0x708) / (s32)arg0->spawnArg1;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F82 = (s16)temp_v0;
    if (arg0->spawnArg1 < temp_v1) {
        taskKill(arg0);
        D_shelter_b4_reservoir_8018492C = 0;
    }
}

void func_shelter_b4_reservoir_8017E558(Task* arg0)
{
    TmdObject*     obj   = arg0->extra;
    GsCOORDINATE2* coord = obj->coords;

    if (arg0->state == 0) {
        coord->coord.t[0] = -1000;
        coord->coord.t[1] = -1000;
        coord->coord.t[2] = -5000;
        coord->flg        = 0;
        arg0->state++;
    }
    if (arg0->state == 2) {
        coord->coord.t[0] = -1000;
        coord->coord.t[1] = -1000;
        coord->coord.t[2] = -5000;
        coord->flg        = 0;
        arg0->state++;
    }
    if (arg0->state == 3) {
        coord->flg         = 0;
        coord->coord.t[1] += 4;
    }
    if (D_8007216C != 8) {
        obj->flags = 0x84;
    } else {
        obj->flags    = 0;
        obj->otOffset = 0;
    }
}

/// Event callback that runs the screen wave. Called with zero or less, it sets
/// the MDEC decode mode to 2, fills the wave's context (peak 0x60 reached in
/// one step, tinted 0x40/0x80/0x80) and spawns the wave task with it; called
/// with a positive value, it stores that value as the running wave's mode, so
/// 1 fades it out and 2 ends it.
void func_shelter_b4_reservoir_8017E610(s32 arg0)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (arg0 <= 0) {
        queue->field_22A                        = 2;
        D_shelter_b4_reservoir_80187624.field_0 = 1;
        D_shelter_b4_reservoir_80187624.field_2 = 0x60;
        D_shelter_b4_reservoir_80187624.field_9 = 0x40;
        D_shelter_b4_reservoir_80187624.field_8 = 1;
        D_shelter_b4_reservoir_80187624.field_A = 0x80;
        D_shelter_b4_reservoir_80187624.field_B = 0x80;
        Task_SpawnFromTable(&D_shelter_b4_reservoir_80184724, 0, 0, (s32)&D_shelter_b4_reservoir_80187624);
        return;
    }
    D_shelter_b4_reservoir_80187628 = arg0;
}

void func_shelter_b4_reservoir_8017E690(s32 arg0)
{
    switch (arg0) {
        case 0:
            D_shelter_b4_reservoir_8018492C = (s32)Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 1, 0x96, 0);
            break;
        case 1:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill((Task*)D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F80 = -0x1F4;
            break;
        case 2:
            D_shelter_b4_reservoir_8018492C = (s32)Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 2, 0x96, 0);
            break;
        case 3:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill((Task*)D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F82 = 0;
            break;
    }
}

void func_shelter_b4_reservoir_8017E770(s32 arg0)
{
    D_shelter_b4_reservoir_80184930->state = arg0;
}

void func_shelter_b4_reservoir_8017E780(s32 arg0)
{
    func_shelter_b4_reservoir_80182B04(10, arg0, 0x140);
}
