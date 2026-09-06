#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern void           func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern void           func_80132210(void);
extern GpMsgEntry     D_shelter_1f_tent_80181CDC[];
extern GpAreaApplyRec D_shelter_1f_tent_801842D4[];
extern GpAreaApplyRec D_shelter_1f_tent_801843A8[];
extern GpAreaApplyRec D_shelter_1f_tent_801843B0[];
extern GpAreaApplyRec D_shelter_1f_tent_801843B8[];
extern s32            D_801362B8;
extern s32            D_80137890;

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_tent/shelter_1f_tent_2", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_tent/shelter_1f_tent_2", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_tent/shelter_1f_tent_2", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_tent/shelter_1f_tent_2", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_tent/shelter_1f_tent_2", RoomsShared8017ea68Title);

void func_shelter_1f_tent_8017F9F0(Task* task)
{
    s32 idx;
    s32 val;

    task->field_24 = D_shelter_1f_tent_80181CDC;
    Game_SetPtrSlot(task, 7);
    if (Game_Session->field_9 == 1) {
        func_80132210();
    }
    if (GameFlag_GetNibble(0x109) == 0) {
        GameFlag_SetNibble(0x109, 1);
        GameFlag_SetNibble(0x1B3, 2);
        GameFlag_SetNibble(0x1B0, 2);
        GameFlag_SetNibble(0x1AE, 2);
        GameFlag_SetNibble(0x1CD, 2);
        GameFlag_SetNibble(0xFC, 0);
        GameFlag_SetNibble(0x1B6, 0);
        GameFlag_SetNibble(0xBA, 0);
        GameFlag_SetNibble(0x1BA, 2);
        GameFlag_SetNibble(0x1BB, 2);
        func_800E3FAC(0xA2, 0x36);
        Gp_FillPlayerHpMp();
        Gp_ApplyAreaRecs(D_shelter_1f_tent_801842D4);
        func_800E8634((s32)&D_801362B8, 0, (s32)&D_80137890);
        if (GameFlag_GetNibble(0x112) != 0) {
            Gp_ApplyAreaRecs(D_shelter_1f_tent_801843B0);
            Gp_ApplyAreaRecs(D_shelter_1f_tent_801843B8);
            GameFlag_SetNibble(3, 0);
            idx = 0x155;
            val = 0xB;
        } else {
            Gp_ApplyAreaRecs(D_shelter_1f_tent_801843A8);
            GameFlag_SetNibble(3, 0);
            idx = 0x155;
            val = 0xA;
        }
        GameFlag_SetNibble(idx, val);
        if (GameFlag_GetNibble(0x73) != 0) {
            GameFlag_SetNibble(0x4B, 8);
        }
    } else {
        SndEvt_EnqueueType6(0x551C0009, 0, 0);
        SndEvt_EnqueueType6(0x551C000A, 0, 0);
    }
    task->state = task->state + 1;
}
