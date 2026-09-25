#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

/// Main-executable counter whose lowest bit the flicker alternates on.
extern s32 D_80070F70;

/// 0x30-byte scratch `func_actor_105100_80132C2C` takes from `G_SCRATCH_HEAD`:
/// `delta` is the player offset whose length feeds `Gp_ComputeDamage`, and
/// `ofs` is the spark offset handed to `Gp_SpawnEff`.
typedef struct Actor105100HitScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ byte    pad_10[0x10];
    /* 0x20 */ SVECTOR ofs;
    /* 0x28 */ byte    pad_28[8];
} Actor105100HitScratch;
STATIC_ASSERT_SIZEOF(Actor105100HitScratch, 0x30);

/// The work block of the glowing projectile this overlay spawns as a second
/// enemy task, the 0x80 bytes its spawn handler asks `memCalloc` for. `obj0`
/// is the body the collision lists carry and `rec20` the contact record whose
/// key ends the flight; `obj38` is the second body, unlinked beside the first
/// when the task is destroyed. The teardown handler reaches both of them
/// through `Actor105100Work`, whose two bodies sit at the same offsets --
/// whether the two blocks are really one type is unsettled, since nothing
/// else in this overlay reads the bytes they have in common.
///
/// `field_70` accumulates the per-axis jitter the hover step applies to the
/// coordinate, and the step holds that accumulation inside a fixed bound.
/// `field_78` counts frames within the current step and `field_7A` selects
/// it, `field_7C` is the speed the flight doubles each frame up to a cap, and
/// `field_7E` is the size the billboard is drawn at.
typedef struct Actor105100ProjWork {
    /* 0x00 */ GpObj        obj0;
    /* 0x20 */ GpRec18      rec20;
    /* 0x38 */ GpObj        obj38;
    /* 0x58 */ GpActorD4Rec pose;
    /* 0x70 */ SVECTOR      field_70;
    /* 0x78 */ u16          field_78;
    /* 0x7A */ s16          field_7A;
    /* 0x7C */ u16          field_7C;
    /* 0x7E */ s16          field_7E;
} Actor105100ProjWork;
STATIC_ASSERT_SIZEOF(Actor105100ProjWork, 0x80);

/// 0x38-byte scratch the projectile's per-frame handler takes from
/// `G_SCRATCH_HEAD`: `rot` is the jitter offset it adds to the coordinate and,
/// in the launch step, the rotation `RotMatrix` turns into `mat` before the
/// GTE multiplies it into the coordinate; `vec` is the offset to the player
/// the aiming step orients along. The size is pinned by the handler, which
/// claims and releases the block by decrementing and incrementing the scratch
/// head a whole element at a time.
typedef struct Actor105100ProjScratch {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ VECTOR  vec;
    /* 0x30 */ SVECTOR rot;
} Actor105100ProjScratch;
STATIC_ASSERT_SIZEOF(Actor105100ProjScratch, 0x38);

/// The actor's animation work area. `field_58E` is the pose the animation
/// tables are indexed by and `field_598` the step of the schedule that drives
/// it. `field_592` is unsigned in this overlay's view -- the accumulation in
/// `func_actor_105100_80136408` loads it `lhu` and adds with `addu` -- so the
/// signed compares against it in `func_actor_105100_801360AC` and
/// `func_actor_105100_801361C4` cast at the use (`(s16)work->field_592`)
/// instead of retyping the field.
///
/// The three objects at 0x47C / 0x4E4 / 0x51C are `GpObj` collision bodies,
/// each with the `GpRec18` run that follows it as its table: the first hangs
/// off `&coord[3]`, the second off the model's own coordinate and the third
/// off the third-party model's.
typedef struct Actor105100Work {
    /* 0x000 */ GpObj      obj0;
    /* 0x020 */ byte       pad_20[4];
    /* 0x024 */ s32        field_24;
    /* 0x028 */ byte       pad_28[0x10];
    /* 0x038 */ GpObj      obj38;
    /* 0x058 */ byte       pad_58[0x2B4];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C;
    /* 0x45C */ MATRIX     field_45C;
    /* 0x47C */ GpObj      obj47C;
    /* 0x49C */ GpRec18    field_49C[3];
    /* 0x4E4 */ GpObj      obj4E4;
    /* 0x504 */ GpRec18    field_504[1];
    /* 0x51C */ GpObj      obj51C;
    /* 0x53C */ GpRec18    field_53C[1];
    /* 0x554 */ GpEffArg   field_554; // record the death effect is spawned with
    /* 0x55C */ GpEffWork* field_55C;
    /* 0x560 */ MATRIX     field_560;
    /* 0x580 */ s32        field_580;
    /* 0x584 */ s32        field_584;
    /* 0x588 */ s32        field_588;
    /* 0x58C */ s16        field_58C; // hit cooldown; armed from `Gp_GetIdParam2` of the hitting record
    /* 0x58E */ u16        field_58E;
    /* 0x590 */ s16        field_590;
    /* 0x592 */ u16        field_592;
    /* 0x594 */ s16        field_594;
    /* 0x596 */ s16        field_596;
    /* 0x598 */ s16        field_598;
    /* 0x59A */ u16        field_59A;
    /* 0x59C */ u16        field_59C;
    /* 0x59E */ u16        field_59E;
    /* 0x5A0 */ s16        field_5A0; // sign of the player offset dotted with the player's facing axis
    /* 0x5A2 */ s16        field_5A2; // non-zero while the attack body is running; the body clears it when it finishes
    /* 0x5A4 */ s16        field_5A4; // state of the attack body `func_actor_105100_80133CE4`
    /* 0x5A6 */ u16        field_5A6; // its frame counter
    /* 0x5A8 */ s16        field_5A8;
    /* 0x5AA */ s16        field_5AA;
    /* 0x5AC */ s16        field_5AC;
    /* 0x5AE */ u16        field_5AE;
    /* 0x5B0 */ s16        field_5B0;
    /* 0x5B2 */ s16        field_5B2;
    /* 0x5B4 */ s16        field_5B4;
    /* 0x5B6 */ s16        field_5B6;
    /* 0x5B8 */ u16        field_5B8;
    /* 0x5BA */ s16        field_5BA; // 1 while the death cutscene message is pending; cleared after 0x13F4, gates step 3
    /* 0x5BC */ s16        field_5BC;
    /* 0x5BE */ u16        field_5BE; // accumulated damage toward the 0x1A4 stagger threshold
    /* 0x5C0 */ u16        field_5C0; // frames the stagger window stays open; loaded 0xBC on a hit
    /* 0x5C2 */ s16        field_5C2;
    /* 0x5C4 */ byte       pad_5C4[4];
} Actor105100Work;

/// The child spawner allocates this 0x50-byte collision and reaction block:
/// a GpObj, one contact record, and the state the reaction handlers drive.
/// It is also a second view of the work area's 0x38 record, held by the per-frame handler
/// `func_actor_105100_801354E8`: `field_40` is the reaction it dispatches on,
/// `field_48` the countdown that reaction runs for and `field_4E` the pose the
/// schedule is stepped through. Those bytes are `obj38`, a `GpObj`, to the
/// object-list code, so the handler casts the work pointer to this view rather than
/// reaching them through `Actor105100Work`.
///
/// The reaction sub-handlers keep their own state in the same record:
/// `field_44` is the approach point `Actor105100Work`'s `obj38` vector is
/// aimed at and `field_46` the pass they are on -- 0 builds that aim, 1 walks
/// the coordinate along it. A `GpObj` cannot carry either, `field_44` sitting
/// over its `ctx.recs` collision pointer, which is why
/// `func_actor_105100_801359B4` reads them here and takes the aim vector
/// itself from `Actor105100Work::obj38`.
typedef struct Actor105100Rec {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[1];
    /* 0x38 */ byte    pad_38[8];
    /* 0x40 */ s16     field_40;
    /* 0x42 */ s16     field_42;
    /* 0x44 */ s16     field_44;
    /* 0x46 */ s16     field_46;
    /* 0x48 */ s16     field_48;
    /* 0x4A */ byte    pad_4A[4];
    /* 0x4E */ u16     field_4E;
} Actor105100Rec;
STATIC_ASSERT_SIZEOF(Actor105100Rec, 0x50);

/// Third view of the work area, held by the schedule entry
/// `func_actor_105100_8013329C`: the `field_5A8` / `field_5AA` pair taken as
/// one word. Every other handler reads the halves apart, so they are `s16`
/// fields of `Actor105100Work`; this entry gates on both at once (a single
/// `lw` at 0x5A8) and therefore reaches the pair through this view.
typedef struct Actor105100Gate {
    /* 0x000 */ byte pad_0[0x5A8];
    /* 0x5A8 */ s32  field_5A8;
} Actor105100Gate;
STATIC_ASSERT_SIZEOF(Actor105100Gate, 0x5AC);

void func_actor_105100_80132414(GsCOORDINATE2* arg0, s32 arg1);
void func_actor_105100_801327B4(GpEnemy* arg0, Task* arg1);
void func_actor_105100_80132AA0(GpEnemy* arg0, Task* arg1);
void func_actor_105100_80132C2C(Task* arg0);
void func_actor_105100_80133134(Task* arg0);
void func_actor_105100_8013329C(Task* arg0, GpEnemy* arg1);
void func_actor_105100_8013345C(Task* arg0, GpEnemy* arg1);
void func_actor_105100_801336B8(Task* arg0, GpEnemy* arg1);
void func_actor_105100_80133A14(Task* arg0, GpEnemy* arg1);
void func_actor_105100_80133CE4(Task* arg0);
void func_actor_105100_80134130(Task* arg0);
void func_actor_105100_80134284(GpEnemy* arg0, Task* arg1);
void func_actor_105100_801347D4(GpEnemy* arg0, Task* arg1);
void func_actor_105100_80134B00(GpEnemy* arg0, Task* arg1);
void func_actor_105100_80135278(GpEnemy* arg0, Task* arg1);
void func_actor_105100_801354E8(GpEnemy* arg0, Task* arg1);
void func_actor_105100_80135674(Task* arg0);
void func_actor_105100_801359B4(Task* arg0);
void func_actor_105100_80135B40(Task* arg0);
void func_actor_105100_80135E54(Task* arg0);
void func_actor_105100_80135F50(Task* arg0);
void func_actor_105100_80135FCC(Task* arg0);
void func_actor_105100_801360AC(Task* arg0);
void func_actor_105100_801361C4(Task* arg0);
void func_actor_105100_801362A0(Task* arg0);
void func_actor_105100_80136318(Task* arg0);
void func_actor_105100_80136408(Task* arg0);
void func_actor_105100_801364CC(Task* arg0);
void func_actor_105100_80136524(Task* arg0);
void func_actor_105100_80136574(Task* arg0, MATRIX* arg1, s16 arg2, s32 arg3);
void func_actor_105100_801366D8(GpEnemy* arg0, Task* arg1);
void func_actor_105100_80136788(GpEnemy* arg0, Task* arg1);

void    func_8017FC40(GsCOORDINATE2* arg0, s32 arg1, u16 arg2);
void    func_8018294C(Task* arg0);
void    func_800B4114(Actor105100Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);
MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern s32 D_80115728;

/// Main-executable globals with no module header yet: a `D_80114C12` of 1 or a
/// live `D_80071075` means a cutscene is already up, so the death handler skips
/// message 0x13F4.
extern u8 D_80071075;
extern s8 D_80114C12;

/// Main-executable global with no module header yet: the remaining-enemy count
/// `func_actor_105100_80136318` tests to decide whether the fight is over.
extern s16 D_80073BA0;

/// The run of HP caps at 0x8014139C; `func_actor_105100_80135FCC` reads the
/// first entry. Declared as an aggregate on purpose: a bare `extern u16` makes
/// `true_dependence` (`sched.c:846`) drop the dependence between the store to
/// `GpEnemy::hp` and this load -- the store is in-struct with a
/// varying address, this load a scalar MEM at a fixed one -- and sched2 then
/// hoists this load above the store, ahead of the `sll`.
extern u16 D_actor_105100_8014139C[1];

/// The s16 animation-id run at 0x801414C8, one entry per work state at
/// `Actor105100Work::field_58E`; `func_actor_105100_80136408` reads the entry
/// the new state selects before it re-queues every slot.
extern s16 D_actor_105100_801414C8[];

/// The spawn's pair tables. `Gp_PackPair` packs the `GpU16Pair` at 0x80141380
/// into the work's third list node (`Actor105100Work::obj4E4.key`), and the
/// `GpPairSrcE` at 0x80141398 is the pair source the context points at with
/// `GpEnemy::param` -- its `hpMax` seeds the enemy's HP.
extern GpU16Pair  D_actor_105100_80141380;
extern GpPairSrcE D_actor_105100_80141398;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_105100_80141488[];

/// Animation block the attack body hands the player with message 0x3F4.
extern void* D_actor_105100_801414B4;

/// The approach points the `field_40 == 1` reaction walks the model through,
/// indexed by `Actor105100Rec::field_44`. Only the x and z halves are read: the
/// reaction subtracts the model's current position and walks the resulting
/// planar delta.
extern SVECTOR D_actor_105100_80141418[6];

extern SVECTOR D_actor_105100_801413E8[];
extern s16     D_actor_105100_80141448[];
extern s16     D_actor_105100_80141450[];

/// Where each spawned projectile starts relative to the parent's coordinate,
/// indexed by `Actor105100Work::field_5AE`.
extern SVECTOR D_actor_105100_801414E0[];

/// The enemy task's state handlers, indexed by `Task::state`: spawn/setup,
/// per-frame tick and teardown.
const GpEnemyTaskFuncTable3 D_actor_105100_80131E24 = {
    {
        func_actor_105100_801327B4,
        func_actor_105100_80132AA0,
        func_actor_105100_80134284,
    },
};

/// Projects `coord` onto two `POLY_FT4` billboards, lights `Gp_RoomCoords[2]`
/// as a point light at that position, and traces the ground for the ground-quad
/// helper when `Gp_State1C->groundTrace` is set.
void func_actor_105100_80131EBC(GsCOORDINATE2* coord, s16 size)
{
    GsCOORDINATE2  ground;
    POLY_FT4*      prim;
    s16            intensity;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = (s16)(shifted >> 0x11);
    light->head.b               = (s16)(shifted >> 0x12);
    light->head.u.at.local.t[0] = (s32)coord->coord.t[0];
    light->head.u.at.local.t[1] = (s32)coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    scratch                     = SCRATCH_HEAD_ADDR;
    block                       = (GpRingScratch*)SCRATCH_HEAD_AT(scratch, void) - 1;
    block->vec.vx               = *(u16*)&coord->workm.t[0];
    alias                       = block;
    vy                          = *(u16*)&coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc                             = alias;
    sc->vec.vy                     = vy;
    sc->vec.vz                     = *(u16*)&coord->workm.t[2];
    Gp_LcgState                    = random;
    SCRATCH_HEAD_AT(scratch, void) = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&block->otz);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        sc->otz              = (s32)(sc->otz + 1);
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2EU;
        *(u16*)&prim->tpage  = 0x29;
        if (D_80070F70 & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2F;
        prim->tpage          = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_State1C->groundTrace != 0) {
            if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                func_actor_105100_80132414(&ground, (s32)(s16)(outerSize * 2));
            }
        }
    }
    SCRATCH_HEAD(void) =
        (u8*)SCRATCH_HEAD(void) + sizeof(GpRingScratch);
}

/// Draws a flat textured quad on the ground under the actor: the corners of
/// the unit quad `D_80111E38`, scaled by `arg1` and turned into view
/// orientation, are placed around `arg0`'s world translation and projected.
/// When all four project, a semi-transparent `POLY_FT4` is queued one step
/// behind their depth, its texture alternating between two frames with the
/// display's animation frame.
void func_actor_105100_80132414(GsCOORDINATE2* arg0, s32 arg1)
{
    void**                scratch;
    u8*                   head;
    OverlayGroundScratch* sc;
    POLY_FT4*             prim;
    GpQuadCorner*         tbl;
    SVECTOR*              v;
    s32                   i;
    s32                   otz;
    s32                   flag;
    s32                   u;
    s32                   prod;

    scratch = SCRATCH_HEAD_ADDR;
    head    = (u8*)SCRATCH_HEAD_AT(scratch, void) - sizeof(OverlayGroundScratch);

    SOFT_TOUCH_REG(head);
    SCRATCH_HEAD_AT(scratch, void) = head;
    sc                             = (OverlayGroundScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = sc->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy0);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_ldv3(&sc->vec[1], &sc->vec[2], &sc->vec[3]);
        gte_rtpt();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_stszotz(&otz);
            otz++;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);

            prim->r0    = 0x30;
            prim->g0    = 0x20;
            prim->b0    = 0x20;
            prim->tpage = 0x28;
            prim->clut  = 0x428C;
            setSemiTrans(prim, 1);
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
            prim->v0 = 0x38;
            prim->u0 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
            prim->v1 = 0x38;
            prim->u1 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
            prim->v2 = 0x57;
            prim->u2 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
            prim->v3 = 0x57;
            prim->u3 = u;
            prim->x0 = *(u16*)&sc->sxy0.vx;
            prim->y0 = *(u16*)&sc->sxy0.vy;
            prim->x1 = *(u16*)&sc->sxy1.vx;
            prim->y1 = *(u16*)&sc->sxy1.vy;
            prim->x2 = *(u16*)&sc->sxy2.vx;
            prim->y2 = *(u16*)&sc->sxy2.vy;
            prim->x3 = *(u16*)&sc->sxy3.vx;
            prim->y3 = *(u16*)&sc->sxy3.vy;
            addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(sizeof(OverlayGroundScratch));
}

/// Spawn/setup handler. It allocates the 0x5C4-byte work block and hangs it off
/// the task, points the model object at the block's two `MATRIX`es (0x45C the
/// light matrix, 0x43C the colour one) and fills the context's coordinate,
/// pair source and HP (`field_40`, seeded from the record's `hpMax`).
///
/// The block's 0x14-prefix then becomes the `GpAnimCtx`: `func_800B3F84` loads
/// the animation data into it over the nineteen `GpAnimSlot`s, and slots 1..18
/// are reset. The three list nodes at 0x47C / 0x4E4 / 0x51C are linked into the
/// global object lists with their collision tables (`Gp_InitRec18Table`), which
/// also sets each node's 0x8000 "last element" flag -- then the second node's is
/// cleared again. `&coord[3]` -- the actor's fourth coordinate -- is what the
/// first node, `field_554` and the context's `field_18` all hang off.
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler; otherwise the task moves to the tick handler (`state` 1).
void func_actor_105100_801327B4(GpEnemy* arg0, Task* arg1)
{
    Actor105100Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpRec18*         records1;
    GpRec18*         records2;
    GpRec18*         records3;
    s32              i;

    obj   = arg1->extra;
    coord = obj->coords;
    work  = memCalloc(0x5C4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_45C;
    obj->colorMtx  = &work->field_43C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &((TmdObject*)arg1->extra)->coords[3];
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0x64;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &D_actor_105100_80141398;
    arg0->recs                 = work->field_49C;
    arg0->hp                   = D_actor_105100_80141398.hpMax;
    work->field_554.coord      = &((TmdObject*)arg1->extra)->coords[3];
    work->field_554.spawnArgLo = 0x500;
    work->field_554.spawnArgHi = 3;
    func_800B3F84((GpAnimCtx*)work, D_actor_105100_80141488, (TmdObject*)obj, work->field_30C,
                  ((ActorAnimRig19*)work)->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_560       = coord->coord;
    work->field_594       = 0x2800;
    work->field_5A8       = 1;
    work->field_59E       = 0xF;
    work->field_59A       = 0x96;
    work->obj47C.coord    = &((TmdObject*)arg1->extra)->coords[3];
    records1              = work->field_49C;
    work->obj47C.ctx.recs = records1;
    work->obj47C.pos.vx   = 0;
    work->obj47C.pos.vy   = 0x1F4;
    work->obj47C.pos.vz   = 0;
    work->obj47C.key      = 0x30033;
    work->obj47C.radius   = 0x320;
    work->obj47C.flags    = 1U;
    Gp_LinkObj(2, &work->obj47C);
    Gp_InitRec18Table(records1, 3, 0);
    work->obj47C.flags    = (u16)(work->obj47C.flags | 0x8000);
    work->obj51C.coord    = ((TmdObject*)arg1->extra)->coords;
    records2              = work->field_53C;
    work->obj51C.ctx.recs = records2;
    work->obj51C.pos.vx   = 0;
    work->obj51C.pos.vy   = 0;
    work->obj51C.pos.vz   = -0x12C;
    work->obj51C.key      = 0;
    work->obj51C.radius   = 0x4B0;
    work->obj51C.flags    = 1U;
    Gp_LinkObj(2, &work->obj51C);
    Gp_InitRec18Table(records2, 1, 0);
    work->obj51C.flags    = (u16)(work->obj51C.flags | 0x8000);
    work->obj4E4.coord    = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    records3              = work->field_504;
    work->obj4E4.ctx.recs = records3;
    work->obj4E4.pos.vx   = 0;
    work->obj4E4.pos.vy   = 0;
    work->obj4E4.pos.vz   = 0;
    work->obj4E4.key      = Gp_PackPair(&D_actor_105100_80141380, 5);
    work->obj4E4.radius   = 0x1F4;
    work->obj4E4.flags    = 1U;
    Gp_LinkObj(3, &work->obj4E4);
    Gp_InitRec18Table(records3, 1, 0);
    work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
    arg1->state        = 1;
}

void func_actor_105100_80132AA0(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Actor105100Work* work;
    s32              state;
    s32              one;

    obj   = arg1->extra;
    state = Gp_StateF0.field_4;
    work  = arg1->work;
    coord = obj->coords;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags               = 0;
    arg0->node.state.b.flags = 8;
    if (work->field_5BC != 0) {
        SndEvt_EnqueueType9(0x40000000);
        work->field_5BC = 0;
    }
    goto default_body;
case1:
    func_actor_105100_801364CC(arg1);
    func_actor_105100_80136524(arg1);
    goto join_12;
case2:
    obj->flags               = 0x80;
    arg0->node.state.b.flags = one;
join_12:
    SOFT_USE_REG(work);
    if (work->field_5BC == 0) {
        SndEvt_EnqueueType8(0x40000000);
    }
    work->field_5BC = state;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        func_actor_105100_80135E54(arg1);
    }
    func_actor_105100_80132C2C(arg1);
    func_actor_105100_80133134(arg1);
    if (work->field_5A2 != 0) {
        func_actor_105100_80133CE4(arg1);
    }
    func_actor_105100_80136408(arg1);
    func_actor_105100_80134130(arg1);
    func_actor_105100_80136574(arg1, &work->field_560, work->field_594, 1);
    if (work->field_5A8 != 0) {
        func_8018294C(arg1);
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    func_actor_105100_801364CC(arg1);
    func_actor_105100_80136524(arg1);
}

/// Per-frame hit handler: walks the three `field_49C` contact records as
/// 0x18-byte slots of the work block itself. A type-2 id lands only while the
/// `field_58C` cooldown is clear. Damage is the player distance through
/// `Gp_ComputeDamage`, quadrupled on a successful `Gp_RollEnemyChance`, and
/// halved (or zeroed for 0x8000 ids) while `field_5A8` is 1. The id parameter
/// may set a stagger flag, `Gp_SetObjFlag2`, or `Gp_SetObjFlag4`. HP is applied
/// through `func_800E2C78` / `func_800DA6E8`; at 0 the schedule goes to step 7,
/// and accumulated `field_5BE` past 0x1A4 (or the stagger flag) sends it to
/// step 6. A new id sparks `func_800FDB18` once, and `Gp_GetIdParam2` arms the
/// cooldown. The tail releases both record tables and steps `field_5C0`.
void func_actor_105100_80132C2C(Task* arg0)
{
    s32                    flag;
    s32                    lastId;
    Actor105100HitScratch* sc;
    Actor105100Work*       work;
    GpEnemy*               ctx;
    GsCOORDINATE2*         coord;
    Actor105100Work*       rec;
    u16                    timer;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    register u32           orig asm("s0");
    u32                    damage;
    s32                    snd;
    s32                    pan;
    s16                    amount;
    u32                    param;
    u16                    hp;
    GpEffWork*             eff;
    s32                    key;
    s32                    wait;

    flag = 0;
    SCRATCH_PUSH(Actor105100HitScratch);
    sc = SCRATCH_HEAD(Actor105100HitScratch);
    SOFT_TOUCH_REG(sc);
    lastId = 0;
    coord  = ((TmdObject*)arg0->extra)->coords;
    work   = arg0->work;
    ctx    = arg0->spawnArg2;
    if (work->field_58C != 0) {
        timer           = (u16)work->field_58C - 1;
        work->field_58C = timer;
        if ((timer << 16) <= 0) {
            work->field_58C = 0;
        }
    }
    rec = work;
    do {
        if ((u16)(rec->field_49C[0].key >> 16) == 2 && work->field_58C == 0) {
            dx           = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vx = dx;
            dy           = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            sc->delta.vy = dy;
            dz           = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            sc->delta.vz = dz;
            orig         = Gp_ComputeDamage((u32)rec->field_49C[0].key,
                                            SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
            damage       = orig;
            if (Gp_RollEnemyChance((GpEnemy*)ctx, (u32)rec->field_49C[0].key, 0) != 0) {
                damage = (orig << 16) >> 14;
                Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[3], 0, NULL);
            }
            SOFT_TOUCH_REG2(orig, damage);
            if (work->field_5A8 == 1) {
                if (rec->field_49C[0].key & 0x8000) {
                    damage = 0;
                } else {
                    damage = ((s16)damage + ((u32)(damage << 16) >> 31)) >> 1;
                }
                sc->ofs.vx = 0;
                sc->ofs.vy = 0;
                sc->ofs.vz = 0xC8;
                Gp_SpawnEff(0x601AC, &((TmdObject*)arg0->extra)->coords[3], 0, &sc->ofs);
                snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x4033000D;
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            }
            amount = damage;
            func_800DA6E8(&ctx->node, amount, 0);
            if (amount != 0) {
                param = Gp_GetIdParam0(rec->field_49C[0].key) & 0xFFFF;
                switch (param) {
                    case 0:
                        break;
                    case 1:
                        if ((rec->field_49C[0].key & 0x3F) != 0x1C) {
                            flag = 1;
                        }
                        break;
                    case 2:
                        if (work->field_5A8 == 0) {
                            Gp_SetObjFlag2(ctx, rec->field_49C[0].key, 0);
                        }
                        break;
                    case 3:
                        if (work->field_5A8 == 0) {
                            Gp_SetObjFlag4(ctx, rec->field_49C[0].key, 0);
                        }
                        break;
                    case 4:
                        flag = 1;
                        break;
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                }
                func_800E2C78(ctx, rec->field_49C[0].key, (s16)damage, 0);
                hp      = ctx->hp - damage;
                ctx->hp = hp;
                if ((hp << 16) <= 0) {
                    work->field_596 = 7;
                    work->field_598 = 0;
                    eff             = work->field_55C;
                    if (eff != NULL) {
                        eff->task->state = 4;
                        work->field_55C  = NULL;
                    }
                } else {
                    work->field_5BE += damage;
                    work->field_5C0  = 0xBC;
                    if ((s16)work->field_5BE >= 0x1A4 || flag == 1) {
                        work->field_5C0 = 0;
                        work->field_5BE = 0;
                        work->field_596 = 6;
                        work->field_598 = 0;
                        eff             = work->field_55C;
                        if (eff != NULL) {
                            eff->task->state = 4;
                            work->field_55C  = NULL;
                        }
                        if (work->field_5B4 != 0) {
                            work->field_5B4 = 0;
                            work->field_5B6 = 1;
                            work->field_5AA = 0;
                        }
                        work->field_5AC = 0;
                    }
                }
                work->obj4E4.flags &= 0x7FFF;
                key                 = rec->field_49C[0].key;
                if (lastId != key) {
                    lastId = key;
                    SCHED_BARRIER();
                    func_800FDB18(Gp_GetIdParam1(key) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[3], NULL,
                                  &work->field_554);
                }
                wait = Gp_GetIdParam2(rec->field_49C[0].key);
                if (wait > 0) {
                    work->field_58C = wait;
                }
            }
        }
        rec = (Actor105100Work*)((u8*)rec + 0x18);
        SOFT_TOUCH_REG(rec);
    } while ((s32)rec < (s32)&work->obj38.pos);
    Gp_ClearRec18Occupied(work->field_49C);
    timer           = work->field_5C0 - 1;
    work->field_5C0 = timer;
    if ((timer << 16) <= 0) {
        work->field_5BE = 0;
    }
    if (work->field_53C[0].flags & 1) {
        if ((work->field_53C[0].key & 0xFFFF0000) == 0x10000 && D_80073BA0 > 0) {
            work->field_5A2      = 1;
            Gp_StateC08.field_6 |= 1;
        }
        Gp_ClearRec18Occupied(work->field_53C);
    }
    SCRATCH_POP(Actor105100HitScratch);
}

/// The enemy's step dispatcher, run every frame out of the `field_596` schedule
/// the three handlers below this one step through. Bit 3 of `Gp_StateF0.field_1D`
/// is a reset request: it is cleared here and the block is put back on step 6
/// with the schedule and the animation re-arm both dropped.
///
/// Step 7 is terminal -- `func_actor_105100_80136318` retires the enemy and the
/// task stops being dispatched -- so it falls straight through to the tail, as
/// does a step outside 0..7. The tail runs the shared post-hit reaction
/// (`Gp_StateF0.field_1D` bit 2) and steps the `field_5AA` timer down while it is
/// positive. Step 0 also raises bit 1 of `Gp_StateF0.field_1D` once the HP drops
/// under the cap in `D_actor_105100_8014139C`.
void func_actor_105100_80133134(Task* arg0)
{
    Actor105100Work* work;
    GpEnemy*         ctx;
    s16              state;

    work = arg0->work;
    ctx  = arg0->spawnArg2;
    if (Gp_StateF0.field_1D & 8) {
        Gp_StateF0.field_1D &= 0xF7;
        work->field_596      = 6;
        work->field_598      = 0;
        work->field_5A8      = 0;
    }
    state = work->field_596;
    switch (state) {
        case 0:
            if (ctx->hp < (s32)*D_actor_105100_8014139C) {
                Gp_StateF0.field_1D |= 2;
            }
            func_actor_105100_8013329C(arg0, ctx);
            break;
        case 1:
            func_actor_105100_8013345C(arg0, ctx);
            break;
        case 2:
            func_actor_105100_801336B8(arg0, ctx);
            break;
        case 3:
            func_actor_105100_80133A14(arg0, ctx);
            break;
        case 4:
            func_actor_105100_80135F50(arg0);
            break;
        case 5:
            func_actor_105100_801360AC(arg0);
            break;
        case 6:
            func_actor_105100_801361C4(arg0);
            break;
        case 7:
            func_actor_105100_80136318(arg0);
        default:
            break;
    }
    if (Gp_StateF0.field_1D & 4) {
        func_actor_105100_80135FCC(arg0);
    }
    if (work->field_5AA > 0) {
        work->field_5AA = (u16)work->field_5AA - 1;
    }
}

/// The run of poses at 0x801413A8 this step's reroll picks from, one `s16`
/// entry per draw. Declared as an aggregate on purpose: a bare `extern u16`
/// makes `true_dependence` (`sched.c:846`) drop the dependence between the
/// entry load and the `sh` to `Actor105100Work::field_598`, and sched2 then
/// sinks that store past the `sw` of the LCG state instead of leaving the
/// lookup and the pose store adjacent at the end of the block.
extern u16 D_actor_105100_801413A8[16];

/// The enemy's aim-retry step, run every frame the schedule is on step 1.
///
/// The `field_59E` countdown at the top is the aim timer: it is stepped down
/// whenever the battle is not paused (`Gp_StateF0::field_0`), and on the frame
/// it runs out the arming state drops to 1 (aimed) and the 0xA-frame hold is
/// armed through `D_80062735`. `field_5A8` is the pair of gate flags and is
/// tested as one word -- see `Actor105100Gate`.
///
/// The reroll itself is the LCG: the state advances, the pose is the table
/// entry the high nibble selects, and the `field_5B2` interval counter counts
/// attempts until it reaches 3, which sends the schedule on to step 3 (the
/// thrown pose) instead of back to the reroll.
void func_actor_105100_8013329C(Task* arg0, GpEnemy* arg1)
{
    Actor105100Work* work;
    Actor105100Gate* gate;
    s16              state;
    s16              pose;
    u16              timer;
    u16              count;

    work = arg0->work;
    if (Gp_StateF0.field_0 == 0) {
        timer           = work->field_59E - 1;
        work->field_59E = timer;
        if ((timer << 16) <= 0) {
            Gp_ArmStateF0(1);
            D_80062735 = 0xA;
        }
    }
    gate = (Actor105100Gate*)work;
    if (gate->field_5A8 == 0) {
        work->field_596 = 4;
        work->field_598 = 0;
        return;
    }
    state = work->field_598;
    switch (state) {
        case 0:
            count           = work->field_59A - 1;
            work->field_59A = count;
            if ((count << 16) <= 0) {
                work->field_59A = 0;
                if (work->field_5B6 == 0) {
                    state = 2;
                    if (work->field_5B2 < 3) {
                        state = 1;
                    }
                    work->field_598 = state;
                    return;
                }
                work->field_598 = 3;
                work->field_5B6 = 0;
                return;
            }
            return;
        case 1: {
            u16* tbl = D_actor_105100_801413A8;
            u32  rnd = (Gp_LcgState * 5) + 0x71357911;

            pose            = (s16)tbl[(rnd >> 16) & 0xF];
            count           = (u16)work->field_5B2;
            Gp_LcgState     = rnd;
            work->field_598 = 0;
            count           = count + 1;
            work->field_5B2 = count;
            work->field_596 = pose;
            return;
        }
        case 2:
            work->field_596 = 3;
            work->field_598 = 0;
            work->field_5B2 = 0;
            return;
        case 3: {
            u16* tbl = D_actor_105100_801413A8;
            u32  rnd = (Gp_LcgState * 5) + 0x71357911;

            Gp_LcgState     = rnd;
            work->field_596 = (s16)tbl[(rnd >> 16) & 0xF];
            work->field_598 = 0;
            break;
        }
    }
}

/// The enemy descriptor run at 0x80141464 the spawn below draws from. Declared
/// as a scalar rather than an aggregate on purpose: only its address is taken,
/// so the two words `Gp_SpawnEnemyFromTable` splits it into are the function's
/// addend, not a load this function has to model.
extern TaskDesc D_actor_105100_80141464;

/// The enemy's summon step, run every frame the schedule is on step 1. It is
/// the half of the appearance that runs before the model shows: sub-step 0
/// seeds the closing pose (3) and zeroes the spawn timer `field_59C` and the
/// spawned count `field_5AE`, arms the `field_5AC` gate the spawn tests, and
/// draws the LCG into `field_59A`, the aim window (`0x9E` .. `0xBD`).
///
/// Sub-step 1 holds everything on the animation frame counter `field_592`: not
/// until it passes `0x58` does the spawn timer start counting, and every expiry
/// sends one enemy out through `Gp_SpawnEnemyFromTable` -- up to four, gated on
/// `field_5AC` still being 1 -- with a fresh `0xF` .. `0x1E` interval drawn the
/// same way. The aim window steps down in parallel: at `0xF` it latches the
/// gate to 2, and at zero the step moves on to 2 with the hold pose `0xA`. The
/// single frame `field_592 == 0x58` is the cue: it builds the enemy's own id
/// into `field_580` and fires the type-6 event with the model coordinate's pan
/// and depth, which is what plays the summon as the model becomes visible.
///
/// Sub-step 2 waits out `field_592` to `0x1A`, then drops the whole step back
/// to pose 1, sub-step 0 and gate 0, rerolls `field_59A` to a `0` .. `0x3F`
/// window and fires the type-7 event on the id sub-step 1 built, clearing it.
void func_actor_105100_8013345C(Task* arg0, GpEnemy* arg1)
{
    Actor105100Work* work;
    GsCOORDINATE2*   coord;
    s16              step;
    s32              pan;
    u16              spawnTimer;
    u16              aimTimer;
    u32              rnd;
    u32              spawnRnd;
    u32              resetRnd;

    work  = arg0->work;
    step  = work->field_598;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (step) {
        case 0:
            work->field_58E = 3;
            work->field_59C = 0;
            work->field_5AE = 0;
            work->field_598 = 1;
            work->field_5AC = 1;
            rnd             = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState     = rnd;
            work->field_59A = ((rnd >> 16) & 0x1F) + 0x9E;
            return;
        case 1:
            if ((s16)work->field_592 >= 0x58) {
                spawnTimer      = work->field_59C - 1;
                work->field_59C = spawnTimer;
                if ((spawnTimer << 16) <= 0 && (s16)work->field_5AE < 4 && work->field_5AC == 1) {
                    Gp_SpawnEnemyFromTable(&D_actor_105100_80141464, 1, 0,
                                           (GpEnemy*)arg0->spawnArg2);
                    work->field_5AE += 1;
                    spawnRnd         = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState      = spawnRnd;
                    work->field_59C  = ((spawnRnd >> 16) & 0xF) + 0xF;
                }
            }
            if ((s16)work->field_59A == 0xF) {
                work->field_5AC = 2;
            }
            aimTimer        = work->field_59A - 1;
            work->field_59A = aimTimer;
            if ((aimTimer << 16) <= 0) {
                work->field_598 = 2;
                work->field_58E = 0xA;
            }
            if ((s16)work->field_592 == 0x58) {
                work->field_580 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40330004;
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_580, pan,
                                    (s8)gpGetObjDepth(coord));
                return;
            }
            return;
        case 2:
            if ((s16)work->field_592 >= 0x1A) {
                work->field_58E = 1;
                work->field_596 = 0;
                work->field_598 = 0;
                work->field_5AC = 0;
                resetRnd        = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = resetRnd;
                work->field_59A = (resetRnd >> 16) & 0x3F;
                SndEvt_EnqueueType7(work->field_580, 1);
                work->field_580 = 0;
            }
            break;
    }
}

/// The 16-entry run at 0x801413C8 this step's LCG draw picks `field_5B0`
/// from. Four entries are 0 (two children), five are 1 (three), seven are 2
/// (one).
extern u16 D_actor_105100_801413C8[16];

/// The enemy's split-spawn step, run every frame the schedule is on step 2.
/// It sits between the summon (`func_actor_105100_8013345C`) and the show
/// (`func_actor_105100_80133A14`).
///
/// Sub-step 0 seeds the closing pose (3), arms the `field_5AC` gate to 3,
/// zeroes the spawned count `field_5AE`, and draws the LCG into `field_5B0`
/// from `D_actor_105100_801413C8`.
///
/// Sub-step 1 holds on the animation frame counter `field_592`: the single
/// frame `field_592 == 0x1E` spawns the `0x800601A8` puff at the model's
/// coordinate -- offset 0/-0x6D6/0x320, life 0x3C -- and plays `...0007`.
/// Once the frame reaches `0x5A` it emits 2, 3 or 1 children through
/// `Gp_SpawnEnemyFromTable` according to `field_5B0`, loads the hold timer
/// from `D_actor_105100_80141448`, and plays `...0008` into `field_584`.
///
/// Sub-step 2 waits out `field_59A`, or bails as soon as `field_5AE` is still
/// 0, then moves on to pose `0xA` and fires the type-7 event on the id
/// sub-step 1 built.
///
/// Sub-step 3 waits `field_592` to `0x1A`, kills the puff, drops the gate,
/// and rerolls `field_59A` to a `0` .. `0x3F` window.
void func_actor_105100_801336B8(Task* arg0, GpEnemy* arg1)
{
    Actor105100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          pos;
    s16              step;
    s32              pan;
    s32              pan2;
    s32              snd;
    u16              timer;

    work  = arg0->work;
    step  = work->field_598;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (step) {
        case 0: {
            u16* tbl;
            u32  rnd;
            s16  kind;

            work->field_58E = 3;
            work->field_598 = 1;
            tbl             = D_actor_105100_801413C8;
            rnd             = (Gp_LcgState * 5) + 0x71357911;
            kind            = tbl[(rnd >> 16) & 0xF];
            Gp_LcgState     = rnd;
            work->field_5AE = 0;
            work->field_5AC = 3;
            work->field_5B0 = kind;
            return;
        }
        case 1:
            if ((s16)work->field_592 == 0x1E) {
                pos.vx          = 0;
                pos.vy          = -0x6D6;
                pos.vz          = 0x320;
                work->field_55C = Gp_SpawnEff(0x800601A8, ((TmdObject*)arg0->extra)->coords, 0x3C, &pos);
                snd             = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40330007;
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_592 >= 0x5A) {
                switch (work->field_5B0) {
                    case 0:
                        Gp_SpawnEnemyFromTable(&D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2);
                        Gp_SpawnEnemyFromTable(&D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2);
                        break;
                    case 1:
                        Gp_SpawnEnemyFromTable(&D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2);
                        Gp_SpawnEnemyFromTable(&D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2);
                        Gp_SpawnEnemyFromTable(&D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2);
                        break;
                    case 2:
                        Gp_SpawnEnemyFromTable(&D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2);
                        break;
                }
                work->field_59A = D_actor_105100_80141448[work->field_5B0];
                work->field_598 = 2;
                work->field_584 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40330008;
                pan2            = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_584, pan2, (s8)gpGetObjDepth(coord));
                return;
            }
            return;
        case 2:
            timer           = work->field_59A - 1;
            work->field_59A = timer;
            if ((timer << 16) <= 0 || (s16)work->field_5AE == 0) {
                work->field_598 = 3;
                work->field_58E = 0xA;
                SndEvt_EnqueueType7(work->field_584, 1);
                work->field_584 = 0;
            }
            break;
        case 3: {
            u32        rnd;
            GpEffWork* eff;

            if ((s16)work->field_592 >= 0x1A) {
                work->field_58E = 1;
                work->field_596 = 0;
                work->field_598 = 0;
                rnd             = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rnd;
                eff             = work->field_55C;
                work->field_59A = (rnd >> 16) & 0x3F;
                if (eff != NULL) {
                    eff->task->state = 4;
                }
                work->field_55C = NULL;
                work->field_5AC = 0;
            }
            break;
        }
    }
}

/// The appearance handler, the step the aim-retry schedule hands to once it
/// wants the enemy to show up. `field_598` is the sub-state it walks through:
///
/// Step 0 seeds the show pose (`field_58E` 3), the `field_59A` timer at its
/// high-water 0xBC and the `field_5AA` aim window, then spawns the
/// `0x800601A9` puff at the model's coordinate -- lifted to the top of the
/// model by the 0x1F4/-0x6D6/0 z-offset vector -- and announces the
/// appearance on the `...0009` sound. Step 1 waits the timer out and, on the
/// 0x5A midpoint, plays the `...000A` sound; when the timer expires it moves
/// the schedule to step 2, plays `...000B` and runs the pad lerp in. Step 2
/// holds the enemy on the `0x8000` list flag while `field_592` is 0xC and
/// releases it after, moving to step 3 once it passes 0x1B. Step 3 clears
/// both flags and, past 0x1C, puts the schedule back on step 0 with a fresh
/// timer drawn from the gameplay LCG.
///
/// The pan and depth are cast at the call rather than through locals: the
/// sign extension then occupies the argument's own temporary (`$s0`) instead
/// of `work`'s register, which is what the original allocation needs.
void func_actor_105100_80133A14(Task* arg0, GpEnemy* arg1)
{
    Actor105100Work* work;
    GsCOORDINATE2*   self;
    SVECTOR          pos;
    s32              snd;
    u16              timer;
    u32              rnd;

    work = arg0->work;
    self = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_598) {
        case 0:
            work->field_58E = 3;
            work->field_59A = 0xBC;
            work->field_5AA = 0x1E;
            work->field_5A8 = 0;
            work->field_5B4 = 1;
            work->field_598 = 1;
            pos.vx          = 0;
            pos.vy          = -0x6D6;
            pos.vz          = 0x1F4;
            work->field_55C = Gp_SpawnEff(0x800601A9, ((TmdObject*)arg0->extra)->coords, (s16)work->field_59A + 0xA, &pos);
            work->field_588 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40330009;
            SndEvt_EnqueueType6(work->field_588, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            break;
        case 1:
            timer           = work->field_59A - 1;
            work->field_59A = timer;
            if ((timer << 16) <= 0) {
                work->field_598 = 2;
                work->field_58E = 4;
                SndEvt_EnqueueType7(work->field_588, 1);
                work->field_588 = 0;
                snd             = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x4033000B;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
                Gp_SpawnPadLerp(0xF, 8, 0xFF);
            }
            if ((s16)work->field_59A == 0x5A) {
                snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x4033000A;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            }
            break;
        case 2:
            if ((s16)work->field_592 == 0xC) {
                work->field_55C     = NULL;
                work->obj4E4.flags |= 0x8000;
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            } else {
                work->obj4E4.flags &= 0x7FFF;
            }
            if ((s16)work->field_592 >= 0x1B) {
                work->field_598 = 3;
                work->field_58E = 5;
            }
            break;
        case 3:
            work->field_5B4 = 0;
            if ((s16)work->field_592 >= 0x1C) {
                work->field_58E = 1;
                work->field_596 = 0;
                work->field_598 = 0;
                rnd             = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rnd;
                work->field_59A = (rnd >> 16) & 0x3F;
            }
            break;
    }
}

/// The attack body, run while `field_5A2` is set. It carves an
/// `ActorAttackScratch` from `G_SCRATCH_HEAD` and steps `field_5A4`:
/// state 0 records which side of the player it is on (`field_5A0`), plays its
/// grab animation and spawns the effect; state 1 drags the player towards the
/// actor for 0x10 frames and hands over after 0x1E/0x20; state 2 waits for the
/// animation to finish and clears `field_5A2`.
void func_actor_105100_80133CE4(Task* arg0)
{
    Actor105100Work*    work;
    GsCOORDINATE2*      coord;
    Task*               player;
    GsCOORDINATE2*      target;
    ActorAttackScratch* scratch;
    void*               head;
    s32                 sound;
    s32                 count;

    work               = arg0->work;
    player             = gameGetPtrSlot(3);
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = (u8*)head - sizeof(ActorAttackScratch);
    scratch            = SCRATCH_HEAD(ActorAttackScratch);
    coord              = ((TmdObject*)arg0->extra)->coords;
    target             = ((TmdObject*)player->extra)->coords;

    switch (work->field_5A4) {
        case 0:
            if (((GameActor*)player->work)->field_954 != 2) {
                scratch->delta.vx           = target->coord.t[0] - coord->coord.t[0];
                scratch->delta.vy           = 0;
                scratch->delta.vz           = target->coord.t[2] - coord->coord.t[2];
                work->field_5A0             = (scratch->delta.vx * target->coord.m[0][2] + scratch->delta.vz * target->coord.m[2][2]) > 0;
                scratch->anim.animBlock.ptr = &D_actor_105100_801414B4;
                scratch->anim.field_4       = work->field_5A0 + 1;
                scratch->anim.field_8       = 0;
                scratch->anim.field_C       = 0;
                scratch->anim.field_10      = 1;
                Gp_DispatchMsg(player, 0x3F4, (s32)scratch, 0);
                work->field_5A4 = 1;
                work->field_5A6 = 0;
                Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
                sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 7;
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                scratch->dir.vx = 0;
                scratch->dir.vy = -1000;
                scratch->dir.vz = 0;
                Gp_SpawnEff(0x601AC, ((TmdObject*)player->extra)->coords, 0, &scratch->dir);
            } else {
                work->field_5A2 = 0;
            }
            break;
        case 1:
            if ((s16)work->field_5A6 < 0x10) {
                scratch->delta.vx = target->coord.t[0] - coord->coord.t[0];
                scratch->delta.vy = target->coord.t[1] - coord->coord.t[1];
                scratch->delta.vz = target->coord.t[2] - coord->coord.t[2];
                VectorNormalS(&scratch->delta, &scratch->dir);
                scratch->place.pos.vx = target->coord.t[0] + ((scratch->dir.vx * 25) >> 10);
                scratch->place.pos.vy = 0;
                scratch->place.pos.vz = target->coord.t[2] + ((scratch->dir.vz * 25) >> 10);
                scratch->place.rot.vx = 0;
                if (work->field_5A0 == 0) {
                    scratch->place.rot.vy = (ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) + 0x800) & 0xFFF;
                } else {
                    scratch->place.rot.vy = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & 0xFFF;
                }
                scratch->place.rot.vz = 0;
                Gp_DispatchMsg(player, 0x3E9, (s32)&scratch->place, 0);
            }
            if ((s16)work->field_5A6 == 0x10) {
                sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x55190003;
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            count = (s16)++work->field_5A6;
            if ((work->field_5A0 != 0 && count >= 0x1E) || (work->field_5A0 == 0 && count >= 0x20)) {
                scratch->anim.animBlock.ptr = &D_actor_105100_801414B4;
                scratch->anim.field_4       = work->field_5A0 + 3;
                scratch->anim.field_8       = 0;
                scratch->anim.field_C       = 0;
                scratch->anim.field_10      = 1;
                Gp_DispatchMsg(player, 0x3F4, (s32)scratch, 0);
                work->field_5A4 = 2;
                work->field_5A6 = 0;
            }
            break;
        case 2:
            if ((s16)++work->field_5A6 >= 0x25) {
                if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                    Gp_DispatchMsg(player, 0x3F1, 0, 0);
                    work->field_5A4 = 0;
                    work->field_5A6 = 0;
                    work->field_5A2 = 0;
                }
            }
            break;
    }
    SCRATCH_POP_BYTES(sizeof(ActorAttackScratch));
}

void func_actor_105100_80134130(Task* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor105100Work* work;
    GsCOORDINATE2*   self;
    GpAnimRec*       rec;

    work = arg0->work;
    self = ((TmdObject*)arg0->extra)->coords;
    rec  = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->obj38.prev);
    if (rec != NULL) {
        if (!(rec->flags & 0x20) && (work->field_5B8 & 0x20)) {
            snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40330001;
            pan = (s8)Gp_GetObjPan(self);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
        }
        if (!(rec->flags & 0x10) && (work->field_5B8 & 0x10)) {
            snd  = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40330002;
            pan2 = (s8)Gp_GetObjPan(self);
            SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(self));
        }
        work->field_5B8 = (u16)(rec->flags & 0x30);
    }
}

/// Teardown handler in `D_actor_105100_80131E24`. Mode 1 of `Gp_StateF0.field_4` only
/// refreshes the actor colour; mode 2 hides the model and returns. Otherwise it
/// walks `field_598`: unlink the collision bodies, play the death clip, fire
/// the 0x13F4 cutscene, shrink the model, then destroy the enemy.
void func_actor_105100_80134284(GpEnemy* arg0, Task* arg1)
{
    SVECTOR          dir;
    VECTOR           pos;
    Task*            actor;
    TmdObject*       obj;
    Actor105100Work* work;
    GsCOORDINATE2*   coord;
    Task*            player;
    s32              state;
    s32              i;
    s32              val;
    s32              snd;
    s16              flag;
    GsCOORDINATE2*   colorCoord;

    actor  = arg1;
    obj    = actor->extra;
    work   = actor->work;
    coord  = obj->coords;
    player = gameGetPtrSlot(3);
    state  = Gp_StateF0.field_4;
    if (state == 1) {
        goto color_update;
    }
    if (state >= 2) {
        if (state == 2) {
            ((TmdObject*)actor->extra)->flags = 0x80;
            return;
        }
    }
    if (work->field_5A2 != 0) {
        func_actor_105100_80133CE4(actor);
    }
    switch (work->field_598) {
        case 0:
            work->field_58E = 1;
            work->field_594 = 0x1000;
            work->field_560 = coord->coord;
            arg0->recs      = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->obj47C);
            Gp_UnlinkObj(&work->obj51C);
            Gp_UnlinkObj(&work->obj4E4);
            Gp_SetLightMode(arg0, 1);
            work->field_59A = 0;
            work->field_598 = 1;
            goto color_update;
        case 1:
            if ((s16)++work->field_59A == 0xA) {
                obj->flags = (u16)obj->flags | 2;
            }
            if ((s16)work->field_59A >= 0x1F) {
                work->field_58E = 7;
                work->field_598 = 2;
                Gp_ReleaseStateF0Add(actor, 0x33);
                work->field_5BA = 1;
            }
            work = actor->work;
            i    = 1;
            if ((s16)work->field_58E != work->field_590) {
                work->field_590 = work->field_58E;
                work->field_592 = 0;
                val             = D_actor_105100_801414C8[(s16)work->field_58E];
                do {
                    func_800B4114(work, i, (s16)work->field_58E, 0, val);
                    i++;
                } while (i < 0x13);
            } else {
                TOUCH_REG(i);
                work->field_592 += i;
                do {
                    Gp_AnimTickIndex((GpAnimCtx*)work, i);
                    i++;
                } while (i < 0x13);
            }
            goto color_update;
        case 2:
            flag = work->field_5BA;
            if ((flag == 1) && (((GameActor*)player->work)->field_954 != 2) && (D_80114C12 != flag) &&
                (D_80071075 == 0)) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
                work->field_5BA = 0;
            }
            if ((s16)work->field_592 == 0xB) {
                snd = ((((GpEnemy*)actor->spawnArg2)->placeKey >> 12) << 8) | 0x4033000E;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                Gp_SpawnPadLerp(0xA, 0xFF, 0x40);
            }
            if ((s16)work->field_592 == 0x28) {
                snd = ((((GpEnemy*)actor->spawnArg2)->placeKey >> 12) << 8) | 0x40330003;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            }
            if ((s16)work->field_592 == 0x36) {
                dir.vx = 0;
                dir.vy = 0;
                dir.vz = 0x96;
                Gp_SpawnEff(0x600A5, &((TmdObject*)actor->extra)->coords[3], 5, &dir);
                work->field_59A = 0;
            }
            if (((s16)work->field_592 >= 0x36) && (work->field_5BA == 0)) {
                work->field_598 = 3;
            }
            work = actor->work;
            i    = 1;
            if ((s16)work->field_58E != work->field_590) {
                work->field_590 = work->field_58E;
                work->field_592 = 0;
                val             = D_actor_105100_801414C8[(s16)work->field_58E];
                do {
                    func_800B4114(work, i, (s16)work->field_58E, 0, val);
                    i++;
                } while (i < 0x13);
            } else {
                TOUCH_REG(i);
                work->field_592 += i;
                do {
                    Gp_AnimTickIndex((GpAnimCtx*)work, i);
                    i++;
                } while (i < 0x13);
            }
            goto color_update;
        case 3:
            if (work->field_594 >= 0x201) {
                work->field_594 = (u16)work->field_594 - 0x50;
            }
            func_actor_105100_80136574(actor, &work->field_560, work->field_594, 0);
            if ((s16)++work->field_59A >= 0x3C) {
                work->field_598 = 4;
            }
        color_update:
            colorCoord = ((TmdObject*)actor->extra)->coords;
            pos.vx     = colorCoord->workm.t[0];
            pos.vy     = colorCoord->workm.t[1];
            pos.vz     = colorCoord->workm.t[2];
            Gp_UpdateActorColor(actor->spawnArg2, &pos, 0, 0);
            return;
        case 4:
            Gp_DestroyEnemy((GpEnemy*)arg0, actor);
            return;
    }
}

/// Setup handler of the projectile task. It allocates the task's
/// `Actor105100ProjWork` and, if that fails, tears the enemy down and stays on
/// this handler.
///
/// The model's coordinate starts as a copy of the parent's, moved by the
/// `D_actor_105100_801414E0` entry the parent's `field_5AE` selects and then
/// jittered on each axis by up to 127 units either way from the gameplay LCG.
/// The two list nodes are linked into list 3 - `obj0` on the model coordinate,
/// `obj38` on the pose segment - the collision table is initialised, the
/// billboard size and a random frame count are seeded, and the task moves to
/// `state` 1.
///
/// `seed`, `transY`, `index` and `temp` are shared or split the way they are
/// because the original's register allocation and scheduling depend on it:
/// the state is read before the Y store, which goes through a plain `long*`,
/// and the third table index and address live in temporaries reused later.
void func_actor_105100_801347D4(GpEnemy* arg0, Task* arg1)
{
    Task*                parent;
    Actor105100Work*     parentWork;
    Actor105100ProjWork* work;
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       parentCoord;
    long*                transY;
    void*                temp;
    s32                  index;
    s32                  offsetY;
    u32                  seed;
    u32                  rollX;
    u32                  rollY;
    u32                  rollZ;
    u32                  rollA;
    u32                  rollB;
    s32                  amountX;
    s32                  amountY;
    s32                  amountZ;
    s32                  signX;
    s32                  signY;
    s32                  signZ;
    s32                  posX;
    s32                  posY;
    s32                  posZ;

    parent      = arg1->parent;
    coord       = ((TmdObject*)arg1->extra)->coords;
    parentCoord = ((TmdObject*)parent->extra)->coords;
    parentWork  = (Actor105100Work*)parent->work;
    work        = (Actor105100ProjWork*)memCalloc(0x80, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    arg1->work        = work;
    coord->sub        = &gGfxViewCoord;
    coord->coord      = parentCoord->coord;
    coord->coord.t[0] = parentCoord->coord.t[0] + D_actor_105100_801414E0[(s16)parentWork->field_5AE].vx;
    offsetY           = D_actor_105100_801414E0[(s16)parentWork->field_5AE].vy;
    seed              = Gp_LcgState;
    transY            = &coord->coord.t[1];
    *transY           = parentCoord->coord.t[1] + offsetY;
    rollX             = (Gp_LcgState = seed * 5 + 0x71357911) >> 16;
    index             = (s16)parentWork->field_5AE;
    temp              = &D_actor_105100_801414E0[index];
    coord->coord.t[2] = parentCoord->coord.t[2] + (amountX = ((SVECTOR*)temp)->vz);
    amountX           = rollX & 0x7F;
    signX             = rollX & 0x80;
    posX              = coord->coord.t[0];
    coord->coord.t[0] = !signX ? posX - amountX : posX + amountX;

    rollY             = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
    amountY           = rollY & 0x7F;
    signY             = rollY & 0x80;
    posY              = coord->coord.t[1];
    coord->coord.t[1] = !signY ? posY - amountY : posY + amountY;

    rollZ             = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
    amountZ           = rollZ & 0x7F;
    signZ             = rollZ & 0x80;
    posZ              = coord->coord.t[2];
    coord->coord.t[2] = !signZ ? posZ - amountZ : posZ + amountZ;

    coord->flg          = 0;
    work->obj0.coord    = ((TmdObject*)arg1->extra)->coords;
    work->obj0.ctx.recs = &work->rec20;
    work->obj0.pos.vx   = 0;
    work->obj0.pos.vy   = 0;
    work->obj0.pos.vz   = 0;
    work->obj0.key      = Gp_PackPair(&D_actor_105100_80141380, 0);
    work->obj0.radius   = 0xC8;
    work->obj0.flags    = 1;
    Gp_LinkObj(3, &work->obj0);
    work->pose.end0.vx    = 0;
    work->pose.end0.vy    = 0;
    work->pose.end0.vz    = 0;
    work->pose.end1.vx    = 0;
    work->pose.end1.vy    = 0;
    work->pose.end1.vz    = -0x190;
    work->pose.end0Radius = 1;
    work->pose.end1Radius = 1;
    work->pose.recs       = &work->rec20;
    index                 = work->obj0.flags;
    index                &= 0x7FFF;
    work->obj0.flags      = index;
    temp                  = ((TmdObject*)arg1->extra)->coords;
    work->obj38.ctx.d4rec = &work->pose;
    work->obj38.pos.vx    = 0;
    work->obj38.pos.vy    = 0;
    work->obj38.pos.vz    = 0;
    work->obj38.key       = 0;
    work->obj38.radius    = 0;
    work->obj38.flags     = 3;
    work->obj38.coord     = temp;
    Gp_LinkObj(3, &work->obj38);
    work->obj38.flags &= 0xBFFF;
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->field_7E = 0x190;
    rollA          = (Gp_LcgState * 5) + 0x71357911;
    rollB          = (rollA * 5) + 0x71357911;
    Gp_LcgState    = rollB;
    work->field_78 = ((rollA >> 16) & 0xF) + ((rollB >> 16) & 7);
    arg1->state    = 1;
}

/// The projectile task's state handlers, indexed by `Task::state`: setup,
/// per-frame flight and teardown.
const GpEnemyTaskFuncTable3 D_actor_105100_80131E90 = {
    {
        func_actor_105100_801347D4,
        (GpEnemyTaskFunc)func_actor_105100_80134B00,
        func_actor_105100_801366D8,
    },
};

/// Per-frame handler of the glowing projectile this overlay spawns as its
/// second enemy task, the middle entry of `D_actor_105100_80131E90`. Mode 1 of
/// `Gp_StateF0.field_4` only redraws the billboard and mode 2 skips the frame.
///
/// `field_7A` steps the projectile through its life. It first hovers, jittering
/// its coordinate by a per-axis offset the gameplay LCG draws and taking each
/// offset only while the accumulated jitter stays inside its bound, and waits
/// there for the parent's state: gone, and the task ends; ready, and a
/// countdown launches it. It then turns onto its heading and starts
/// accelerating, aims itself at the player, and flies, arming its two
/// collision bodies once it is clear of the ground. The flight ends when the
/// contact record reports a hit or the time runs out: the projectile is
/// re-keyed and widened to the burst, which is spawned with its own effect and
/// sound, and a last step fades the body out and destroys the task.
void func_actor_105100_80134B00(GpEnemy* arg0, Task* arg1)
{
    Actor105100ProjWork*    work;
    Actor105100Work*        parentWork;
    GsCOORDINATE2*          coord;
    Actor105100ProjScratch* scratch;
    s32                     state;
    u32                     rng;
    u32                     hi;
    s32                     val;
    u16                     speed;
    u16                     timer;
    s32                     snd;
    s32                     n;

    work       = (Actor105100ProjWork*)((Actor105100Work*)arg1->work);
    coord      = ((TmdObject*)arg1->extra)->coords;
    parentWork = (arg1->parent)->work;
    state      = Gp_StateF0.field_4;
    if (state == 1) {
        func_actor_105100_80131EBC(coord, work->field_7E);
        return;
    }
    if (state < 2) {
        goto body;
    }
    if (state == 2) {
        return;
    }
body:
    SCRATCH_PUSH(Actor105100ProjScratch);
    scratch = SCRATCH_HEAD(Actor105100ProjScratch);
    switch (work->field_7A) {
        case 0:
            rng         = Gp_LcgState * 5 + 0x71357911;
            hi          = rng >> 16;
            val         = hi & 0x3F;
            Gp_LcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vx = val;
            if (ABS(work->field_70.vx + (s16)val) < 0x1F4) {
                work->field_70.vx += val;
                coord->coord.t[0] += scratch->rot.vx;
            }
            rng         = Gp_LcgState * 5 + 0x71357911;
            hi          = rng >> 16;
            val         = hi & 0x3F;
            Gp_LcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vy = val;
            if (ABS(work->field_70.vy + (s16)val) < 0x1F4) {
                work->field_70.vy += val;
                coord->coord.t[1] += scratch->rot.vy;
            }
            rng         = Gp_LcgState * 5 + 0x71357911;
            hi          = rng >> 16;
            val         = hi & 0x3F;
            Gp_LcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vz = val;
            if (ABS(work->field_70.vz + (s16)val) < 0x1F4) {
                work->field_70.vz += val;
                coord->coord.t[2] += scratch->rot.vz;
            }
            if (parentWork->field_5AC == 0) {
                arg1->state = 2;
            }
            if (parentWork->field_5AC == 2) {
                timer          = work->field_78 - 1;
                work->field_78 = timer;
                if ((timer << 16) <= 0) {
                    work->field_7A = 1;
                    work->field_7C = 1;
                    work->field_78 = 0;
                }
            }
            goto update;
        case 1:
            scratch->rot.vx = 0x20;
            scratch->rot.vy = 0;
            scratch->rot.vz = 0;
            RotMatrix(&scratch->rot, &scratch->mat);
            gte_SetRotMatrix(&coord->coord);
            gte_ldclmv(&scratch->mat);
            gte_rtir();
            gte_stclmv(&coord->coord);
            gte_ldclmv((char*)&scratch->mat + 2);
            gte_rtir();
            gte_stclmv((char*)&coord->coord + 2);
            gte_ldclmv((char*)&scratch->mat + 4);
            gte_rtir();
            gte_stclmv((char*)&coord->coord + 4);
            speed          = work->field_7C * 2;
            work->field_7C = speed;
            if ((s16)speed >= 0x33) {
                work->field_7C = 0x32;
            }
            coord->coord.t[0] += (coord->coord.m[0][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[1] += (coord->coord.m[1][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * (s16)work->field_7C) >> 12;
            timer              = work->field_78 + 1;
            work->field_78     = timer;
            if ((s16)timer >= 0x10) {
                work->field_7A = 2;
                work->field_78 = 0;
            }
            goto update;
        case 2:
            timer          = work->field_78 + 1;
            work->field_78 = timer;
            if ((s16)timer >= 3) {
                scratch->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                scratch->vec.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                scratch->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                Gp_OrientAlong(&scratch->vec, &coord->coord, 0);
                work->field_7A = 3;
                work->field_78 = 0;
                work->field_7C = 1;
            }
            goto update;
        update:
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            func_actor_105100_80131EBC(coord, work->field_7E);
            break;
        case 3:
            n = 4;
            if ((s16)++work->field_78 == n) {
                snd = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x40330005;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (coord->coord.t[1] >= -0xF9F) {
                work->obj0.flags  |= 0x8000;
                work->obj38.flags |= 0x4000;
            }
            speed          = work->field_7C * 2;
            work->field_7C = speed;
            if ((s16)speed >= 0x12D) {
                work->field_7C = 0x12C;
            }
            coord->coord.t[0] += (coord->coord.m[0][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[1] += (coord->coord.m[1][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * (s16)work->field_7C) >> 12;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_actor_105100_80131EBC(coord, work->field_7E);
            if (work->rec20.key != 0 || (s16)work->field_78 >= 0x1A) {
                work->obj38.flags &= 0xBFFF;
                Gp_ClearRec18Occupied(&work->rec20);
                work->obj0.key    = Gp_PackPair(&D_actor_105100_80141380, 1);
                work->obj0.radius = 0x1F4;
                work->field_78    = 0x1E;
                work->field_7A    = n;
                Gp_SpawnEff(0x601A7, coord, 0, NULL);
                snd = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x40330006;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            break;
        case 4:
            if ((s16)work->field_78 == 0x14) {
                work->obj0.flags &= 0x7FFF;
            }
            timer          = work->field_78 - 1;
            work->field_78 = timer;
            if ((timer << 16) <= 0) {
                arg1->state = 2;
            }
            break;
    }
    SCRATCH_POP(Actor105100ProjScratch);
}

void func_actor_105100_80135278(GpEnemy* arg0, Task* arg1)
{
    Task*            parent;
    Actor105100Work* work;
    Actor105100Rec*  obj;
    GsCOORDINATE2*   dst;
    GsCOORDINATE2*   src;

    parent = arg1->parent;
    work   = (Actor105100Work*)parent->work;
    dst    = ((TmdObject*)arg1->extra)->coords;
    src    = ((TmdObject*)parent->extra)->coords;

    if (D_actor_105100_80141450[work->field_5B0 * 3 + (s16)work->field_5AE] == -1 ||
        (obj = (Actor105100Rec*)memCalloc(0x50, 0)) == NULL) {
        USE_REG(arg0);
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    arg1->work    = obj;
    obj->field_40 = work->field_5B0;
    obj->field_42 = work->field_5AE;
    work->field_5AE++;
    obj->field_44     = D_actor_105100_80141450[work->field_5B0 * 3 + obj->field_42];
    obj->field_48     = D_actor_105100_80141448[obj->field_40];
    obj->field_4E     = 3;
    dst->sub          = &gGfxViewCoord;
    dst->coord        = src->coord;
    dst->coord.t[0]   = src->coord.t[0] + D_actor_105100_801413E8[obj->field_44].vx;
    dst->coord.t[1]   = src->coord.t[1] + D_actor_105100_801413E8[obj->field_44].vy;
    dst->coord.t[2]   = src->coord.t[2] + D_actor_105100_801413E8[obj->field_44].vz;
    dst->flg          = 0;
    obj->obj.coord    = ((TmdObject*)arg1->extra)->coords;
    obj->obj.ctx.recs = obj->rec;
    obj->obj.pos.vx   = 0;
    obj->obj.pos.vy   = 0;
    obj->obj.pos.vz   = 0;
    obj->obj.key      = Gp_PackPair(&D_actor_105100_80141380, obj->field_40 + 2);
    obj->obj.radius   = 0xC8;
    obj->obj.flags    = 1;
    Gp_LinkObj(3, &obj->obj);
    Gp_InitRec18Table(obj->rec, 1, 0);
    obj->obj.flags |= 0x8000;
    arg1->state     = 1;
}

/// The per-frame handler the `state == 1` dispatch runs: it hands the reaction
/// `field_40` selects to one of the `80135674` / `801359B4` / `80135B40`
/// sub-handlers, retimes the pose every 6/0xB/0x10 frames of the countdown in
/// `field_48`, and ends the fight (`state = 2`) once that countdown, the work's
/// `field_24` and the parent's `field_5AC` all say so.
void func_actor_105100_801354E8(GpEnemy* arg0, Task* arg1)
{
    Actor105100Rec*  rec;
    Actor105100Work* parentWork;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;
    s16              timer;
    u16              count;

    rec        = (Actor105100Rec*)((Actor105100Work*)arg1->work);
    parentWork = (arg1->parent)->work;
    state      = Gp_StateF0.field_4;
    coord      = ((TmdObject*)arg1->extra)->coords;
    one        = 1;

    if (state == one) {
        func_8017FC40(coord, 0x80, rec->field_4E);
        return;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto done;
    }
default_body:
    if (rec->field_40 == one) {
        goto rec1;
    }
    if (rec->field_40 >= 2) {
        goto ge2;
    }
    if (rec->field_40 == 0) {
        goto rec0;
    }
    goto join;
ge2:
    if (rec->field_40 == 2) {
        goto rec2;
    }
    goto join;
rec0:
    func_actor_105100_80135674(arg1);
    goto join;
rec1:
    func_actor_105100_801359B4(arg1);
    goto join;
rec2:
    func_actor_105100_80135B40(arg1);
join:
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    timer = rec->field_48;
    if (timer < 6) {
        rec->field_4E = 0;
    } else if (timer < 0xB) {
        rec->field_4E = 1;
    } else if (timer < 0x10) {
        rec->field_4E = 2;
    }
    func_8017FC40(coord, 0x80, rec->field_4E);
    count         = (u16)rec->field_48 - 1;
    rec->field_48 = count;
    if ((count << 16) <= 0 || ((Actor105100Work*)rec)->field_24 != 0 ||
        parentWork->field_5AC == 0) {
        parentWork->field_5AE = parentWork->field_5AE - 1;
        arg1->state           = 2;
    }
done:
    return;
}

/// Reaction 0's handler (`field_40 == 0`), which walks the model along a
/// two-leg path through `D_actor_105100_80141418`: `field_44`, then
/// `field_44 + 3`. Pass 0 builds the first-leg aim, measures both legs and
/// stores the per-frame step (total length over `obj38.pos.vx`) plus how many
/// frames the first leg takes; pass 1 walks that step and re-aims at the
/// second point when the countdown hits 0; pass 2 keeps walking.
void func_actor_105100_80135674(Task* arg0)
{
    Actor105100Work* work;
    Actor105100Rec*  rec;
    GsCOORDINATE2*   coord;
    VECTOR*          head;
    VECTOR*          vec;
    s16              state;
    s32              dx;
    s32              dz;
    s32              dx2;
    s32              dz2;
    s32              dist;
    s32              speed;
    s16              timer;

    head                 = SCRATCH_HEAD(VECTOR);
    vec                  = head - 1;
    SCRATCH_HEAD(VECTOR) = vec;
    work                 = arg0->work;
    rec                  = (Actor105100Rec*)work;
    state                = rec->field_46;
    coord                = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            vec->vx = D_actor_105100_80141418[rec->field_44].vx - coord->coord.t[0];
            vec->vy = 0;
            vec->vz = D_actor_105100_80141418[rec->field_44].vz - coord->coord.t[2];
            VectorNormalS(vec, (SVECTOR*)&work->obj38);
            dx      = vec->vx;
            dz      = vec->vz;
            dist    = SquareRoot0(dx * dx + dz * dz);
            vec->vx = D_actor_105100_80141418[rec->field_44 + 3].vx -
                      D_actor_105100_80141418[rec->field_44].vx;
            vec->vy = 0;
            dz2     = D_actor_105100_80141418[rec->field_44 + 3].vz -
                  D_actor_105100_80141418[rec->field_44].vz;
            vec->vz            = dz2;
            dx2                = vec->vx;
            speed              = (dist + SquareRoot0(dx2 * dx2 + dz2 * dz2)) / work->obj38.pos.vx;
            rec->field_46      = 1;
            work->obj38.pos.vz = speed;
            work->obj38.pos.vy = dist / (s16)speed;
            break;
        case 1:
            coord->coord.t[0] += (((SVECTOR*)&work->obj38)->vx * work->obj38.pos.vz) >> 12;
            coord->coord.t[2] += (((SVECTOR*)&work->obj38)->vz * work->obj38.pos.vz) >> 12;
            timer              = (u16)work->obj38.pos.vy - 1;
            work->obj38.pos.vy = timer;
            if ((timer << 16) <= 0) {
                vec->vx = D_actor_105100_80141418[rec->field_44 + 3].vx - coord->coord.t[0];
                vec->vy = 0;
                vec->vz = D_actor_105100_80141418[rec->field_44 + 3].vz - coord->coord.t[2];
                VectorNormalS(vec, (SVECTOR*)&work->obj38);
                rec->field_46 = 2;
            }
            break;
        case 2:
            coord->coord.t[0] += (((SVECTOR*)&work->obj38)->vx * work->obj38.pos.vz) >> 12;
            coord->coord.t[2] += (((SVECTOR*)&work->obj38)->vz * work->obj38.pos.vz) >> 12;
            break;
    }
    SCRATCH_POP(VECTOR);
}

/// Reaction 1's handler (`field_40 == 1`), which walks the model towards the
/// approach point `field_44` selects from `D_actor_105100_80141418`. The first
/// pass (`field_46 == 0`) builds the planar delta in 16 bytes of scratch,
/// normalises it into the record's own 0x38 vector and stores the step it then
/// travels per frame -- the delta's length over `obj38.field_10`; the second
/// (`field_46 == 1`) applies that step to the coordinate every frame.
void func_actor_105100_801359B4(Task* arg0)
{
    Actor105100Work* work;
    Actor105100Rec*  rec;
    GsCOORDINATE2*   coord;
    VECTOR*          head;
    VECTOR*          vec;
    s16              state;
    s32              dx;
    s32              dz;
    s32              speed;

    head                 = SCRATCH_HEAD(VECTOR);
    vec                  = head - 1;
    SCRATCH_HEAD(VECTOR) = vec;
    work                 = arg0->work;
    rec                  = (Actor105100Rec*)work;
    state                = rec->field_46;
    coord                = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            vec->vx = D_actor_105100_80141418[rec->field_44].vx - coord->coord.t[0];
            vec->vy = 0;
            vec->vz = D_actor_105100_80141418[rec->field_44].vz - coord->coord.t[2];
            VectorNormalS(vec, (SVECTOR*)&work->obj38);
            dx                 = vec->vx;
            dz                 = vec->vz;
            speed              = SquareRoot0(dx * dx + dz * dz) / work->obj38.pos.vx;
            rec->field_46      = 1;
            work->obj38.pos.vz = speed;
            break;
        case 1:
            coord->coord.t[0] += (((SVECTOR*)&work->obj38)->vx * work->obj38.pos.vz) >> 12;
            coord->coord.t[2] += (((SVECTOR*)&work->obj38)->vz * work->obj38.pos.vz) >> 12;
            break;
    }
    SCRATCH_POP(VECTOR);
}

void func_actor_105100_80135B40(Task* arg0)
{
    ActorFaceScratch* sc;
    GsCOORDINATE2*    coord;
    s32               ang;
    s32               cur;
    s16               target;
    s16               diff;
    s32               adiff;
    s16               snap;
    s32               next;
    s32               step;

    sc           = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    coord        = ((TmdObject*)arg0->extra)->coords;
    sc->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = 0;
    sc->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    ang          = ratan2((s32)(s16)sc->delta.vx, (s32)(s16)sc->delta.vz) & 0xFFF;
    snap         = ang;
    cur          = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    target       = cur;
    diff         = ang - cur;
    adiff        = diff >= 0 ? diff : -diff;
    if (adiff < 0x800) {
        target = ang;
        if (adiff >= 0x51) {
            next = (s16)cur;
            if (diff > 0) {
                target = next + 0x50;
            } else {
                target = next - 0x50;
            }
        }
    } else {
        if (diff > 0) {
            if (0x1000 - diff < 0x51) {
                goto snapTurn;
            } else {
                goto turn;
            }
        } else if (0x1000 + diff < 0x51) {
            goto snapTurn;
        } else {
            goto turn;
        }
    snapTurn:
        target = snap;
        goto done;
    turn:
        step = (s16)target;
        if (diff > 0) {
            target = step - 0x50;
        } else {
            target = step + 0x50;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = target;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    coord->coord.t[0] += (coord->coord.m[0][2] * 0xF) >> 0xA;
    coord->coord.t[2] += (coord->coord.m[2][2] * 0xF) >> 0xA;
    SCRATCH_POP_BYTES(0x18);
}

/// Unless the player is in an event, draws from the gameplay LCG and on one
/// frame in four spawns effect `D_80115728` on `arg0`, offset in a random
/// horizontal direction; `arg1` is or-ed into the spawn flags.
void func_actor_105100_80135CEC(GsCOORDINATE2* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (Gp_State1C->eventState == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            ang         = ((u32)Gp_LcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            Gp_SpawnEff(D_80115728, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}

void func_actor_105100_80135DF8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_105100_80135E54(Task* arg0)
{
    GpEnemy*         enemy;
    Actor105100Work* work;
    s32              state;
    s32              damage;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags & 1) {
        enemy->reactionFlags = flags & 0xFE;
    }
    if ((enemy->reactionFlags & 2) && (work->field_596 != 5)) {
        work->field_596 = 5;
        work->field_598 = 0;
        work->field_5C2 = 1;
    }
    if (enemy->reactionFlags & 0xC) {
        tick = Gp_TickObjFlag4(enemy) << 0x10;
        if (tick != 0) {
            damage = tick >> 0x12;
            func_800DA6E8(&enemy->node, damage, 0);
            state     = (u16)enemy->hp - damage;
            enemy->hp = state;
            state   <<= 0x10;
            if (state <= 0) {
                state = 7;
            } else {
                state = 6;
            }
            work->field_596 = state;
            work->field_598 = 0;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }
}

/// Second step of the `field_598` schedule: arms pose 2 with the `field_59A`
/// timer at 0x3C frames, then, when the timer runs out, hands the pose back to
/// the schedule entry step and returns it to 0.
void func_actor_105100_80135F50(Task* arg0)
{
    Actor105100Work* work;
    s32              state;
    u16              timer;

    work  = arg0->work;
    state = work->field_598;
    switch (state) {
        case 0:
            work->field_58E = 2;
            work->field_59A = 0x3C;
            work->field_598 = 1;
            break;
        case 1:
            timer           = work->field_59A - 1;
            work->field_59A = timer;
            if ((timer << 16) <= 0) {
                work->field_5A8 = state;
                work->field_58E = state;
                work->field_596 = 0;
                work->field_598 = 0;
                work->field_59A = 0;
            }
            break;
    }
}

void func_actor_105100_80135FCC(Task* arg0)
{
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;
    s32            snd;
    s32            pan;
    u16            hp;

    enemy                = arg0->spawnArg2;
    coord                = ((TmdObject*)arg0->extra)->coords;
    Gp_StateF0.field_1D &= 0xFB;
    hp                   = enemy->hp + 0x50;
    enemy->hp            = hp;
    if (D_actor_105100_8014139C[0] < (s16)hp) {
        enemy->hp = D_actor_105100_8014139C[0];
    }
    func_800DA6E8(&enemy->node, -0x50, 0);
    Gp_SpawnEff(0x601AF, NULL, 0, NULL);
    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x4033000C;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
}

/// Opening stage of the `field_598` schedule: arms pose 8, releases the held
/// effect slot and drops the `obj4E4.flags` pose bit, then waits on
/// `Gp_TickObjFlag2` before clearing the enemy's flag-2 bit. On the last stage
/// it waits out the `field_592` timer and returns the schedule to step 0.
void func_actor_105100_801360AC(Task* arg0)
{
    Actor105100Work* work;
    GpEnemy*         enemy;
    GpEffWork*       eff;
    s32              state;

    work  = arg0->work;
    state = work->field_598;
    enemy = arg0->spawnArg2;
    switch (state) {
        case 0:
            work->field_58E = 8;
            work->field_598 = 1;
            if (work->field_5B4 != 0) {
                work->field_5B4 = 0;
                work->field_5B6 = 1;
                work->field_5AA = 0;
            }
            work->field_5AC = 0;
            func_actor_105100_801362A0(arg0);
            eff                = work->field_55C;
            work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
            if (eff != NULL) {
                eff->task->state = 4;
                work->field_55C  = NULL;
            }
            if (work->field_5B6 == 0) {
                work->field_5AA = 0x1E;
            }
            break;
        case 1:
            if (Gp_TickObjFlag2(enemy) != 0) {
                work->field_58E       = 9;
                work->field_598       = 2;
                work->field_5C2       = 0;
                enemy->reactionFlags &= 0xFD;
            }
            break;
        case 2:
            if ((s16)work->field_592 >= 0xB) {
                work->field_596 = 0;
                work->field_598 = 0;
                work->field_58E = 1;
            }
            break;
    }
}

/// First stage of the `field_598` schedule: arms the pose and the effect slot,
/// and on the next stage waits out the `field_592` timer before handing the
/// state back on, either aborting (0) or resuming (8) depending on `field_5C2`.
void func_actor_105100_801361C4(Task* arg0)
{
    Actor105100Work* work;
    GpEffWork*       eff;
    s32              state;

    work  = arg0->work;
    state = work->field_598;
    switch (state) {
        case 0:
            work->field_58E = 6;
            work->field_598 = 1;
            work->field_5B4 = 0;
            work->field_5AC = 0;
            func_actor_105100_801362A0(arg0);
            eff                = work->field_55C;
            work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
            if (eff != NULL) {
                eff->task->state = 4;
                work->field_55C  = NULL;
            }
            if (work->field_5B6 == 0) {
                work->field_5AA = 0x1E;
            }
            break;
        case 1:
            if ((s16)work->field_592 >= 0x1D) {
                if (work->field_5C2 == 0) {
                    work->field_596 = 0;
                    work->field_58E = state;
                } else {
                    work->field_596 = 5;
                    work->field_58E = 8;
                }
                work->field_598 = 0;
            }
            break;
    }
}

void func_actor_105100_801362A0(Task* arg0)
{
    Actor105100Work* work;
    s32              snd;

    work = arg0->work;

    snd = work->field_580;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_580 = 0;
    }
    snd = work->field_584;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_584 = 0;
    }
    snd = work->field_588;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_588 = 0;
    }
}

/// Last-enemy handler. While the remaining-enemy count is still positive it
/// retires the queued sound events, unlinks the running effect, drops the
/// 0x8000 bit of `obj4E4.flags` and pins the task to the tick handler (`state` 2);
/// once the count is spent it puts the enemy's HP (`GpEnemy::hp`)
/// at 1 and arms pose 6, leaving `state` alone.
///
/// The work block is read twice on purpose. The two loads do not CSE (the
/// `field_5B4` / `field_5AC` stores sit between them), and the first pointer is
/// still live at the tail for `obj4E4.flags` and `field_55C`, so the second one
/// needs a register of its own.
void func_actor_105100_80136318(Task* arg0)
{
    Actor105100Work* work;
    Actor105100Work* sndWork;
    GpEffWork*       eff;
    s32              snd;

    work = arg0->work;
    if (D_80073BA0 <= 0) {
        ((GpEnemy*)arg0->spawnArg2)->hp = 1;
        work->field_596                 = 6;
        work->field_598                 = 0;
        return;
    }

    work->field_5B4 = 0;
    work->field_5AC = 0;

    sndWork = arg0->work;

    snd = sndWork->field_580;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_580 = 0;
    }
    snd = sndWork->field_584;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_584 = 0;
    }
    snd = sndWork->field_588;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_588 = 0;
    }

    eff                = work->field_55C;
    work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
    if (eff != NULL) {
        eff->task->state = 4;
        work->field_55C  = NULL;
    }

    arg0->state = 2;
}

void func_actor_105100_80136408(Task* arg0)
{
    Actor105100Work* work;
    s32              i;
    s32              val;

    work = arg0->work;
    i    = 1;
    if ((s16)work->field_58E != work->field_590) {
        work->field_590 = work->field_58E;
        work->field_592 = 0;
        val             = D_actor_105100_801414C8[(s16)work->field_58E];
        do {
            func_800B4114(work, i, (s16)work->field_58E, 0, val);
            i++;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        work->field_592 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}

/// Relights the actor at its model's world position: copies the model
/// coordinate's translation into a `VECTOR` and hands it with the context to
/// `Gp_UpdateActorColor`, with no blend parameters.
void func_actor_105100_801364CC(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

/// The child collision task's state handlers, indexed by `Task::state`:
/// setup, per-frame reaction and teardown.
const GpEnemyTaskFuncTable3 D_actor_105100_80131EB0 = {
    {
        func_actor_105100_80135278,
        (GpEnemyTaskFunc)func_actor_105100_801354E8,
        func_actor_105100_80136788,
    },
};

void func_actor_105100_80136524(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x9C4, 0x80);
}

/// Sets the model's root coordinate to `arg1` scaled by `arg2`, and marks it
/// for recomputation. The scale is built in a 0x30-byte block borrowed from
/// the scratchpad: an identity rotation is written word-wise and
/// `ScaleMatrix` scales it, on all three axes when `arg3` is non-zero and on
/// Y alone when it is zero.
///
/// The scratchpad head is written twice, from two separate computations of
/// `head - 0x30`. CSE cannot substitute a value that holds no register, so
/// the store keeps the block-local `$v1` while `blk` - which crosses both
/// calls - is copied into `$s0` by `reload_cse_regs`. Folding the two into one
/// variable allocates `blk`'s register for the store as well and loses the
/// copy, the delay-slot fill and the frame layout.
void func_actor_105100_80136574(Task* arg0, MATRIX* arg1, s16 arg2, s32 arg3)
{
    void*              head;
    ActorScaleScratch* blk;
    GsCOORDINATE2*     coord;

    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = (u8*)head - 0x30;
    blk                = (ActorScaleScratch*)((u8*)head - 0x30);
    coord              = ((TmdObject*)arg0->extra)->coords;

    if (arg3 == 0) {
        blk->scale.vx = 0x1000;
        blk->scale.vy = arg2;
        blk->scale.vz = 0x1000;
    } else {
        blk->scale.vx = arg2;
        blk->scale.vy = arg2;
        blk->scale.vz = arg2;
    }

    coord->coord = *arg1;

    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;

    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->mat.mat);
    coord->flg = 0;
    SCRATCH_POP_BYTES(0x30);
}

void func_actor_105100_8013667C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_105100_801366D8(GpEnemy* arg0, Task* arg1)
{
    Actor105100Work* work;

    work = (Actor105100Work*)arg1->work;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    Gp_DestroyEnemy(arg0, arg1);
}

void func_actor_105100_8013672C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131EB0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_105100_80136788(GpEnemy* arg0, Task* arg1)
{
    Gp_UnlinkObj(arg1->work);
    Gp_DestroyEnemy(arg0, arg1);
}
