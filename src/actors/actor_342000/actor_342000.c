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

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80161EA4);

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

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_801625D8);

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
