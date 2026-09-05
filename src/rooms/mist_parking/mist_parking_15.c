#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s16      D_80071076;
extern TaskDesc RoomsShared8018397cDesc;
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
extern void                func_800B0928(Task* task, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
/// Position triple in a parking-lot layout table; 8 bytes with a trailing pad.
typedef struct {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 pad;
} MistParkingVec;
/// 12-byte layout record copied verbatim by `func_mist_parking_80183BAC`.
typedef struct {
    /* 0x0 */ s8 b[12];
} MistParkingBlob;
/// Table of pointers into the room layout data: the two-entry vector list at
/// `field_4`, the six-entry list at `field_8` and the two 12-byte records at
/// `field_C`. `D_mist_parking_8018FCB8` is the template, `D_mist_parking_80192204`
/// the live copy.
typedef struct {
    /* 0x00 */ s32              field_0;
    /* 0x04 */ MistParkingVec*  field_4;
    /* 0x08 */ MistParkingVec*  field_8;
    /* 0x0C */ MistParkingBlob* field_C;
    /* 0x10 */ void*            field_10;
} MistParkingLayout;
extern MistParkingLayout D_mist_parking_8018FCB8;
extern MistParkingLayout D_mist_parking_80192204;

void func_mist_parking_80183BAC(s32 arg0)
{
    MistParkingLayout* dst;
    MistParkingLayout* src;
    MistParkingVec     d;
    s32                i;

    dst = &D_mist_parking_80192204;
    src = &D_mist_parking_8018FCB8;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].x = src->field_4[i].x;
        dst->field_4[i].y = src->field_4[i].y;
        dst->field_4[i].z = src->field_4[i].z;
        dst->field_C[i]   = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x = src->field_8[i].x;
        dst->field_8[i].y = src->field_8[i].y;
        dst->field_8[i].z = src->field_8[i].z;
    }

    if (arg0 == 0) {
        d.x = 0;
        d.y = 0;
    } else {
        d.x = 0;
        d.y = 0x7D0;
    }
    d.z = 0;

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x += d.x;
        dst->field_8[i].y += d.y;
        dst->field_8[i].z += d.z;
    }
}

void func_mist_parking_80183D58(Task* task)
{
    GameActor* actor;
    GpWorkObj* work;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)((Task*)Game_GetPtrSlot(3))->idMap;
    if (D_801156F9 == 0) {
        idx = actor->field_438[1].field_4 - 0x2F;
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
            work = Gp_FindWorkById(Game_Session->field_6 | (Game_Session->field_7 << 8));
            func_800B0928(Game_GetPtrSlot(3), work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            Task_Kill(task);
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
            if (Game_Session->field_1 != 0) {
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
            if (Game_Session->field_1 != 0) {
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
            if (Game_Session->field_1 != 0) {
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
            if (Game_Session->field_1 != 0) {
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
            Task_Kill(task);
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
            if (Game_Session->field_1 != 0) {
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
            Task_Kill(task);
            break;
    }
}

void func_mist_parking_80184408(s32 arg0)
{
    Gp_RunCapCmd(arg0, 0);
}

void func_mist_parking_80184428(s32 arg0)
{
    Task_SpawnFromTable(&RoomsShared8018397cDesc, 0, arg0, 0);
    Game_Session->field_64 = 1;
}

void func_mist_parking_80184468(s32 arg0)
{
    Mc_SaveData.field_7 = 1;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    Mc_SaveData.field_6 = arg0;
    D_80071076          = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        Fs_BeginBootLoad(&Mc_SaveData.field_4, 0);
    }
}
