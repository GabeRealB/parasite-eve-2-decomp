#include "common.h"

#include "main/task.h"
#include "main/tmd.h"
#include "main/sound.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_405800.h"

void func_actor_405800_801329C8(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, u8 arg5);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80131FC8);

void func_actor_405800_80132670(Task* arg0)
{
    Actor405800Work* work;

    work                   = (Actor405800Work*)arg0->idMap;
    work->obj_4B4.field_8  = &((TmdObject*)arg0->extra)->field_8[3];
    work->obj_4B4.field_C  = work->rec_4D4;
    work->obj_4B4.field_10 = 0;
    work->obj_4B4.field_12 = 0;
    work->obj_4B4.field_14 = 0x110;
    work->obj_4B4.field_18 = 0x3003A;
    work->obj_4B4.field_1C = 0x2F0;
    work->obj_4B4.flags    = 1;
    Gp_LinkObj(2, &work->obj_4B4);
    Gp_InitRec18Table(work->rec_4D4, 8, 0);
    work->obj_4B4.flags   |= 0x8000;
    work->obj_594.field_8  = ((TmdObject*)arg0->extra)->field_8;
    work->obj_594.field_C  = work->rec_5B4;
    work->obj_594.field_10 = 0;
    work->obj_594.field_12 = -0x220;
    work->obj_594.field_14 = 0;
    work->obj_594.field_18 = 0x3003A;
    work->obj_594.field_1C = 0x460;
    work->obj_594.flags    = 1;
    Gp_LinkObj(2, &work->obj_594);
    Gp_InitRec18Table(work->rec_5B4, 8, 0);
    work->rec_744.field_4  = 0xBB8;
    work->rec_744.field_10 = 0xA;
    work->rec_744.field_12 = 0xA;
    work->rec_744.field_0  = 0;
    work->rec_744.field_C  = 0;
    work->rec_744.field_8  = 0;
    work->rec_744.field_14 = work->rec_75C;
    work->obj_594.flags   |= 0x4000;
    work->obj_724.field_8  = ((TmdObject*)arg0->extra)->field_8;
    work->obj_724.field_C  = (GpRec18*)&work->rec_744;
    work->obj_724.field_10 = 0;
    work->obj_724.field_12 = -0x190;
    work->obj_724.field_14 = 0;
    work->obj_724.field_18 = 0x30005;
    work->obj_724.field_1C = 0;
    work->obj_724.flags    = 3;
    Gp_LinkObj(2, &work->obj_724);
    Gp_InitRec18Table(work->rec_75C, 8, 0);
    work->obj_724.flags   &= 0x3FFF;
    work->obj_6B4.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_6B4.field_8  = &((TmdObject*)arg0->extra)->field_8[7];
    work->obj_6B4.field_C  = work->rec_6F4;
    work->obj_6B4.field_10 = -0x460;
    work->obj_6B4.field_12 = 0;
    work->obj_6B4.field_14 = 0;
    work->obj_6B4.field_1C = 0x290;
    work->obj_6B4.flags    = 1;
    Gp_LinkObj(3, &work->obj_6B4);
    Gp_InitRec18Table(work->rec_6F4, 1, 0);
    work->obj_6B4.flags   &= 0x7FFF;
    work->obj_674.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_674.field_8  = &((TmdObject*)arg0->extra)->field_8[7];
    work->obj_674.field_C  = work->rec_6F4;
    work->obj_674.field_10 = -0x200;
    work->obj_674.field_12 = 0;
    work->obj_674.field_14 = 0;
    work->obj_674.field_1C = 0x250;
    work->obj_674.flags    = 1;
    Gp_LinkObj(3, &work->obj_674);
    Gp_InitRec18Table(work->rec_6F4, 1, 0);
    work->obj_674.flags   &= 0x7FFF;
    work->obj_6D4.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_6D4.field_8  = &((TmdObject*)arg0->extra)->field_8[10];
    work->obj_6D4.field_C  = work->rec_70C;
    work->obj_6D4.field_10 = 0x460;
    work->obj_6D4.field_12 = 0;
    work->obj_6D4.field_14 = 0;
    work->obj_6D4.field_1C = 0x290;
    work->obj_6D4.flags    = 1;
    Gp_LinkObj(3, &work->obj_6D4);
    Gp_InitRec18Table(work->rec_70C, 1, 0);
    work->obj_6D4.flags   &= 0x7FFF;
    work->obj_694.field_18 = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_694.field_8  = &((TmdObject*)arg0->extra)->field_8[10];
    work->obj_694.field_C  = work->rec_70C;
    work->obj_694.field_10 = 0x200;
    work->obj_694.field_12 = 0;
    work->obj_694.field_14 = 0;
    work->obj_694.field_1C = 0x250;
    work->obj_694.flags    = 1;
    Gp_LinkObj(3, &work->obj_694);
    Gp_InitRec18Table(work->rec_70C, 1, 0);
    work->obj_694.flags &= 0x7FFF;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801329C8);

void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2)
{
    func_actor_405800_801329C8(arg0, 3, 9, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 9, 0xA, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xA, 0xB, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 3, 6, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 6, 7, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 7, 8, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 5, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 0xC, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xC, 0xD, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xD, 0xE, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 0xF, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xF, 0x10, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0x10, 0x11, 0x100, arg1, arg2);
}

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

void func_actor_405800_80133CD0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              sound;
    s32              pan;

    if (((Actor405800Work*)arg0->idMap)->field_852 < 0x1450) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050004;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work = (Actor405800Work*)arg0->idMap;
        if (((s8)work->field_895 >= 0) || ((work->field_895 & 0x7F) != 1)) {
            work->field_895 = 0x81;
            work->field_896 = 0;
        }
        Gp_ArmStateF0(1);
        work2            = (Actor405800Work*)arg0->idMap;
        work2->field_846 = 0xC;
        work2->field_848 = 0;
    }
}

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
