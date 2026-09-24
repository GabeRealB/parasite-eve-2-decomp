#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/D4.h"

extern GpMsgEntry D_shelter_b4_lower_sewer_80181E44[];
extern TaskDesc   D_shelter_b4_lower_sewer_80181E70[];

s32 func_shelter_b4_lower_sewer_8017D654(void)
{
    return 0;
}

s32 func_shelter_b4_lower_sewer_8017D65C(void)
{
    return 0;
}

void func_shelter_b4_lower_sewer_8017D664(Task* task)
{
    task->msgTable = D_shelter_b4_lower_sewer_80181E44;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xB7) != 0) {
        Task_SpawnFromTable(D_shelter_b4_lower_sewer_80181E70, 0, 0, 0);
    }
    task->state = (s32)(task->state + 1);
}

void func_shelter_b4_lower_sewer_8017D6CC(void)
{
}
INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_lower_sewer/shelter_b4_lower_sewer", RoomsShared8017d878Table);
