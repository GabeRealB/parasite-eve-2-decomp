#include "common.h"

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
extern Task*    D_mist_parking_8019532C;
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
extern s32                 D_mist_parking_80191154;
extern s32                 D_mist_parking_80191214;
extern s32                 D_mist_parking_80191304;
extern s32                 D_mist_parking_801913C4;
extern MistParkingCapState D_mist_parking_80195334;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_12", func_mist_parking_80183BAC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_12", func_mist_parking_80183D58);

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

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_12", func_mist_parking_801842DC);

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
