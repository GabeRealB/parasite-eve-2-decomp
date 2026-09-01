#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

/// Scratch state of the security-room cap script, stored at `Task::idMap`.
///
/// `func_acropolis_security_room_8017FA18` -- state 0 of the family whose
/// handler table is `D_acropolis_security_room_8017D63C` -- allocates it with
/// `Mem_Calloc(0x10, 0)` and stores it straight into the `Task::idMap` slot,
/// so the size below is the allocation and not a guess; the same function
/// parks the family's `GpMsgEntry[]` in `Task::field_24`. The overlay's other
/// two allocators (`Mem_Calloc(0xA)` in `func_acropolis_security_room_8017D9DC`
/// and `Mem_Calloc(4)` in `func_acropolis_security_room_80180368`) belong to
/// other task families and to a different block.
typedef struct {
    /* 0x0 */ s32   field_0; // sub-step picked by the previous cap event
    /* 0x4 */ Task* child;   // task this state spawned, polled by Task_PollKill
    /* 0x8 */ u16   frames;  // frames the current state has been running
    /* 0xA */ byte  pad_A[0x6];
} AcropolisSecurityRoomState;
STATIC_ASSERT_SIZEOF(AcropolisSecurityRoomState, 0x10);

void func_acropolis_security_room_8017FD64(s32 arg0);

/// The two `TaskDesc`s this room's script spawns from: index 0 is
/// `func_acropolis_security_room_80180368`, index 1 is
/// `func_acropolis_security_room_801804CC`.
extern TaskDesc D_acropolis_security_room_80182700[];

extern s8  D_8007216C;
extern s16 D_80114D08;

/// 0xFF-terminated area-record lists applied as the script ends.
extern GpAreaApplyRec D_acropolis_security_room_80184F50[];
extern GpAreaApplyRec D_acropolis_security_room_80184F78[];
extern GpAreaApplyRec D_acropolis_security_room_80184F7C[];
extern GpAreaApplyRec D_acropolis_security_room_80184F80[];

void func_acropolis_security_room_8017F1BC(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(9);
    if ((flag == 0) || (flag == 2)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(3, 1, 0);
        } else if (step == 1) {
            Gp_ClearCollectedBit(0x104);
            SndEvt_EnqueueType6(0x51060001, 0, 0);
            GameFlag_SetNibble(9, GameFlag_GetNibble(9) | 1);
            GameFlag_SetNibble(1, 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
            st->field_0 = 0;
            task->state = 6;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F80);
            Task_Kill((Task*)task->spawnArg2);
            return;
        } else {
            Gp_StartCapSlot(3, 1, 2);
        }
    } else if ((flag == 1) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(3, 1, 1);
        } else {
            Gp_StartCapSlot(3, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

void func_acropolis_security_room_8017F300(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(9);
    if ((flag == 0) || (flag == 1)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(4, 1, 0);
        } else if (step == 2) {
            Gp_ClearCollectedBit(0x103);
            SndEvt_EnqueueType6(0x51060001, 0, 0);
            GameFlag_SetNibble(9, GameFlag_GetNibble(9) | 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
            st->field_0           = 0;
            task->state           = 0xA;
            Game_Session->field_1 = 1;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F50);
            if (GameFlag_GetNibble(3) < 3) {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F78);
            } else {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F7C);
            }
            Task_Kill((Task*)task->spawnArg2);
            return;
        } else {
            Gp_StartCapSlot(4, 1, 2);
            task->state = 2;
            return;
        }
    } else if ((flag == 2) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(4, 1, 1);
        } else {
            Gp_StartCapSlot(4, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017F480);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017F8E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017F9C8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FA18);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FB20);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FB54);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FBA4);

void func_acropolis_security_room_8017FC30(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    D_8007216C             = 3;
    Display_ReleaseRef();
    Task_Kill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FCB0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FD64);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FE24);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FE6C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FF0C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FF84);

void func_acropolis_security_room_8017FFD0(Task* arg0)
{
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayer3F3(0);
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180010);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180030);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801800A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8018014C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801801C4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180218);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180294);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180308);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180368);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801804CC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801805A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180A78);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180E34);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80181108);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801817A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80181C84);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80181E28);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80182574);
