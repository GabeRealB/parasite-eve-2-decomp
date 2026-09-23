#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Scratch state of the room's first cap script: the task family whose state
/// table is `RoomsShared8017d8d0States` (dispatcher `func_shelter_r47_80182B18`).
/// `memCalloc(0x54)` in its state-0 entry `func_shelter_r47_8018138C`, stored
/// at `Task::work`.
typedef struct {
    /* 0x00 */ s16 field_0[5];
    /* 0x0A */ u8  pad_A[2];
    /* 0x0C */ s16 field_C[5];
    /* 0x16 */ u8  pad_16[2];
    /* 0x18 */ s16 field_18; ///< committed to game flag 0xAC when the script ends
    /* 0x1A */ s16 field_1A; ///< committed to game flag 0xD5 when the script ends
    /* 0x1C */ s16 field_1C; ///< committed to game flag 0xAE when the script ends
    /* 0x1E */ s16 field_1E; ///< committed to game flag 0xD6 when the script ends
    /* 0x20 */ s16 field_20; ///< committed to game flag 0xD2 when the script ends
    /* 0x22 */ u8  pad_22[2];
    /* 0x24 */ s16 field_24;
    /* 0x26 */ s16 field_26;
    /* 0x28 */ s16 field_28;
    /* 0x2A */ s16 field_2A;
    /* 0x2C */ s16 field_2C;
    /* 0x2E */ s16 field_2E;
    /* 0x30 */ s16 field_30;
    /* 0x32 */ s16 field_32;
    /* 0x34 */ s16 selection; ///< `id` of the hotspot the player confirmed
    /* 0x36 */ u16 fade;      ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    /* 0x38 */ u8  pad_38[2];
    /* 0x3A */ s16 field_3A;
    /* 0x3C */ s16 field_3C;
    /* 0x3E */ s16 field_3E;
    /* 0x40 */ s16 field_40;
    /* 0x42 */ s16 field_42; ///< counter gating the move to state 3
    /* 0x44 */ s16 step;     ///< sub-step selected by the running cap event
    /* 0x46 */ s16 field_46;
    /* 0x48 */ s16 field_48;
    /* 0x4A */ u8  promptKind; ///< `promptKind` of the hotspot the player confirmed
    /* 0x4B */ u8  pad_4B[3];
    /* 0x4E */ u8  field_4E;   ///< `Mc_SaveData.at4.loc.view` saved on entry
    /* 0x4F */ u8  pad_4F[2];
    /* 0x51 */ s8  field_51;
    /* 0x52 */ s8  field_52; ///< set when `field_1E` is non-zero on entry
    /* 0x53 */ u8  pad_53;
} ShelterR47State;
STATIC_ASSERT_SIZEOF(ShelterR47State, 0x54);

/// Scratch state of the room's second cap script: the task family whose state
/// table is `D_shelter_r47_8017D7DC` (dispatcher `func_shelter_r47_80185214`).
/// `memCalloc(0x30)` in its state-0 entry `func_shelter_r47_8018431C`, stored
/// at `Task::work`.
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
    /* 0x29 */ u8  field_29; ///< low byte of `Mc_SaveData.at4.loc.view` saved on entry
    /* 0x2A */ s8  field_2A;
    /* 0x2B */ u8  pad_2B[5];
} ShelterR47State2;
STATIC_ASSERT_SIZEOF(ShelterR47State2, 0x30);

extern u8 D_8007216C;

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_3", RoomsShared8017ea68Title);

s32  func_shelter_r47_8018097C(Task* task);
void func_shelter_r47_80181914(Task* task, s32 arg1);
void func_shelter_r47_80182AA0(Task* task);
void func_shelter_r47_801832EC(Task* task);
void func_shelter_r47_80183B84(Task* task);
void func_shelter_r47_80183E24(void);
void func_shelter_r47_80183F0C(void);
void func_shelter_r47_80183FF4(Task* task, s16 arg1);
void func_shelter_r47_80184124(Task* task, s16 arg1);
s32  func_shelter_r47_80182B9C(Task* task, RoomHotspot* table, s16 x, s16 y);

/// Task spawned by the room's cap script; polled and cleared by
/// `func_shelter_r47_80180714`.
extern Task* D_shelter_r47_8018A690;

/// Hotspot table hit-tested by `func_shelter_r47_80182B9C`.
extern RoomHotspot D_shelter_r47_80186FB4[];

extern TaskDesc D_shelter_r47_801872F0;

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
            taskKill(task);
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
            taskKill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_8018097C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80180C48);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80180F38);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_3", func_shelter_r47_80181148);

void func_shelter_r47_8018138C(Task* task)
{
    ShelterR47State* work;
    RoomHotspot*     hs;
    s32              arg1;

    work = memCalloc(0x54, false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2          = Task_SpawnFromTable(&D_shelter_r47_801872F0, 0, 1, 0);
    task->work               = work;
    work->field_4E           = Mc_SaveData.at4.loc.view;
    Mc_SaveData.at4.loc.view = 0x10;
    task->state++;
    Display_AcquireRef();

    hs = D_shelter_r47_80186FB4;
    while (hs->id != -1) {
        hs->hit = 0;
        hs++;
    }

    work->field_28             = -0xF8;
    work->field_2A             = -0x68;
    work->field_2C             = -0x98;
    work->field_2E             = 0x80;
    work->field_30             = -0x98;
    work->field_32             = 0x90;
    work->field_24             = 0xB0;
    work->field_26             = -0x68;
    work->field_0[0]           = 0xAA;
    work->field_C[0]           = -0x53;
    work->field_0[1]           = 0xBE;
    work->field_C[1]           = -0x43;
    work->field_0[2]           = 0xD2;
    work->field_C[2]           = -0x33;
    work->field_0[3]           = 0xE6;
    work->field_C[3]           = -0x23;
    work->field_0[4]           = 0xFA;
    work->field_C[4]           = -0x13;
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    func_shelter_r47_80182AA0(task);
    if (work->field_1E == 0) {
        work->field_52 = 0;
        work->field_46 = 0x140;
    } else {
        work->field_52 = 1;
        work->field_46 = 0;
    }
    {
        ShelterR47State* w = (ShelterR47State*)task->work;

        w->field_3A = 0xFF;
        w->field_3C = 0xFF;
        w->field_3E = 0xFF;
        w->field_40 = 0xFF;
    }
    arg1 = task->spawnArg1;
    if (arg1 == 1) {
        work->field_51 = arg1;
    }
}

/// Hotspot state of the room's first cap script: redraws the scene, then
/// hit-tests the action cursor against the room's hotspot table. A miss
/// highlights the prompt (`mode` 1); a hit with the prompt confirmed
/// (`buttons[0].state` 2) hands the raised entry's `id` / `promptKind` to the
/// work block and advances to state 4. `field_51` value 4 jumps to state 0xC,
/// and with `field_51` clear a dismissed prompt advances to state 6.
void func_shelter_r47_80181568(Task* task)
{
    ShelterR47State*  work;
    RoomHotspot*      hs;
    RoomActionPrompt* prompt;

    hs     = D_shelter_r47_80186FB4;
    prompt = &D_80114D28;
    work   = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    if (work->field_51 == 4) {
        task->state = 0xC;
        return;
    }
    prompt->targetId = 0x80;
    if (func_shelter_r47_80182B9C(task, hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if ((prompt->buttons[0].state == 2) && (hs->id != -1)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->selection  = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 4;
                    return;
                }
                hs++;
            } while (hs->id != -1);
        }
    } else {
        prompt->mode = 1;
    }
    if (work->field_51 == 0 && prompt->buttons[1].state == 2) {
        task->state = 6;
    }
}
