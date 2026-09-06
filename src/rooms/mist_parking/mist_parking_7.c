#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

extern void func_mist_parking_80183708(s32 arg0);

extern s32 D_mist_parking_8018F374;
extern s32 D_mist_parking_8018F4AC;
extern s32 D_mist_parking_8018F5E4;
extern s32 D_mist_parking_8018F824;
extern s32 D_mist_parking_8018F9A4;
extern s32 D_mist_parking_8018FA4C;
extern s32 D_mist_parking_8018FB3C;
extern s32 D_mist_parking_8018FBFC[];
extern s32 D_mist_parking_8018FC10[];

typedef struct {
    /* 0x0 */ s16 timer;
    /* 0x2 */ s16 index;
} MistParkingScanState;

extern MistParkingScanState D_mist_parking_80195328;

void func_mist_parking_80182A44(Task* task)
{
    s32                   i;
    s32                   flag;
    s16                   idx;
    MistParkingScanState* st = &D_mist_parking_80195328;

    switch (task->state) {
        case 0:
            Mem_Set(st, 0, 4);
            func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
            Gp_RunCapCmd(2, 0);
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
            func_mist_parking_80183708(2);
            func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
            Gp_RunCapCmd(6, 0);
            task->state++;
            break;
        case 3:
            if (Game_Session->field_1 != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            switch (Gp_GetCapEventKey()) {
                case 1:
                    st->timer = 10;
                    Gp_RunCapCmd(7, 0);
                    task->state = 4;
                    break;
                case 4:
                    for (i = 0; i < 5; i++) {
                        flag = i + 0x125;
                        if (GameFlag_GetNibble(flag) == 2) {
                            GameFlag_SetNibble(flag, 3);
                        }
                    }
                    func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                    Gp_RunCapCmd(8, 0);
                    task->state = 6;
                    break;
                case 3:
                    for (i = 0; i < 4; i++) {
                        flag = i + 0x125;
                        if (GameFlag_GetNibble(flag) == 2 && Gp_GiveItem(Gp_ScanPtrs[3], D_mist_parking_8018FBFC[i], D_mist_parking_8018FC10[i]) != 0) {
                            GameFlag_SetNibble(flag, 3);
                            Gp_SetCurBit2Flag(i + 0x20, 2);
                        }
                    }
                    if (GameFlag_GetNibble(0x129) == 2 && func_800B7420(0x6C) == 0) {
                        if (Gp_GiveItem(D_8010D55C, 0x6C, 1) != 0) {
                            GameFlag_SetNibble(0x129, 3);
                            Gp_SetCurBit2Flag(0x24, 2);
                        }
                    }
                    func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                    Gp_RunCapCmd(4, 0);
                    task->state = 6;
                    break;
            }
            break;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            st->timer--;
            if (st->timer == 5) {
                idx = st->index;
                if (GameFlag_GetNibble(idx + 0x125) == 2) {
                    Gp_StartCapSlot(5, 0, idx);
                }
                return;
            }
            if (st->timer != 0) {
                return;
            }
            idx = st->index;
            if (Gp_GetCurBit2Flag(idx + 0x20) != 1) {
                GameFlag_SetNibble(idx + 0x125, 3);
            }
            st->timer = 10;
            st->index++;
            if (st->index >= 5) {
                task->state++;
            }
            break;
        case 5:
            if (Game_Session->field_1 != 0) {
                return;
            }
            if (Gp_CapBusy() == 0) {
                func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                Gp_RunCapCmd(2, 0);
                task->state++;
            }
            /* fallthrough */
        case 6:
            if (Game_Session->field_1 != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state++;
            break;
        case 7:
            task->killCountdown++;
            if (task->killCountdown >= 0xB) {
                func_mist_parking_80183708(0);
                Gp_RunCapCmd(4, 0);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case 8:
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 1) {
                func_800E8614((s32)&D_mist_parking_8018FA4C, 1);
            } else {
                func_800E8614((s32)&D_mist_parking_8018FB3C, 1);
            }
            task->state++;
            break;
        case 9:
            task->killCountdown++;
            if (task->killCountdown == 0xA) {
                Gp_RunCapCmd(3, 0);
            }
            if (Game_Session->field_1 != 0) {
                return;
            }
            task->state++;
            break;
        case 10:
            Gp_MsgPlayerWeapon(1);
            Task_Kill(task);
            break;
    }
}

void func_mist_parking_80182F60(Task* task)
{
    s32 key;

    switch (task->state) {
        case 0:
            func_800E8614((s32)&D_mist_parking_8018F374, 1);
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
                case 4:
                    func_800E8614((s32)&D_mist_parking_8018F4AC, 1);
                    break;
                case 5:
                    func_800E8614((s32)&D_mist_parking_8018F5E4, 1);
                    break;
                case 6:
                    func_800E8614((s32)&D_mist_parking_8018F824, 1);
                    break;
            }
            task->state++;
            break;
        case 4:
            if (task->spawnArg1 == 4) {
                Gp_MsgPlayerWeapon(1);
            }
            Task_Kill(task);
            break;
    }
}
