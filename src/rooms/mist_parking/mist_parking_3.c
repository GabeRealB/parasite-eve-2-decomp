#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

/// Cutscene script blob argument of `func_800E8614`.
extern void func_800E8614(s32 arg0, s32 arg1);

/// Cutscene script blobs played by the MIST parking item-hand-over scene.
extern s32 D_mist_parking_8018F9A4;
extern s32 D_mist_parking_8018FA4C;
extern s32 D_mist_parking_8018FB3C;
/// Item ids / quantities handed over, one pair per scan flag `0x125 + i`.
extern s32 D_mist_parking_8018FBFC[];
extern s32 D_mist_parking_8018FC10[];

/// Per-item playback cursor of the hand-over scene. `timer` counts a slot
/// down from 10 (5 starts the cap slot, 0 commits the flag), `index` walks
/// the five `0x125 + i` scan flags.
typedef struct {
    /* 0x0 */ s16 timer;
    /* 0x2 */ s16 index;
} MistParkingHandOver;

extern MistParkingHandOver D_mist_parking_80195328;

extern void func_mist_parking_80183708(s32 arg0);

void func_mist_parking_80182A44(Task* task)
{
    MistParkingHandOver* st = &D_mist_parking_80195328;
    s32                  i;
    s32                  flag;
    s16                  idx;
    s32                  cmd;

    switch (task->state) {
        case 0:
            Mem_Set(st, 0, 4);
            func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
            Gp_RunCapCmd(2, 0);
            task->state++;
            break;
        case 1:
            if (Game_Session->field_1 != 0) {
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
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
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
            }
            switch (Gp_GetCapEventKey()) {
                case 1:
                    st->timer = 0xA;
                    Gp_RunCapCmd(7, 0);
                    task->state = 4;
                    return;
                case 4:
                    for (i = 0; i < 5; i++) {
                        flag = i + 0x125;
                        if (GameFlag_GetNibble(flag) == 2) {
                            GameFlag_SetNibble(flag, 3);
                        }
                    }
                    func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                    cmd = 8;
                    break;
                case 3:
                    for (i = 0; i < 4; i++) {
                        flag = i + 0x125;
                        if (GameFlag_GetNibble(flag) == 2) {
                            if (Gp_GiveItem(Gp_ScanPtrs[3], D_mist_parking_8018FBFC[i],
                                            D_mist_parking_8018FC10[i]) != NULL) {
                                GameFlag_SetNibble(flag, 3);
                                Gp_SetCurBit2Flag(i + 0x20, 2);
                            }
                        }
                    }
                    if (GameFlag_GetNibble(0x129) == 2 && func_800B7420(0x6C) == 0) {
                        if (Gp_GiveItem(D_8010D55C, 0x6C, 1) != NULL) {
                            GameFlag_SetNibble(0x129, 3);
                            Gp_SetCurBit2Flag(0x24, 2);
                        }
                    }
                    func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                    cmd = 4;
                    break;
                default:
                    return;
            }
            Gp_RunCapCmd(cmd, 0);
            task->state = 6;
            return;
        case 4:
            if (Gp_CapBusy() != 0) {
                break;
            }
            st->timer--;
            if (st->timer == 5) {
                idx = st->index;
                if (GameFlag_GetNibble(idx + 0x125) == 2) {
                    Gp_StartCapSlot(5, 0, idx);
                }
                break;
            }
            if (st->timer != 0) {
                break;
            }
            idx = st->index;
            if (Gp_GetCurBit2Flag(idx + 0x20) != 1) {
                GameFlag_SetNibble(idx + 0x125, 3);
            }
            st->timer = 0xA;
            st->index++;
            if (st->index >= 5) {
                task->state++;
            }
            break;
        case 5:
            if (Game_Session->field_1 != 0) {
                break;
            }
            if (Gp_CapBusy() == 0) {
                func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                Gp_RunCapCmd(2, 0);
                task->state++;
            }
            /* fallthrough */
        case 6:
            if (Game_Session->field_1 != 0) {
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
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
                break;
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
                break;
            }
            task->state++;
            break;
        case 10:
            Gp_MsgPlayerWeapon(1);
            Task_Kill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80182F60);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018307C);

void func_mist_parking_801830F8(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183100);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018312C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018316C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801831F0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018326C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801832AC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183304);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801833F8);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183434);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018345C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801834D4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018354C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018357C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183600);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183634);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183688);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801836CC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183708);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183780);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801837A4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801837B8);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018397C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801839CC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183A28);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183AC4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183B40);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183BAC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183D58);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80183EAC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801842DC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80184408);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80184428);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80184468);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801844EC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018451C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018459C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801845D0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80184624);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80184668);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801846A4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8018471C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80184728);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80184A18);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80184E8C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80185814);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_8017D8F8);
