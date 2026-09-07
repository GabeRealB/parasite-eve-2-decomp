#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_405800.h"

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80131FC8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80132670);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801329C8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80132E3C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80132FE0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_8013315C);

void func_actor_405800_8013340C(Task* arg0)
{
    GpEnemy*         enemy;
    Actor405800Work* work;
    s16              hp;
    s32              maxHp;
    s32              quarter;

    enemy   = (GpEnemy*)arg0->spawnArg2;
    hp      = enemy->field_40;
    work    = (Actor405800Work*)arg0->idMap;
    maxHp   = enemy->field_42 << 0x10;
    quarter = maxHp >> 0x12;
    if ((quarter + (maxHp >> 0x11)) < hp) {
        work->field_836 = 0x10;
        work->field_838 = 0;
        return;
    }
    if (quarter < hp) {
        work->field_836 = 0x20;
        work->field_838 = 0x40;
        return;
    }
    if ((maxHp >> 0x13) < hp) {
        work->field_836 = 0x30;
        work->field_838 = 0x80;
        return;
    }
    if ((maxHp >> 0x14) < hp) {
        work->field_836 = 0x40;
        work->field_838 = 0xC0;
        return;
    }
    work->field_836 = 0x50;
    work->field_838 = 0x100;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801334B8);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", ActorsShared801328ccTable);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80133800);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80133CD0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80133DB0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80133F48);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801340E0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80134314);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_8013471C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801348E4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80134A64);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80134C00);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80134E80);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801351BC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80135558);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801356A8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80135780);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80135A3C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80135E28);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801361F8);
