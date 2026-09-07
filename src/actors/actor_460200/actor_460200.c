#include "common.h"

#include "actors/actors_shared_80132514.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern s8 D_8007272D;

extern s32 D_actor_460200_80137AA0;
extern s32 D_actor_460200_80137BA8;
extern s32 D_actor_460200_80137CB0;
extern s32 D_actor_460200_80137DA0;
extern s32 D_actor_460200_80137F98;
extern s32 D_actor_460200_80137FE0;
extern s32 D_actor_460200_80138028;
extern s32 D_actor_460200_80138070;

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80131E2C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80131FB0);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132090);

void func_actor_460200_801320E0(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(arg0);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132124);

void func_actor_460200_80132204(s8 arg0)
{
    D_8007272D = arg0;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132210);

void func_actor_460200_801322B8(void)
{
    switch (GameFlag_GetNibble(0x114)) {
        case 0:
            func_800E8614((s32)&D_actor_460200_80137AA0, 0);
            GameFlag_SetNibble(0x114, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_460200_80137BA8, 0);
            GameFlag_SetNibble(0x114, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_460200_80137CB0, 0);
            GameFlag_SetNibble(0x114, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_460200_80137DA0, 0);
            break;
    }
}

void func_actor_460200_80132390(void)
{
    switch (GameFlag_GetNibble(0x115)) {
        case 0:
            func_800E8614((s32)&D_actor_460200_80137F98, 0);
            GameFlag_SetNibble(0x115, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_460200_80137FE0, 0);
            GameFlag_SetNibble(0x115, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_460200_80138028, 0);
            GameFlag_SetNibble(0x115, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_460200_80138070, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132468);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801325FC);

void func_actor_460200_80132808(void* enemy, Task* task);
void func_actor_460200_80132468(void* enemy, Task* task);

void func_actor_460200_801327B4(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_460200_80132808, func_actor_460200_80132468 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132808);

void func_actor_460200_80132950(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132978);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132A04);

void func_actor_460200_80132A50(Task* task)
{
    ActorsShared80132514Work* work;
    s32                       i;

    work = (ActorsShared80132514Work*)task->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132AC8);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132B2C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132B98);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132C14);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132C8C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132CAC);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132D74);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132F0C);

void func_actor_460200_80132D74(void* enemy, Task* task);
void func_actor_460200_8013311C(void* enemy, Task* task);

void func_actor_460200_801330C8(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_460200_80132D74, func_actor_460200_8013311C };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_8013311C);

void func_actor_460200_8013322C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133254);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801332E0);

void func_actor_460200_8013332C(Task* task)
{
    ActorsShared80132514Work* work;
    s32                       i;

    work = (ActorsShared80132514Work*)task->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801333A4);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133408);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80133474);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801334F0);

INCLUDE_RODATA("actors/nonmatchings/actor_460200/actor_460200", D_actor_460200_80131E20);
