#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "actors/actor_323300.h"
#include "actors/actors_shared_80132808.h"

#include "gameplay/3CD8.h"

#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/tmd.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_gpl12_real() __asm__ volatile("nop; nop; .word 0x4BA8003E")

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Message 0x7DB handler: the payload halfword selects show, child spawn/kill,
/// placement-plus-preset, or the part-6 effect. Case 12 inlines the 0x7D3
/// preset body of `func_actor_323300_801628B8` against `D_actor_323300_801725C8`.
s32 func_actor_323300_80162360(Task* arg0, s32 arg1, Actor323300Msg7DB* msg, Actor323300Placement* place)
{
    Actor323300Work*       w;
    Actor323300Work*       work;
    Actor323300AnimPreset* preset;
    TmdObject*             extra;
    Task*                  spawned;
    GsCOORDINATE2*         src;
    GsCOORDINATE2*         dst;
    SVECTOR                vec;
    s32                    i;

    w = (Actor323300Work*)arg0->work;
    switch (msg->field_2) {
        case 0:
            func_actor_323300_80162208(arg0, 0x7D5, 1, 0);
            break;
        case 10:
            spawned      = Task_SpawnFromTable(D_actor_323300_8017255C, 1, 0, 0);
            w->field_4B8 = spawned;
            if (spawned != NULL) {
                src        = ((TmdObject*)arg0->extra)->coords;
                dst        = ((TmdObject*)spawned->extra)->coords;
                dst->coord = src->coord;
            }
            w->field_500 = 0;
            break;
        case 11:
            if (w->field_4B8 != NULL) {
                taskKill(w->field_4B8);
            }
            w->field_500 = 0;
            SndEvt_EnqueueType7(0x52100006, 1);
            break;
        case 12:
            w->field_4FC = 1;
            w->field_4FE = 0;
            w->field_4BC = place->pos.vx;
            w->field_4C0 = place->pos.vy;
            w->field_4C4 = place->pos.vz;
            w->field_4F4 = place->rot.vx;
            w->field_4F6 = place->rot.vy;
            w->field_4F8 = place->rot.vz;
            w->field_43F = 2;

            preset = &D_actor_323300_801725C8;
            work   = (Actor323300Work*)arg0->work;
            extra  = arg0->extra;
            if (preset->field_0 != work->field_43E) {
                work->field_43E = preset->field_0;
                work->field_43D = -1;
                func_800B3F84(&work->anim, D_actor_323300_80172558[work->field_43E], extra,
                              work->pad_30C, work->slots);
            }
            if (preset->field_4 != work->field_43D) {
                work->field_43D = preset->field_4;
                if (preset->field_8 != 0 && work->field_43C != 0) {
                    for (i = 1; i < 0x13; i++) {
                        func_800B4114(&work->anim, i, work->field_43D, 0, preset->field_C);
                    }
                } else {
                    for (i = 1; i < 0x13; i++) {
                        Gp_AnimResetSlot(&work->anim, i, work->field_43D);
                    }
                }
                for (i = 1; i < 0x13; i++) {
                    Gp_AnimTickIndex(&work->anim, i);
                }
                work->field_43C = 1;
            }
            break;
        case 13:
            vec.vy = 0x3C;
            vec.vx = 0;
            vec.vz = 0xC8;
            Gp_SpawnEff(0x600A2, &((TmdObject*)arg0->extra)->coords[6], 0xA, &vec);
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300_2", func_actor_323300_80162630);

void func_actor_323300_8016269C(Task* arg0)
{
    Gp_UnlinkObj(&((Actor323300Work*)arg0->work)->obj);
    Gp_EnemyTaskExit(arg0);
}

void func_actor_323300_801626D0(Task* arg0)
{
    TmdObject*       ext;
    Actor323300Work* work;

    ext           = arg0->extra;
    work          = (Actor323300Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

void func_actor_323300_801626EC(Task* arg0)
{
}

void func_actor_323300_801626F4(Task* arg0)
{
    Actor323300Work* work                = (Actor323300Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_323300_80162748,
        func_actor_323300_801627B4,
    };

    states[(s16)work->field_4FE](arg0);
}

void func_actor_323300_80162748(Task* arg0)
{
    Actor323300Work* work;
    s32              i;

    work = (Actor323300Work*)arg0->work;
    func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725C8, 0);
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = 8;
    }
    work->field_4CC = 0;
    work->field_4D0 = 0;
    work->field_4D4 = 0;
    work->field_4FE++;
}

/// State handler at index 1 of the two-entry table `func_actor_323300_801626F4`
/// dispatches, the turn-to-face body whose twins are
/// `func_actor_335800_801631A4`, `func_actor_113100_801330E8` and
/// `func_actor_350700_80162764`. Euler-extracts the root coordinate into `vec`
/// and, while the yaw gap to the target `work->field_4F6` stays under 0x41,
/// snaps `vec.vy` to that target, plays anim 0x7D3 through
/// `func_actor_323300_801628B8` and parks all 18 animation slots at 0x16 --
/// `field_4FC` and `field_4FE` go back to zero, so the handler re-runs. A wider
/// gap steps `vec.vy` toward the target by 0x40 instead. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, with `flg`
/// cleared so the next `Gp_UpdateCoord` recomputes it.
void func_actor_323300_801627B4(Task* arg0)
{
    Actor323300Work*   work;
    Actor323300Matrix* words;
    GsCOORDINATE2*     coord;
    SVECTOR            vec;
    s16                diff;
    s32                vy;
    s32                i;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor323300Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4F6 - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy = work->field_4F6;
        func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725DC, 0);
        for (i = 1; i < 0x13; i++) {
            work->slots[i].rate = 0x16;
        }
        work->field_4FC = 0;
        work->field_4FE = 0;
    }

    words                = (Actor323300Matrix*)&coord->coord;
    words->ident.m00_m01 = 0x1000;
    words->ident.m02_m10 = 0;
    words->ident.m11_m12 = 0x1000;
    words->ident.m20_m21 = 0;
    words->ident.m22     = 0x1000;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300_2", func_actor_323300_801628B8);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300_2", func_actor_323300_801629F0);

void func_actor_323300_80162A6C(Task* arg0, GpMimeSrc* arg1, s32 arg2)
{
    s32        i;
    s32        count;
    s32        off;
    TmdSource* src;
    u16*       dst;
    u16*       from;
    u16*       dstMid;
    u16*       fromMid;
    SVECTOR*   nrm;
    SVECTOR*   nrmA;
    SVECTOR*   nrmB;
    SVECTOR*   nrmDst;
    s32        blend;
    s32        inv;
    u16        vx;
    u16        vz;

    i     = 0;
    count = arg1->field_16;
    src   = ((TmdObject*)arg0->extra)->source;
    off   = arg1->field_14 * 8;
    from  = (u16*)((u8*)arg1->field_8 + off);
    nrm   = src->normals;
    dst   = (u16*)((u8*)src->verts + off);
    if (count > 0) {
        fromMid = from + 2;
        dstMid  = dst + 2;
        do {
            vx         = *from;
            from      += 4;
            i         += 1;
            *dst       = vx;
            dst       += 4;
            dstMid[-1] = fromMid[-1];
            vz         = fromMid[0];
            fromMid   += 4;
            dstMid[0]  = vz;
            dstMid    += 4;
        } while (i < count);
    }
    blend = arg2;
    inv   = 0x1000 - blend;
    gteMIMefunc(src->verts + arg1->field_14, arg1->field_0, arg1->field_16, blend);
    nrmA = (SVECTOR*)arg1->field_4;
    if (nrmA != NULL) {
        count = arg1->field_12;
        nrmB  = arg1->field_C;
        i     = 0;
        if (count > 0) {
            do {
                gte_lddp(blend);
                gte_ldsv(nrmA);
                gte_gpf12_real();
                nrmDst = nrm + i;
                gte_lddp(inv);
                gte_ldsv(nrmB);
                gte_gpl12_real();
                nrmB++;
                i++;
                nrmA++;
                gte_stsv(nrmDst);
            } while (i < count);
        }
    }
}

void func_actor_323300_80162BE4(Task* arg0)
{
    Actor323300MtxWork* work;
    TmdObject*          extra;
    TmdObject*          model;
    TmdSource*          src;
    GsCOORDINATE2*      coords;
    SVECTOR*            dst;
    SVECTOR*            from;
    GpMimeSrc*          ctl;
    SVECTOR*            nrm;
    long*               translation;
    s32                 i;
    s32                 part;

    extra              = arg0->extra;
    arg0->exitCallback = func_actor_323300_801634B0;
    work               = (Actor323300MtxWork*)memCalloc(0x6B0, 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work         = work;
    work->field_444    = -1;
    work->field_440    = -1;
    work->field_44C    = 0x3000;
    extra->clut        = 2;
    extra->tpageOffset = 2;
    extra->clutOffset  = 4;
    extra->tpage       = 0;
    extra->lightLevel  = 0xFFF;
    extra->flags      &= 0xFF7F;
    tmdProcessStream(extra);
    tmdProcessStream(extra);
    func_actor_323300_80163718(arg0, 0x7D3, &D_actor_323300_80174A74, 0);
    func_actor_323300_8016369C(arg0, 0x7D3, &D_actor_323300_80174AB0, 0);
    model = arg0->extra;
    ctl   = &D_801865D0;
    SCHED_BARRIER();
    dst = ctl->field_8;
    nrm = ctl->field_C;
    SCHED_BARRIER();
    src  = model->source;
    from = (SVECTOR*)src->verts;
    for (i = 0; i < ctl->field_10; i++) {
        dst[i].vx = (u16)from[i].vx;
        dst[i].vy = (u16)from[i].vy;
        dst[i].vz = (u16)from[i].vz;
    }
    if (ctl->field_4 != 0) {
        from = (SVECTOR*)src->normals;
        i    = 0;
        if (ctl->field_12 > 0) {
            dst = nrm;
            do {
                dst[i].vx = (u16)from[i].vx;
                dst[i].vy = (u16)from[i].vy;
                dst[i].vz = (u16)from[i].vz;
                i++;
            } while (i < ctl->field_12);
        }
    }
    func_actor_323300_80163510(arg0);
    for (part = 1; part < 0x13; part++) {
        coords                     = ((TmdObject*)arg0->extra)->coords;
        translation                = coords[part].coord.t;
        work->partPos[part].vx     = translation[0];
        (work->partPos + part)->vy = coords[part].coord.t[1];
        (work->partPos + part)->vz = coords[part].coord.t[2];
    }
    DEF_REG(model);
    arg0->state += 1;
}

/// Per-frame squash driver for the 0x6B0 `Actor323300MtxWork` block, and the
/// runner the model-display path calls once the block's animation has been
/// started: it ticks the 18 slots like `func_actor_323300_80163718` does, folds
/// `field_44C` -- the 0x3000 countdown `func_actor_323300_80162BE4` seeds, 0x40
/// per frame -- into the 0..0xFFF ramp `func_actor_323300_80162A6C` blends the
/// model's vertices with, and republishes that ramp onto `TmdObject::lightLevel`,
/// the intensity the shading path scales its RGB by. While the countdown is
/// still above 0x1000 the turn angle handed to `func_actor_323300_8016359C` is
/// `(0x1000 - field_44C) / 4`, i.e. the ramp read the other way round.
///
/// The three coordinate nodes at parts 3..5 are then flattened: each is copied
/// off into `shadow[0..2]` first, then squashed in place through
/// `ScaleMatrix` -- parts 3 and 4 to 0.2 on Y, part 5 to identity -- and the
/// *copies* become the parents of parts 4, 5 and 6, so the squash does not
/// compound down the part chain. The Y translation the squash removes from
/// parts 4 and 5 is folded out of their own `coord.t[1]` by the same 0.8 and
/// the same ramp. Part 6's shading is rebound to the third copy's translation
/// before the countdown drops, so the whole ramp runs out exactly when it
/// reaches zero.
void func_actor_323300_80162DF0(Task* arg0)
{
    Actor323300MtxWork* work;
    TmdObject*          extra;
    GsCOORDINATE2*      coord;
    VECTOR              vec;
    s32                 blend;
    s32                 i;

    work  = (Actor323300MtxWork*)arg0->work;
    extra = (TmdObject*)arg0->extra;

    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }

    blend = work->field_44C;
    if (blend >= 0x2000) {
        blend = 0xFFF;
    } else if (blend > 0x1000) {
        blend -= 0x1000;
    } else {
        blend = 0;
    }

    func_actor_323300_80162A6C(arg0, &D_801865D0, blend);
    extra->lightLevel = blend;

    if (work->field_44C < 0x1000) {
        func_actor_323300_8016359C(arg0, (s16)(((0x1000 - work->field_44C) << 14) >> 16));
    }

    coord           = &((TmdObject*)arg0->extra)->coords[3];
    work->shadow[0] = *coord;
    vec.vx          = 0x1000;
    vec.vy          = 0x333;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);

    coord           = &((TmdObject*)arg0->extra)->coords[4];
    work->shadow[1] = *coord;
    coord->sub      = &work->shadow[0];
    vec.vx          = 0x1000;
    vec.vy          = 0x333;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);
    coord->coord.t[1] = work->partPos[4].vy - work->partPos[4].vy * 0.8 * blend / 4096.0;

    coord           = &((TmdObject*)arg0->extra)->coords[5];
    work->shadow[2] = *coord;
    coord->sub      = &work->shadow[1];
    vec.vx          = 0x1000;
    vec.vy          = 0x1000;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);
    coord->coord.t[1] = work->partPos[5].vy - work->partPos[5].vy * 0.8 * blend / 4096.0;

    coord      = &((TmdObject*)arg0->extra)->coords[6];
    coord->sub = &work->shadow[2];
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);

    work->field_44C -= 0x40;
    if (work->field_44C < 0) {
        work->field_44C = 0;
    }
}

/// Same walk as `ActorsShared80132808`, then an extra `RotMatrixX(angle / 2)`
/// after the yaw so the joint is pitched as well as turned.
void func_actor_323300_80163188(GsCOORDINATE2* coord, s16 angle)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    ActorsShared80132808_Accumulate(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(angle, rotation);
    RotMatrixX(angle / 2, rotation);
    out = ActorsShared80132808_Localize(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

void func_actor_323300_801634B0(Task* arg0)
{
    GsCOORDINATE2* base;
    GsCOORDINATE2* node;
    GsCOORDINATE2* sub;

    do {
        base      = ((TmdObject*)arg0->extra)->coords;
        sub       = base + 3;
        node      = base + 4;
        node->sub = sub;
    } while (0);
    sub                                      = ((TmdObject*)arg0->extra)->coords + 5;
    sub->sub                                 = node;
    ((TmdObject*)arg0->extra)->coords[6].sub = sub;
    taskKill(arg0);
}

/// Splats an identity light/colour pair into the `memCalloc(0x6B0)` work block
/// `func_actor_323300_80162BE4` parked in `Task::work`, republishes them onto
/// `TmdObject::lightMtx` / `field_20`, then re-derives model part 1's world
/// matrix -- clearing its dirty flag, rebuilding it from its parent and
/// rebinding the actor's shading to the part's translation.
void func_actor_323300_80163510(Task* arg0)
{
    Actor323300MtxWork* work;
    Actor323300Matrix*  light;
    Actor323300Matrix*  color;
    GsCOORDINATE2*      coords;
    TmdObject*          extra;

    extra  = arg0->extra;
    work   = (Actor323300MtxWork*)arg0->work;
    coords = extra->coords;

    work->light.ident.m00_m01 = 0x1000;
    light                     = &work->light;
    light->ident.m02_m10      = 0;
    light->ident.m11_m12      = 0x1000;
    light->ident.m20_m21      = 0;
    light->ident.m22          = 0x1000;

    work->color.ident.m00_m01 = 0x1000;
    color                     = &work->color;
    color->ident.m02_m10      = 0;
    color->ident.m11_m12      = 0x1000;
    color->ident.m20_m21      = 0;
    color->ident.m22          = 0x1000;

    extra->lightMtx = &light->mat;
    extra->colorMtx = &color->mat;

    coords[1].flg = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Re-aims the per-part coordinate nodes at index 5 and index 2 from one turn
/// angle: the angle is clamped to +-0x400 -- a quarter turn either way -- then
/// `func_actor_323300_80163188` rebuilds node 5 from two thirds of it and node
/// 2 from half, and nodes 5 down to 2 have their dirty flag cleared so the next
/// `Gp_UpdateCoord` re-derives them. The lower clamp tests `arg1` rather than
/// the clamped copy; that is the same test, because the upper clamp has already
/// pinned the copy to 0x400 whenever the angle was out of range upwards.
void func_actor_323300_8016359C(Task* arg0, s16 arg1)
{
    s16 var;

    var = arg1;
    if (var > 0x400) {
        var = 0x400;
    }
    if (arg1 < -0x400) {
        var = -0x400;
    }

    func_actor_323300_80163188(&((TmdObject*)arg0->extra)->coords[5], (var * 2) / 3);
    func_actor_323300_80163188(&((TmdObject*)arg0->extra)->coords[2], var / 2);

    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    ((TmdObject*)arg0->extra)->coords[3].flg = 0;
    ((TmdObject*)arg0->extra)->coords[2].flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300_2", func_actor_323300_8016369C);

/// Start-preset handler for the 0x6B0 `Actor323300MtxWork` block
/// `func_actor_323300_80162BE4` parks in `Task::work`, and the twin of
/// `func_actor_323300_801628B8` (which drives the 0x504 block the same way).
/// A preset bank the block is not already on re-seeds it: the animation id is
/// reset to -1, the bank is stored and the bank's animation source goes to
/// `func_800B3F84` with the block's context, slots and matrix table. A
/// different animation id then restarts every slot 1..0x12 -- through
/// `func_800B4114` when the preset asks for it and the block has been started
/// before, through `Gp_AnimResetSlot` otherwise -- ticks them once and latches
/// `field_43C` so the next preset takes the first branch.
s32 func_actor_323300_80163718(Task* arg0, s32 arg1, Actor323300AnimPreset* arg2, s32 arg3)
{
    Actor323300MtxWork* work;
    TmdObject*          ext;
    s32                 i;

    work = (Actor323300MtxWork*)arg0->work;
    ext  = arg0->extra;
    if (arg2->field_0 != work->field_440) {
        work->field_440 = arg2->field_0;
        work->field_444 = -1;
        func_800B3F84(&work->anim, D_actor_323300_80174A70[work->field_440], ext,
                      work->pad_30C, work->slots);
    }
    if (arg2->field_4 != work->field_444) {
        work->field_444 = arg2->field_4;
        if (arg2->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_444, 0, arg2->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_444);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300_2", func_actor_323300_80163840);
