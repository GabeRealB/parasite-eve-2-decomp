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

extern SVECTOR D_shelter_r47_80187624[];
extern SVECTOR D_shelter_r47_80187664[];

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_5", func_shelter_r47_80185354);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_5", func_shelter_r47_80185450);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_5", func_shelter_r47_801855B8);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_5", func_shelter_r47_8018571C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_5", func_shelter_r47_8018580C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_5", func_shelter_r47_8018585C);

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
