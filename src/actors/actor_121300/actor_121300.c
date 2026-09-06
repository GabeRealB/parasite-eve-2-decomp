#include "common.h"

#include "actors/actor_121300.h"
#include "main/fs.h"
#include "main/task.h"

extern TaskDesc ActorsShared80136280Desc;

extern Task* D_actor_121300_8013D418;
extern s16   D_actor_121300_8013D41C;

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80131EB0);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_801326EC);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80132818);

INCLUDE_RODATA("actors/nonmatchings/actor_121300/actor_121300", D_actor_121300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013293C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133064);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013322C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013343C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133580);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133730);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133854);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133BFC);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133D98);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013400C);

void func_actor_121300_801340F0(void)
{
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013411C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_801341A8);

void func_actor_121300_80134224(s32 arg0, s32 arg1, s32 arg2)
{
    Task_SpawnFromTable(&ActorsShared80136280Desc, 0xA, arg2, arg0);
}

void func_actor_121300_80134250(s16 arg0)
{
    Actor121300Work* work = (Actor121300Work*)D_actor_121300_8013D418->idMap;

    work->field_498 = arg0;
    work->field_49A = 0;
}

void func_actor_121300_80134270(void)
{
    CdCmd_Queue.field_22A = 0;
}

void func_actor_121300_8013427C(void)
{
    Actor121300Work* work = (Actor121300Work*)D_actor_121300_8013D418->idMap;

    D_actor_121300_8013D41C = 0;
    work->field_480         = 2;
    CdCmd_Queue.field_22A   = 0;
    Gp_DispatchMsg(work->field_488, 0x3F3, 1, 0);
    CdCmd_CancelReplaceAndActivate();
}
