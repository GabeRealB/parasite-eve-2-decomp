#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "actors/actor_342000.h"

#include <psyq/inline_c.h>

/// `gpf 12` -- general purpose interpolation. The `inline_c.h` macro of that
/// name assembles to a different word, so spell the instruction out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern GpMsgEntry D_actor_342000_801648A8[];

/// Animation-id bank `Actor342000Work::field_288` indexes; a negative entry
/// means the bank is empty and the slots are left alone.
extern s16 D_actor_342000_80164810[];

/// Per-`spawnArg1` translation seeds for the child model's part coordinate.
extern SVECTOR D_actor_342000_80164900[];

/// `func_800B4114` is declared locally with a signed `arg2`; see `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Ticks slots `(arg1 == 8)..arg1-1` of the task's animation context (slot 0 is
/// skipped for the eight-slot actor). If every one of them then has
/// `GpAnimSlot::field_10` bit 0x100 set, passes them the
/// `D_actor_342000_80164810` id and returns 1; otherwise returns 0. The gotos
/// reproduce retail's block layout.
s32 func_actor_342000_80161EA4(Task* arg0, u16 arg1)
{
    Actor342000Work* work;
    Actor342000Work* ctx;
    u16              i;
    u16              done;
    u16              start;
    u16              anim;
    s32              first;

    anim  = arg1 == 8;
    start = anim;
    work  = (Actor342000Work*)arg0->idMap;
    for (i = start; i < arg1; i++) {
        Gp_AnimTickIndex(&work->ctx, i);
    }
    i    = start;
    done = 1;
    for (; i < arg1; i++) {
        if (!(work->slots[i].field_10 & 0x100)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_342000_80164810[work->field_288] >= 0) {
            anim  = D_actor_342000_80164810[work->field_288];
            ctx   = (Actor342000Work*)arg0->idMap;
            first = arg1 == 8;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = first; i < arg1; i++) {
                func_800B4114(&ctx->ctx, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

void func_actor_342000_8016201C(Task* arg0)
{
    TmdObject*           extra;
    TmdObject*           mdl;
    Actor342000ColorMtx* mtx;
    VECTOR               pos;

    if (arg0->state == 0) {
        extra       = (TmdObject*)arg0->extra;
        mtx         = (Actor342000ColorMtx*)Mem_Malloc(0x44, 0);
        arg0->idMap = (TaskIdMap*)mtx;
        if (mtx == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(mtx, 0, 0x44);
            mtx->field_40  = (Task*)arg0->spawnArg2;
            extra->field_C = 0;
            if (arg0->spawnArg1 != 0) {
                extra->field_E = 0x1F;
            }
            ((TmdObject*)arg0->extra)->field_8->sub = &Gfx_ViewCoord;
            extra->field_20                         = &mtx->color;
            extra->field_1C                         = &mtx->light;
            arg0->field_24                          = D_actor_342000_801648A8;
            Task_Reparent(mtx->field_40, arg0);
        }
        arg0->state += 1;
    }

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162158);

/// Display handler of the actor's child model. The spawn tick seeds the
/// model's part coordinate translation from the `D_actor_342000_80164900` entry
/// `Task::spawnArg1` selects; state 1 resets the work block's coordinate to
/// identity and scales each column by the parent's `Actor342000Work::field_264`
/// through `gpf 12` (the same scratchpad idiom as `func_actor_342000_801628C8`).
/// Every tick then mirrors the parent model's `TmdObject::field_C` flags and
/// hands the second part translation to `func_800D7A9C`.
///
/// `one` is a named pseudo so the 0x1000 load leads state 1 (it fills the
/// dispatch branch's delay slot); `TOUCH_REG(mtx)` stops cse re-addressing the
/// first column read through the work block.
void func_actor_342000_801625D8(Task* arg0)
{
    register short       t4 asm("t4");
    register short       t5 asm("t5");
    register short       t6 asm("t6");
    Actor342000Work*     work;
    Actor342000MatWords* mtx;
    Actor342000Work*     data;
    s32                  one;
    GsCOORDINATE2*       coord;
    VECTOR*              sc;
    TmdObject*           extra;
    u8*                  head;
    SVECTOR*             sv;
    u32                  scratch;
    VECTOR               pos;

    work = (Actor342000Work*)arg0->idMap;

    switch (arg0->state) {
        case 0:
            func_actor_342000_80162158(arg0);
            work              = (Actor342000Work*)arg0->idMap;
            coord             = ((TmdObject*)arg0->extra)->field_8;
            coord->coord.t[0] = D_actor_342000_80164900[arg0->spawnArg1].vx;
            coord->coord.t[1] = D_actor_342000_80164900[arg0->spawnArg1].vy;
            coord->coord.t[2] = D_actor_342000_80164900[arg0->spawnArg1].vz;
            coord->flg        = 0;
            arg0->state      += 1;
            break;
        case 1:
            one  = 0x1000;
            data = (Actor342000Work*)work->field_298->idMap;
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            scratch            = *(u32*)(head + 0x3FC);
            mtx                = (Actor342000MatWords*)&work->coord.coord;
            mtx->ident.m00_m01 = one;
            mtx->ident.m02_m10 = 0;
            mtx->ident.m11_m12 = one;
            mtx->ident.m20_m21 = 0;
            mtx->ident.m22     = one;
            TOUCH_REG(mtx);
            sv = (SVECTOR*)(scratch - 8);
            sc = &data->field_264;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
            TOUCH_REG(sv);

            COMPILER_BARRIER();
            t4     = mtx->mat.m[0][0];
            t5     = mtx->mat.m[1][0];
            t6     = mtx->mat.m[2][0];
            sv->vx = t4;
            sv->vy = t5;
            sv->vz = t6;
            gte_lddp(sc->vx);
            gte_ldsv(sv);
            gte_gpf12_real();
            gte_stsv(sv);
            t4               = sv->vx;
            t5               = sv->vy;
            t6               = sv->vz;
            mtx->mat.m[0][0] = t4;
            mtx->mat.m[1][0] = t5;
            mtx->mat.m[2][0] = t6;

            COMPILER_BARRIER();
            t4     = mtx->mat.m[0][1];
            t5     = mtx->mat.m[1][1];
            t6     = mtx->mat.m[2][1];
            sv->vx = t4;
            sv->vy = t5;
            sv->vz = t6;
            gte_lddp(sc->vy);
            gte_ldsv(sv);
            gte_gpf12_real();
            gte_stsv(sv);
            t4               = sv->vx;
            t5               = sv->vy;
            t6               = sv->vz;
            mtx->mat.m[0][1] = t4;
            mtx->mat.m[1][1] = t5;
            mtx->mat.m[2][1] = t6;

            COMPILER_BARRIER();
            t4     = mtx->mat.m[0][2];
            t5     = mtx->mat.m[1][2];
            t6     = mtx->mat.m[2][2];
            sv->vx = t4;
            sv->vy = t5;
            sv->vz = t6;
            gte_lddp(sc->vz);
            gte_ldsv(sv);
            gte_gpf12_real();
            gte_stsv(sv);
            t4               = sv->vx;
            t5               = sv->vy;
            t6               = sv->vz;
            mtx->mat.m[0][2] = t4;
            mtx->mat.m[1][2] = t5;
            mtx->mat.m[2][2] = t6;

            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            scratch         = *(u32*)(head + 0x3FC);
            work->coord.flg = 0;
            scratch        += 8;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(scratch) : "memory");
            break;
    }
    ((TmdObject*)arg0->extra)->field_C = ((TmdObject*)work->field_298->extra)->field_C;
    extra                              = (TmdObject*)arg0->extra;
    pos.vx                             = ((TmdObject*)arg0->extra)->field_8[1].workm.t[0];
    pos.vy                             = ((TmdObject*)arg0->extra)->field_8[1].workm.t[1];
    pos.vz                             = ((TmdObject*)arg0->extra)->field_8[1].workm.t[2];
    func_800D7A9C(extra, &pos, 0, 3);
}

/// Display state 1 rebuilds the actor coordinate: an identity rotation is
/// splatted through `Actor342000MatWords`, the euler angles below it are
/// composed onto it (Y, then X, then Z) and every column is scaled by the
/// matching component of `Actor342000Work::field_264` -- a scratchpad `SVECTOR`
/// is gathered from the column, run through `GPF` and scattered back, with the
/// scratch released again at the end. The tail is shared with the other display
/// states: it ticks the actor's own animation bank and the two child tasks and
/// hands the model's second part translation to `func_800D7A9C`.
///
/// The `lui` / `sw` pair around the scratchpad is written out because that is
/// the only way to reach `G_SCRATCH_HEAD` as a `lui` plus a `0x3FC` displacement
/// rather than a materialised 32-bit constant.
void func_actor_342000_801628C8(Task* arg0)
{
    register short       t4 asm("t4");
    register short       t5 asm("t5");
    register short       t6 asm("t6");
    Actor342000Work*     work;
    Actor342000Work*     data;
    Actor342000MatWords* mtx;
    s32*                 ang;
    VECTOR*              sc;
    TmdObject*           extra;
    u8*                  head;
    SVECTOR*             sv;
    u32                  scratch;
    VECTOR               pos;

    work = (Actor342000Work*)arg0->idMap;

    switch (arg0->state) {
        case 0:
            func_actor_342000_80162158(arg0);
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_342000_801648B8, 0);
            arg0->state += 1;
            return;
        case 1:
            mtx                = (Actor342000MatWords*)&work->coord.coord;
            mtx->ident.m00_m01 = 0x1000;
            mtx->ident.m02_m10 = 0;
            mtx->ident.m11_m12 = 0x1000;
            mtx->ident.m20_m21 = 0;
            mtx->ident.m22     = 0x1000;
            ang                = &work->field_274;
            Gfx_RotMatrixY(&mtx->mat, ang[1], 1);
            Gfx_RotMatrixX(&mtx->mat, ang[0], 0);
            Gfx_RotMatrixZ(&mtx->mat, ang[2], 0);
            TOUCH_REG(mtx);
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            scratch = *(u32*)(head + 0x3FC);
            sc      = &work->field_264;
            sv      = (SVECTOR*)(scratch - 8);
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
            TOUCH_REG(sv);

            COMPILER_BARRIER();
            t4     = mtx->mat.m[0][0];
            t5     = mtx->mat.m[1][0];
            t6     = mtx->mat.m[2][0];
            sv->vx = t4;
            sv->vy = t5;
            sv->vz = t6;
            gte_lddp(sc->vx);
            gte_ldsv(sv);
            gte_gpf12_real();
            gte_stsv(sv);
            t4               = sv->vx;
            t5               = sv->vy;
            t6               = sv->vz;
            mtx->mat.m[0][0] = t4;
            mtx->mat.m[1][0] = t5;
            mtx->mat.m[2][0] = t6;

            COMPILER_BARRIER();
            t4     = mtx->mat.m[0][1];
            t5     = mtx->mat.m[1][1];
            t6     = mtx->mat.m[2][1];
            sv->vx = t4;
            sv->vy = t5;
            sv->vz = t6;
            gte_lddp(sc->vy);
            gte_ldsv(sv);
            gte_gpf12_real();
            gte_stsv(sv);
            t4               = sv->vx;
            t5               = sv->vy;
            t6               = sv->vz;
            mtx->mat.m[0][1] = t4;
            mtx->mat.m[1][1] = t5;
            mtx->mat.m[2][1] = t6;

            COMPILER_BARRIER();
            t4     = mtx->mat.m[0][2];
            t5     = mtx->mat.m[1][2];
            t6     = mtx->mat.m[2][2];
            sv->vx = t4;
            sv->vy = t5;
            sv->vz = t6;
            gte_lddp(sc->vz);
            gte_ldsv(sv);
            gte_gpf12_real();
            gte_stsv(sv);
            t4               = sv->vx;
            t5               = sv->vy;
            t6               = sv->vz;
            mtx->mat.m[0][2] = t4;
            mtx->mat.m[1][2] = t5;
            mtx->mat.m[2][2] = t6;

            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            scratch         = *(u32*)(head + 0x3FC);
            work->coord.flg = 0;
            scratch        += 8;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(scratch) : "memory");
            /* fallthrough */
        default:
            data = (Actor342000Work*)arg0->idMap;
            func_actor_342000_80161EA4(arg0, 8);
            func_actor_342000_80161EA4(data->field_29C, 4);
            func_actor_342000_80161EA4(data->field_2A0, 4);
            extra  = (TmdObject*)arg0->extra;
            pos.vx = ((TmdObject*)arg0->extra)->field_8[1].workm.t[0];
            pos.vy = ((TmdObject*)arg0->extra)->field_8[1].workm.t[1];
            pos.vz = ((TmdObject*)arg0->extra)->field_8[1].workm.t[2];
            func_800D7A9C(extra, &pos, 0, 3);
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_342000/actor_342000", D_actor_342000_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162BBC);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162F28);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_8016382C);
