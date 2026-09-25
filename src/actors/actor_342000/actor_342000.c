#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// Position + rotation argument of the overlay's id 0x7D4 message handlers,
/// one per message table (`func_actor_342000_801640C0` and
/// `func_actor_342000_80164034`). `field_0` / `field_4` / `field_8` are copied
/// onto a coordinate's `coord.t`; `field_10` / `field_12` / `field_14` are the
/// euler angles its matrix is rebuilt from. Same shape as gameplay's
/// `GpXformArg`.
typedef struct _Actor342000Move {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[4];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
} Actor342000Move;
STATIC_ASSERT_SIZEOF(Actor342000Move, 0x18);

/// Script command payload of the overlay's message handlers -- the `arg2` of
/// the id 0x7DB handler `func_actor_342000_80164110`, which reads `field_2`
/// and latches it in `Actor342000Work::field_2AA`. Same shape as the
/// neighbouring overlays' command payloads.
typedef struct _Actor342000Cmd {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor342000Cmd;
STATIC_ASSERT_SIZEOF(Actor342000Cmd, 0x4);

/// A `MATRIX` plus the word-wise view `func_actor_342000_801628C8` splats the
/// actor coordinate's identity rotation through: five aligned stores rather
/// than nine halfword ones.
typedef union Actor342000MatWords {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor342000MatWords;
STATIC_ASSERT_SIZEOF(Actor342000MatWords, 0x20);

/// Per-instance work block for the overlay's model actor.
///
/// `func_actor_342000_80162158` allocates it with `Mem_Malloc(0x2AC, 0)`,
/// `Mem_Set`s it to zero over the same 0x2AC bytes and stores it in the
/// `Task::work` slot (0x1C), so the size below is the allocation, not a
/// guess: the actor reuses that pointer field for its own work block and it is
/// *not* a `TaskIdMap` here. Reach it with `(Actor342000Work*)task->work`.
///
/// `field_2A4` is the coordinate node the actor's model is re-parented to:
/// `func_actor_342000_80162158` seeds it with `&gGfxViewCoord`, and the exit
/// callback `func_actor_342000_80163F88` writes it back into
/// `((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)->sub`.
///
/// `coord` is the actor's own rotation node. `func_actor_342000_801628C8`
/// builds `coord.coord` from the euler angles below it (`Gfx_RotMatrixY` of
/// `field_278`, then `X` of `field_274`, then `Z` of `field_27C`, word loads),
/// scales each of its columns by the matching `field_264` component through
/// `gpf 12` and clears `coord.flg`; `func_actor_342000_801640C0` writes all of
/// it from an `Actor342000Move`.
///
/// `field_264` holds that per-axis scale, 1.12 fixed point like the matrix it
/// multiplies: each column `j` is gathered into a scratchpad `SVECTOR`, run
/// through `GPF` against `field_264[j]` and written back.
///
/// `field_298` is the parent actor task a child model display handler
/// (`func_actor_342000_801625D8`) mirrors its flags and column scale from.
///
/// `field_29C` / `field_2A0` are the actor's two child tasks; the per-frame tail
/// of `func_actor_342000_801628C8` ticks them with `func_actor_342000_80161EA4`.
///
/// `field_2AA` latches the `Actor342000Cmd::field_2` the id 0x7DB handler was
/// last called with; command 0xA additionally refills `field_264` from the
/// handler's second payload.
///
/// The block opens with the actor's animation context -- `ctx` and the eight
/// `GpAnimSlot`s `func_800B3F84` initialises from this overlay's banks -- so the
/// block pointer is also the `GpAnimCtx*` the animation helpers take:
/// `func_actor_342000_80161EA4` ticks it and passes the id bank
/// `field_288` indexes. Slot 0 is the child slot that function skips. `light` /
/// `color` at 0x1D4 / 0x1F4 are the pair `func_actor_342000_80162158`
/// republishes onto the model's `TmdObject::lightMtx` / `field_20`, exactly as
/// the neighbouring actor overlays lay out theirs.
typedef struct Actor342000Work {
    /* 0x000 */ GpAnimCtx      ctx;
    /* 0x014 */ GpAnimSlot     slots[8];
    /* 0x154 */ byte           pad_154[0x80];
    /* 0x1D4 */ MATRIX         light;
    /* 0x1F4 */ MATRIX         color;
    /* 0x214 */ GsCOORDINATE2  coord;
    /* 0x264 */ VECTOR         field_264;
    /* 0x274 */ s32            field_274;
    /* 0x278 */ s32            field_278;
    /* 0x27C */ s32            field_27C;
    /* 0x280 */ byte           pad_280[0x8];
    /* 0x288 */ s32            field_288;
    /* 0x28C */ byte           pad_28C[0xC];
    /* 0x298 */ Task*          field_298;
    /* 0x29C */ Task*          field_29C;
    /* 0x2A0 */ Task*          field_2A0;
    /* 0x2A4 */ GsCOORDINATE2* field_2A4;
    /* 0x2A8 */ byte           pad_2A8[0x2];
    /* 0x2AA */ u16            field_2AA;
} Actor342000Work;
STATIC_ASSERT_SIZEOF(Actor342000Work, 0x2AC);

/// Work block of the overlay's event/sequence task -- the one
/// `D_actor_342000_80165070` points at.
///
/// `func_actor_342000_8016382C` allocates it with `memCalloc(0x80, 0)`,
/// `Mem_Set`s 0x80 bytes and stores it in that task's `Task::work` slot, so
/// the size is anchored. The same function publishes its owning task in
/// `D_actor_342000_80165070`, which is how the leaf helpers below reach it:
/// `(Actor342000EventWork*)D_actor_342000_80165070->work`.
///
/// `field_48` is the `gameGetPtrSlot(3)` task every `Gp_DispatchMsg` in the
/// overlay is aimed at; `field_50` / `field_5C` / `field_60` / `field_64` are
/// spawned child tasks the teardown helpers kill. `field_7A` and `field_7C`
/// are once-only latches guarding a sound cue and the fade-out setup.
typedef struct Actor342000EventWork {
    /* 0x00 */ Actor342000Move field_0[2];
    /* 0x30 */ Actor342000Move field_30;
    /* 0x48 */ Task*           field_48;
    /* 0x4C */ s32             field_4C;
    /* 0x50 */ Task*           field_50;
    /* 0x54 */ Task*           field_54;
    /* 0x58 */ Task*           field_58;
    /* 0x5C */ Task*           field_5C;
    /* 0x60 */ Task*           field_60;
    /* 0x64 */ Task*           field_64;
    /* 0x68 */ u16             field_68;
    /* 0x6A */ u16             field_6A;
    /* 0x6C */ u16             field_6C;
    /* 0x6E */ byte            pad_6E[0x2];
    /* 0x70 */ s16             field_70;
    /* 0x72 */ s16             field_72;
    /* 0x74 */ u16             field_74;
    /* 0x76 */ byte            pad_76[0x2];
    /* 0x78 */ s16             field_78;
    /* 0x7A */ u16             field_7A;
    /* 0x7C */ u16             field_7C;
    /* 0x7E */ u16             field_7E;
} Actor342000EventWork;
STATIC_ASSERT_SIZEOF(Actor342000EventWork, 0x80);

/// Session id payload of message 0x7DA, sent to the `gameGetPtrSlot(4)` task
/// by `func_actor_342000_8016382C`: `GameSession::at4.loc.stage`, then `at4.loc.area`.
typedef struct Actor342000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor342000Msg7DA;
STATIC_ASSERT_SIZEOF(Actor342000Msg7DA, 0x4);

/// Colour-matrix work block of the overlay's model actor:
/// `func_actor_342000_8016201C` `Mem_Malloc`s 0x44 bytes for it and parks it in
/// the task's `Task::work` slot (0x1C), which is *not* a `TaskIdMap` here.
///
/// The two matrices are the light/colour pair `Tmd_SetupDraw` loads: the same
/// function republishes them onto `((TmdObject*)task->extra)->field_1C` and
/// `field_20`, which otherwise point at `Gp_DefaultMtx` / `Gp_DefaultMtx2` via
/// `Gp_BindDefaultMtx`. `field_40` is the `Task::spawnArg2` spawner, reparented
/// to the actor on the spawn tick.
typedef struct Actor342000ColorMtx {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ Task*  field_40;
} Actor342000ColorMtx;
STATIC_ASSERT_SIZEOF(Actor342000ColorMtx, 0x44);

/// Channel block of the overlay's fade task, sized by its own
/// `memCalloc(8, 0)` and parked in that task's `Task::work` slot. The three
/// channels start at 0xFF and fall by the task's `spawnArg1` each frame.
typedef struct Actor342000Fade {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ s16  r;
    /* 0x4 */ s16  g;
    /* 0x6 */ s16  b;
} Actor342000Fade;
STATIC_ASSERT_SIZEOF(Actor342000Fade, 0x8);

/// The task owning the `Actor342000EventWork` block, published by
/// `func_actor_342000_8016382C`.
extern Task* D_actor_342000_80165070;

/// Message 0x7D4's static payload, handed to `Gp_DispatchMsg` by the actor's
/// spawn tick. Same shape as the `Actor342000Move` the handler takes.
extern Actor342000Move D_actor_342000_801648B8;

/// Fixed placement `func_actor_342000_8016439C` warps slot 3 to, sent as
/// message 0x3E9 and again as 0x3F2 by `func_actor_342000_80162BBC`.
extern Actor342000Move D_actor_342000_80164948;

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
/// `GpAnimSlot.flags` bit 0x100 set, passes them the
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
        if (!(work->slots[i].flags & 0x100)) {
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
            taskKill(arg0);
        } else {
            Mem_Set(mtx, 0, 0x44);
            mtx->field_40 = (Task*)arg0->spawnArg2;
            extra->flags  = 0;
            if (arg0->spawnArg1 != 0) {
                extra->otOffset = 0x1F;
            }
            ((TmdObject*)arg0->extra)->coords->sub = &gGfxViewCoord;
            extra->colorMtx                        = &mtx->color;
            extra->lightMtx                        = &mtx->light;
            arg0->msgTable                         = D_actor_342000_801648A8;
            Task_Reparent(mtx->field_40, arg0);
        }
        arg0->state += 1;
    }

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
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

    coord                                  = &w->coord;
    coord->sub                             = ((Actor342000Work*)arg0->work)->field_2A4;
    ((TmdObject*)arg0->extra)->coords->sub = coord;
    coord->coord.t[0]                      = 0;
    coord->coord.t[1]                      = 0;
    coord->coord.t[2]                      = 0;
    mtx                                    = (Actor342000MatWords*)&w->coord.coord;
    mtx->ident.m00_m01                     = 0x1000;
    mtx->ident.m02_m10                     = 0;
    mtx->ident.m11_m12                     = 0x1000;
    mtx->ident.m20_m21                     = 0;
    mtx->ident.m22                         = 0x1000;
    w->coord.flg                           = 0;
    ((TmdObject*)arg0->extra)->coords->flg = 0;
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
    GpAreaPlace*     rec;
    u16              i;

    extra      = (TmdObject*)arg0->extra;
    work       = (Actor342000Work*)Mem_Malloc(0x2AC, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    w = work;
    Mem_Set(w, 0, 0x2AC);
    w->field_298    = (Task*)arg0->spawnArg2;
    extra->lightMtx = &w->light;
    extra->colorMtx = &w->color;
    arg0->msgTable  = D_actor_342000_801648E8;
    rec             = ((GpCdAreaRec*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc))->field_0;
    for (; rec->entryId != 0xFF; rec++) {
        if (rec->entryId == 0x20) {
            break;
        }
    }
    Gp_SetTmdBytes(extra, (s8)rec->tpage, (s8)rec->clut);
    switch (arg0->spawnArg1) {
        case 0:
            w->field_2A4 = &gGfxViewCoord;
            Actor342000_InitCoord(arg0, w);
            func_800B3F84(&w->ctx, D_actor_342000_801647F8, extra, &w->pad_154, w->slots);
            ctx = (Actor342000Work*)arg0->work;
            for (i = 1; i < 8; i++) {
                ctx->slots[i].rate = 0x10;
                Gp_AnimResetSlot(&ctx->ctx, i, 0);
            }
            break;
            do {
            } while (0);
        case 1:
            w->field_2A4 = ((TmdObject*)w->field_298->extra)->coords;
            Actor342000_InitCoord(arg0, w);
            ((Actor342000Work*)w->field_298->work)->field_29C = arg0;
            func_800B3F84(&w->ctx, D_actor_342000_80164800, extra, &w->pad_154, w->slots);
            ctx2 = (Actor342000Work*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx2->slots[i].rate = 0x10;
                Gp_AnimResetSlot(&ctx2->ctx, i, 0);
            }
            break;
            do {
            } while (0);
        case 2:
            w->field_2A4 = ((TmdObject*)w->field_298->extra)->coords;
            Actor342000_InitCoord(arg0, w);
            ((Actor342000Work*)w->field_298->work)->field_2A0 = arg0;
            func_800B3F84(&w->ctx, D_actor_342000_80164808, extra, &w->pad_154, w->slots);
            ctx3 = (Actor342000Work*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx3->slots[i].rate = 0x10;
                Gp_AnimResetSlot(&ctx3->ctx, i, 0);
            }
            break;
        default:
            w->field_2A4 = &((TmdObject*)w->field_298->extra)->coords[D_actor_342000_80164900[arg0->spawnArg1].pad];
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
/// Every tick then mirrors the parent model's `TmdObject::flags` flags and
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
            coord             = ((TmdObject*)arg0->extra)->coords;
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
            gte_gpf12();
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
            gte_gpf12();
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
            gte_gpf12();
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
    ((TmdObject*)arg0->extra)->flags = ((TmdObject*)work->field_298->extra)->flags;
    extra                            = (TmdObject*)arg0->extra;
    pos.vx                           = ((TmdObject*)arg0->extra)->coords[1].workm.t[0];
    pos.vy                           = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    pos.vz                           = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
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
            gte_gpf12();
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
            gte_gpf12();
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
            gte_gpf12();
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
            pos.vx = ((TmdObject*)arg0->extra)->coords[1].workm.t[0];
            pos.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
            pos.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
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
                        work->field_68    = 0;
                        msg.animBlock.ptr = D_actor_342000_801647E8;
                        msg.field_4       = 0;
                        msg.field_8       = 0;
                        msg.field_C       = 0;
                        msg.field_10      = 0;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
                    }
                    return;
            }
            return;
        case 3:
            Gp_DispatchMsg(work->field_48, 0x3E9, (s32)&D_actor_342000_80164948, 0);
            msg.animBlock.ptr = D_actor_342000_801647E8;
            msg.field_4       = 0;
            msg.field_8       = 0;
            msg.field_C       = 0;
            msg.field_10      = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            break;
        case 4:
            msg.animBlock.ptr = D_actor_342000_801647E8;
            msg.field_4       = 1;
            msg.field_8       = 1;
            msg.field_C       = 10;
            msg.field_10      = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            break;
        case 5: {
            s32 weaponId;
            s32 anim;

            weaponId            = D_80073BA9;
            anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.animBlock.index = anim;
            msg.field_4         = 1;
            msg.field_8         = 0;
            msg.field_C         = 0;
            msg.field_10        = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            break;
        }
        case 6:
            msg.animBlock.ptr = D_actor_342000_801647E8;
            msg.field_4       = 2;
            msg.field_8       = 1;
            msg.field_C       = 10;
            msg.field_10      = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            break;
        case 7: {
            s32 weaponId;
            s32 anim;

            weaponId            = D_80073BA9;
            anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.animBlock.index = anim;
            msg.field_4         = 1;
            msg.field_8         = 1;
            msg.field_C         = 10;
            msg.field_10        = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            break;
        }
        case 8:
            msg.animBlock.ptr = D_actor_342000_801647E8;
            msg.field_4       = 3;
            msg.field_8       = 0;
            msg.field_C       = 0;
            msg.field_10      = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            ev = (Actor342000EventWork*)D_actor_342000_80165070->work;
            if (ev->field_7A == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
                ev->field_7A = 1;
            }
            break;
    }
    work->field_68 = 0;
}

extern Actor342000Move D_actor_342000_80164818[2];
extern Actor342000Move D_actor_342000_80164848[2];
extern Actor342000Move D_actor_342000_80164878[2];
extern Actor342000Move D_actor_342000_801648D0;
extern u8              D_8007216C;
extern s32             D_80070F70;
extern s32             D_80144A74;
extern s32             D_80144A7C;

void func_80143490(s32 arg0);

static inline void Actor342000_CopyMove(Actor342000Move* dst, Actor342000Move* src)
{
    dst->field_0  = src->field_0;
    dst->field_4  = src->field_4;
    dst->field_8  = src->field_8;
    dst->field_10 = src->field_10;
    dst->field_12 = src->field_12;
    dst->field_14 = src->field_14;
}

static inline void Actor342000_SetAnim(Task* task, u16 anim, u16 blend, u16 n)
{
    Actor342000Work* ctx;
    u16              i;
    u16              first;

    first = n == 8;
    ctx   = (Actor342000Work*)task->work;
    if (blend == 0) {
        for (i = first; i < n; i++) {
            ctx->slots[i].rate = 0x10;
            Gp_AnimResetSlot(&ctx->ctx, i, anim);
        }
    } else {
        for (i = first; i < n; i++) {
            func_800B4114(&ctx->ctx, i, anim, 0, blend);
        }
    }
}

static inline s32 Actor342000_Sway(s32 x, s32 d)
{
    if (D_80070F70 & 1) {
        return x + d;
    }
    return x - d;
}

static inline void Actor342000_Add(s32* value, s32 delta)
{
    *value += delta;
}

static inline void Actor342000_Store(s32* dst, s32 value)
{
    *dst = value;
}

void func_actor_342000_80162F28(Task* arg0)
{
    Actor342000EventWork* work;
    Actor342000Work*      actor;
    Actor342000Move*      src;
    s32                   v;

    work  = (Actor342000EventWork*)arg0->work;
    actor = (Actor342000Work*)work->field_50->work;
    switch ((u16)work->field_70) {
        case 1:
            switch ((u16)work->field_72) {
                case 0:
                    Gp_DispatchMsg(work->field_50, 0x7D5, 1, 0);
                    Gp_DispatchMsg(work->field_5C, 0x7D5, 1, 0);
                    Gp_DispatchMsg(work->field_60, 0x7D5, 1, 0);
                    Actor342000_CopyMove(&work->field_0[0], &D_actor_342000_80164818[0]);
                    Actor342000_CopyMove(&work->field_0[1], &D_actor_342000_80164818[1]);
                    actor->field_264.vx     = 0x1000;
                    actor->field_264.vy     = 0x1000;
                    actor->field_264.vz     = 0x1000;
                    src                     = &D_actor_342000_801648B8;
                    work->field_30.field_0  = src->field_0;
                    work->field_30.field_4  = src->field_4;
                    work->field_30.field_8  = src->field_8;
                    work->field_30.field_10 = src->field_10;
                    work->field_30.field_12 = src->field_12;
                    work->field_30.field_14 = src->field_14;
                    work->field_74          = 0;
                    work->field_72++;
                case 1:
                    if (++work->field_74 == 60) {
                        Actor342000_SetAnim(work->field_50, 1, 10, 8);
                        Actor342000_SetAnim(work->field_54, 1, 10, 4);
                        Actor342000_SetAnim(work->field_58, 1, 10, 4);
                    }
                    actor->field_264.vx      -= 4;
                    work->field_0[0].field_0 += 5;
                    work->field_0[1].field_0 -= 5;
                    Gp_DispatchMsg(work->field_5C, 0x7D4, (s32)&work->field_0[0], 0);
                    Gp_DispatchMsg(work->field_60, 0x7D4, (s32)&work->field_0[1], 0);
                    work->field_30.field_4 += 3;
                    Gp_DispatchMsg(work->field_50, 0x7D4, (s32)&work->field_30, 0);
                    break;
            }
            return;
        case 2:
            switch ((u16)work->field_72) {
                case 0:
                    Gp_DispatchMsg(work->field_5C, 0x7D5, 0, 0);
                    Gp_DispatchMsg(work->field_60, 0x7D5, 0, 0);
                    Gp_DispatchMsg(work->field_50, 0x7D4, (s32)&D_actor_342000_801648D0, 0);
                    Actor342000_SetAnim(work->field_50, 0, 0, 8);
                    Actor342000_SetAnim(work->field_54, 0, 0, 4);
                    Actor342000_SetAnim(work->field_58, 0, 0, 4);
                    work->field_72++;
                case 1:
                    actor->field_264.vx -= 4;
                    break;
            }
            return;
        case 3:
            switch ((u16)work->field_72) {
                case 0:
                    Gp_DispatchMsg(work->field_5C, 0x7D5, 1, 0);
                    Gp_DispatchMsg(work->field_60, 0x7D5, 1, 0);
                    Gp_DispatchMsg(work->field_50, 0x7D4, (s32)&D_actor_342000_801648B8, 0);
                    Actor342000_CopyMove(&work->field_0[0], &D_actor_342000_80164848[0]);
                    Actor342000_CopyMove(&work->field_0[1], &D_actor_342000_80164848[1]);
                    work->field_72++;
                case 1:
                    actor->field_264.vx -= 4;
                    Actor342000_Add(&work->field_0[0].field_0, 5);
                    Actor342000_Add(&work->field_0[1].field_0, -5);
                    v = Actor342000_Sway(work->field_0[0].field_8, -20);
                    Actor342000_Store(&work->field_0[0].field_8, v);
                    v = Actor342000_Sway(work->field_0[1].field_8, 20);
                    Actor342000_Store(&work->field_0[1].field_8, v);
                    Gp_DispatchMsg(work->field_5C, 0x7D4, (s32)&work->field_0[0], 0);
                    Gp_DispatchMsg(work->field_60, 0x7D4, (s32)&work->field_0[1], 0);
                    break;
            }
            return;
        case 0:
            break;
        case 4:
            D_8007216C     = 8;
            work->field_64 = Task_Spawn(1, 0x2D, 0x10, 0);
            break;
        case 5:
            if (work->field_64 != NULL) {
                taskKill(work->field_64);
            }
            break;
        case 6:
            if ((u16)work->field_72 == 0) {
                SndEvt_EnqueueType6(0x5428000B, 0, 0);
                work->field_7E = 1;
                work->field_72++;
            }
            if (gGameSession->at4.loc.view == 0xF) {
                Gp_DispatchMsg(work->field_5C, 0x7D5, 0, 0);
                Gp_DispatchMsg(work->field_60, 0x7D5, 0, 0);
            } else {
                Gp_DispatchMsg(work->field_5C, 0x7D5, 1, 0);
                Gp_DispatchMsg(work->field_60, 0x7D5, 1, 0);
            }
            work->field_0[0].field_0 += 5;
            work->field_0[1].field_0 -= 5;
            Gp_DispatchMsg(work->field_5C, 0x7D4, (s32)&work->field_0[0], 0);
            Gp_DispatchMsg(work->field_60, 0x7D4, (s32)&work->field_0[1], 0);
            return;
        case 7:
            D_8007216C = work->field_78;
            break;
        case 8:
            switch ((u16)work->field_72) {
                case 0:
                    Gp_DispatchMsg(work->field_5C, 0x7D5, 1, 0);
                    Gp_DispatchMsg(work->field_60, 0x7D5, 1, 0);
                    Actor342000_CopyMove(&work->field_0[0], &D_actor_342000_80164878[0]);
                    Actor342000_CopyMove(&work->field_0[1], &D_actor_342000_80164878[1]);
                    work->field_72++;
                case 1:
                    work->field_0[0].field_0 += 5;
                    work->field_0[1].field_0 -= 5;
                    if (work->field_0[0].field_0 >= 0x36B0) {
                        work->field_0[0].field_0 = 0x36B0;
                        work->field_0[1].field_0 = 0x36B0;
                        Task_Reparent(arg0, Gp_SpawnScript18((s32)&D_80144A74, (s32)&D_80144A7C));
                        func_80143490(3);
                        work->field_70 = 0;
                    }
                    Gp_DispatchMsg(work->field_5C, 0x7D4, (s32)&work->field_0[0], 0);
                    Gp_DispatchMsg(work->field_60, 0x7D4, (s32)&work->field_0[1], 0);
                    break;
            }
            return;
        case 9:
            SndEvt_EnqueueType7(0x5428000B, 1);
            SndEvt_EnqueueType6(0x5428000C, 0, 0);
            Task_Reparent(arg0, Gp_SpawnScript18((s32)&D_80144A74, (s32)&D_80144A7C));
            func_80143490(3);
            break;
        default:
            break;
    }
    work->field_70 = 0;
}

/// Spawn table of the event task's children: entry 2 is the script parent,
/// 3..7 its five script tasks and 8/9 the two effect actors.
extern TaskDesc D_actor_342000_80164FF8;

extern u8             D_actor_342000_80164968;
extern u8             D_actor_342000_80164E30;
extern s8             D_8007216D;
extern s8             D_80114C11;
extern u16            D_801855DE;
extern TaskDesc       D_80187150;
extern GpAreaApplyRec D_8018FB6C[];
extern u16            D_8018FBC8;

void func_80180FE4(s32 arg0, s32 arg1, s32 arg2);
void func_8018507C(void);

/// The event task's leaf steps, inlined here; `actor_342000_3.c` carries the
/// same bodies as out-of-line functions (`func_actor_342000_801641FC`,
/// `801642B4`, `801642D4`, `80164154`).
static inline void Actor342000_KillFx(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_5C != NULL) {
        taskKill(work->field_5C);
    }
    if (work->field_60 != NULL) {
        taskKill(work->field_60);
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
    gGameSession->at4.loc.room   = 7;
    D_8007216D                   = 7;
    gGameSession->eventRoomIndex = 6;
    gGameSession->field_133      = 1;
    gGameSession->roomObjsDirty  = 1;
    Gp_ApplyAreaRecs(D_8018FB6C);
}

/// Event/sequence task body, idle while a cutscene, pause or mode switch is up.
/// State 0 allocates the `Actor342000EventWork` block and spawns the effect
/// actors (a spawn with `GameSession::skipEventIntro` set skips to state 4);
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
    if (D_801855DE != 0 || gGameSession->field_65 != 0 || D_80114C11 != 0 || Gp_StateF0.field_4 != 0) {
        return;
    }
    if (gGameSession->enemyCullZone != 0) {
        if (work->field_7E != 0) {
            SndEvt_EnqueueType7(0x5428000B, 0xA);
            work->field_7E = 0;
        }
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = (Actor342000EventWork*)memCalloc(0x80U, false);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(alloc, 0U, 0x80U);
                alloc->field_48         = gameGetPtrSlot(3);
                D_actor_342000_80165070 = arg0;
                alloc->field_4C         = (s32)Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
            }
            work = (Actor342000EventWork*)arg0->work;
            if ((u8)gGameSession->skipEventIntro == 0) {
                msg.field_0 = gGameSession->at4.loc.stage;
                msg.field_1 = gGameSession->at4.loc.area;
                msg.field_2 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
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
            if (gGameSession->eventState == 0) {
                gGameSession->sceneClock = D_8018FBC8;
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
                work->field_78 = gGameSession->at4.loc.view;
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
                msg.field_0 = gGameSession->at4.loc.stage;
                msg.field_1 = gGameSession->at4.loc.area;
                msg.field_2 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
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

/// Fade task of the overlay's task table: its first tick allocates the
/// channel block and seeds every channel at 0xFF; each tick then draws the
/// full-screen fade overlay and steps the channels down by `spawnArg1`, killing
/// the task once `r` has gone negative.
void func_actor_342000_80163EAC(Task* arg0)
{
    Actor342000Fade* fade;
    Actor342000Fade* alloc;

    fade = (Actor342000Fade*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor342000Fade*)memCalloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

void func_actor_342000_80163F88(Task* task)
{
    Actor342000Work* work;
    GsCOORDINATE2*   coord;

    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    work  = (Actor342000Work*)task->work;

    coord->sub = work->field_2A4;
    taskKill(task);
}

/// Message 0x7D5 handler of both of the overlay's message tables: sets the
/// draw bits of the task's `TmdObject` from the mode in `arg2`. Mode 0 sets
/// 0x80 and clears 0x4, mode 1 clears both, mode 2 sets both.
void func_actor_342000_80163FB8(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)arg0->extra;
    switch (arg2) {
        case 0:
            extra->flags = (extra->flags | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Message 0x7D4 handler of the table at `D_actor_342000_801648A8`: copies
/// the payload onto the model's root coordinate, the three longs as its
/// translation and the three angles as its rotation (Y, then X, then Z), and
/// marks the coordinate dirty.
void func_actor_342000_80164034(Task* task, s32 arg1, Actor342000Move* arg2)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = arg2->field_0;
    coord->coord.t[1] = arg2->field_4;
    mtx               = &coord->coord;
    coord->coord.t[2] = arg2->field_8;
    Gfx_RotMatrixY(mtx, arg2->field_12, 1);
    Gfx_RotMatrixX(mtx, arg2->field_10, 0);
    Gfx_RotMatrixZ(mtx, arg2->field_14, 0);
    coord->flg = 0;
}

void func_actor_342000_801640C0(Task* arg0, s32 arg1, Actor342000Move* arg2)
{
    Actor342000Work* work;
    GsCOORDINATE2*   coord;

    work              = (Actor342000Work*)arg0->work;
    coord             = &work->coord;
    coord->coord.t[0] = arg2->field_0;
    coord->coord.t[1] = arg2->field_4;
    coord->coord.t[2] = arg2->field_8;
    work->field_274   = arg2->field_10;
    work->field_278   = arg2->field_12;
    work->field_27C   = arg2->field_14;
    work->coord.flg   = 0;
}

void func_actor_342000_80164110(Task* arg0, s32 arg1, Actor342000Cmd* arg2, Actor342000Move* arg3)
{
    Actor342000Work* work;

    work = (Actor342000Work*)arg0->work;
    if (arg2->field_2 == 0xA) {
        work->field_264.vx = arg3->field_0;
        work->field_264.vy = arg3->field_4;
        work->field_264.vz = arg3->field_8;
    }
    work->field_2AA = arg2->field_2;
}

void func_actor_342000_80164154(void)
{
    gGameSession->at4.loc.room   = 7;
    D_8007216D                   = 7;
    gGameSession->eventRoomIndex = 6;
    gGameSession->field_133      = 1;
    gGameSession->roomObjsDirty  = 1;
    Gp_ApplyAreaRecs(D_8018FB6C);
}

void func_actor_342000_801641B4(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_50 != NULL) {
        Task_CallExit(work->field_50);
    }
    work->field_50 = NULL;
}

void func_actor_342000_801641FC(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_5C != NULL) {
        taskKill(work->field_5C);
    }
    if (work->field_60 != NULL) {
        taskKill(work->field_60);
    }
    work->field_5C = NULL;
    work->field_60 = NULL;
}

void func_actor_342000_80164260(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_7A == 0) {
        SndEvt_EnqueueType6(0x54280005, 0, 0);
        work->field_7A = 1;
    }
}

void func_actor_342000_801642B4(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->work;
    work->field_68 = arg0;
    work->field_6A = 0;
}

void func_actor_342000_801642D4(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->work;
    work->field_70 = arg0;
    work->field_72 = 0;
}

extern s8 D_8007272D;

void func_actor_342000_801642F4(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_7C == 0) {
        Gp_StateF0.field_6       = 0;
        Gp_StateF0.field_1       = 0xF;
        Gp_StateF0.field_0       = 0;
        Gp_StateF0.field_2       = 0;
        Gp_StateF0.field_3       = 0;
        gGameSession->flowFlags |= 0x80;
        D_8007272D               = 0xD;
        work->field_7C           = 1;
    }
}

void func_actor_342000_80164364(s32 arg0)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    Gp_DispatchMsg(work->field_48, 0x3F3, arg0, 0);
}

/// Warps the slot-3 task to the overlay's fixed placement (0x3E9), installs
/// the animation set the current weapon selects (`D_80073BA9 + 1` for the
/// alternate block, `+ 0x22` for the base one, sent as 0x3E8 to the slot
/// `gameGetPtrSlot(3)` returns), raises 0x3F3, kills the child in
/// `field_64`, and cancels any pending CD command replacement.
void func_actor_342000_8016439C(void)
{
    Actor342000EventWork* work;
    GpAnimArg             msg;
    s32                   weaponId;
    s32                   anim;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    Gp_DispatchMsg(work->field_48, 0x3E9, (s32)&D_actor_342000_80164948, 0);
    func_8018507C();
    weaponId            = D_80073BA9;
    anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.animBlock.index = anim;
    msg.field_4         = 1;
    msg.field_8         = 0;
    msg.field_C         = 0;
    msg.field_10        = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
    Gp_DispatchMsg(((Actor342000EventWork*)D_actor_342000_80165070->work)->field_48, 0x3F3, 1, 0);
    if (work->field_64 != NULL) {
        taskKill(work->field_64);
        work->field_64 = NULL;
    }
    CdCmd_CancelReplaceAndActivate();
}

/// Script callback: queues the replacement overlay load.
void func_actor_342000_8016447C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Script callback: queues the overlay load.
void func_actor_342000_8016449C(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Script callback: restores the stream random state, then cancels the
/// pending overlay replacement and activates the loaded one.
void func_actor_342000_801644BC(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
