#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/sound.h"
#include "main/task.h"

/// Scratch state of the room's first cap script: the task family whose state
/// table is `D_shelter_r47_8017D6C8` (dispatcher `func_shelter_r47_80182B18`).
/// `Mem_Calloc(0x54)` in its state-0 entry `func_shelter_r47_8018138C`, stored
/// at `Task::idMap`.
typedef struct {
    /* 0x00 */ u8  pad_0[0x18];
    /* 0x18 */ s16 field_18; ///< committed to game flag 0xAC when the script ends
    /* 0x1A */ s16 field_1A; ///< committed to game flag 0xD5 when the script ends
    /* 0x1C */ s16 field_1C; ///< committed to game flag 0xAE when the script ends
    /* 0x1E */ s16 field_1E; ///< committed to game flag 0xD6 when the script ends
    /* 0x20 */ s16 field_20; ///< committed to game flag 0xD2 when the script ends
    /* 0x22 */ u8  pad_22[0x14];
    /* 0x36 */ u16 fade;     ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    /* 0x38 */ u8  pad_38[0xA];
    /* 0x42 */ s16 field_42; ///< counter gating the move to state 3
    /* 0x44 */ s16 step;     ///< sub-step selected by the running cap event
    /* 0x46 */ u8  pad_46[2];
    /* 0x48 */ s16 field_48;
    /* 0x4A */ u8  pad_4A[7];
    /* 0x51 */ s8  field_51;
    /* 0x52 */ u8  pad_52[2];
} ShelterR47State;
STATIC_ASSERT_SIZEOF(ShelterR47State, 0x54);

/// Scratch state of the room's second cap script: the task family whose state
/// table is `D_shelter_r47_8017D7DC` (dispatcher `func_shelter_r47_80185214`).
/// `Mem_Calloc(0x30)` in its state-0 entry `func_shelter_r47_8018431C`, stored
/// at `Task::idMap`.
typedef struct {
    /* 0x00 */ u8  pad_0[0xA];
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  pad_C[0xA];
    /* 0x16 */ s16 field_16;
    /* 0x18 */ s16 field_18;
    /* 0x1A */ u8  pad_1A[2];
    /* 0x1C */ s16 field_1C;
    /* 0x1E */ u8  pad_1E[4];
    /* 0x22 */ u16 fade;     ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    /* 0x24 */ u8  pad_24[5];
    /* 0x29 */ u8  field_29; ///< low byte of `Mc_SaveData.field_4` saved on entry
    /* 0x2A */ s8  field_2A;
    /* 0x2B */ u8  pad_2B[5];
} ShelterR47State2;
STATIC_ASSERT_SIZEOF(ShelterR47State2, 0x30);

/// Menu input lock, counted down by `Gp_TickMenuLock`.
extern s16 Gp_MenuLockDelay;
extern s16 D_80114D08;
extern u8  D_8007216C;

s32  func_shelter_r47_8018097C(Task* task);
void func_shelter_r47_80181914(Task* task, s32 arg1);
void func_shelter_r47_801832EC(Task* task);
void func_shelter_r47_80183B84(Task* task);
void func_shelter_r47_80183E24(void);
void func_shelter_r47_80183F0C(void);
void func_shelter_r47_80183FF4(Task* task, s16 arg1);
void func_shelter_r47_80184124(Task* task, s16 arg1);

/// Task spawned by the room's cap script; polled and cleared by
/// `func_shelter_r47_80180714`.
extern Task* D_shelter_r47_8018A690;

extern SVECTOR D_shelter_r47_80187624[];
extern SVECTOR D_shelter_r47_80187664[];

void Room_Draw05(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw18(SVECTOR* v, s32 arg1, s32 arg2);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018097C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80180C48);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80180F38);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80181148);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018138C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80181568);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801816CC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80181914);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80181F14);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801820C0);

void func_shelter_r47_80182348(Task* task)
{
    ShelterR47State* state;
    ShelterR47State* done;
    u16              fade;
    u8               level;

    state = (ShelterR47State*)task->idMap;
    func_shelter_r47_80181914(task, 0);
    fade        = state->fade + 0x10;
    state->fade = fade;
    if ((s16)fade >= 0x100) {
        state->fade = 0xFF;
        done        = (ShelterR47State*)task->idMap;
        GameFlag_SetNibble(0xAC, done->field_18);
        GameFlag_SetNibble(0xD5, done->field_1A);
        GameFlag_SetNibble(0xAE, done->field_1C);
        GameFlag_SetNibble(0xD6, done->field_1E);
        GameFlag_SetNibble(0xD2, done->field_20);
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        Gp_MenuLockDelay = 8;
        D_80114D08       = 0xA;
        Display_ReleaseRef();
        Game_Session->field_1  = 0;
        Game_Session->field_68 = 0;
        Game_Session->field_66 = 0;
        Task_Kill((Task*)task->spawnArg2);
        Task_RequestKill(task, 0);
    }
    level = (u8)state->fade;
    Fade_DrawOverlay(level, level, level, 2);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182470);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801828D0);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182AA0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182B18);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182B9C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182C78);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182CA4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182DAC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182E78);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80182FDC);

void func_shelter_r47_80183068(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->idMap;
    func_shelter_r47_80181914(task, 0);
    state->field_48 = 0;
    state->field_42 = 0x10;
    task->state++;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801830B8);

void func_shelter_r47_80183170(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->idMap;
    func_shelter_r47_80181914(task, 0);
    if ((state->field_42 == 0) && (Gp_CapBusy() == 0)) {
        task->state = 3;
    }
}

void func_shelter_r47_801831C8(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->idMap;
    func_shelter_r47_80181914(task, 0);
    state->fade = 0;
    task->state++;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80183210);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80183234);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80183284);

void func_shelter_r47_801832E4(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801832EC);

void func_shelter_r47_8018337C(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->idMap;
    GameFlag_SetNibble(0xAC, state->field_18);
    GameFlag_SetNibble(0xD5, state->field_1A);
    GameFlag_SetNibble(0xAE, state->field_1C);
    GameFlag_SetNibble(0xD6, state->field_1E);
    GameFlag_SetNibble(0xD2, state->field_20);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801833DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80183484);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80183B84);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80183E24);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80183F0C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80183FF4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80184124);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018431C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801844A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80184658);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018489C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80184AE0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80184F40);

void func_shelter_r47_80185028(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->idMap;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
    state->fade = 0;
    task->state++;
}

void func_shelter_r47_80185098(Task* task)
{
    ShelterR47State2* state;
    u16               fade;
    u8                level;

    state = (ShelterR47State2*)task->idMap;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
    fade        = state->fade + 0x10;
    state->fade = fade;
    if ((s16)fade >= 0x100) {
        state->fade = 0xFF;
        if (task->spawnArg1 != 1) {
            Gp_MsgPlayerWeapon(1);
        }
        Gp_MsgPlayer3F3(1);
        Display_ReleaseRef();
        if (state->field_2A != 1) {
            Game_Session->field_1 = 0;
        }
        Game_Session->field_66 = 0;
        Task_Kill((Task*)task->spawnArg2);
        Task_RequestKill(task, 0);
    }
    SndEvt_EnqueueType7(0x542F0005, 1);
    level = (u8)state->fade;
    Fade_DrawOverlay(level, level, level, 2);
}

void func_shelter_r47_801851B8(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->idMap;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80185214);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801852A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80185354);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80185450);

void func_shelter_r47_80185510(Task* task)
{
    ShelterR47State2* state;

    state      = (ShelterR47State2*)task->idMap;
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    SndEvt_EnqueueType7(0x542F0005, 1);
    Display_ReleaseRef();
    D_8007216C             = state->field_29;
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    Task_Kill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_801855B8);

void func_shelter_r47_801856AC(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->idMap;
    func_shelter_r47_801851B8(task);
    if (state->field_A >= 0xB5) {
        SndEvt_EnqueueType6(0x542F0003, 0, 0);
        state->field_16 = 0x50;
        state->field_18 = 0x60;
        task->state     = 2;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018571C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018580C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018585C);

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
