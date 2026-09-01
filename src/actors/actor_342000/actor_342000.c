#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "actors/actor_342000.h"
INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80161EA4);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_8016201C);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162158);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_801625D8);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_801628C8);

INCLUDE_RODATA("actors/nonmatchings/actor_342000/actor_342000", D_actor_342000_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162BBC);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162F28);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_8016382C);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80163EAC);

void func_actor_342000_80163F88(Task* task)
{
    Actor342000Work* work;
    GsCOORDINATE2*   coord;

    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    work  = (Actor342000Work*)task->idMap;

    coord->sub = work->field_2A4;
    Task_Kill(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80163FB8);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80164034);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_801640C0);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80164110);

extern s8             D_8007216D;
extern GpAreaApplyRec D_8018FB6C[];

void func_actor_342000_80164154(void)
{
    Game_Session->field_5        = 7;
    D_8007216D                   = 7;
    Game_Session->unknown_133[1] = 6;
    Game_Session->unknown_133[0] = 1;
    Game_Session->field_76       = 1;
    Gp_ApplyAreaRecs(D_8018FB6C);
}

void func_actor_342000_801641B4(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    if (work->field_50 != NULL) {
        Task_CallExit(work->field_50);
    }
    work->field_50 = NULL;
}

void func_actor_342000_801641FC(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    if (work->field_5C != NULL) {
        Task_Kill(work->field_5C);
    }
    if (work->field_60 != NULL) {
        Task_Kill(work->field_60);
    }
    work->field_5C = NULL;
    work->field_60 = NULL;
}

void func_actor_342000_80164260(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    if (work->field_7A == 0) {
        SndEvt_EnqueueType6(0x54280005, 0, 0);
        work->field_7A = 1;
    }
}

void func_actor_342000_801642B4(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    work->field_68 = arg0;
    work->field_6A = 0;
}

void func_actor_342000_801642D4(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    work->field_70 = arg0;
    work->field_72 = 0;
}

extern s8 D_8007272D;

void func_actor_342000_801642F4(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    if (work->field_7C == 0) {
        Gp_StateF0.field_6      = 0;
        Gp_StateF0.field_1      = 0xF;
        Gp_StateF0.field_0      = 0;
        Gp_StateF0.field_2      = 0;
        Gp_StateF0.field_3      = 0;
        Game_Session->field_69 |= 0x80;
        D_8007272D              = 0xD;
        work->field_7C          = 1;
    }
}

void func_actor_342000_80164364(s32 arg0)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    Gp_DispatchMsg(work->field_48, 0x3F3, arg0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_8016439C);
