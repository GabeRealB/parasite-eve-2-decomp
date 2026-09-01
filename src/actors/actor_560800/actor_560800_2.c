#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_actor_560800_801718F0;

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136930);

void func_actor_560800_801369A0(void)
{
    Display_SpawnWithOt(&D_actor_560800_801718F0, 0xD, 0, 0);
    D_8007106B = 2;
    Gp_SpawnViewTasks();
}

void func_actor_560800_801369E0(Task* arg0)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        Task_Kill(arg0);
        Display_ResetHeapWrapper();
    }
}

void func_actor_560800_80136A20(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(1);
    func_800E6D4C(0x180, 0);
}

void func_actor_560800_80136A54(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(2);
    func_800E6D4C(0x180, 0);
}
