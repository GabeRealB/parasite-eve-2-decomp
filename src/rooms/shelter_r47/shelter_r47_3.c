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

s32 func_shelter_r47_8018061C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x542F0011, 0, 0);
    }
    return 0;
}

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

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D700);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D728);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D740);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D758);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D770);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D7A0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D7C8);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", D_shelter_r47_8017D7DC);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D80C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D824);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", jtbl_shelter_r47_8017D83C);
