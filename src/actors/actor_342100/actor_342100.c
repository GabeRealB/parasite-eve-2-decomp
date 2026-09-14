#include "common.h"

#include "actors/actor_342100.h"
#include "gameplay/D4.h"
#include "main/session.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

/// Single-entry, type-0 spawn table: `func_actor_342100_80163408` starts entry
/// 0 and hands it the address of `Actor342100Work::field_20`. The task's
/// callback takes that block back through `Task::spawnArg2` and works the
/// surrounding fields -- it reads `field_22` and zeroes the words at +0x4 and
/// +0x6 -- so the seeded pair and the table belong together.
extern TaskDesc D_actor_342100_801648DC;

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80161E70);

INCLUDE_RODATA("actors/nonmatchings/actor_342100/actor_342100", D_actor_342100_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162748);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_801629B8);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162AB0);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162C88);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162DDC);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162F54);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_801630A4);

void func_actor_342100_80163344(Actor342100* arg0, s32 arg1, s32 arg2)
{
    arg0->field_30 = arg2;
}

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_8016334C);

void func_actor_342100_801633D0(s32 arg0)
{
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->idMap;

    Gp_DispatchMsg(work->field_34, 0x7DB, arg0, 0);
}

/// Seed the spawn entry's two parameters and start the task that consumes
/// them, passing the block itself as `Task::spawnArg2`.
void func_actor_342100_80163408(void)
{
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->idMap;

    work->field_20 = 0x258;
    work->field_22 = 0x100;
    Task_SpawnFromTable(&D_actor_342100_801648DC, 0, 0, (s32)&work->field_20);
}

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80163454);

void func_actor_342100_80163518(void)
{
    D_80073BA0              = 0;
    Game_Session->field_128 = 3;
}
