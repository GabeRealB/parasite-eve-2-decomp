#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"

/// Scratch state of the room's cap script, stored at `Task::idMap`.
typedef struct {
    /* 0x00 */ u8  pad_0[0x44];
    /* 0x44 */ s16 step; ///< sub-step selected by the running cap event
    /* 0x46 */ u8  pad_46[0xB];
    /* 0x51 */ s8  field_51;
} ShelterR47State;

s32  func_shelter_r47_8018097C(Task* task);
void func_shelter_r47_80181914(Task* task, s32 arg1);
void func_shelter_r47_801832EC(Task* task);

/// Task spawned by the room's cap script; polled and cleared by
/// `func_shelter_r47_80180714`.
extern Task* D_shelter_r47_8018A690;

extern SVECTOR D_shelter_r47_80187624[];
extern SVECTOR D_shelter_r47_80187664[];

void Room_Draw05(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw18(SVECTOR* v, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018061C);

void func_shelter_r47_80180650(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            Gp_RunCapCmd1(task->spawnArg1);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
        advance:
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            Task_Kill(task);
            break;
    }
}

void func_shelter_r47_80180714(Task* task)
{
    s32 out;

    if (Task_PollKill(D_shelter_r47_8018A690, &out) != 0) {
        Gp_MsgPlayer3F3(1);
        Gp_MsgPlayerWeapon(1);
        if (Game_Session->field_9 == 1) {
            Gp_MsgSlot4Chain(0, 1);
        }
        if (Game_GetPtrSlot(0xA) != NULL) {
            Gp_MsgAlly3F3(1);
            Gp_MsgAllyWeapon(1);
        }
        D_shelter_r47_8018A690 = NULL;
        Task_Kill(task);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801807B4);

void func_shelter_r47_8018080C(Task* task)
{
    s32 nibble;

    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            Gp_RunCapCmd1(8);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_ResetCap();
            Gp_MsgPlayerWeapon(1);
            nibble = GameFlag_GetNibble(0x165);
            if (nibble < 3) {
                GameFlag_SetNibble(0x165, nibble + 1);
            }
        default:
            Task_Kill(task);
            break;
    }
}

void func_shelter_r47_801808D4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x140, 0x100);
            Gp_RunCapCmd1(7);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_ResetCap();
            Gp_MsgPlayerWeapon(1);
        default:
            Task_Kill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018097C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80180C48);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80180F38);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181148);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018138C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181568);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801816CC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181914);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181F14);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801820C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182348);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182470);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801828D0);

s32 func_shelter_r47_801829B8(Task* task, s16 arg1)
{
    ShelterR47State* state;
    s8               step;

    state = (ShelterR47State*)task->idMap;
    step  = state->field_51;
    switch (step) {
        case 1:
            if (arg1 != step) {
                Gp_StartCapSlot(0x10, 0, 1);
                return 0;
            }
            state->field_51 = 2;
            return 1;
        case 2:
            if (arg1 != step) {
                Gp_StartCapSlot(0x10, 0, 2);
                return 0;
            }
            state->field_51 = 3;
            return 1;
        case 3:
            if (arg1 != step) {
                Gp_StartCapSlot(0x10, 0, 3);
                return 0;
            }
            state->field_51 = 4;
            return 1;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182AA0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182B18);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182B9C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182C78);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182CA4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182DAC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182E78);

void func_shelter_r47_80182F18(Task* task)
{
    s16 step;
    s32 flag;
    s32 value;

    func_shelter_r47_80181914(task, 0);
    if ((s16)func_shelter_r47_8018097C(task) != 0) {
        func_shelter_r47_801832EC(task);
        step = ((ShelterR47State*)task->idMap)->step;
        switch (step) {
            case 0:
                flag  = 0x1C6;
                value = 2;
                break;
            case 1:
                flag  = 0x1C6;
                value = 0;
                break;
            case 4:
                flag  = 0x1C4;
                value = 2;
                break;
            case 5:
                flag  = 0x1C4;
                value = 0;
                break;
            default:
                task->state = 3;
                return;
        }
        GameFlag_SetNibble(flag, value);
        task->state = 3;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182FDC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183068);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801830B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183170);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801831C8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183210);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183234);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183284);

void func_shelter_r47_801832E4(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801832EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018337C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801833DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183484);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183B84);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183E24);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183F0C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183FF4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80184124);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018431C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801844A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80184658);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018489C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80184AE0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80184F40);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185028);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185098);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801851B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185214);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801852A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185354);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185450);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185510);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801855B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801856AC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018571C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018580C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018585C);

void func_shelter_r47_801858BC(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 5:
            Room_Draw18(&D_shelter_r47_80187624[0], 0x60, 0xA0);
            Room_Draw13(&D_shelter_r47_80187624[1], 0x280, 0x444);
            Room_Draw13(&D_shelter_r47_80187624[2], 0x280, 0x444);
            Room_Draw13(&D_shelter_r47_80187624[3], 0x280, 0x444);
            Room_Draw13(&D_shelter_r47_80187624[4], 0x280, 0x444);
            Room_Draw13(&D_shelter_r47_80187624[5], 0x280, 0x444);
            Room_Draw13(&D_shelter_r47_80187624[6], 0x100, 0x344);
            Room_Draw13(&D_shelter_r47_80187624[7], 0x300, 0x344);
            Room_Draw13(&D_shelter_r47_80187624[8], 0x280, 0x344);
            Room_Draw13(&D_shelter_r47_80187624[9], 0x180, 0x344);
            break;
        case 13:
            Room_Draw13(&D_shelter_r47_80187664[0], 0x280, 0x344);
            Room_Draw13(&D_shelter_r47_80187664[1], 0x180, 0x344);
            break;
        case 14:
            Room_Draw18(&D_shelter_r47_80187624[0], 0x60, 0xA0);
            Room_Draw13(&D_shelter_r47_80187624[6], 0x100, 0x344);
            Room_Draw13(&D_shelter_r47_80187624[8], 0x280, 0x344);
            Room_Draw13(&D_shelter_r47_80187624[9], 0x180, 0x344);
            break;
        case 44:
            Room_Draw05(&D_shelter_r47_80187624[0], 0x60, 0xA0);
            Room_Draw13(&D_shelter_r47_80187624[6], 0x100, 0x344);
            break;
    }
}
