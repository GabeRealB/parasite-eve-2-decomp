#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400500.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `TmdObject` loads that
 * follow. Declared as a scalar, `func_actor_400500_80134B88` scores 87.27%
 * (12 register and 8 reorder penalties); as a one-element array it is exact,
 * the same remedy `actor_400600` needed for the same global. */
extern void* D_800678F0[1];

/* Model streams in the overlay's own `.data`, selected through `D_800678F0`. */
extern u8 D_actor_400500_8014393C[];
extern u8 D_actor_400500_80143F40[];
extern u8 D_actor_400500_80144624[];

void func_8009EA50(s32 arg0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132000);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013226C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132438);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132628);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132AB0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132C54);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132D74);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132E94);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133160);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133358);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133460);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801335E8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133B14);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013403C);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013456C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801348D8);

void func_actor_400500_80134B88(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;

    D_800678F0[0] = D_actor_400500_8014393C;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
    if (eff != NULL) {
        src           = (TmdObject*)arg0->extra;
        dst           = (TmdObject*)eff->field_0->extra;
        dst->field_24 = src->field_24;
        dst->field_25 = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
    }
    D_800678F0[0] = D_actor_400500_80143F40;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    if (eff2 != NULL) {
        src2           = (TmdObject*)arg0->extra;
        dst2           = (TmdObject*)eff2->field_0->extra;
        dst2->field_24 = src2->field_24;
        dst2->field_25 = src2->field_25;
        if (dst2->field_18 != NULL) {
            Tmd_ProcessStream(dst2);
            Tmd_ProcessStream(dst2);
        }
    }
    D_800678F0[0] = D_actor_400500_80144624;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    if (eff3 != NULL) {
        src3           = (TmdObject*)arg0->extra;
        dst3           = (TmdObject*)eff3->field_0->extra;
        dst3->field_24 = src3->field_24;
        dst3->field_25 = src3->field_25;
        if (dst3->field_18 != NULL) {
            Tmd_ProcessStream(dst3);
            Tmd_ProcessStream(dst3);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80134D6C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135414);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131E4C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135770);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135EBC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801361EC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013662C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136864);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801369A4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136B94);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136D00);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136EB8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80137034);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801371A0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80137338);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80137478);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801375B8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013771C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138088);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131EE4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801385D0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801387E8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013899C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138B78);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138CE8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138DC4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138EA0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013905C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801391B0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801392D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139448);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801395D0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013973C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139AC4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139C1C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139D70);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139F6C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A0B8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A484);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A5D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A700);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A8E4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AA98);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013ABE4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AD60);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AF44);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B228);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B374);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B4A4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B5E0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B720);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B920);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BA24);

void func_actor_400500_8013BAA4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;

    work = (Actor400500Work*)arg0->idMap;
    if (work->field_A40 == 4) {
        work->field_A42 = 0;
        return;
    }
    if (!(work->field_A1E & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->idMap;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 1;
        work2->field_9FA = 2;
        work->field_A04  = 0;
        work->field_A18  = 0;
        work->field_A10  = 0;
        work->field_A12  = 0;
        work->field_A0A  = work->field_A0A + 1;
        return;
    }
    work->field_A42 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BB18);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BBB0);

void func_actor_400500_8013BC9C(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    work->field_A42 = 1;
    work->field_A04 = 0;
    work->field_A18 = 0;
    work->field_A10 = 0;
    work->field_A12 = 0;
    work->field_A0A = work->field_A0A + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BCCC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BD64);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BE50);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BEC4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BFB0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C018);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C108);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C174);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C218);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C348);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C3C4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C474);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C508);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C578);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C61C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C750);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C7A4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C818);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C908);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C9D4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CA38);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CB0C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CBD8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CCDC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CDA8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CE9C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CF68);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D078);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D144);

void func_actor_400500_8013D210(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GsCOORDINATE2*   coord;

    work              = (Actor400500Work*)arg0->idMap;
    coord             = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work->field_94C   = 0x800;
    work->field_94A   = 0;
    coord->coord.t[0] = 0x4074;
    coord->coord.t[1] = -0xFA0;
    coord->coord.t[2] = -0x2710;
    work->field_A04   = 0;
    work2             = (Actor400500Work*)arg0->idMap;
    work2->field_9F8  = 4;
    work2->field_9FE  = 1;
    work2->field_9FA  = 2;
    work->field_A08   = work->field_A08 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D274);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D2D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D3B8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D420);

void func_actor_400500_8013D4F0(Task* arg0)
{
    GpEnemy*         enemy;
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->idMap;
    if (enemy->field_40 > 0) {
        if (Gp_TickObjFlag2((GpObj5D*)enemy) != 0) {
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->idMap;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 0x10;
                work2->field_9FA = 2;
            } else {
                work3            = (Actor400500Work*)arg0->idMap;
                work3->field_9F8 = 0x10;
                work3->field_9FE = 0x12;
                work3->field_9FA = 2;
            }
            work->field_A08 = 2;
        }
    } else {
        work->field_A42 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D59C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D630);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D6A0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D744);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D878);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D8CC);

void func_actor_400500_8013D958(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    u16              frame;

    work            = (Actor400500Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 0x18) {
        work->field_A20 = 0;
        work->field_A24 = 0x1000;
        work->field_A28 = 0xFF;
        func_8009EA50(work->field_A20);
        model->field_2C = work->field_A24;
        work->field_A04 = 0;
        work->field_A06 = work->field_A06 + 1;
    }
}

void func_actor_400500_8013D9DC(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    arg0->state     = 3;
    work->field_A06 = 0;
    work->field_A08 = 0;
}

void func_actor_400500_8013D9F4(Task* arg0)
{
    TmdObject*       model;
    Actor400500Work* work;

    model           = (TmdObject*)arg0->extra;
    work            = (Actor400500Work*)arg0->idMap;
    model->field_C |= 0x80;
    work->field_A04 = 0;
    work->field_A28 = 0;
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013DA24(Task* arg0)
{
    Actor400500Work* work;
    u16              frame;

    work            = (Actor400500Work*)arg0->idMap;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 2) {
        work->field_A06 = work->field_A06 + 1;
    }
}

void func_actor_400500_8013DA68(Task* arg0)
{
    TmdObject*       model;
    Actor400500Work* work;

    model = (TmdObject*)arg0->extra;
    work  = (Actor400500Work*)arg0->idMap;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    func_actor_400500_80134B88(arg0);
    work->field_A06 = work->field_A06 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DACC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DAE4);

void func_actor_400500_8013DB64(Task* arg0, s16 arg1)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    work->field_A06 = arg1;
    work->field_A08 = 0;
}

s32 func_actor_400500_8013DB78(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    if ((work->field_A16 < (0x640 - (work->field_9DC * 8))) && ((u32)(work->field_A36 - 0x300) >= 0xA01U)) {
        work->field_A06 = 9;
        work->field_A08 = 0;
        return 1;
    }
    return 0;
}

void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2)
{
    MATRIX         local;
    GsCOORDINATE2* coord;

    coord = &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local);
    arg2->x    = local.t[0];
    arg2->z    = local.t[2];
    coord->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DC4C);

void func_actor_400500_8013DCBC(Task* arg0, s16 arg1, s16 arg2)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    work->field_9F8 = arg2;
    work->field_9FE = arg1;
    work->field_9FA = 2;
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DCD4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DD8C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DDEC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DE2C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DE98);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DEFC);

void func_actor_400500_8013DF50(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->idMap;

    work->field_A3C = 0;
    work->field_A3E = 0;
}

void func_actor_400500_8013DF64(void)
{
}

void func_actor_400500_8013DF6C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DF74);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DFE4);
