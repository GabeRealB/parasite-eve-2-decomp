#include "common.h"

#include "actors/actor_121300.h"
#include "main/fs.h"
#include "main/task.h"

extern TaskDesc ActorsShared80136280Desc;

extern Task* D_actor_121300_8013D418;
extern s16   D_actor_121300_8013D41C;

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300_2", func_actor_121300_801341A8);

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
