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
/// table is `RoomsShared8017d8d0States` (dispatcher `func_shelter_r47_80182B18`).
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

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017ea68Title);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_8018097C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80180C48);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80180F38);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80181148);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_8018138C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80181568);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_801816CC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80181914);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80181F14);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_801820C0);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80182470);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_801828D0);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80182AA0);
