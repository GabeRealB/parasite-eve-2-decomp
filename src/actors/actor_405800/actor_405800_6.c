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
#include "actors/actors_shared_80139948.h"
#include "actors/actors_shared_8013a0b0.h"

extern u8  D_actor_405800_801514D8[];
extern s32 Gp_LcgState;

void func_actor_405800_80135A3C(Task* arg0, s16 arg1);
s32  func_actor_405800_80136A1C(Task* arg0);
s32  func_actor_405800_80136B94(Task* arg0);
void func_actor_405800_80136E14(Task* arg0);
void func_actor_405800_8013706C(Task* arg0, s16 arg1);
void func_actor_405800_80137994(Task* arg0, s16 arg1);
void func_actor_405800_801379F8(Task* task);
void func_actor_405800_80139F0C(Task* task, u8 arg1);
void func_actor_405800_80139FB0(Task* task, s16 arg1);
void func_actor_405800_8013A0F4(Task* arg0);
void func_actor_405800_8013A1E0(Task* task, s16 arg1, s16 arg2);
void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_80138854);

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

void func_actor_405800_801388E4(Task* task)
{
    GpEnemy*         enemy;
    Actor405800Work* work;
    TmdObject*       model;

    enemy                = (GpEnemy*)task->spawnArg2;
    work                 = (Actor405800Work*)task->idMap;
    model                = (TmdObject*)task->extra;
    work->obj_6D4.flags &= 0x7FFF;
    work->obj_6B4.flags &= 0x7FFF;
    work->obj_694.flags &= 0x7FFF;
    work->obj_674.flags &= 0x7FFF;
    Gp_UnlinkNode(&enemy->node);
    if (work->field_85A == 4) {
        work->field_842 = 0;
        model->field_C |= 0x80;
        func_actor_405800_80139FB0(task, 7);
    } else if (work->field_890 == 0) {
        work->field_846 = work->field_846 + 1;
    } else {
        func_actor_405800_80139FB0(task, 9);
    }
}

void func_actor_405800_801389AC(Task* task)
{
    Actor405800Work* work = (Actor405800Work*)task->idMap;

    Gp_ReleaseStateF0Add((GpObj20E*)task, 0);
    func_actor_405800_8013A1E0(task, D_actor_405800_801514D8[work->field_872], 0x10);
    func_actor_405800_8013A0F4(task);
    work->field_846 = work->field_846 + 1;
}

void func_actor_405800_80138A18(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    func_actor_405800_8013A0F4(task);
    if ((s16)ActorsShared8013a0b0(task) != 0) {
        work->field_846 = work->field_846 + 1;
    }
}

void func_actor_405800_80138A70(Task* task)
{
    Actor405800Work* work  = (Actor405800Work*)task->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->field_8;

    ((GpEnemy*)task->spawnArg2)->field_54 = 0;
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_6B4);
    Gp_UnlinkObj(&work->obj_6D4);
    Gp_UnlinkObj(&work->obj_674);
    Gp_UnlinkObj(&work->obj_694);
    Gp_UnlinkObj(&work->obj_724);
    work->flags_83C.h.field_83E = 0x1000;
    work->matrix_0              = coord->coord;
    Gp_SetLightMode((GpObj4C*)task->spawnArg2, 1);
    work->field_842 = 0;
    work->field_846++;
}

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

void func_actor_405800_80138C30(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)task->extra;
    work  = (Actor405800Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    func_actor_405800_80136E14(task);
    work->field_866 = 0;
    Gp_ReleaseStateF0Add((GpObj20E*)task, 0);
    enemy->field_54 = 0;
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_6B4);
    Gp_UnlinkObj(&work->obj_6D4);
    Gp_UnlinkObj(&work->obj_674);
    Gp_UnlinkObj(&work->obj_694);
    Gp_UnlinkObj(&work->obj_724);
    work2            = (Actor405800Work*)task->idMap;
    task->state      = 3;
    work2->field_846 = 0;
    work2->field_848 = 0;
}

void func_actor_405800_80138CF0(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    func_actor_405800_8013A1F8(task, 9, 0x10, 2);
    work->field_84C = 0;
    work->field_84E = 0;
    work->field_86A = work->field_92;
    func_actor_405800_8013A0F4(task);
    work->field_846 = work->field_846 + 1;
}

void func_actor_405800_80138D54(Task* task)
{
    Actor405800Work* work;
    GsCOORDINATE2*   coord;

    work               = (Actor405800Work*)task->idMap;
    coord              = ((TmdObject*)task->extra)->field_8;
    work->field_84C   += 2;
    work->field_84E   += work->field_84C;
    coord->coord.t[1] += work->field_84E;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1] = (s16)work->field_92;
        func_actor_405800_8013A1E0(task, 0x13, 0x10);
        work->field_84 += 0x800;
        ActorsShared80139948(task);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_842 = 0;
        work->field_890 = 0;
        work->field_891 = 1;
        work->field_846++;
    }
    func_actor_405800_8013A0F4(task);
}

void func_actor_405800_80138E20(Task* task)
{
    Actor405800Work* work;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)task->idMap;
    if ((s16)work->field_842 == 0) {
        sound   = ((GpEnemy*)task->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050006;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
        work->field_842++;
    }
    if ((ActorsShared8013a0b0(task) << 0x10) != 0) {
        func_actor_405800_80139FB0(task, 1);
    }
    func_actor_405800_8013A0F4(task);
}

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

void func_actor_405800_80138FA8(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    if (((s8)work->field_895 >= 0 || (work->field_895 & 0x7F)) && work->field_83A == 0) {
        work->field_895 = 0x80;
        work->field_896 = 0;
    }
    work->field_880 = 0x18;
    work->field_893 = 0;
    work->field_87E = 0x10;
    func_actor_405800_80135A3C(task, 0x10);
    work->field_848 = work->field_848 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_8013902C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_801390FC);

void func_actor_405800_80139188(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->idMap;
    if (work->field_858 != 0 && work->field_85A == 1) {
        if (work->field_891 == 0) {
            work->field_850 = 0x20;
            work->field_872 = 9;
            work->field_86E = 2;
        } else {
            work->field_850 = 0x20;
            work->field_872 = 0xB;
            work->field_86E = 2;
        }
        return;
    }
    if ((func_actor_405800_80136A1C(arg0) << 0x10) == 0 && (ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->idMap;
            work2->field_846 = 2;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->idMap;
            work3->field_846 = 0xA;
            work3->field_848 = 0;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_80139260);

void func_actor_405800_801392EC(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->idMap;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->idMap;
            work2->field_846 = 2;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->idMap;
            work3->field_846 = 0xA;
            work3->field_848 = 0;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_80139358);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_801393E8);

void func_actor_405800_801394E4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->idMap;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_898 = 0;
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->idMap;
            work2->field_846 = 2;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->idMap;
            work3->field_846 = 0xA;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_80139550(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work            = (Actor405800Work*)task->idMap;
    work->field_84A = 8;
    work->field_850 = 0x10;
    work->field_872 = 7;
    work->field_86E = 1;
    work->field_842 = 0;
    func_actor_405800_80139F0C(task, 0);
    work2 = (Actor405800Work*)task->idMap;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_801395E8(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work            = (Actor405800Work*)task->idMap;
    work->field_84A = 8;
    work->field_850 = 0x10;
    work->field_872 = 8;
    work->field_86E = 1;
    work->field_842 = 0;
    func_actor_405800_80139F0C(task, 1);
    work2 = (Actor405800Work*)task->idMap;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_8013967C(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)task->idMap;
    func_actor_405800_8013706C(task, 0);
    work->field_88B = 0;
    work2           = (Actor405800Work*)task->idMap;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    func_actor_405800_80135A3C(task, work->field_87E);
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_80139700(Task* task)
{
    Actor405800Work* work = (Actor405800Work*)task->idMap;
    Actor405800Work* work2;

    work->field_84 += -(s16)work->field_84 >> 2;
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
        if (work->field_88C == 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        }
        func_actor_405800_80137994(task, 0x3C);
        work->field_84   = 0;
        work2            = (Actor405800Work*)task->idMap;
        work2->field_846 = 2;
        work2->field_848 = 0;
        work->field_88F  = 0;
    }
}

void func_actor_405800_801397B8(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    func_actor_405800_8013706C(task, 1);
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_801397F0(Task* task)
{
    Actor405800Work* work;

    if (((s16)func_actor_405800_80136B94(task) == 0) && ((s16)ActorsShared8013a0b0(task) != 0)) {
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
    if (((s16)func_actor_405800_80136B94(task) == 0) && ((s16)ActorsShared8013a0b0(task) != 0)) {
        work->field_85E  = 0x12C;
        work2            = (Actor405800Work*)task->idMap;
        work2->field_846 = 2;
        work2->field_848 = 0;
    }
}

void func_actor_405800_80139928(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GsCOORDINATE2*   coord;
    u16              next;

    work  = (Actor405800Work*)task->idMap;
    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    func_actor_405800_801379F8(task);
    work2            = (Actor405800Work*)task->idMap;
    work2->field_84A = 4;
    work2->field_850 = 0x10;
    work2->field_872 = 0x20;
    work2->field_86E = 1;
    work->field_84C  = 0x40;
    work->field_84E  = 0;
    work->field_842  = 0;
    work->field_98   = coord->coord.t[0];
    next             = work->field_848;
    work->field_9C   = coord->coord.t[2];
    work->field_88F  = 1;
    work->field_848  = next + 1;
}

void func_actor_405800_801399C4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              sound;
    s32              id;
    s32              pan;

    work = (Actor405800Work*)arg0->idMap;
    if ((s16)work->field_842 == 0) {
        id = 0x40050003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_88F = 0;
        work->field_842++;
    }
    if ((func_actor_405800_80136B94(arg0) << 0x10) == 0 && (ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_85E  = 0x12C;
        work2            = (Actor405800Work*)arg0->idMap;
        work2->field_846 = 2;
        work2->field_848 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_80139AC4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_80139B3C);

void func_actor_405800_80139BD8(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              rnd;

    work = (Actor405800Work*)arg0->idMap;
    if ((func_actor_405800_80136B94(arg0) << 0x10) != 0) {
        rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState     = rnd;
        work->field_87C = ((rnd >> 0x10) & 0x7F) + 0x1E;
    } else if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_87C  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work2            = (Actor405800Work*)arg0->idMap;
        work2->field_846 = 0xA;
        work2->field_848 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_6", func_actor_405800_80139C98);

void func_actor_405800_80139D24(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->idMap;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->idMap;
            work2->field_84A = 0x1E;
            work2->field_850 = 0x10;
            work2->field_872 = 0x10;
            work2->field_86E = 1;
        } else {
            work3            = (Actor405800Work*)arg0->idMap;
            work3->field_84A = 0x1E;
            work3->field_850 = 8;
            work3->field_872 = 0x14;
            work3->field_86E = 1;
        }
        work->field_848 = work->field_848 + 1;
    }
}

void func_actor_405800_80139DC0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->idMap;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->idMap;
            work2->field_846 = 2;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->idMap;
            work3->field_846 = 0xA;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_80139E2C(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_80139E48(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->idMap;
    func_actor_405800_801379F8(arg0);
    if (work->field_852 > 2000) {
        work2            = (Actor405800Work*)arg0->idMap;
        work2->field_846 = 0xD;
        work2->field_848 = 0;
    } else {
        work3            = (Actor405800Work*)arg0->idMap;
        work3->field_846 = 8;
        work3->field_848 = 0;
    }
}

void func_actor_405800_80139EAC(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->idMap;
    u32              rnd;

    work->field_84A = 4;
    work->field_850 = 0x10;
    work->field_872 = 1;
    work->field_86E = 1;
    rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState     = rnd;
    work->field_882 = ((rnd >> 0x10) & 0x3F) + 0x5A;
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_80139F0C(Task* task, u8 arg1)
{
    Actor405800Work* work = (Actor405800Work*)task->idMap;
    s32              mode = arg1;

    if (mode == 0) {
        work->field_88E = 1;
    } else if (mode == 1) {
        work->field_88D = mode;
    }
}

s32 func_actor_405800_80139F3C(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->idMap;

    if (work->field_858 == 1 && work->field_890 == 0) {
        switch (work->field_85A) {
            case 3:
                work->field_846 = 5;
                work->field_848 = 0;
                work->field_85A = 0;
                return 1;
            case 5:
                work->field_846 = 0xF;
                work->field_848 = 0;
                work->field_85A = 0;
                return 1;
        }
    }
    work->field_85A = 0;
    return 0;
}

void func_actor_405800_80139FB0(Task* task, s16 arg1)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_846 = arg1;
    work->field_848 = 0;
}
