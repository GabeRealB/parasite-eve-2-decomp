#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_405800.h"

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the loads that follow. The
 * one-element array is the remedy measured on `actor_400600`, where a
 * `SOFT_BARRIER()` was enough for a byte store but not for this pointer one. */
extern void* D_800678F0[1];

/* Model streams in this overlay's own data. */
extern u8 D_actor_405800_8013FB18[];
extern u8 D_actor_405800_8014086C[];
extern u8 D_actor_405800_80140F10[];
extern u8 D_actor_405800_80141430[];

/* Resolved through `configs/USA/sym/actors.imports.txt`. */
void func_8009EA50(s16 arg0);

/* Still `INCLUDE_ASM` in this unit. */
void func_actor_405800_80135E28(Task* arg0);
s32  func_actor_405800_80136A1C(Task* arg0);
s32  func_actor_405800_80136B94(Task* arg0);
void func_actor_405800_801383CC(Task* arg0, SVECTOR* arg1, s16 arg2);
s32  func_actor_405800_801385F4(Task* arg0);
void func_actor_405800_8013A0F4(Task* arg0);

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

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131E54);

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

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131EAC);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131EB8);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131ECC);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131EDC);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131EE8);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131EF4);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131F04);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80136388);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80136A1C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80136B94);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80136CE0);

void func_actor_405800_80136E14(Task* task)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    GpEffWork* eff4;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* dst4;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;
    TmdObject* src4;

    D_800678F0[0] = D_actor_405800_8013FB18;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[5], 0x200, NULL);
    if (eff != NULL) {
        src           = (TmdObject*)task->extra;
        dst           = (TmdObject*)eff->field_0->extra;
        dst->field_24 = src->field_24;
        dst->field_25 = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
    }
    D_800678F0[0] = D_actor_405800_8014086C;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[13], 0x200, NULL);
    if (eff2 != NULL) {
        src2           = (TmdObject*)task->extra;
        dst2           = (TmdObject*)eff2->field_0->extra;
        dst2->field_24 = src2->field_24;
        dst2->field_25 = src2->field_25;
        if (dst2->field_18 != NULL) {
            Tmd_ProcessStream(dst2);
            Tmd_ProcessStream(dst2);
        }
    }
    D_800678F0[0] = D_actor_405800_80140F10;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3           = (TmdObject*)task->extra;
        dst3           = (TmdObject*)eff3->field_0->extra;
        dst3->field_24 = src3->field_24;
        dst3->field_25 = src3->field_25;
        if (dst3->field_18 != NULL) {
            Tmd_ProcessStream(dst3);
            Tmd_ProcessStream(dst3);
        }
    }
    D_800678F0[0] = D_actor_405800_80141430;
    eff4          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4           = (TmdObject*)task->extra;
        dst4           = (TmdObject*)eff4->field_0->extra;
        dst4->field_24 = src4->field_24;
        dst4->field_25 = src4->field_25;
        if (dst4->field_18 != NULL) {
            Tmd_ProcessStream(dst4);
            Tmd_ProcessStream(dst4);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[3], 0x200, NULL);
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_8013706C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_8013728C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801373E0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801375C4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137908);

void func_actor_405800_80137948(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_88E = 0;
    work->field_88D = 0;
}

void func_actor_405800_8013795C(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    if (work->field_85C > 0) {
        work->field_85C = work->field_85C - 1;
    }
    if (work->field_85E > 0) {
        work->field_85E = work->field_85E - 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137994);

void func_actor_405800_801379F8(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_742 = work->field_742 & 0xBFFF;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137A14);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137A60);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137B34);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137B9C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137C04);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137C78);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137CEC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137D60);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137DE4);

void func_actor_405800_80137E64(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    s16              count;

    work = (Actor405800Work*)task->idMap;
    func_actor_405800_80137948(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0) {
        count           = work->field_87C - 1;
        work->field_87C = count;
        if (count == 0) {
            work2            = (Actor405800Work*)task->idMap;
            work2->field_846 = 0xB;
            work2->field_848 = 0;
            return;
        }
        func_actor_405800_801383CC(task, &work->field_A8, 0x18);
        func_actor_405800_80135E28(task);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137EF0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137F58);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80137FCC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138040);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801380C0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138154);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801381BC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138224);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138294);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_8013836C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801383CC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138478);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138514);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801385F4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138634);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138698);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138788);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801387DC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138854);

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

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801388E4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801389AC);

void func_actor_405800_80138A18(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    func_actor_405800_8013A0F4(task);
    if ((s16)func_actor_405800_801385F4(task) != 0) {
        work->field_846 = work->field_846 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138A70);

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

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138C30);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138CF0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138D54);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138E20);

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

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80138FA8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_8013902C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801390FC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139188);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139260);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801392EC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139358);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801393E8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801394E4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139550);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801395E8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_8013967C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139700);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801397B8);

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

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139928);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_801399C4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139AC4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139B3C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139BD8);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139C98);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139D24);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139DC0);

void func_actor_405800_80139E2C(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_848 = work->field_848 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139E48);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139EAC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139F0C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139F3C);

void func_actor_405800_80139FB0(Task* task, s16 arg1)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_846 = arg1;
    work->field_848 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_80139FC4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800", func_actor_405800_8013A0F4);

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
