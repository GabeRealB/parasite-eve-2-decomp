#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

extern s8         D_80071090;
extern u8         D_8007216C;
extern u8         D_80072170;
extern GpMsgEntry D_dryfield_trailer_coach_80184FA0[];
extern s32        D_dryfield_trailer_coach_801853F4;
extern s32        D_dryfield_trailer_coach_80185964;

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_801827F8);

void func_dryfield_trailer_coach_80182850(void)
{
    s32 cond;

    cond  = GameFlag_GetNibble(0x28) >= 2;
    cond += 1;
    Gp_StartCapSlot(3, 0, cond);
}

/// State 0 of the trailer-coach cutscene task. It parks the room's message
/// table in the task, then either starts the scene (day 2) or asks the stage
/// for area 1, and advances to state 1.
void func_dryfield_trailer_coach_80182888(Task* arg0)
{
    arg0->field_24 = D_dryfield_trailer_coach_80184FA0;
    Game_SetPtrSlot(arg0, 7);
    if (D_80072170 == 2) {
        func_800E8634((s32)&D_dryfield_trailer_coach_801853F4, 0, (s32)&D_dryfield_trailer_coach_80185964);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 4);
    } else {
        Stage_RequestFromAreaTable(1);
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_trailer_coach_8018291C(void)
{
    char pad[0x10];

    if (D_8007216C == 8) {
        D_80071090 = 0;
    } else {
        D_80071090 = 3;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_80182950);
