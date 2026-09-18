#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

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
    work  = (Actor342000Work*)arg0->work;
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
            ctx   = (Actor342000Work*)arg0->work;
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
        extra      = (TmdObject*)arg0->extra;
        mtx        = (Actor342000ColorMtx*)Mem_Malloc(0x44, 0);
        arg0->work = (TaskIdMap*)mtx;
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

void              func_actor_342000_80163F88(Task* arg0);
extern GpAnimSet* D_actor_342000_801647F8[];
extern GpAnimSet* D_actor_342000_80164800[];
extern GpAnimSet* D_actor_342000_80164808[];
extern GpMsgEntry D_actor_342000_801648E8[];

/// Parents the work block's own coordinate to `Actor342000Work::field_2A4`,
/// hangs the model's part coordinate off it and resets it to an identity
/// matrix with no translation. Every `func_actor_342000_80162158` case repeats
/// it; as a function its address pseudos are born at their first use instead
/// of being hoisted to the top of each case.
static inline void Actor342000_InitCoord(Task* arg0, Actor342000Work* w)
{
    GsCOORDINATE2*       coord;
    Actor342000MatWords* mtx;

    coord                                   = &w->coord;
    coord->sub                              = ((Actor342000Work*)arg0->work)->field_2A4;
    ((TmdObject*)arg0->extra)->field_8->sub = coord;
    coord->coord.t[0]                       = 0;
    coord->coord.t[1]                       = 0;
    coord->coord.t[2]                       = 0;
    mtx                                     = (Actor342000MatWords*)&w->coord.coord;
    mtx->ident.m00_m01                      = 0x1000;
    mtx->ident.m02_m10                      = 0;
    mtx->ident.m11_m12                      = 0x1000;
    mtx->ident.m20_m21                      = 0;
    mtx->ident.m22                          = 0x1000;
    w->coord.flg                            = 0;
    ((TmdObject*)arg0->extra)->field_8->flg = 0;
}

/// Spawn tick shared by the actor and its child model tasks: allocates and
/// zeroes the work block, republishes its light/colour matrices onto the model,
/// applies the area record 0x20's TMD bytes and, per `Task::spawnArg1`, parents
/// the coordinate (view, parent model, or the parent part
/// `D_actor_342000_80164900` names) and binds the animation bank. Cases 1 and 2
/// register themselves on the parent as `field_29C` / `field_2A0`.
///
/// The empty loops before `case 1:` / `case 2:` make reorg fill the dispatch
/// delay slots from those arms; one `ctx` per case keeps each short-lived so
/// the work pointer outranks it for `$s1`.
void func_actor_342000_80162158(Task* arg0)
{
    TmdObject*       extra;
    Actor342000Work* work;
    Actor342000Work* ctx;
    Actor342000Work* ctx2;
    Actor342000Work* ctx3;
    Actor342000Work* w;
    GpCdRec10*       rec;
    u16              i;

    extra      = (TmdObject*)arg0->extra;
    work       = (Actor342000Work*)Mem_Malloc(0x2AC, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    w = work;
    Mem_Set(w, 0, 0x2AC);
    w->field_298    = (Task*)arg0->spawnArg2;
    extra->field_1C = &w->light;
    extra->field_20 = &w->color;
    arg0->field_24  = D_actor_342000_801648E8;
    rec             = ((GpCdAreaRec*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->field_4))->field_0;
    for (; rec->field_0 != 0xFF; rec++) {
        if (rec->field_0 == 0x20) {
            break;
        }
    }
    Gp_SetTmdBytes(extra, (s8)rec->field_D, (s8)rec->field_E);
    switch (arg0->spawnArg1) {
        case 0:
            w->field_2A4 = &Gfx_ViewCoord;
            Actor342000_InitCoord(arg0, w);
            func_800B3F84(&w->ctx, D_actor_342000_801647F8, (GpAnimObj*)extra, &w->pad_154, w->slots);
            ctx = (Actor342000Work*)arg0->work;
            for (i = 1; i < 8; i++) {
                ctx->slots[i].field_9 = 0x10;
                Gp_AnimResetSlot(&ctx->ctx, i, 0);
            }
            break;
            do {
            } while (0);
        case 1:
            w->field_2A4 = ((TmdObject*)w->field_298->extra)->field_8;
            Actor342000_InitCoord(arg0, w);
            ((Actor342000Work*)w->field_298->work)->field_29C = arg0;
            func_800B3F84(&w->ctx, D_actor_342000_80164800, (GpAnimObj*)extra, &w->pad_154, w->slots);
            ctx2 = (Actor342000Work*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx2->slots[i].field_9 = 0x10;
                Gp_AnimResetSlot(&ctx2->ctx, i, 0);
            }
            break;
            do {
            } while (0);
        case 2:
            w->field_2A4 = ((TmdObject*)w->field_298->extra)->field_8;
            Actor342000_InitCoord(arg0, w);
            ((Actor342000Work*)w->field_298->work)->field_2A0 = arg0;
            func_800B3F84(&w->ctx, D_actor_342000_80164808, (GpAnimObj*)extra, &w->pad_154, w->slots);
            ctx3 = (Actor342000Work*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx3->slots[i].field_9 = 0x10;
                Gp_AnimResetSlot(&ctx3->ctx, i, 0);
            }
            break;
        default:
            w->field_2A4 = &((TmdObject*)w->field_298->extra)->field_8[D_actor_342000_80164900[arg0->spawnArg1].pad];
            Actor342000_InitCoord(arg0, w);
            break;
    }
    Task_Reparent(w->field_298, arg0);
    arg0->exitCallback = func_actor_342000_80163F88;
}

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

    work = (Actor342000Work*)arg0->work;

    switch (arg0->state) {
        case 0:
            func_actor_342000_80162158(arg0);
            work              = (Actor342000Work*)arg0->work;
            coord             = ((TmdObject*)arg0->extra)->field_8;
            coord->coord.t[0] = D_actor_342000_80164900[arg0->spawnArg1].vx;
            coord->coord.t[1] = D_actor_342000_80164900[arg0->spawnArg1].vy;
            coord->coord.t[2] = D_actor_342000_80164900[arg0->spawnArg1].vz;
            coord->flg        = 0;
            arg0->state      += 1;
            break;
        case 1:
            one  = 0x1000;
            data = (Actor342000Work*)work->field_298->work;
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

    work = (Actor342000Work*)arg0->work;

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
            data = (Actor342000Work*)arg0->work;
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

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// base weapon id, `D_8007218A` selects the alternate animation block.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Animation payload of the 0x3F4 messages sent to the slot-3 task.
extern u8 D_actor_342000_801647E8[];

/// Placement sent as message 0x3E9 by sequence step 1.
extern Actor342000Move D_actor_342000_80164930;

/// Per-tick sequence driver of the event task: raises 0x3ED on `field_48`,
/// then runs the one-shot step latched in `field_68` (warps, animation
/// changes for the slot-3 task, the step-2 wait on 0x3F0 plus an 11-tick
/// delay, and step 8's sound cue) and clears it. Cases 5 and 7 keep their
/// weapon id locals block-scoped; sharing one pseudo across both cases moves
/// the `D_80073BA9` load ahead of the flag load.
void func_actor_342000_80162BBC(Task* arg0)
{
    Actor342000EventWork* work;
    Actor342000EventWork* ev;
    GpAnimArg             msg;

    work = (Actor342000EventWork*)arg0->work;
    if (work->field_48 != NULL) {
        Gp_DispatchMsg(work->field_48, 0x3ED, 0, 0);
    }
    switch (work->field_68) {
        case 0:
            break;
        case 1:
            Gp_PulseState1C();
            Gp_StateC08.field_6 |= 1;
            Gp_DispatchMsg(work->field_48, 0x3E9, (s32)&D_actor_342000_80164930, 0);
            break;
        case 2:
            switch (work->field_6A) {
                case 0:
                    Gp_DispatchMsg(work->field_48, 0x3F2, (s32)&D_actor_342000_80164948, 0);
                    work->field_6A++;
                    return;
                case 1:
                    if (Gp_DispatchMsg(work->field_48, 0x3F0, 0, 0) == 0) {
                        work->field_6C = 0;
                        work->field_6A++;
                    }
                    return;
                case 2:
                    if (++work->field_6C > 10) {
                        work->field_68 = 0;
                        msg.field_0    = D_actor_342000_801647E8;
                        msg.field_4    = 0;
                        msg.field_8    = 0;
                        msg.field_C    = 0;
                        msg.field_10   = 0;
                        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
                    }
                    return;
            }
            return;
        case 3:
            Gp_DispatchMsg(work->field_48, 0x3E9, (s32)&D_actor_342000_80164948, 0);
            msg.field_0  = D_actor_342000_801647E8;
            msg.field_4  = 0;
            msg.field_8  = 0;
            msg.field_C  = 0;
            msg.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            break;
        case 4:
            msg.field_0  = D_actor_342000_801647E8;
            msg.field_4  = 1;
            msg.field_8  = 1;
            msg.field_C  = 10;
            msg.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            break;
        case 5: {
            s32 weaponId;
            s32 anim;

            weaponId     = D_80073BA9;
            anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.field_0  = (void*)anim;
            msg.field_4  = 1;
            msg.field_8  = 0;
            msg.field_C  = 0;
            msg.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            break;
        }
        case 6:
            msg.field_0  = D_actor_342000_801647E8;
            msg.field_4  = 2;
            msg.field_8  = 1;
            msg.field_C  = 10;
            msg.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            break;
        case 7: {
            s32 weaponId;
            s32 anim;

            weaponId     = D_80073BA9;
            anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.field_0  = (void*)anim;
            msg.field_4  = 1;
            msg.field_8  = 1;
            msg.field_C  = 10;
            msg.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            break;
        }
        case 8:
            msg.field_0  = D_actor_342000_801647E8;
            msg.field_4  = 3;
            msg.field_8  = 0;
            msg.field_C  = 0;
            msg.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            ev = (Actor342000EventWork*)D_actor_342000_80165070->work;
            if (ev->field_7A == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
                ev->field_7A = 1;
            }
            break;
    }
    work->field_68 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162F28);

/// Spawn table of the event task's children: entry 2 is the script parent,
/// 3..7 its five script tasks and 8/9 the two effect actors.
extern TaskDesc D_actor_342000_80164FF8;

/// The two placements the event task seeds its work block with in state 4.
extern Actor342000Move D_actor_342000_80164818[2];
extern u8              D_actor_342000_80164968;
extern u8              D_actor_342000_80164E30;
extern u8              D_8007216C;
extern s8              D_8007216D;
extern s8              D_80114C11;
extern u8              D_801153F4;
extern u16             D_801855DE;
extern TaskDesc        D_80187150;
extern GpAreaApplyRec  D_8018FB6C[];
extern u16             D_8018FBC8;

void func_80180FE4(s32 arg0, s32 arg1, s32 arg2);
void func_8018507C(void);
void func_actor_342000_80162F28(Task* arg0);

/// The event task's leaf steps, inlined here; `actor_342000_3.c` carries the
/// same bodies as out-of-line functions (`func_actor_342000_801641FC`,
/// `801642B4`, `801642D4`, `80164154`).
static inline void Actor342000_CopyMove(Actor342000Move* dst, Actor342000Move* src)
{
    dst->field_0  = src->field_0;
    dst->field_4  = src->field_4;
    dst->field_8  = src->field_8;
    dst->field_10 = src->field_10;
    dst->field_12 = src->field_12;
    dst->field_14 = src->field_14;
}

static inline void Actor342000_KillFx(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_5C != NULL) {
        Task_Kill(work->field_5C);
    }
    if (work->field_60 != NULL) {
        Task_Kill(work->field_60);
    }
    work->field_5C = NULL;
    work->field_60 = NULL;
}

static inline void Actor342000_SetAction(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->work;
    work->field_68 = arg0;
    work->field_6A = 0;
}

static inline void Actor342000_SetMode(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->work;
    work->field_70 = arg0;
    work->field_72 = 0;
}

static inline void Actor342000_EnterArea(void)
{
    gGameSession->field_5        = 7;
    D_8007216D                   = 7;
    gGameSession->unknown_133[1] = 6;
    gGameSession->unknown_133[0] = 1;
    gGameSession->field_76       = 1;
    Gp_ApplyAreaRecs(D_8018FB6C);
}

/// Event/sequence task body, idle while a cutscene, pause or mode switch is up.
/// State 0 allocates the `Actor342000EventWork` block and spawns the effect
/// actors (a spawn with `GameSession::unknown_137[0]` set skips to state 4);
/// states 1..10 spawn the script tasks, seed the placements and run the timed
/// hand-off to area 0x21, and state 11 kills the task. `SOFT_BARRIER()` keeps
/// state 7's `D_8007216C` store ahead of the state load, as in retail.
void func_actor_342000_8016382C(Task* arg0)
{
    Actor342000Msg7DA     msg;
    Actor342000EventWork* work;
    Actor342000EventWork* ev;
    Actor342000EventWork* alloc;
    Actor342000EventWork* seq;
    Actor342000Move*      src;
    Actor342000Move*      dst;
    Task*                 child;
    u16                   i;
    s16                   timer;

    work = (Actor342000EventWork*)arg0->work;
    if (D_801855DE != 0 || gGameSession->field_65 != 0 || D_80114C11 != 0 || D_801153F4 != 0) {
        return;
    }
    if (gGameSession->field_136 != 0) {
        if (work->field_7E != 0) {
            SndEvt_EnqueueType7(0x5428000B, 0xA);
            work->field_7E = 0;
        }
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = (Actor342000EventWork*)Mem_Calloc(0x80U, false);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
            } else {
                Mem_Set(alloc, 0U, 0x80U);
                alloc->field_48         = (Task*)Game_GetPtrSlot(3);
                D_actor_342000_80165070 = arg0;
                alloc->field_4C         = (s32)Gp_FindWorkById(gGameSession->field_6 | (gGameSession->field_7 << 8))->field_0;
            }
            work = (Actor342000EventWork*)arg0->work;
            if ((u8)gGameSession->unknown_137[0] == 0) {
                msg.field_0 = gGameSession->field_7;
                msg.field_1 = gGameSession->field_6;
                msg.field_2 = 0;
                Gp_DispatchMsg((Task*)Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
                work->field_5C = Task_SpawnFromTable(&D_actor_342000_80164FF8, 8, 0, (s32)arg0);
                work->field_60 = Task_SpawnFromTable(&D_actor_342000_80164FF8, 9, 0, (s32)arg0);
                goto next;
            }
            work->field_5C = Task_SpawnFromTable(&D_actor_342000_80164FF8, 8, 1, (s32)arg0);
            work->field_60 = Task_SpawnFromTable(&D_actor_342000_80164FF8, 9, 1, (s32)arg0);
            func_80180FE4(0x17, 0, 0x3C);
            arg0->state = 4;
            break;
        case 1:
            work->field_50 = Task_SpawnFromTable(&D_actor_342000_80164FF8, 2, 0, (s32)arg0);
            for (i = 0; i < 5; i++) {
                child = Task_SpawnFromTable(&D_actor_342000_80164FF8, i + 3, i + 1, (s32)work->field_50);
                if (i == 0) {
                    work->field_54 = child;
                }
                if (i == 1) {
                    work->field_58 = child;
                }
            }
            goto next;
        case 2:
            Gp_MsgPlayerWeapon(0);
            func_800E8634((s32)&D_actor_342000_80164968, 0, (s32)&D_actor_342000_80164E30);
            goto next;
        case 3:
            if (gGameSession->field_1 == 0) {
                gGameSession->field_120 = D_8018FBC8;
                Task_SpawnFromTable(&D_80187150, 0, 1, 0);
                gGameSession->field_135 = 2;
                Task_RequestKill(arg0, 0);
                return;
            }
            func_actor_342000_80162BBC(arg0);
            func_actor_342000_80162F28(arg0);
            break;
        case 4:
            Actor342000_CopyMove(&work->field_0[0], &D_actor_342000_80164818[0]);
            Actor342000_CopyMove(&work->field_0[1], &D_actor_342000_80164818[1]);
            work->field_70      = 6;
            arg0->killCountdown = 0;
            arg0->state++;
            break;
        case 5:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 0x1A5) {
                work->field_78 = gGameSession->field_4;
                Actor342000_SetAction(7);
                Actor342000_SetMode(6);
                arg0->killCountdown = 0;
                arg0->state++;
            }
            func_actor_342000_80162F28(arg0);
            break;
        case 6:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 2) {
                D_8007216C = 0x21;
                goto next;
            }
            break;
        case 7:
            D_8007216C = 0x21;
            SOFT_BARRIER();
            arg0->killCountdown = 0;
            arg0->state++;
            break;
        case 8:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 0x3C) {
                Actor342000_KillFx();
                Actor342000_SetMode(7);
                Actor342000_EnterArea();
                msg.field_0 = gGameSession->field_7;
                msg.field_1 = gGameSession->field_6;
                msg.field_2 = 0;
                Gp_DispatchMsg((Task*)Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
                arg0->killCountdown = 0;
                arg0->state++;
                break;
            }
            break;
        case 9:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 2) {
                Actor342000_SetMode(9);
                func_8018507C();
                Gp_DispatchMsg(work->field_48, 0x3F1, 0, 0);
                gGameSession->field_135 = 2;
                goto next;
            }
            break;
        case 10:
        next:
            arg0->state++;
            break;
        case 11:
            Task_RequestKill(arg0, 0);
            return;
    }
    if ((u32)(arg0->state - 6) < 5U) {
        func_actor_342000_80162BBC(arg0);
        func_actor_342000_80162F28(arg0);
    }
}
