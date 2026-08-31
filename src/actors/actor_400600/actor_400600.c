#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400600.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`; `D_80115417` is one byte of the run
 * of gameplay flags at 0x80115408..0x8011541B. Neither object's real aggregate
 * shape is known here, and both are deliberately declared as one-element arrays
 * rather than as a bare `void*` / `s8`: GCC 2.8.1's
 * `fixed_scalar_and_varying_struct_p` alias heuristic assumes a fixed-address
 * *scalar* never aliases a pointer-based *struct* reference, so with the scalar
 * form it sinks the store to this global past the `Actor400600Work` loads that
 * follow it and the instruction schedule stops matching. Referencing them
 * through an aggregate restores the dependence. */
extern void* D_800678F0[1];
extern s8    D_80115417[1];

extern s32 Gp_LcgState;

extern u8 D_actor_400600_8014220C[];
extern u8 D_actor_400600_80143604[];
extern u8 D_actor_400600_80143B24[];
extern u8 D_actor_400600_80144994[];

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2);

/* Still `INCLUDE_ASM` in this unit; `func_actor_400600_80139CAC` and
 * `func_actor_400600_8013A0B0` are called both with and without an argument,
 * so they keep an unprototyped declaration. */
void func_actor_400600_801361AC();
s32  func_actor_400600_80136FA8();
s32  func_actor_400600_801370F4();
void func_actor_400600_801387DC(Task* arg0, s32 arg1);
void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
void func_actor_400600_80139CAC();
s32  func_actor_400600_8013A0B0();
void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013203C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132294);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132704);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801328A8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801329EC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132B3C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132C70);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132E10);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80132F3C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133118);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801332F4);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E6C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E7C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131E9C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131EAC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133434);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801337A8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133B88);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133CB0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133E38);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80133FC0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134218);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134570);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013479C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134970);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134B98);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80134E28);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801350F4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135450);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135578);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801356E0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135998);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801361AC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136558);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136670);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F34);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F40);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F60);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F70);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F7C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F8C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F9C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136968);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136FA8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801370F4);

void func_actor_400600_80137240(Task* arg0)
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

    D_800678F0[0] = D_actor_400600_8014220C;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[4], 0x200, NULL);
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
    D_800678F0[0] = D_actor_400600_80143604;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
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
    D_800678F0[0] = D_actor_400600_80143B24;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[16], 0x200, NULL);
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
    D_800678F0[0] = D_actor_400600_80144994;
    eff4          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4           = (TmdObject*)arg0->extra;
        dst4           = (TmdObject*)eff4->field_0->extra;
        dst4->field_24 = src4->field_24;
        dst4->field_25 = src4->field_25;
        if (dst4->field_18 != NULL) {
            Tmd_ProcessStream(dst4);
            Tmd_ProcessStream(dst4);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137498);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801376EC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137840);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137AF0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137C34);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80137EF0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138224);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801383E4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801387DC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013886C);

s32 func_actor_400600_8013892C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if ((work->field_710.h.flags & 1) || (work->field_710.word & 0x01020000)) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013896C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138A24);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138AA4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138AB8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138AF0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138B40);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138B5C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138C34);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138D78);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138EA0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80138FD4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801390FC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139110);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139218);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139280);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801392E8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013935C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801393D0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139444);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801394E0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139560);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139608);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139670);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801396E4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139764);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801397E4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139878);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_801398E0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139948);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139A78);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139AE8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139BA0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139C00);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139CAC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139D98);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139DB0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139DCC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139E68);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139F4C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80139FE0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A0B0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A0F0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A170);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A26C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A2C0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A338);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A3A8);

void func_actor_400600_8013A3B8(void)
{
}

void func_actor_400600_8013A3C0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A3C8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A4AC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A518);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A570);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A638);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A6C4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A808);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A820);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A864);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A908);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013A990);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013AA5C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013AAD8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013AB44);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013AB98);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013AC14);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013AD3C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013ADA4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013AE88);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013AF04);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B018);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B0FC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B150);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B1DC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B2A8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B394);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B410);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B520);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B640);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B6F4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B740);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B830);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B8AC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013B984);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BA00);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BA6C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BAEC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BB88);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BBF4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BC68);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BCD8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BD54);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BDF0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BE58);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BE90);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BF48);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BF80);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013BFD4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C038);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C074);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C104);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C124);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C1C0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C238);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C2D4);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C394);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C410);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C4AC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C518);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C534);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C598);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C5F8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C6B0);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C7E8);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C874);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C940);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013C9DC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013CACC);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013CB40);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013CB70);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_8013CC04);
