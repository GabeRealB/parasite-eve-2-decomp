#include "common.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136930);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801369A0);

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

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136A88);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136AA8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801376E0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80137820);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80137BEC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80137F58);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801384EC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801386D4);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80138A4C);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80138BCC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80138D04);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80138FC8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80139360);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801393EC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80139440);
