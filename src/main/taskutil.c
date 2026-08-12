#include "common.h"

#include "main/display.h"
#include "main/stage.h"
#include "main/unknown_syms.h"

const u16 Display_WidthTable[] = {
    0x100,
    0x140,
    0x180,
    0x200,
    0x280,
    0,
};

const u16 Display_HeightTable[] = {
    0xF0,
    0x1E0,
};

const TaskFuncTable6 D_80013E98 = { {
    Stage_WaitCdActivate,
    Stage_WaitCdAndSpawn,
    Display_TransitionTask,
    Display_TaskLoadStep,
    Stage_WaitCdEntry,
    Stage_FinishCdFollowUp,
} };

void Task_KillMaybeSpawn(Task* arg0)
{
    if (Display_State.field_112 != 0) {
        Task_SpawnFromTable((TaskDesc*)&D_80725C54, 0, 0, 0);
    }
    Task_Kill(arg0);
}
