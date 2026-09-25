#include "common.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "rooms/mist_parking.h"

/// Scratch state of the parking-lot cap script driven by
/// `func_mist_parking_80183EAC`, cleared with `Mem_Set` when the task starts.
typedef struct {
    /* 0x0 */ u16 timer; // ticks down between companion slots
    /* 0x2 */ s16 slot;  // companion slot 0..4 being walked
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 cmd;   // cap command replayed by state 5
} MistParkingCapState;
void                       func_mist_parking_801846A4(s32 arg0);
extern s32                 D_mist_parking_80190C74;
extern s32                 D_mist_parking_80190D64;
extern s32                 D_mist_parking_80190E84;
extern s32                 D_mist_parking_80191034;
extern s32                 D_mist_parking_80191154;
extern s32                 D_mist_parking_80191214;
extern s32                 D_mist_parking_80191304;
extern s32                 D_mist_parking_801913C4;
extern MistParkingCapState D_mist_parking_80195334;
extern u8                  D_801156F9;
extern s32                 D_mist_parking_80190874;
extern s8                  D_mist_parking_801908C8[];
extern GpGridParams        D_mist_parking_8018FCB8;
extern GpGridParams        D_mist_parking_80192204;

void func_mist_parking_80183BAC(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_mist_parking_80192204;
    src = &D_mist_parking_8018FCB8;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

void func_mist_parking_80183D58(Task* task)
{
    GameActor* actor;
    GpWorkObj* work;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)(gameGetPtrSlot(3))->work;
    if (D_801156F9 == 0) {
        idx = actor->field_438[1].nextSet - 0x2F;
        if ((idx > 0) && (idx < D_mist_parking_80190874)) {
            flag = D_mist_parking_801908C8[idx];
        } else {
            flag = 0;
        }
        if (task->state == 0) {
            if ((flag != 0) || (task->spawnArg1 != 0)) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
            func_800B0928(gameGetPtrSlot(3), (Task*)work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

void func_mist_parking_80183EAC(Task* task)
{
    MistParkingCapState* st = &D_mist_parking_80195334;
    s32                  cmd;
    s32                  i;
    s16                  slot;
    s16                  slot2;
    s32                  key;
    u16                  raw;
    s16                  count;
    u16                  tick;
    u16                  tick2;
    s16                  next;

    switch (task->state) {
        case 0:
            Mem_Set(st, 0, 8);
            func_mist_parking_801846A4(1);
            func_800E8614((s32)&D_mist_parking_80191154, 1);
            Gp_RunCapCmd(6, 0);
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            for (i = 0; i < 5; i++) {
                if (GameFlag_GetNibble(i + 0x125) == 2) {
                    task->state = 2;
                    return;
                }
            }
            task->state = 6;
            break;
        case 2:
            func_mist_parking_801846A4(2);
            func_800E8614((s32)&D_mist_parking_80191154, 1);
            Gp_RunCapCmd(1, 0);
            st->field_4 = 1;
            task->state++;
            break;
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 1) {
                Gp_RunCapCmd(7, 0);
                st->timer   = 0xA;
                st->cmd     = 2;
                task->state = 4;
            } else {
                st->cmd     = 3;
                task->state = 5;
            }
            break;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            raw       = st->timer - 1;
            st->timer = raw;
            count     = raw;
            if (count == 5) {
                slot = st->slot;
                if (GameFlag_GetNibble(slot + 0x125) == 2) {
                    Gp_StartCapSlot(5, 0, slot);
                }
                return;
            }
            if (count != 0) {
                return;
            }
            slot2 = st->slot;
            if (Gp_GetCurBit2Flag(slot2 + 0x20) != 1) {
                GameFlag_SetNibble(slot2 + 0x125, 3);
            }
            st->timer = 0xA;
            next      = (u16)st->slot + 1;
            st->slot  = next;
            if (next >= 5) {
                task->state++;
            }
            break;
        case 5:
            func_800E8614((s32)&D_mist_parking_80191154, 1);
            Gp_RunCapCmd(st->cmd, 0);
            task->state++;
            break;
        case 6:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            tick                = task->killCountdown + 1;
            task->killCountdown = tick;
            if ((s16)tick == 0xA) {
                func_mist_parking_801846A4(1);
                Gp_RunCapCmd(9, 0);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case 7:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 8:
            key             = Gp_GetCapEventKey();
            task->spawnArg1 = key;
            if (key == 6) {
                func_800E8614((s32)&D_mist_parking_80191214, 1);
                st->field_4 = 1;
            } else if (key == 7) {
                func_800E8614((s32)&D_mist_parking_80191304, 1);
                st->field_4 = 1;
            } else {
                func_800E8614((s32)&D_mist_parking_801913C4, 1);
            }
            task->state++;
            break;
        case 9:
            tick2               = task->killCountdown + 1;
            task->killCountdown = tick2;
            if ((s16)tick2 == 0xA) {
                switch (task->spawnArg1) {
                    case 6:
                        Gp_RunCapCmd(7, 0);
                        break;
                    case 7:
                        Gp_RunCapCmd(8, 0);
                        break;
                    case 8:
                        if (st->field_4 != 0) {
                            Gp_RunCapCmd(7, 0);
                        } else {
                            Gp_RunCapCmd(0xA, 0);
                        }
                        break;
                }
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            cmd                 = 0xA;
            task->killCountdown = 0;
            if (task->spawnArg1 == 7) {
                cmd = 6;
            }
            task->state = cmd;
            break;
        case 10:
            Gp_MsgPlayerWeapon(1);
            func_mist_parking_801846A4(0);
            taskKill(task);
            break;
    }
}

void func_mist_parking_801842DC(Task* task)
{
    s32 key;

    switch (task->state) {
        case 0:
            func_mist_parking_801846A4(1);
            func_800E8614((s32)&D_mist_parking_80190C74, 1);
            task->state++;
            break;
        case 1:
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case 2:
            key             = Gp_GetCapEventKey();
            task->spawnArg1 = key;
            switch (key) {
                case 1:
                    func_800E8614((s32)&D_mist_parking_80190D64, 1);
                    break;
                case 2:
                    func_800E8614((s32)&D_mist_parking_80190E84, 1);
                    break;
                case 3:
                    func_800E8614((s32)&D_mist_parking_80191034, 1);
                    break;
            }
            task->state++;
            break;
        case 4:
            if (task->spawnArg1 == 1) {
                Gp_MsgPlayerWeapon(1);
            }
            func_mist_parking_801846A4(0);
            taskKill(task);
            break;
    }
}

void func_mist_parking_80184408(s32 arg0)
{
    Gp_RunCapCmd(arg0, 0);
}

void func_mist_parking_80184428(s32 arg0)
{
    Task_SpawnFromTable(&D_mist_parking_8018FC24, 0, arg0, 0);
    gGameSession->freezeRoomObjs = 1;
}

void func_mist_parking_80184468(s32 arg0)
{
    Mc_SaveData.at4.loc.stage = 1;
    Mc_SaveData.at4.loc.warp  = 1;
    Mc_SaveData.at4.loc.room  = 1;
    Mc_SaveData.at4.loc.area  = arg0;
    gDisplayState.roomVariant = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 0);
    }
}

/// Spawns entry 0 of `D_mist_parking_80190824`.
void func_mist_parking_801844EC(void)
{
    Task_SpawnFromTable(&D_mist_parking_80190824, 0, 0, 0);
}

void func_mist_parking_8018451C(void)
{
    func_800BC4BC();
    Player_Status.field_26    = 1;
    Mc_SaveData.at4.loc.area  = 5;
    Mc_SaveData.at4.loc.stage = 1;
    Mc_SaveData.at4.loc.warp  = 1;
    Mc_SaveData.at4.loc.room  = 1;
    gDisplayState.roomVariant = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 0);
}

/// Spawns entry 1 of `D_mist_parking_80190824` and keeps its handle in
/// `D_mist_parking_8019532C`.
void func_mist_parking_8018459C(void)
{
    D_mist_parking_8019532C = Task_SpawnFromTable(&D_mist_parking_80190824, 1, 0, 0);
}

/// Hands `phase` (0 or 1) to the task in `D_mist_parking_8019532C` as its
/// `spawnArg1`; any other value kills the task and drops the handle.
void func_mist_parking_801845D0(s32 phase)
{
    Task* t = D_mist_parking_8019532C;

    if (t == NULL) {
        return;
    }
    if (phase >= 2) {
        goto kill;
    }
    if (phase < 0) {
        goto kill;
    }
    t->spawnArg1 = phase;
    return;
kill:
    taskKill(D_mist_parking_8019532C);
    D_mist_parking_8019532C = NULL;
}

void func_mist_parking_80184624(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(&D_mist_parking_80190824, 2U), arg0, 0, 0);
}

void func_mist_parking_80184668(Task* arg0)
{
    s32 temp_v0;

    temp_v0         = arg0->spawnArg1 - 1;
    arg0->spawnArg1 = temp_v0;
    if (temp_v0 < 0) {
        taskKill(arg0);
        Stage_SetEndingFlag();
    }
}

void func_mist_parking_801846A4(s32 arg0)
{
    Gp_ResetCap();
    switch (arg0) {
        case 1:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x140, 0x100);
            break;
        case 2:
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x2C0, 0);
            break;
    }
}

/// Drops the handle in `D_mist_parking_8019532C` without killing the task.
/// Its caller passes an argument, which is unused.
void func_mist_parking_8018471C(s32 arg0)
{
    D_mist_parking_8019532C = NULL;
}
