#include "common.h"

#include "main/task.h"
#include "main/tmd.h"
#include "main/sound.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_405800.h"
#include "actors/actors_shared_8013a0b0.h"

extern s32 Gp_LcgState;
extern u8  D_actor_405800_801513F8[];

void func_actor_405800_801329C8(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, u8 arg5);
/* Unprototyped so the first jal keeps a nop delay slot; a0 still holds the task. */
s32  func_actor_405800_80136A1C();
void func_actor_405800_80135A3C(Task* arg0, s16 arg1);
s32  func_actor_405800_8013728C(Task* arg0);
s32  func_actor_405800_801373E0(Task* arg0);
void func_actor_405800_801379F8(Task* task);

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

void func_actor_405800_801340E0(Task* arg0)
{
    Actor405800Msg3FF msg;
    Actor405800Msg3F8 query;
    Actor405800Work*  work;
    Actor405800Work*  work2;
    Actor405800Work*  work3;
    s32               base;
    s32               sound;
    s32               pan;

    work = (Actor405800Work*)arg0->idMap;
    if (Gp_ActorSlots[0]->actor->field_954 == 2 || (func_actor_405800_8013728C(arg0) << 0x10) != 0) {
        func_actor_405800_801379F8(arg0);
        work3            = (Actor405800Work*)arg0->idMap;
        work3->field_846 = 2;
        work3->field_848 = 0;
        func_actor_405800_80135A3C(arg0, work->field_87E);
        return;
    }
    query.field_14 = 0x18;
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&query, 0) != 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        if (work->field_890 == 0) {
            work3            = (Actor405800Work*)arg0->idMap;
            work3->field_846 = 2;
            work3->field_848 = 0;
            return;
        }
        work2            = (Actor405800Work*)arg0->idMap;
        work2->field_846 = 0xD;
        work2->field_848 = 0;
        return;
    }
    work->field_86A = work->field_92;
    func_actor_405800_801379F8(arg0);
    work->field_890      = 0;
    Gp_StateC08.field_6 |= 1;
    work->field_88F      = 1;
    work->field_9A       = work->field_92;
    msg.field_0          = D_actor_405800_801513F8;
    msg.field_8          = 0;
    msg.field_C          = 0;
    msg.field_10         = 0;
    msg.field_4          = 4;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0xBFFF;
    work2                = (Actor405800Work*)arg0->idMap;
    work2->field_850     = 0x10;
    work2->field_872     = 0x21;
    work2->field_84A     = 4;
    work2->field_86E     = 1;
    work->field_842      = 0;
    work->field_844      = 0;
    base                 = 0x40050004;
    if ((arg0->spawnArg1 & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_884 = 0;
    work->field_848++;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80134314);

void func_actor_405800_8013471C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   player;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;

    work            = (Actor405800Work*)arg0->idMap;
    coord           = ((TmdObject*)arg0->extra)->field_8;
    player          = Gp_ActorSlots[0]->extra->field_8;
    work->field_84 += -(s16)work->field_84 >> 2;
    work->field_842++;
    if ((s16)work->field_842 >= 8) {
        work->obj_594.flags |= 0x4000;
        coord->coord.t[0]   += (s16)work->field_98;
        coord->coord.t[2]   += (s16)work->field_9C;
        work->obj_4B4.flags |= 0x8000;
        work->field_84C     += 6;
        work->field_84E     += work->field_84C;
        y                    = coord->coord.t[1] + work->field_84E;
        coord->coord.t[1]    = y;
        if (y >= work->field_9A) {
            coord->coord.t[1] = work->field_9A;
            player->flg       = 0;
            Gp_UpdateCoord(player);
            id = 0x40050003;
            if ((arg0->spawnArg1 & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
            pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
            work->obj_594.flags |= 0x4000;
            work2                = (Actor405800Work*)arg0->idMap;
            work2->field_84A     = 2;
            work2->field_872     = 0x19;
            work2->field_850     = 0x10;
            work2->field_86E     = 1;
            work->field_848++;
        }
    }
}

void func_actor_405800_801348E4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    GsCOORDINATE2*   coord;
    s16              v;

    work  = (Actor405800Work*)arg0->idMap;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    v     = func_actor_405800_8013728C(arg0);
    if (v != 0) {
        if (v < 0x4E9) {
            func_actor_405800_801379F8(arg0);
            work3            = (Actor405800Work*)arg0->idMap;
            work3->field_846 = 2;
            work3->field_848 = 0;
            return;
        }
        work->field_98 = (u16)coord->coord.t[0] + ((rsin((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12);
        work->field_9C = (u16)coord->coord.t[2] + ((rcos((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12);
    } else {
        work->field_98 = (u16)coord->coord.t[0] + ((rsin((s16)work->field_82 + 0x800) * 0x1770) >> 12);
        work->field_9C = (u16)coord->coord.t[2] + ((rcos((s16)work->field_82 + 0x800) * 0x1770) >> 12);
    }
    func_actor_405800_801379F8(arg0);
    work2            = (Actor405800Work*)arg0->idMap;
    work2->field_84A = 4;
    work2->field_850 = 0x10;
    work2->field_872 = 0x15;
    work2->field_86E = 1;
    work->field_84C  = -0x2A;
    work->field_84E  = 0;
    work->field_842  = 0;
    work->field_848++;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80134A64);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80134C00);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80134E80);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801351BC);

void func_actor_405800_80135558(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    u32              sound;
    s32              pan;
    u32              rnd;

    work = (Actor405800Work*)arg0->idMap;
    if ((s16)work->field_842 == 0) {
        work->field_88F = 0;
        sound           = 0x40050006 | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan             = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan           >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_842++;
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        if (work->field_85A != 3) {
            work2            = (Actor405800Work*)arg0->idMap;
            work2->field_84A = 2;
            work2->field_850 = 0x10;
            work2->field_872 = 0x14;
            work2->field_86E = 1;
            work->field_848++;
            return;
        }
        work->field_85A  = 0;
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_87C  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work3            = (Actor405800Work*)arg0->idMap;
        work3->field_846 = 5;
        work3->field_848 = 0;
    }
}

void func_actor_405800_801356A8(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    u16              count;
    u32              rnd;

    work = (Actor405800Work*)arg0->idMap;
    if (((func_actor_405800_80136A1C() << 0x10) == 0) && ((func_actor_405800_801373E0(arg0) << 0x10) == 0)) {
        count           = (u16)work->field_882 - 1;
        work->field_882 = count;
        if ((count << 0x10) == 0) {
            rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
            work->field_838 = ((rnd >> 0x10) & 0x3F) + 0x1E;
            work2           = (Actor405800Work*)arg0->idMap;
            Gp_LcgState     = rnd;
            if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
                work2->field_895 = 0x81;
                work2->field_896 = 0;
            }
            work3            = (Actor405800Work*)arg0->idMap;
            work3->field_846 = 2;
            work3->field_848 = 0;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80135780);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80135A3C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80135E28);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801361F8);
