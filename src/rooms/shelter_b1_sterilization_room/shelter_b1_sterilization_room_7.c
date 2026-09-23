#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "gameplay/1A8.h"
#include "gameplay/3FB8.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `gpf 1`. The `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern TaskDesc D_shelter_b1_sterilization_room_80188504;
extern s32      D_shelter_b1_sterilization_room_8018C340;
extern s32      D_shelter_b1_sterilization_room_80188C94;
extern s32      D_shelter_b1_sterilization_room_80188E14;

extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern s32  D_shelter_b1_sterilization_room_8018873C;
extern s32  D_shelter_b1_sterilization_room_80188AB4;

extern u32     Gp_LcgState;
extern SVECTOR D_shelter_b1_sterilization_room_80189334[];

void func_shelter_b1_sterilization_room_801826F0(GsCOORDINATE2* coord, s16 frame, s16 arg2, s16 arg3);

void func_shelter_b1_sterilization_room_801813A0(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(8);
            gGameSession->eventState = 1;
            arg0->state              = arg0->state + 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                func_800E8634((s32)&D_shelter_b1_sterilization_room_80188C94, 0, (s32)&D_shelter_b1_sterilization_room_80188E14);
                GameFlag_SetNibble(0x77, 1);
                gGameSession->restartMode = 0;
            } else {
                gGameSession->eventState = 0;
                Gp_MsgPlayerWeapon(1);
            }
            taskKill(arg0);
            return;
        default:
            gGameSession->eventState = 0;
            taskKill(arg0);
            return;
    }
}

void func_shelter_b1_sterilization_room_801814B0(void)
{
    Mc_SaveData.at4.loc.area = 0x27;
    Mc_SaveData.at4.loc.warp = 3;
    Mc_SaveData.at4.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_shelter_b1_sterilization_room_801814FC(Task* arg0)
{
    s32 state = arg0->state;

    switch (state) {
        case 0:
            gGameSession->viewDirty = 1;
            arg0->state            += 1;
            break;
        case 1:
            Mc_SaveData.at4.loc.room    = 2;
            gGameSession->at4.loc.room  = 2;
            gGameSession->roomObjsDirty = state;
            arg0->state                += 1;
            break;
        default:
            taskKill(arg0);
            break;
    }
}

void func_shelter_b1_sterilization_room_80181588(Task* arg0)
{
    if (arg0->state == 0) {
        Gp_MsgPlayerWeapon(0);
        Gp_RunCapCmd1(9);
        arg0->state += 1;
        return;
    }
    Gp_MsgPlayerWeapon(1);
    taskKill(arg0);
}

void func_shelter_b1_sterilization_room_801815EC(void)
{
    if (!(D_shelter_b1_sterilization_room_8018C340 & 0x20)) {
        D_shelter_b1_sterilization_room_8018C340 |= 0x20;
        Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80188504, 5, 0, 0);
    }
}

void func_shelter_b1_sterilization_room_80181634(Task* arg0)
{
    D_shelter_b1_sterilization_room_8018C340 = 0;
    taskKill(arg0);
}

void func_shelter_b1_sterilization_room_80181658(void)
{
    SndEvt_EnqueueTypeA(0x54100006, 0, 0x24);
    SndEvt_EnqueueTypeA(0x54100007, 0, 0x24);
}

void func_shelter_b1_sterilization_room_80181698(s32 arg0)
{
    Gp_ResetCap();
    if (arg0 == 1) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x2C0, 0x100);
    }
}

void func_shelter_b1_sterilization_room_801816E0(Task* task)
{
    s32 cmd;
    s32 flag;

    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            if (task->spawnArg1 != 0) {
                flag = GameFlag_GetNibble(0x77);
                cmd  = 8;
                if (flag == 0) {
                    cmd = 7;
                }
                Gp_RunCapCmd1(cmd);
                GameFlag_SetNibble(0x149, 1);
            } else {
                flag = GameFlag_GetNibble(0x77);
                cmd  = 6;
                if (flag != 0) {
                    GameFlag_SetNibble(0x14A, 1);
                    GameFlag_SetNibble(0x151, 1);
                    cmd = 9;
                }
                Gp_RunCapCmd1(cmd);
            }
            task->state++;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                taskKill(task);
            }
            return;
    }
}

void func_shelter_b1_sterilization_room_801817EC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            Gp_RunCapCmd1(task->spawnArg1);
            task->state = task->state + 1;
            /* fallthrough */
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                taskKill(task);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_8018188C);

/// Per-frame update of a drifting effect drawn by
/// `func_shelter_b1_sterilization_room_801826F0`. State 0 seeds the work block
/// from the LCG and takes a direction from a table indexed by the 12-bit angle
/// in `spawnArg1`, scaled through the GTE by `field_28` and jittered into the
/// velocity `field_10`. Each tick then moves the coordinate by that velocity
/// and adds `field_2A` to `field_24`; while an event is running the tick
/// counter is held instead. The drawn frame advances every `field_20` ticks
/// and the task is released once ten frames have passed.
void func_shelter_b1_sterilization_room_801823D8(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            base;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    work->field_22++;
    switch (task->state) {
        case 0:
            base             = ((GpEffSpawnArg*)&task->spawnArg1)->field_2;
            work->field_24   = (u16)((((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF) + 0x180) + base;
            work->field_26   = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFFF;
            task->spawnArg1 &= 0xFFF;
            work->field_20   = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) + 1;
            work->field_28   = ((s16)work->field_24 >> 5) + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
            work->field_2A   = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF;
            gte_lddp(work->field_28);
            gte_ldsv(&D_shelter_b1_sterilization_room_80189334[task->spawnArg1 / 16]);
            gte_gpf12_real();
            vec = &work->field_10;
            gte_stsv(vec);
            work->field_10.vx -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            work->field_10.vy -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            work->field_10.vz -= (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) - 8;
            task->state        = 1;
        case 1:
            if (Gp_State1C->eventState == 0) {
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
                work->field_24    += work->field_2A;
            } else {
                work->field_22--;
            }
            func_shelter_b1_sterilization_room_801826F0(coord, ((s16)work->field_22 - 1) / (s16)work->field_20,
                                                        work->field_24, work->field_26);
            if ((s16)work->field_20 * 10 - 1 < (s16)work->field_22) {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_801826F0);
