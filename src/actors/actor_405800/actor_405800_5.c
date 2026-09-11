#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "actors/actor_405800.h"

void func_8009EA50(s16 arg0);
s32  func_actor_405800_80136B94(Task* arg0);
s32  func_actor_405800_801385F4(Task* arg0);
void func_actor_405800_8013A0F4(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138698);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138788);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801387DC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138854);

void func_actor_405800_801388C4(Task* task)
{
    ((Actor405800Work*)task->idMap)->field_88B = 1;
}

void func_actor_405800_801388D4(void)
{
}

void func_actor_405800_801388DC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801388E4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801389AC);

void func_actor_405800_80138A18(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    func_actor_405800_8013A0F4(task);
    if ((s16)func_actor_405800_801385F4(task) != 0) {
        work->field_846 = work->field_846 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138A70);

void func_actor_405800_80138B50(Task* task)
{
    Actor405800Work* work;
    TmdObject*       ext;
    u16              count;

    work            = (Actor405800Work*)task->idMap;
    ext             = (TmdObject*)task->extra;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 0x18) {
        work->field_832 = 0;
        work->field_834 = 0x1000;
        work->field_866 = 0xFF;
        func_8009EA50(work->field_832);
        ext->field_2C   = work->field_834;
        work->field_842 = 0;
        work->field_846 = work->field_846 + 1;
    }
}

void func_actor_405800_80138BD4(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    task->state     = 3;
    work->field_846 = 0;
    work->field_848 = 0;
}

void func_actor_405800_80138BEC(Task* task)
{
    Actor405800Work* work;
    u16              count;

    work            = (Actor405800Work*)task->idMap;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 2) {
        work->field_846 = work->field_846 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138C30);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138CF0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138D54);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138E20);

void func_actor_405800_80138EF0(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    if (work->field_824 != NULL) {
        Task_Kill(work->field_824);
    }
    if (work->field_828 != NULL) {
        Task_Kill(work->field_828);
    }
    work->field_842 = 0;
    work->field_846 = work->field_846 + 1;
}

void func_actor_405800_80138F54(Task* task)
{
    Actor405800Work* work;
    u16              count;

    work            = (Actor405800Work*)task->idMap;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 0x12D) {
        Gp_DestroyEnemy((GpEnemy*)task->spawnArg2, task);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138FA8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_8013902C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801390FC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139188);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139260);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801392EC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139358);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801393E8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801394E4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139550);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801395E8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_8013967C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139700);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801397B8);

void func_actor_405800_801397F0(Task* task)
{
    Actor405800Work* work;

    if (((s16)func_actor_405800_80136B94(task) == 0) && ((s16)func_actor_405800_801385F4(task) != 0)) {
        work            = (Actor405800Work*)task->idMap;
        work->field_846 = 2;
        work->field_848 = 0;
    }
}

void func_actor_405800_80139844(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work             = (Actor405800Work*)task->idMap;
    work->field_842  = 0;
    work2            = (Actor405800Work*)task->idMap;
    work2->field_850 = 0x10;
    work2->field_872 = 0x16;
    work2->field_86E = 2;
    work->field_848  = work->field_848 + 1;
}

void func_actor_405800_80139880(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_84A = 4;
    work->field_850 = 0x10;
    work->field_872 = 0x15;
    work->field_86E = 1;
    work->field_842 = 0;
    work->field_9A  = -0x9C4;
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_801398C0(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)task->idMap;
    if (((s16)func_actor_405800_80136B94(task) == 0) && ((s16)func_actor_405800_801385F4(task) != 0)) {
        work->field_85E  = 0x12C;
        work2            = (Actor405800Work*)task->idMap;
        work2->field_846 = 2;
        work2->field_848 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139928);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_801399C4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139AC4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139B3C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139BD8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139C98);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139D24);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139DC0);

void func_actor_405800_80139E2C(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_848 = work->field_848 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139E48);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139EAC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139F0C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139F3C);

void func_actor_405800_80139FB0(Task* task, s16 arg1)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_846 = arg1;
    work->field_848 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80139FC4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_8013A0F4);

void func_actor_405800_8013A1E0(Task* task, s16 arg1, s16 arg2)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_850 = arg2;
    work->field_872 = arg1;
    work->field_86E = 2;
}

void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_84A = arg3;
    work->field_850 = arg2;
    work->field_872 = arg1;
    work->field_86E = 1;
}
