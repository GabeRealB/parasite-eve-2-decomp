#include "common.h"

#include "psyq/inline_c.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400600.h"
#include "actors/actors_shared_80139948.h"
#include "actors/actors_shared_80139c00.h"
#include "actors/actors_shared_80139dcc.h"
#include "actors/actors_shared_8013a0b0.h"
#include "actors/actors_shared_8016a538.h"
#include "actors/actor_400600_anim.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`; `D_80115417` is one byte of the run
 * of gameplay flags at 0x80115408..0x8011541B.
 *
 * Storing to a bare `extern` global next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `Actor400600Work` loads
 * that follow. Two remedies work and which one is needed was measured, not
 * chosen: the byte store to `D_80115417` matches with `SOFT_BARRIER()` after
 * it, so that one is declared as the scalar it is; the pointer store to
 * `D_800678F0` checksums wrong with the barrier and matches only as an
 * aggregate, so its one-element array stays and is doing real work.
 * `D_80115414`, from the same flag run, is declared as the aggregate
 * `actor_400600_7.c` needs, which matches here too. */
extern void* D_800678F0[1];
extern s8    D_80115414[1];
extern s8    D_80115417;

extern s32 Gp_LcgState;

extern u8 D_801153F4;

extern s32 D_80115738;
extern s32 D_8011574C;

extern GpU16Pair D_actor_400600_80144EA8;

extern u8 D_actor_400600_80151A48[];

extern TaskDesc D_actor_400600_80151AF8;

void func_8017D9B8(s32);

/* One of the sub-state tables in this unit's leading rodata. The original wrote
 * it as a local array initializer, so GCC 2.8.1 put the four constant pointers
 * in the constant pool and had the dispatcher copy them onto the stack. Writing
 * that initializer here instead would emit the pool where the *function* sits in
 * the file, which is after every `INCLUDE_RODATA` above and so at the wrong
 * address; reading the splat-owned table as a `TaskFuncTable4` reproduces the
 * same copy while leaving the rodata where it is. */
extern const TaskFuncTable6 D_actor_400600_80131E54;
extern const TaskFuncTable4 D_actor_400600_80131E6C;
extern const TaskFuncTable8 D_actor_400600_80131E7C;
extern const TaskFuncTable4 D_actor_400600_80131E9C;
extern const TaskFuncTable3 D_actor_400600_80131F34;
extern const TaskFuncTable8 D_actor_400600_80131F40;
extern const TaskFuncTable4 D_actor_400600_80131F60;
extern const TaskFuncTable3 D_actor_400600_80131F70;
extern const TaskFuncTable4 D_actor_400600_80131F7C;
extern const TaskFuncTable4 D_actor_400600_80131F8C;
extern const TaskFuncTable3 D_actor_400600_80131F9C;
extern const TaskFuncTable3 D_actor_400600_80132030;

extern u8 D_actor_400600_8014220C[];
extern u8 D_actor_400600_80143604[];
extern u8 D_actor_400600_80143B24[];
extern u8 D_actor_400600_80144994[];

/* Part indices into the model's coordinate array, terminated by -1. */
extern s16 D_actor_400600_80151B88[];

/* Still `INCLUDE_ASM` in this overlay; `func_actor_400600_80139CAC` is called
 * both with and without an argument, so it keeps an unprototyped declaration. */
void func_actor_400600_80135998(Task* arg0, s16 arg1);
s32  func_actor_400600_801376EC(Task* arg0);
void func_actor_400600_80138B40(Task* arg0);
void func_actor_400600_80136558(Task* arg0);
void func_actor_400600_80136670(Task* arg0);
void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade);
void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2);
void ActorsShared8013a2c0(Task* arg0);
void func_actor_400600_801361AC();
s32  func_actor_400600_80136FA8();
s32  func_actor_400600_801370F4();
s32  func_actor_400600_80137AF0(Task* arg0);
s32  func_actor_400600_80137C34(Task* arg0);
void func_actor_400600_80137498(Task* arg0, s16 arg1);
void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
void func_actor_400600_80139CAC();
void func_actor_400600_80139E68(Task* arg0, s16 arg1, Actor400600ViewPos* arg2);
void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_actor_400600_8013B6F4(Task* arg0);
void func_actor_400600_8013B740(Task* arg0);
void func_actor_400600_8013B830(Task* arg0);
void func_actor_400600_8013B8AC(Task* arg0);
void func_actor_400600_8013B984(Task* arg0);
void func_actor_400600_8013BA00(Task* arg0);
void func_actor_400600_8013BC68(Task* arg0);
void func_actor_400600_80133E38(Task* arg0);
void func_actor_400600_8013BBF4(Task* arg0);
void func_actor_400600_80133CB0(Task* arg0);
void func_actor_400600_8013BFD4(Task* arg0);
void func_actor_400600_80134B98(Task* arg0);
void func_actor_400600_8013C518(Task* arg0);
void func_actor_400600_8013C534(Task* arg0);
void func_actor_400600_8013C598(Task* arg0);
s32  func_actor_400600_8013CACC(Task* arg0);
void func_actor_400600_8013C5F8(Task* arg0);
void func_actor_400600_80132294(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, u8 arg5);
void func_actor_400600_80135DDC(Task* arg0);

s32 func_actor_400600_80136FA8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->idMap;
    if (work->field_72E != 1) {
        return 0;
    }
    if (work->field_768 == 0) {
        switch (work->field_730) {
            case 1:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 3;
                work2->field_71E = 0;
                break;
            case 2:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 4;
                work2->field_71E = 0;
                break;
            case 3:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 5;
                work2->field_71E = 0;
                break;
            case 4:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 4;
                work2->field_71E = 0;
                break;
            case 5:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 0xF;
                work2->field_71E = 0;
                break;
        }
        work->field_730 = 0;
    } else {
        switch (work->field_730) {
            case 1:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 3;
                work2->field_71E = 0;
                work->field_730  = 0;
                break;
            case 2:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 4;
                work2->field_71E = 0;
                work->field_730  = 0;
                break;
            case 3:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 0xE;
                work2->field_71E = 0;
                break;
            case 4:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 4;
                work2->field_71E = 0;
                work->field_730  = 0;
                break;
            case 5:
                work2            = (Actor400600Work*)arg0->idMap;
                work2->field_71C = 0xE;
                work2->field_71E = 0;
                work->field_730  = 0;
                break;
        }
    }
    work->field_76D = 0;
    func_actor_400600_80138B40(arg0);
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_801370F4);

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

/* `n` is one variable carrying first `out.vz` and then the speed: the reuse is
 * an anti-dependence that keeps the `field_4` store ahead of `li 10` in sched1,
 * so the tail matches case 2's and jump2 cross-jumps them. Each branch keeps
 * its own matrix pointer so local-alloc puts it in `$s0` ahead of `work`. */
void func_actor_400600_80137498(Task* arg0, s16 arg1)
{
    Actor400600Work*        work = (Actor400600Work*)arg0->idMap;
    SVECTOR                 v;
    SVECTOR                 out;
    ActorsShared8016a538Mat rot;
    s16                     n;

    work->field_76A = arg1;
    switch (arg1) {
        case 0:
            if (work->field_768 == 0) {
                ActorsShared8016a538Mat* m = &rot;

                v.vx              = work->field_A8.x - ((TmdObject*)arg0->extra)->field_8->coord.t[0];
                v.vy              = work->field_A8.y - ((TmdObject*)arg0->extra)->field_8->coord.t[1] - 0x384;
                v.vz              = work->field_A8.z - ((TmdObject*)arg0->extra)->field_8->coord.t[2];
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                m->ident.m11_m12  = 0x1000;
                rot.ident.m20_m21 = 0;
                m->ident.m22      = 0x1000;
                rot.mat.t[0]      = 0;
                rot.mat.t[1]      = 0;
                rot.mat.t[2]      = 0;
                func_8004BFF8(-(s16)work->field_82, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            } else {
                ActorsShared8016a538Mat* m = &rot;

                v.vx              = work->field_A8.x - ((TmdObject*)arg0->extra)->field_8->coord.t[0];
                v.vy              = work->field_A8.y - ((TmdObject*)arg0->extra)->field_8->coord.t[1] - 0x640;
                v.vz              = work->field_A8.z - ((TmdObject*)arg0->extra)->field_8->coord.t[2];
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                m->ident.m11_m12  = 0x1000;
                rot.ident.m20_m21 = 0;
                m->ident.m22      = 0x1000;
                rot.mat.t[0]      = 0;
                rot.mat.t[1]      = 0;
                rot.mat.t[2]      = 0;
                func_8004BFF8(-(s16)work->field_82, &m->mat);
                RotMatrixZ(-(s16)work->field_84, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            }
            work->rec_624.field_0  = out.vx;
            work->rec_624.field_2  = out.vy;
            n                      = out.vz;
            work->rec_624.field_4  = n;
            n                      = 0xA;
            work->rec_624.field_10 = n;
            work->rec_624.field_12 = n;
            break;
        case 1:
            work->rec_624.field_0  = 0;
            work->rec_624.field_2  = 0x190;
            work->rec_624.field_4  = -0xBB8;
            work->rec_624.field_10 = 0x50;
            work->rec_624.field_12 = 0x50;
            break;
        case 2:
            work->rec_624.field_0  = 0;
            work->rec_624.field_2  = -0xBB8;
            work->rec_624.field_4  = 0;
            work->rec_624.field_10 = 0xA;
            work->rec_624.field_12 = 0xA;
            break;
    }
    work->rec_624.field_8 = 0;
    work->rec_624.field_A = 0x64;
    work->rec_624.field_C = 0;
    Gp_ClearRec18Occupied(work->rec_63C);
    work->obj_604.flags |= 0x4000;
}

s32 func_actor_400600_801376EC(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          v;
    s16              dist;
    s32              i;

    dist  = 0;
    work  = (Actor400600Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    for (i = 0; i < 8; i++) {
        if ((work->rec_63C[i].field_4 & 0xFFFF0000) != 0x100000) {
            dist = 0;
        } else {
            if (work->field_76A == 0) {
                dist = 1;
            } else if (work->field_76A == 1) {
                v.vx = work->rec_63C[i].field_8 - coord->workm.t[0];
                v.vy = 0;
                v.vz = work->rec_63C[i].field_C - coord->workm.t[2];
                dist = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
                if (dist == 0) {
                    dist = 1;
                }
            } else if (work->field_76A == 2) {
                v.vx = work->rec_63C[i].field_8 - coord->workm.t[0];
                v.vy = work->rec_63C[i].field_A - coord->workm.t[1];
                v.vz = 0;
                dist = SquareRoot0(v.vx * v.vx + v.vy * v.vy);
                if (dist == 0) {
                    dist = 1;
                }
            }
            break;
        }
    }
    Gp_ClearRec18Occupied(work->rec_63C);
    return dist;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_80137840);

s32 func_actor_400600_80137AF0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    TmdObject*       model2;
    GpEnemy*         enemy;

    work = (Actor400600Work*)arg0->idMap;
    if (work->field_758 > 0) {
        work->field_758--;
        return 0;
    }
    if ((u32)(work->field_72C - 0x400) >= 0x801U && (u32)(work->field_72A - 0x300) >= 0xA01U) {
        if (work->field_728 < 0xBB8) {
            model = (TmdObject*)arg0->extra;
            if (work->field_75C.b.field_75E != 1) {
                work->field_75C.b.field_75E = 1;
                work->field_75C.b.field_75F = 1;
                work->field_740             = 0;
                model->field_C             |= 2;
                Gp_SetLightMode(arg0->spawnArg2, 2);
                func_actor_400600_801387DC(arg0, 2);
            }
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 0x11;
            work2->field_71E = 0;
            return 1;
        }
    } else {
        work = (Actor400600Work*)arg0->idMap;
    }
    model2 = (TmdObject*)arg0->extra;
    enemy  = (GpEnemy*)arg0->spawnArg2;
    if (work->field_75C.b.field_75E != 0) {
        work->field_75C.b.field_75E = 0;
        work->field_75C.b.field_75F = 1;
        work->field_740             = 0;
        model2->field_C             = (model2->field_C | 2) & 0xFF7F;
        Gp_SetLightMode(arg0->spawnArg2, 0);
        enemy->node.field_4 = 4;
        func_actor_400600_801387DC(arg0, 0);
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600_2", func_actor_400600_80137C34);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600_2", D_actor_400600_80132030);

/// `ActorsShared8013a2c0`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
static __inline__ void Actor400600_UpdateColor(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)arg0->extra)->field_8[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0->spawnArg2, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

void func_actor_400600_80137EF0(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable3   fns   = D_actor_400600_80132030;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            Actor400600_TickAnim(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80136558(arg0);
            Actor400600_RebuildRotation(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            Actor400600_UpdateColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

/// Refreshes the parts listed in `D_actor_400600_80151B88`, projects each into
/// view space with `arg1` as the Y, and passes nine fixed pairs of the resulting
/// points to `func_actor_400600_801383E4` along with `arg2` (the fade level at
/// every call site).
void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2)
{
    MATRIX         mtx;
    SVECTOR        pts[11];
    GsCOORDINATE2* coord;
    GsCOORDINATE2* root;
    s32            i;

    root              = ((TmdObject*)arg0->extra)->field_8;
    Gfx_ViewCoord.flg = 0;
    root->flg         = 0;
    for (i = 0; D_actor_400600_80151B88[i] != -1; i++) {
        coord      = &((TmdObject*)arg0->extra)->field_8[D_actor_400600_80151B88[i]];
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &mtx);
        pts[i].vx = mtx.t[0];
        pts[i].vy = arg1;
        pts[i].vz = mtx.t[2];
    }
    func_actor_400600_801383E4(&pts[1], &pts[5], 0x80, arg2);
    func_actor_400600_801383E4(&pts[5], &pts[6], 0x80, arg2);
    func_actor_400600_801383E4(&pts[1], &pts[3], 0x80, arg2);
    func_actor_400600_801383E4(&pts[3], &pts[4], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[2], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[7], 0x80, arg2);
    func_actor_400600_801383E4(&pts[7], &pts[8], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[9], 0x80, arg2);
    func_actor_400600_801383E4(&pts[9], &pts[10], 0x80, arg2);
}

/// Draws a semi-transparent textured quad along the segment from `arg0` to
/// `arg1`: widened by `width` either side, pulled in by half its length at both
/// ends, and shaded grey `shade`. The per-model counterpart of
/// `ActorsShared80163354`, taking view-space points instead of joints.
void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade)
{
    Actor400600QuadScratch* s;
    s16                     angle;
    s32                     halfX;
    s32                     halfZ;
    POLY_FT4*               poly;

    Gfx_ViewCoord.flg = 0;
    s                 = (Actor400600QuadScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor400600QuadScratch));
    Gp_UpdateCoord(&Gfx_ViewCoord);
    angle         = ratan2(arg1->vx - arg0->vx, arg1->vz - arg0->vz);
    halfX         = (arg0->vx - arg1->vx) / 2;
    halfZ         = (arg0->vz - arg1->vz) / 2;
    s->corner0.vx = halfX + (arg0->vx - ((s32)(rcos(angle) * width) >> 0xC));
    s->corner0.vy = arg0->vy;
    s->corner0.vz = halfZ + (arg0->vz + ((s32)(rsin(angle) * width) >> 0xC));
    s->corner1.vx = halfX + (arg0->vx + ((s32)(rcos(angle) * width) >> 0xC));
    s->corner1.vy = arg0->vy;
    s->corner1.vz = halfZ + (arg0->vz - ((s32)(rsin(angle) * width) >> 0xC));
    s->corner2.vx = (arg1->vx - ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
    s->corner2.vy = arg1->vy;
    s->corner2.vz = (arg1->vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
    s->corner3.vx = (arg1->vx + ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
    s->corner3.vy = arg1->vy;
    s->corner3.vz = (arg1->vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                             &s->screen2, &s->screen3, &s->perspective, &s->flags);
    if (s->flags >= 0) {
        poly           = Gpu_PrimCursor;
        Gpu_PrimCursor = (u8*)poly + 0x28;
        setlen(poly, 9);
        poly->code       = 0x2E;
        *(s32*)&poly->x0 = s->screen0;
        *(s32*)&poly->x1 = s->screen1;
        *(s32*)&poly->x2 = s->screen2;
        *(s32*)&poly->x3 = s->screen3;
        setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        poly->tpage = 0x48;
        poly->clut  = 0x4283;
        setRGB0(poly, shade, shade, shade);
        addPrim((u32*)((((u32)(s->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), poly);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor400600QuadScratch);
}

/// Copies this actor's model flags onto both child tasks' models and, for a
/// non-negative `arg1`, sets the children's light mode to it.
void func_actor_400600_801387DC(Task* arg0, s32 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    Task*            child;

    work  = (Actor400600Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    if (work->field_704 != NULL) {
        child                               = work->field_704;
        ((TmdObject*)child->extra)->field_C = model->field_C;
        if (arg1 >= 0) {
            Gp_SetLightMode(child->spawnArg2, arg1);
        }
    }
    if (work->field_708 != NULL) {
        child                               = work->field_708;
        ((TmdObject*)child->extra)->field_C = model->field_C;
        if (arg1 >= 0) {
            Gp_SetLightMode(child->spawnArg2, arg1);
        }
    }
}

/// Returns the id of the first `D_actor_400600_80151B40` zone containing the
/// actor's world XZ position, or 0 if none does.
s16 func_actor_400600_8013886C(Task* arg0)
{
    GpCoordXZ*       coord;
    Actor400600Zone* zone;
    s16              x;
    s16              z;

    coord = (GpCoordXZ*)((TmdObject*)arg0->extra)->field_8;
    x     = coord->field_18;
    z     = coord->field_20;
    for (zone = D_actor_400600_80151B40; zone->id != -1; zone++) {
        if (zone->x <= x && x <= zone->x + zone->w && zone->z <= z && z <= zone->z + zone->h) {
            return zone->id;
        }
    }
    return 0;
}

s32 func_actor_400600_8013892C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if ((work->field_710.h.flags & 1) || (work->field_710.word & 0x01020000)) {
        return 1;
    }
    return 0;
}

void func_actor_400600_8013896C(Task* arg0, s16 arg1)
{
    GsCOORDINATE2* coord;
    SVECTOR        vec;
    s32            i;

    coord = ((TmdObject*)arg0->extra)->field_8;
    Gp_SpawnEff(D_8011574C, coord, 0x40, NULL);
    for (i = 0; i < 16; i++) {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = arg1;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(D_80115738, coord, 0x01202148, &vec);
    }
}

void func_actor_400600_80138A24(Task* arg0, s16 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    GpEnemy*         enemy;

    work  = (Actor400600Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if (arg1 != 0) {
        enemy->node.field_4 = 5;
        model->field_C     |= 0x80;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
        work->field_75C.b.field_75E = 1;
        work->field_740             = 0;
        work->field_75C.b.field_75F = 0;
        work->field_73A             = 0;
    }
}

void func_actor_400600_80138AA4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_766 = 0;
    work->field_765 = 0;
}

void func_actor_400600_80138AB8(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if (work->field_732 > 0) {
        work->field_732 = (u16)work->field_732 - 1;
    }
    if (work->field_710.h.timer > 0) {
        work->field_710.h.timer = (u16)work->field_710.h.timer - 1;
    }
}

void func_actor_400600_80138AF0(Task* arg0, s32 arg1)
{
    u32 rnd1;
    u32 rnd2;

    rnd1                                       = ((u32)Gp_LcgState * 5) + 0x71357911;
    rnd2                                       = (rnd1 * 5) + 0x71357911;
    Gp_LcgState                                = rnd2;
    ((Actor400600Work*)arg0->idMap)->field_732 = arg1 + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
}

void func_actor_400600_80138B40(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->obj_604.flags &= 0xBFFF;
}

void func_actor_400600_80138B5C(Task* arg0, s32 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400600Work*)arg0->idMap;
    if (!(arg1 & 0xFF)) {
        if (work->field_75C.b.field_75E != 0) {
            work->field_75C.b.field_75E = 0;
            work->field_75C.b.field_75F = 1;
            work->field_740             = 0;
            model->field_C              = (model->field_C | 2) & 0xFF7F;
            Gp_SetLightMode(arg0->spawnArg2, 0);
            enemy->node.field_4 = 4;
            func_actor_400600_801387DC(arg0, 0);
        }
    } else if (work->field_75C.b.field_75E != 1) {
        work->field_75C.b.field_75E = 1;
        work->field_75C.b.field_75F = 1;
        work->field_740             = 0;
        model->field_C             |= 2;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
    }
}

void func_actor_400600_80138C34(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable8   fns   = D_actor_400600_80131E7C;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            func_actor_400600_80139CAC(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80136558(arg0);
            ActorsShared80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80132704(arg0, work->field_73C, work->field_73A);
            break;
    }
}

void func_actor_400600_80138D78(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns   = D_actor_400600_80131E9C;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            func_actor_400600_80139CAC(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80136558(arg0);
            ActorsShared80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

void func_actor_400600_80138EA0(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable6   fns   = D_actor_400600_80131E54;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80139CAC(arg0);
            func_actor_400600_80136558(arg0);
            ActorsShared80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80138224(arg0, 0, (u8)work->field_73A);
            break;
    }
}

void func_actor_400600_80138FD4(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns   = D_actor_400600_80131E6C;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80139CAC(arg0);
            func_actor_400600_80136558(arg0);
            ActorsShared80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

void func_actor_400600_801390FC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    work->field_71C = 1;
    work->field_71E = 0;
}

void func_actor_400600_80139110(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B6F4, func_actor_400600_8013B740 };

    func_actor_400600_80138AA4(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
        if ((s16)func_actor_400600_80137C34(arg0) == 0 && (s16)func_actor_400600_80137AF0(arg0) == 0 && (*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x4080000 && work->field_768 != 0 && ((TmdObject*)arg0->extra)->field_8->coord.t[0] > 10000) {
            Actor400600Work* cur = (Actor400600Work*)arg0->idMap;

            cur->field_71C = 0xD;
            cur->field_71E = 0;
        }
    }
}

void func_actor_400600_80139218(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B830, func_actor_400600_8013B8AC };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139280(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B984, func_actor_400600_8013BA00 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_801392E8(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable3   fns  = D_actor_400600_80131F34;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_8013935C(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BBF4, func_actor_400600_80133CB0 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_801393D0(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BC68, func_actor_400600_80133E38 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_80139444(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable8   fns  = D_actor_400600_80131F40;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801394E0(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns  = D_actor_400600_80131F60;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139560(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    SVECTOR          pos;
    s16              count;

    work = (Actor400600Work*)arg0->idMap;
    func_actor_400600_80138AA4(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        count           = work->field_750 - 1;
        work->field_750 = count;
        if (count == 0) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 0xB;
            work2->field_71E = 0;
            return;
        }
        pos.vx = work->field_A8.x;
        pos.vy = work->field_A8.y;
        pos.vz = work->field_A8.z;
        ActorsShared80139c00(arg0, &pos, 0x18);
        func_actor_400600_80135DDC(arg0);
    }
}

void func_actor_400600_80139608(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BFD4, func_actor_400600_80134B98 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139670(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable3   fns  = D_actor_400600_80131F70;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801396E4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns  = D_actor_400600_80131F7C;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139764(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable4   fns  = D_actor_400600_80131F8C;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801397E4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    TaskFuncTable3   fns  = D_actor_400600_80131F9C;

    func_actor_400600_80138AA4(arg0);
    if ((func_actor_400600_8013CACC(arg0) << 0x10) == 0) {
        fns.funcs[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_80139878(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013C518, func_actor_400600_8013C534 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_801398E0(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->idMap;
    void             (*fns[2])(Task*) = { func_actor_400600_8013C598, func_actor_400600_8013C5F8 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}
