#include "common.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
extern TaskDesc D_actor_120300_80141B6C;

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80131EE0);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132004);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801321C8);

INCLUDE_RODATA("actors/nonmatchings/actor_120300/actor_120300", D_actor_120300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132338);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132C60);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133330);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801334A4);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801335D8);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801337C4);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133B5C);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133C38);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133C6C);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133D04);

void func_actor_120300_80133DA4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
    Game_Session->field_52 = 1;
}

void func_actor_120300_80133DD4(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_actor_120300_80133DF4(void)
{
    Gp_RestoreStreamRng();
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133E14);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133E34);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133E54);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133E94);

void func_actor_120300_80133EE4(void)
{
    Task_SpawnFromTable(&D_actor_120300_80141B6C, 4, 9, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80133F14);
