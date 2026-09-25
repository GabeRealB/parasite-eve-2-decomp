#include "common.h"

#include <psyq/abs.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Ground position and collision offset borrowed from G_SCRATCH_HEAD by
/// func_actor_800100_801635F4.
typedef struct {
    /* 0x00 */ VECTOR3 pos;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ u16     vx;
    /* 0x12 */ u16     vy;
    /* 0x14 */ u16     vz;
    /* 0x16 */ u16     pad_16;
} Actor800100ShadowScratch;
STATIC_ASSERT_SIZEOF(Actor800100ShadowScratch, 0x18);

/// Unsigned-byte view used before sign-extending the heading multiplier.
typedef struct {
    byte pad[0x973];
    u8   field_973;
} Actor800100DirByte;

void func_actor_800100_801635F4(Task* arg0);
void func_actor_800100_80163A58(Task* arg0);
void func_actor_800100_80165528(Task* arg0);

/// 0x38 block `func_actor_800100_801624F0` allocates with `memCalloc` when
/// its task enters state 0 and stores at `Task::work`: the launched
/// projectile's object plus its one-entry collision table, whose `field_0` is
/// armed with 2. `obj.ctx.recs` points at `rec`, `obj.coord` at the task's
/// own coordinate, and `obj.key` is the hit payload `0x21C9E`. The
/// projectile flies out along `work->angle` while `work->scale` opens, then
/// drops; `func_actor_800100_801631C8` hands the block back to `Gp_UnlinkObj`
/// on teardown. Same shape as the m4a1_pyke dart's `M4a1PykeBeam`.
typedef struct _Actor800100Beam {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[1];
} Actor800100Beam;
STATIC_ASSERT_SIZEOF(Actor800100Beam, 0x38);

/// 0x5C-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_80166514`: the `GpCoord` it hands to
/// `Gp_PlaceCoordOffset` / `func_actor_800100_801668C0`, the `rot` offset
/// applied to it, and the angle `func_actor_800100_8016709C` returns.
typedef struct _Actor800100PlaceScratch {
    /* 0x00 */ GpCoord coord;
    /* 0x50 */ SVECTOR rot;
    /* 0x58 */ u16     angle;
    /* 0x5A */ byte    pad_5A[2];
} Actor800100PlaceScratch;
STATIC_ASSERT_SIZEOF(Actor800100PlaceScratch, 0x5C);

/// 0x20-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_80164710` and `func_actor_800100_80164B9C`: the lock
/// position `Gp_GetLockPos` fills (also the `VECTOR3` handed to
/// `func_80103C74`), and the `rot` vector above it whose `vx`/`vz`
/// `func_80103D8C` measures. `80164B9C` also treats it as the `VECTOR3`
/// handed to `func_8010BD88` / `func_8010BE5C`.
typedef struct _Actor800100LockScratch {
    /* 0x00 */ VECTOR3 lock;
    /* 0x0C */ byte    pad_C[4];
    /* 0x10 */ VECTOR3 rot;
    /* 0x1C */ byte    pad_1C[4];
} Actor800100LockScratch;
STATIC_ASSERT_SIZEOF(Actor800100LockScratch, 0x20);

/// 0x1C-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_8016666C` to draw the vertical `LINE_G2` that
/// `func_actor_800100_80166514` puts on the placed coordinate. `origin` is the
/// vector pushed through the coordinate's `workm` first — always (0, 0, 0), so
/// `sxy0` is the origin's screen point — and `tip` the second, `angle` units
/// straight up, so `sxy1` is the screen point of the far end. `otz` is the
/// `gte_stszotz` of that second projection, already shifted, and doubles as
/// the `Gp_AddTpageShift` bucket.
typedef struct _Actor800100LineScratch {
    /* 0x00 */ DVECTOR sxy0;
    /* 0x04 */ DVECTOR sxy1;
    /* 0x08 */ s32     otz;
    /* 0x0C */ SVECTOR origin;
    /* 0x14 */ SVECTOR tip;
} Actor800100LineScratch;
STATIC_ASSERT_SIZEOF(Actor800100LineScratch, 0x1C);

/// 0x30-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_80162E90` to draw the projectile's ground splash: the
/// four corners of the unit quad `D_80111E38`, each scaled to the splash
/// half-size, rotated flat into view space by `Gfx_ViewWorldMtx` and moved to
/// the traced ground point. `sxy` is where they land on screen, `vec[0]`
/// through a single `RTPS` and the rest through one `RTPT`. Same shape as the
/// m4a1_pyke dart's splash block, but with `otz` and `flag` kept on the stack
/// instead of in the block.
typedef struct _Actor800100SplashScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ DVECTOR sxy[4];
} Actor800100SplashScratch;
STATIC_ASSERT_SIZEOF(Actor800100SplashScratch, 0x30);

/// One corner of the beam quad `func_actor_800100_801668C0` draws, as an
/// offset in the placed coordinate's own frame: `vy` straight up, `vz` along
/// the face. `D_actor_800100_80161F10` is the four of them.
typedef struct _Actor800100QuadCorner {
    /* 0x00 */ s16 vy;
    /* 0x02 */ s16 vz;
} Actor800100QuadCorner;
STATIC_ASSERT_SIZEOF(Actor800100QuadCorner, 4);

/// 0x44-byte block from `G_SCRATCH_HEAD` used by `func_actor_800100_801668C0`
/// to draw the textured sheet `func_actor_800100_80166514` places: the four
/// `v` corners are the `D_actor_800100_80161F10` (y, z) pairs offset by the
/// coordinate's world `t`, projected through `GsWSMATRIX` into `sxy`, and
/// `otz` is the `gte_stszotz` of the last of them.
typedef struct _Actor800100QuadScratch {
    /* 0x00 */ DVECTOR sxy[4];
    /* 0x10 */ s32     otz;
    /* 0x14 */ VECTOR  work;
    /* 0x24 */ SVECTOR v[4];
} Actor800100QuadScratch;
STATIC_ASSERT_SIZEOF(Actor800100QuadScratch, 0x44);

/// NULL-terminated `GpImgRec*` frame lists for `func_actor_800100_80163A58`,
/// indexed `table[field_987 - 1][field_989]`; `D_actor_800100_80167210` is
/// the `field_98A` sequence.
extern GpImgRec** D_actor_800100_80167200[];
extern GpImgRec** D_actor_800100_80167210[];

/// Translation the flare's own coordinate starts at, `(0, 0x200, 0x40)`.
extern SVECTOR D_actor_800100_80167128;

extern void D_actor_800100_80167130;
extern s16  D_actor_800100_80167218[];
extern s16  D_actor_800100_80167224[];
extern u8   D_actor_800100_80167230[];

void func_actor_800100_80162264(VECTOR3* arg0, u16 arg1, s32 arg2);
/// Draws one frame of the launched projectile's spinning sprite at `pos`:
/// `frame` walks the twelve windows of `D_80111E48`, `width` is the flare's
/// half-width (divided down by the projected depth) and `ang` its spin, so the
/// quad is a square rotated by `ang` rather than an axis-aligned sprite.
void func_actor_800100_80162A14(VECTOR3* pos, u16 frame, u16 width, s16 ang);
void func_actor_800100_80162E90(VECTOR3* arg0, s32 arg1);
void func_actor_800100_801631C8(Task* arg0);
void func_actor_800100_80163214(Task* arg0);
void func_actor_800100_80163C04(Task* arg0);
void func_actor_800100_80163D54(Task* arg0);
void func_actor_800100_80163F04(Task* arg0);
void func_actor_800100_80164184(Task* arg0);
void func_actor_800100_801643F4(Task* arg0);
void func_actor_800100_80164580(Task* arg0);
void func_actor_800100_80164710(Task* arg0);
void func_actor_800100_80164940(Task* arg0);
void func_actor_800100_80164B9C(Task* arg0);
void func_actor_800100_80164E60(Task* arg0);
void func_actor_800100_80165010(Task* arg0);
void func_actor_800100_801652B0(Task* arg0);
void func_actor_800100_801655C0(Task* arg0);
void func_actor_800100_80165630(Task* arg0);
void func_actor_800100_80165664(Task* arg0);
void func_actor_800100_801656C8(Task* arg0);
void func_actor_800100_801656F4(Task* arg0);
void func_actor_800100_80165720(Task* arg0);
void func_actor_800100_80165748(Task* arg0);
void func_actor_800100_801657D8(Task* arg0);
void func_actor_800100_80165818(Task* arg0);
void func_actor_800100_80165850(Task* arg0);
void func_actor_800100_801658E8(Task* arg0);
void func_actor_800100_80165928(Task* arg0);
void func_actor_800100_80165930(Task* arg0);
void func_actor_800100_801659EC(Task* arg0);
void func_actor_800100_80165C38(Task* arg0);
void func_actor_800100_80165DE8(Task* arg0);
void func_actor_800100_80165F50(Task* arg0);
void func_actor_800100_80166190(Task* arg0);
void func_actor_800100_8016666C(GpCoord* arg0, s16 arg1);
void func_actor_800100_801668C0(GpCoord* arg0);
s32  func_actor_800100_80166B40(GpRec18* arg0, GpCoord* arg1, GpCoord* arg2);
void func_actor_800100_80166DD0(Task* arg0);
void func_actor_800100_80166DF0(Task* arg0);
void func_actor_800100_80166E14(Task* arg0);
void func_actor_800100_80166E94(Task* arg0, s32 arg1);
void func_actor_800100_80166EE8(Task* arg0);
s32  func_actor_800100_8016709C(GpCoord* arg0, GpRec18* arg1, GpRec18* arg2);

s32  func_8010BC70(GpCoord* arg0);
s32  func_8010BCF4(Task* arg0, VECTOR3* arg1);
void func_8010BD88(Task* arg0, VECTOR3* arg1);
void func_8010BE5C(Task* arg0, VECTOR3* arg1);
s32  func_80105ED4(Task* arg0);
void Gp_PlaceCoordOffset(GpCoord* arg0, GpCoord* arg1, SVECTOR* arg2);
s32  rand();

void Gp_PlayerMode2State0(Task* arg0);
void Gp_PlayerMode2State1(Task* arg0);
void Gp_PlayerMode2State2(Task* arg0);
void Gp_PlayerMode2State6(Task* arg0);

extern s32 D_80115738;
extern s32 D_8011574C;
extern u16 D_80112F60[];
extern u8* D_actor_800100_801672F8[];
extern u8  D_actor_800100_80167308[];
extern u8  D_actor_800100_80167310[];

/// Per-frame flare task of the actor: while the player model is visible
/// (`field_C & 0x80` clear) and the room is not fading out
/// (`Gp_State1C->eventState < 2`) it claims room-light slot 3 as the flare's
/// coordinate. State 0 hangs that coordinate off the actor's own at the fixed
/// offset and zeroes its `age`; state 1 then dispatches on `spawnArg1`:
///
/// - 1 draws the flare at the coordinate's `workm.t` every frame and re-aims
///   the light at a random angle in `0x400..0xB00`, arming the flare width in
///   `scale`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x60181`
///   as a child of this task, and re-claims the light with a much wider
///   (`0x400` / `0x4000`) falloff and a `0x800..0xF00` angle.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `Gp_State1C->eventState` is non-zero the two drawing sub-states wind
/// `age` back down instead of advancing.
void func_actor_800100_80161F20(Task* task)
{
    GpEffWork*    work;
    GpCoord*      coord;
    GpCoord64*    base;
    GpPointLight* slot;
    GpCoord*      light;
    GpMtxWords*   rot;
    GpEffWork*    eff;
    u32           ang;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    base  = &Gp_RoomCoords[3];
    light = &base->data.coord;
    slot  = &base->data.light;
    if ((gameGetPtrSlot(10)->extra.tmd->flags & 0x80) != 0) {
        return;
    }
    if (Gp_State1C->eventState >= 2) {
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = work->parent;
            rot->m00_m01      = 0x1000;
            rot->m02_m10      = 0;
            rot->m11_m12      = 0x1000;
            rot->m20_m21      = 0;
            rot->m22          = 0x1000;
            coord->coord.t[0] = D_actor_800100_80167128.vx;
            coord->coord.t[1] = D_actor_800100_80167128.vy;
            coord->coord.t[2] = D_actor_800100_80167128.vz;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            task->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            switch (task->spawnArg1) {
                case 0:
                    break;
                case 1:
                    if (Gp_State1C->eventState != 0) {
                        work->age--;
                        func_actor_800100_80162264(
                            (VECTOR3*)&coord->workm.t, work->age, 0x80);
                        break;
                    }
                    func_actor_800100_80162264(
                        (VECTOR3*)&coord->workm.t, work->age, 0x80);
                    base->framesLeft = 4;
                    slot->inner      = 0x80;
                    slot->outer      = 0x400;
                    ang              = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState      = ang;
                    slot->head.r     = ((ang >> 16) & 0x700) + 0x400;
                    slot->head.g     = (u16)slot->head.r >> 1;
                    slot->head.b     = slot->head.r >> 2;
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->flg  = 0;
                    work->scale = 0x40;
                    break;
                case 2:
                    if (Gp_State1C->eventState != 0) {
                        work->age--;
                        break;
                    }
                    if (work->scale < 0x180) {
                        work->scale = work->scale + 0x40;
                    }
                    eff = Gp_SpawnEff(0x60181, coord, work->scale, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->task);
                    }
                    base->framesLeft = 4;
                    slot->inner      = 0x400;
                    slot->outer      = 0x4000;
                    ang              = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState      = ang;
                    slot->head.r     = ((ang >> 16) & 0x700) + 0x800;
                    slot->head.g     = (u16)slot->head.r >> 1;
                    slot->head.b     = slot->head.r >> 2;
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->flg = 0;
                    break;
                case 3:
                    task->spawnArg1 = 1;
                    break;
                case 4:
                    task->spawnArg1 = 0;
                    break;
                case 5:
                    Gp_ReleaseState1CMem(work, task);
                    break;
            }
            break;
    }
}

/// Links one frame of the flare's animated sprite at the world point `pos`.
/// The point is projected through `GsWSMATRIX` by a single `RTPS` and the quad
/// is dropped when that sets a negative `gte_stflg`. `frame % 6` picks one of
/// the six 0x20-wide texture frames on tpage 0x29 (CLUT 0x430D) and
/// `brightness` sizes it: the corners sit `brightness * 31 / otz` from the
/// projected centre, so the sprite shrinks with distance. Same 0x18-byte
/// `GpRingScratch` block and axis-aligned quad as the gameplay sprite
/// drawers.
void func_actor_800100_80162264(VECTOR3* pos, u16 frame, s32 brightness)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    s16            x;
    s16            y;
    u16            uv;
    s32            u0;
    s32            u1;
    u16            vz;

    scratch                                 = SCRATCH_HEAD_ADDR;
    head                                    = SCRATCH_HEAD_AT(scratch, void);
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)pos->vx;
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)pos->vy;
    vz                                      = (u16)pos->vz;
    SCRATCH_HEAD_AT(scratch, void)          = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x430D;
        prim->v0    = 0x98;
        prim->v1    = 0x98;
        prim->v2    = 0xB7;
        prim->v3    = 0xB7;
        uv          = frame % 6;
        u0          = uv << 5;
        u1          = u0 + 0x1F;
        prim->u0    = u0;
        prim->u1    = u1;
        prim->u2    = u0;
        prim->u3    = u1;
        block->step = ((u16)brightness * 31) / block->otz;
        x           = (u16)block->sx - (u16)block->step;
        prim->x2    = x;
        prim->x0    = x;
        x           = (u16)block->sx + (u16)block->step;
        prim->x3    = x;
        prim->x1    = x;
        y           = (u16)block->sy - (u16)block->step;
        prim->y1    = y;
        prim->y0    = y;
        y           = (u16)block->sy + (u16)block->step;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Projectile task of the actor: while the state block says a fade-out is not
/// running it winds `work->age` (the animation frame, halved for the
/// draw) forward, and while one is (`Gp_State1C->eventState` non-zero) it just
/// redraws at the coordinate. `eventState >= 4` tears the task down.
///
/// - State 0 allocates the projectile's `Actor800100Beam`, claims the exit
///   callback, seeds its spin from `Gp_LcgState`, and rotates the scratch
///   `(0, pitch, roll)` vector by the task's own coordinate through the GTE
///   to get the launch direction. It arms the record's payload `0x21C9E`,
///   links the object onto list 1, and falls through.
/// - State 1 steps the coordinate by that direction, redraws, and rolls
///   `Gp_LcgState % 3` to drop a ground impact (`Gp_TraceGroundCoord` plus
///   `func_actor_800100_80162E90` at two thirds of the width) when the room's
///   ground is live. A hit on anything (`func_800DE7CC`) ends the flight into
///   state 2, and a miss after 0x15 frames releases the task.
/// - State 2 keeps falling at four times the speed until the same 0x15.
void func_actor_800100_801624F0(Task* task)
{
    GpCoord          ground;
    SVECTOR          after;
    SVECTOR          before;
    GpCoord*         coord;
    GpEffWork*       work;
    Actor800100Beam* beam;
    s32              fade;
    u32              ang0;
    u32              ang1;
    u32              ang2;
    u32              ang3;

    beam  = (Actor800100Beam*)task->work;
    work  = task->spawnArg2;
    fade  = Gp_State1C->eventState;
    coord = task->extra.tmd->coords;
    if (fade >= 4) {
        if (task->state != 0) {
            Gp_UnlinkObj(&beam->obj);
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    if (fade != 0) {
        Gp_UpdateCoord(coord);
        func_actor_800100_80162A14((VECTOR3*)coord->workm.t,
                                   (work->age >> 1) + 1, work->scale,
                                   work->angle);
        return;
    }
    work->age = work->age + 1;
    switch (task->state) {
        case 0:
            beam = memCalloc(sizeof(Actor800100Beam), 0);
            if (beam == NULL) {
                work->age = 0;
                return;
            }
            task->exitCallback = func_actor_800100_801631C8;
            work->move.vx      = 0;
            ang0               = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState        = ang0;
            work->move.vy      = (u16)task->spawnArg1 - ((ang0 >> 16) & 0x3F);
            work->move.vz      = 0;
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            work->scale        = (u16)task->spawnArg1 + 0x180;
            ang1               = Gp_LcgState * 5 + 0x71357911;
            work->angle        = (ang1 >> 16) & 0xFFF;
            task->state        = 1;
            task->work         = (TaskIdMap*)beam;
            beam->obj.coord    = coord;
            beam->obj.ctx.recs = beam->rec;
            beam->obj.key      = 0x21C9E;
            beam->obj.radius   = work->scale >> 1;
            Gp_LcgState        = ang1;
            beam->obj.flags    = 1;
            Gp_LinkObj(1, &beam->obj);
            beam->rec[0].flags = 2;
            beam->obj.flags   |= 0x8000;
            /* fallthrough */
        case 1:
            work->scale        = work->scale + 0x10;
            work->move.vy      = work->move.vy + 8;
            before.vx          = coord->workm.t[0];
            before.vy          = coord->workm.t[1];
            before.vz          = coord->workm.t[2];
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            func_actor_800100_80162A14((VECTOR3*)coord->workm.t,
                                       (work->age >> 1) + 1, work->scale,
                                       work->angle);
            ang2        = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = ang2;
            if ((u16)((ang2 >> 16) % 3) == 0 && Gp_State1C->groundTrace != 0 &&
                Gp_TraceGroundCoord(coord, &ground) == 1) {
                func_actor_800100_80162E90((VECTOR3*)ground.workm.t,
                                           (s16)((work->scale * 2) / 3));
            }
            if (Gp_CountRec18Hi(beam->obj.ctx.recs, 0x30000) != 0) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            if (func_800DE7CC(&after, &before, NULL, NULL) == 1) {
                Gp_UnlinkObj(&beam->obj);
                task->state   = 2;
                work->move.vx = (u32)rcos(work->angle) >> 8;
                work->move.vy = (u32)rsin(work->angle) >> 8;
                ang3          = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState   = ang3;
                work->move.vz = (u32)rsin((ang3 >> 16) & 0xFFF) >> 8;
                return;
            }
            if (work->age >= 0x15) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            Gp_ClearRec18Occupied(beam->rec);
            return;
        case 2:
            work->scale        = work->scale + 0x40;
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_actor_800100_80162A14((VECTOR3*)coord->workm.t,
                                       (work->age >> 1) + 1, work->scale,
                                       work->angle);
            if (work->age >= 0x15) {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws one frame of the launched projectile's spinning sprite: a single
/// semi-transparent, textured `POLY_FT4` billboarded on the world point `pos`.
/// `frame` walks the twelve sprite windows of `D_80111E48`, `width` is the
/// flare's half-width (divided down by the projected depth) and `ang` its spin,
/// so the quad is a square rotated by `ang` rather than an axis-aligned sprite.
/// `otz` is biased by one before it is used as the divisor so a point on the
/// near plane cannot divide by zero.
void func_actor_800100_80162A14(VECTOR3* pos, u16 frame, u16 width, s16 ang)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    GpEffUv8*        rec;
    u16              idx;
    s32              a;
    u16              vz;

    scratch                                   = SCRATCH_HEAD_ADDR;
    head                                      = SCRATCH_HEAD_AT(scratch, void);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)pos->vx;
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)pos->vy;
    vz                                        = (u16)pos->vz;
    SCRATCH_HEAD_AT(scratch, void)            = block;
    block->vec.vz                             = vz;
    vecp                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    idx = frame % 12;
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        rec         = &D_80111E48[idx];
        prim->clut  = (rec->clutY << 6) | ((rec->clutX >> 4) & 0x3F);
        prim->u0    = rec->u;
        prim->v0    = rec->v;
        prim->u1    = rec->u + 0x27;
        prim->v1    = rec->v;
        prim->u2    = rec->u;
        prim->v2    = rec->v + 0x27;
        prim->u3    = rec->u + 0x27;
        prim->v3    = rec->v + 0x27;
        a           = ang;
        block->dx   = (((width * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy   = (((width * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x0    = block->sx + (u16)block->dx;
        prim->x3    = block->sx - (u16)block->dx;
        prim->y0    = block->sy - (u16)block->dy;
        a           = a + 0x400;
        prim->y3    = block->sy + (u16)block->dy;
        block->dx   = (((width * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy   = (((width * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x1    = block->sx + (u16)block->dx;
        prim->x2    = block->sx - (u16)block->dx;
        prim->y1    = block->sy - (u16)block->dy;
        prim->y2    = block->sy + (u16)block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws the projectile's ground splash at the traced ground point `pos`: the
/// unit quad `D_80111E38` scaled to `width` half-size, laid flat by
/// `Gfx_ViewWorldMtx`, and projected through `GsWSMATRIX` into a 0x30-byte
/// `G_SCRATCH_HEAD` block. The first corner goes through `rtps` and the other
/// three through one `rtpt`; a negative `gte_stflg` drops the quad.
void func_actor_800100_80162E90(VECTOR3* pos, s32 width)
{
    void**                    scratch;
    u8*                       head;
    Actor800100SplashScratch* block;
    POLY_FT4*                 prim;
    GpQuadCorner*             tbl;
    s32                       i;
    s32                       flag;
    s32                       otz;

    scratch = SCRATCH_HEAD_ADDR;
    head    = (u8*)SCRATCH_HEAD_AT(scratch, void) - 0x30;
    SOFT_TOUCH_REG(head);
    SCRATCH_HEAD_AT(scratch, void) = head;
    block                          = (Actor800100SplashScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    tbl = D_80111E38;
    do {
        block->vec[i].vx = tbl[i].x * width;
        block->vec[i].vy = 0;
        block->vec[i].vz = tbl[i].y * width;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&block->vec[i]);
        gte_rtv0();
        gte_stsv(&block->vec[i]);
        (u16) block->vec[i].vx = (u16)block->vec[i].vx + (u16)pos->vx;
        (u16) block->vec[i].vy = (u16)block->vec[i].vy + (u16)pos->vy;
        (u16) block->vec[i].vz = (u16)block->vec[i].vz + (u16)pos->vz;
        i++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy[0]);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy[1], &block->sxy[2], &block->sxy[3]);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_stszotz(&otz);
        otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x40, 0x40, 0x40);
        prim->tpage = 0x29;
        prim->clut  = 0x430F;
        setUV4(prim, 0xE0, 0xC8, 0xFF, 0xC8, 0xE0, 0xE7, 0xFF, 0xE7);
        prim->x0 = block->sxy[0].vx;
        prim->y0 = block->sxy[0].vy;
        prim->x1 = block->sxy[1].vx;
        prim->y1 = block->sxy[1].vy;
        prim->x2 = block->sxy[2].vx;
        prim->y2 = block->sxy[2].vy;
        prim->x3 = block->sxy[3].vx;
        prim->y3 = block->sxy[3].vy;
        addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x30);
}

void func_actor_800100_801631C8(Task* arg0)
{
    GpObj* temp_a0;
    void*  temp_s1;

    temp_a0 = arg0->work;
    temp_s1 = arg0->spawnArg2;
    if (temp_a0 != NULL) {
        Gp_UnlinkObj(temp_a0);
    }
    Gp_ReleaseState1CMem(temp_s1, arg0);
}

void func_actor_800100_80163214(Task* arg0)
{
    GameActor*  actor;
    TmdObject*  extra;
    GpCoord*    coord;
    GpCoord*    next;
    GpCoord*    third;
    McSaveData* save;
    GpRec18*    recs;
    GpObj*      obj;
    GpActorD4*  d4;
    GpEffWork*  eff;
    Task*       task;
    SVECTOR3*   scratch;
    s32         idx;
    s32         packed;
    u8          saved;
    void*       head;

    actor              = arg0->work;
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = head - 8;
    scratch            = (SVECTOR3*)(head - 8);
    extra              = arg0->extra.tmd;
    coord              = extra->coords;
    arg0->state++;
    arg0->msgTable     = &D_actor_800100_80167130;
    arg0->exitCallback = func_actor_800100_80163C04;
    actor->field_938   = 0x14;
    Gp_ActorSlots[1]   = arg0;
    coord->sub         = &gGfxViewCoord;
    coord->flg         = 0;
    extra->flags       = 0;
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
    func_8010BFCC(arg0);
    actor->field_985 = 0x10;
    Gp_AnimResetChildSlots(arg0, actor->field_93C);
    Gp_AnimTickChildSlots(arg0);
    recs            = actor->field_17C;
    obj             = (GpObj*)actor->field_AC;
    actor->field_10 = coord->coord.t[0];
    actor->field_14 = coord->coord.t[1];
    actor->field_18 = coord->coord.t[2];
    obj->ctx.dir    = (GpObjDirRec*)actor->field_88;
    obj->coord      = coord;
    actor->field_90 = (s32)recs;
    save            = &Mc_SaveData;
    obj->pos.vy     = -0x12C;
    obj->pos.vx     = 0;
    obj->pos.vz     = 0;
    packed          = 0x10000;
    {
        s32 temp;

        temp        = save->characterId;
        obj->radius = 0x12C;
        obj->flags  = 4;
        obj->key    = temp | packed | 0x80;
        Gp_LinkObj(0, obj);
    }
    Gp_InitRec18Table((GpRec18*)actor->field_90, 0x12, 0);
    obj->flags     |= 0xC200;
    next            = arg0->extra.tmd->coords + 4;
    obj             = (GpObj*)actor->field_CC;
    obj->ctx.dir    = (GpObjDirRec*)actor->field_94;
    obj->coord      = next;
    actor->field_9C = (s32)recs;
    obj->pos.vx     = 0;
    obj->pos.vy     = 0x64;
    obj->pos.vz     = 0;
    {
        s32 f = 0x14;
        s32 temp;

        temp        = save->characterId;
        obj->radius = 0xDC;
        obj->flags  = f;
        obj->key    = temp | packed | 0x80;
        Gp_LinkObj(0, obj);
    }
    obj->flags     |= 0x8000;
    obj             = (GpObj*)actor->field_EC;
    third           = arg0->extra.tmd->coords;
    obj->ctx.dir    = (GpObjDirRec*)actor->field_A0;
    obj->coord      = third + 1;
    actor->field_A8 = (s32)recs;
    obj->pos.vx     = 0;
    obj->pos.vy     = 0x52;
    obj->pos.vz     = 0;
    {
        s32 temp;

        temp        = save->characterId;
        obj->radius = 0xDC;
        obj->flags  = 4;
        obj->key    = temp | packed | 0x80;
        Gp_LinkObj(0, obj);
    }
    obj->flags            |= 0xC000;
    actor->field_984       = 7;
    saved                  = Player_Status.field_26;
    Player_Status.field_26 = save->companionVariant;
    actor->field_920       = func_80104258(arg0, 0, 5, 1);
    actor->field_924       = func_80104258(arg0, 1, 5, 1);
    Player_Status.field_26 = saved;
    if (actor->field_924 != NULL) {
        task             = func_80104364((Task*)actor->field_924, save->companionType + 1, save->companionVariant, 0);
        actor->field_91C = task;
        if (task != NULL) {
            d4  = actor->field_910;
            idx = D_actor_800100_80167218[save->companionVariant];
            Gp_AttachActorObj(arg0, idx, D_actor_800100_80167224[save->companionVariant]);
            actor->field_124 |= 0x80;
            d4->actionCount   = D_actor_800100_80167230[save->companionVariant];
            if ((u8)save->companionVariant == 4) {
                eff = Gp_SpawnEff(0x80060180, actor->field_91C->extra.tmd->coords, idx, 0);
                if (eff != NULL) {
                    actor->field_914 = eff->task;
                    Task_Reparent((Task*)arg0, eff->task);
                    func_80106350(arg0, idx, 0);
                }
            }
        }
    }
    scratch->vx = 0;
    scratch->vy = -0x200;
    scratch->vz = 0;
    Gp_BindActorD4(arg0, scratch, 0x1000);
    func_8010BF7C(arg0, 0x3C, 0x7F);
    SCRATCH_POP_BYTES(8);
}

void func_actor_800100_801635F4(Task* arg0)
{
    Actor800100ShadowScratch* scratch;
    void**                    scratchHead;
    u8*                       head;
    GameActor*                actor;
    TmdObject*                work;
    TmdObject*                extra;
    GpCoord*                  coord;
    GpCoord*                  ground;
    GpActorD4*                d4;
    Task*                     task;
    GpObj*                    objs[2];
    s32                       dy;
    s32                       i;
    s8                        bits;

    scratchHead                        = SCRATCH_HEAD_ADDR;
    head                               = SCRATCH_HEAD_AT(scratchHead, void);
    extra                              = arg0->extra.tmd;
    SCRATCH_HEAD_AT(scratchHead, void) = head - 0x18;
    work                               = extra;
    scratch                            = (Actor800100ShadowScratch*)(head - 0x18);
    coord                              = work->coords;
    actor                              = arg0->work;
    d4                                 = actor->field_910;

    if (actor->field_954 != 2 &&
        (dy = coord->coord.t[1], dy = dy - actor->field_14, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->field_10;
        coord->coord.t[1] = actor->field_14;
        coord->coord.t[2] = actor->field_18;
    } else {
        actor->field_10 = coord->coord.t[0];
        actor->field_14 = coord->coord.t[1];
        actor->field_18 = coord->coord.t[2];
        if (actor->field_984 & 1) {
            actor->field_992 = func_801011D0(coord, actor->field_90, 0x12, &actor->field_930);
        } else {
            actor->field_992 = 0;
        }
    }

    task = actor->field_91C;
    if (task != NULL) {
        *(GpCoord*)actor->field_3D4 =
            *task->extra.tmd->coords;
        Gfx_RotMatrixX(&((GpCoord*)actor->field_3D4)->workm, -0x400, 0);
    }

    d4->coord = *arg0->extra.tmd->coords;
    Gfx_RotMatrixY(&d4->coord.workm, d4->scanAngle, 0);

    objs[0] = (GpObj*)actor->field_AC;
    objs[1] = (GpObj*)actor->field_EC;
    for (i = 0; i < 2; i++) {
        bits = actor->field_983;
        if ((bits >> i) & 1) {
            actor->field_984 |= 1 << i;
            objs[i]->flags   |= 0x4000;
        } else if (bits & (8 << i)) {
            actor->field_984 &= ~(1 << i);
            objs[i]->flags   &= ~0x4000;
        }
    }
    actor->field_983 = 0;

    if (D_80115768 == 0 && Gp_StateF0.field_4 == 0) {
        func_actor_800100_80165528(arg0);
    }
    func_actor_800100_80163A58(arg0);

    Gp_ClearRec18Occupied(actor->field_17C);
    Gp_ClearRec18Occupied(&actor->field_910->contact);
    if (actor->field_91C != NULL) {
        Gp_ClearRec18Occupied(actor->field_32C);
    }
    if (actor->field_984 & 1) {
        coord->coord.t[1] += 8;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);

    if ((s8)actor->field_986 != 0) {
        scratch->vx = (u16)actor->field_30.vx;
        scratch->vy = (u16)actor->field_30.vy;
        scratch->vz = (u16)actor->field_30.vz;
    } else {
        scratch->vx = (u16)coord->workm.m[0][2] *
                      (s8)((volatile Actor800100DirByte*)actor)->field_973;
        scratch->vy = (u16)coord->workm.m[1][2] *
                      (s8)((volatile Actor800100DirByte*)actor)->field_973;
        scratch->vz = (u16)coord->workm.m[2][2] *
                      (s8)((volatile Actor800100DirByte*)actor)->field_973;
    }
    ((SVECTOR*)actor->field_88)->vx = scratch->vx;
    ((SVECTOR*)actor->field_88)->vy = scratch->vy;
    ((SVECTOR*)actor->field_88)->vz = scratch->vz;
    ((SVECTOR*)actor->field_94)->vx = scratch->vx;
    ((SVECTOR*)actor->field_94)->vy = scratch->vy;
    ((SVECTOR*)actor->field_94)->vz = scratch->vz;
    ((SVECTOR*)actor->field_A0)->vx = scratch->vx;
    ((SVECTOR*)actor->field_A0)->vy = scratch->vy;
    ((SVECTOR*)actor->field_A0)->vz = scratch->vz;

    if (!(work->flags & 0x80)) {
        ground      = arg0->extra.tmd->coords + 1;
        ground->flg = 0;
        Gp_UpdateCoord(ground);
        if (func_800EA1A8((VECTOR3*)ground->workm.t, (VECTOR3*)scratch) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)scratch, 0x200, Gp_State1C->groundShade);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Texture-upload state of the actor: runs two independent sequences, each a
/// countdown (`field_988` / `field_98B`, reloaded with 4 / 8) that advances a
/// frame index (`field_989` / `field_98C`) through the NULL-terminated image
/// list `D_actor_800100_80167200` / `D_actor_800100_80167210` named by the
/// sequence number (`field_987` / `field_98A`), ending the sequence when its
/// list runs out. Every upload posts its image over an 8-byte `RECT` borrowed
/// from `G_SCRATCH_HEAD` and gives it back at the end of the call.
void func_actor_800100_80163A58(Task* arg0)
{
    void**      scratch;
    u8*         head;
    s32         temp;
    RECT*       rect;
    GameActor*  actor;
    GpImgRec*** table;
    s32         idx;
    u32         row;
    GpImgRec*   img;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    actor                          = arg0->work;
    temp                           = (s32)(head - 8);
    SCRATCH_HEAD_AT(scratch, void) = (void*)temp;
    rect                           = (RECT*)temp;

    if ((s8)actor->field_987 != 0) {
        actor->field_988--;
        if ((s8)actor->field_988 <= 0) {
            table = D_actor_800100_80167200;
            idx   = (s8)actor->field_987 - 1;
            img   = table[idx][(s8)actor->field_989];
            if (img != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x28;
                rect->w             = 0x15;
                rect->h             = 8;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_988 = 4;
                actor->field_989++;
            } else {
                actor->field_987 = 0;
            }
        }
    }

    if ((s8)actor->field_98A != 0) {
        actor->field_98B--;
        if ((s8)actor->field_98B <= 0) {
            table = D_actor_800100_80167210;
            idx   = (row = (s8)actor->field_98A - 1);
            img   = table[row][(s8)actor->field_98C];
            if (img != NULL) {
                rect->x = 8;
                rect->y = 0x40;
                rect->w = 0xD;
                rect->h = 0xC;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_98B = 8;
                actor->field_98C++;
            } else {
                actor->field_98A = 0;
            }
        }
    }

    SCRATCH_POP_BYTES(8);
}

void func_actor_800100_80163BF8(Task* arg0)
{
    arg0->state = 3;
}

void func_actor_800100_80163C04(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    Task*      task;

    actor            = arg0->work;
    d4               = actor->field_910;
    Gp_ActorSlots[1] = NULL;
    task             = actor->field_914;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->field_918;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->field_91C;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->field_920;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->field_924;
    if (task != NULL) {
        taskKill(task);
    }
    Gp_UnlinkObj((GpObj*)actor->field_AC);
    Gp_UnlinkObj((GpObj*)actor->field_CC);
    Gp_UnlinkObj((GpObj*)actor->field_EC);
    Gp_UnlinkObj((GpObj*)actor->field_10C);
    Gp_UnlinkObj(&d4->obj);
    taskKill((Task*)arg0);
}

/// State handlers of the actor's main task, indexed by its state.
const TaskFuncTable4 D_actor_800100_80161E3C = { {
    (TaskFunc)func_actor_800100_80163214,
    (TaskFunc)func_actor_800100_801635F4,
    func_actor_800100_80163BF8,
    (TaskFunc)func_actor_800100_80163C04,
} };

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects from the four-entry state table - set-up, the per-frame update, a
/// step that only advances to the last state, and the teardown that kills the
/// actor's child tasks and unlinks its objects. The table is a local, so it is
/// copied from `.rodata` onto the stack on every call.
void func_actor_800100_80163CF0(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800100_80161E3C;
    states.funcs[task->state](task);
}

void func_actor_800100_80163D54(Task* arg0)
{
    GameActor* actor;
    GpCoord*   coord;
    GpCoord*   target;
    s32        flag;
    s32        dist;
    s32        val;
    s16        count;

    actor  = arg0->work;
    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    flag   = (GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(4, 42, 0, 0);
    if (((GameActor*)arg0->work)->field_910->decisionTimer <= 0) {
        func_8010BF7C(arg0, 0xA, 0x1F);
        dist = func_8010BC70(coord);
        if ((dist >= 0x600 && (rand() & 0xFF) >= 0xF1) || (dist >= 0x400 && flag != 0)) {
            func_actor_800100_801656C8(arg0);
        } else {
            count = (u16)actor->field_942 + 1;
            do {
                actor->field_942 = count;
            } while (0);
            if (count >= ((rand() & 3) + 3)) {
                if (actor->field_95E == 0) {
                    actor->field_95E = 1;
                    Gp_AnimPlayChildSlotsEx(arg0, 0x17, 0, 5);
                } else if ((rand() & 0xFF) >= 0xD0) {
                    func_actor_800100_80165720(arg0);
                }
            } else {
                val = func_8010BCF4((Task*)arg0, (VECTOR3*)target->coord.t);
                if (val < 0) {
                    val = -val;
                }
                if (val >= 0x200) {
                    actor->field_90C = NULL;
                    func_actor_800100_801656F4(arg0);
                }
            }
        }
    }
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
}

/// Handlers `func_actor_800100_80165528` runs, indexed by `field_954`.
const TaskFuncTable3 D_actor_800100_80161E4C = { {
    func_actor_800100_80163F04,
    func_actor_800100_80165850,
    func_actor_800100_80165930,
} };

/// Handlers `func_actor_800100_80163F04` runs, indexed by `field_956`.
const TaskFuncTable12 D_actor_800100_80161E58 = { {
    func_actor_800100_80165748,
    func_actor_800100_80164184,
    func_actor_800100_801643F4,
    func_actor_800100_80164580,
    func_actor_800100_801657D8,
    func_actor_800100_80164710,
    func_actor_800100_80164940,
    func_actor_800100_80164B9C,
    func_actor_800100_80165818,
    func_actor_800100_80164E60,
    func_actor_800100_80165010,
    func_actor_800100_801652B0,
} };

/// Drives the actor's `field_954`/`field_956` callback tables while the
/// `field_944` countdown runs, spawning the drip effect every tenth frame.
/// `sp40` / `sp48` hold the effect position: it rides the water surface
/// (`gGameSession.waterY`) minus the actor coordinate's world Y.
void func_actor_800100_80163F04(Task* arg0)
{
    TaskFuncTable12 sp;
    SVECTOR         sp40;
    SVECTOR         sp48;
    GameActor*      actor;
    GpActorD4*      d4;
    GpCoord*        coord;
    s16             temp;
    s16             rem;
    s32             pan;

    sp    = D_actor_800100_80161E58;
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    d4    = actor->field_910;
    if (d4->decisionTimer > 0) {
        d4->decisionTimer--;
    }
    sp.funcs[actor->field_956](arg0);
    if ((u32)(func_80105ED4(arg0) + 0xEFFFFF77) < 4) {
        actor->field_944 = 0x78;
        sp40.vx          = 0;
        sp40.vy          = (u16)gGameSession->waterY - (u16)coord->coord.t[1];
        sp40.vz          = 0;
        Gp_SpawnEff(D_80115738, coord, 0x1202180, &sp40);
        Gp_SpawnEff(D_8011574C, coord, (rand() & 0x1F) | 0x40, &sp40);
    }
    temp = (u16)actor->field_944;
    if (temp != 0) {
        actor->field_944--;
        rem = temp % 10;
        if (rem == 0) {
            sp48.vx = 0;
            sp48.vy = (u16)gGameSession->waterY - (u16)coord->coord.t[1];
            sp48.vz = 0;
            Gp_SpawnEff(D_8011574C, coord, (rand() & 0x1F) | 0x40, &sp48);
        }
    }
    if ((s8)actor->field_97A == 0) {
        func_80109BB4(arg0, actor->field_17C);
        if ((u16)actor->field_96C != 0) {
            func_8010B9A4(arg0);
            pan = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(((Mc_SaveData.companionVariant - 1) << 16) + 0x4065000A, pan, (s8)gpGetObjDepth(coord));
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
    if (Mc_SaveData.companionHp <= 0) {
        Gp_StopPlayerAnim(arg0, 0);
    }
}

/// Lock-on entry and step of the actor's `field_95E` state machine. An aim
/// within `0x301` of the lock node re-arms the actor: `field_956` takes the
/// 10-frame delay, `field_95A`/`field_97E` latch the turn and decay, `field_960`
/// keeps the old `field_956`, and the `field_910` record's `scanDist` /
/// `scanAngle` are re-armed for the next sweep before the slot-1 child
/// animation. Otherwise state 0 zeroes `field_934` and picks state 2 (with
/// `field_958` 3) or state 1 (with `field_958` 1) from `func_8010BC70`'s
/// distance, states 1-3 only raise `field_973`. The shared drive then resets
/// the move when the target is within `0x301`, counts `field_934` up to `0xB4`
/// before latching state 3 through the state-1 entry, and otherwise drops
/// `field_93E` while it is positive, re-arming it to `0x3C` from a `rand()`
/// window and returning to state 0. The target's coordinate goes to
/// `func_8010BD88` and `func_8010BE5C`.
void func_actor_800100_80164184(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GpCoord*   coord;
    GpCoord*   target;
    s32        flag;
    s32        dist;
    s32        r;
    s32        arg;
    u16        timer;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    actor  = arg0->work;
    flag   = (GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(4, 42, 0, 0);
    dist   = func_actor_800100_8016709C(coord, &actor->field_910->contact, NULL);
    if (dist != 0 && dist < 0x301 && flag == 0) {
        GameActor* actor2 = arg0->work;

        timer             = actor2->field_956;
        actor2->field_956 = 0xA;
        actor2->field_95A = 1;
        actor2->field_97E = 1;
        d4                = actor2->field_910;
        actor2->field_954 = 0;
        actor2->field_95C = 0;
        actor2->field_95E = 0;
        actor2->field_960 = timer;
        d4->scanDist      = -1;
        d4->scanAngle     = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
        return;
    }
    switch (actor->field_95E) {
        case 1:
        case 2:
        case 3:
            actor->field_973 = 1;
            goto drive;
        case 0:
            actor->field_934 = 0;
            if (func_8010BC70(coord) >= 0x1600) {
                actor->field_95E = 2;
                actor->field_958 = 3;
                arg              = 4;
            } else {
            enter:
                if (actor->field_95E != 3) {
                    actor->field_95E = 1;
                }
                actor->field_958 = 1;
                arg              = 2;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            actor->field_973 = 1;
        drive:
            dist = func_8010BC70(coord);
            if (dist < 0x301) {
                Gp_ResetActorMove(arg0, 0);
            } else if (actor->field_95E != 3) {
                if (++actor->field_934 == 0xB4) {
                    actor->field_95E = 3;
                    goto enter;
                }
                if (actor->field_93E > 0) {
                    actor->field_93E = (u16)actor->field_93E - 1;
                } else {
                    r = rand() & 0x3FF;
                    if ((0x1000 - r) < dist || actor->field_95E != 2) {
                        if (dist < r + 0x1400 || actor->field_95E != 1) {
                            goto done;
                        }
                    }
                    actor->field_95E = 0;
                    actor->field_93E = 0x3C;
                }
            }
    }
done:
    func_8010BD88(arg0, (VECTOR3*)target->coord.t);
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
}

/// Lock-on drive for the actor's `field_95E` state machine. Builds a `VECTOR3`
/// at `G_SCRATCH_HEAD - 0x10` from the lock node (`Gp_GetLockPos`, or the
/// linked object's coord when `actor->field_90C` is set but flagged), plays the
/// 5/6 child-slot animation on entry, mirrors `field_93E` into `field_975` and
/// resets the actor's move once the aim is close enough.
void func_actor_800100_801643F4(Task* arg0)
{
    void**      scratch;
    VECTOR*     head;
    VECTOR3*    pos;
    GameActor*  actor;
    GpLinkNode* node;
    TmdObject*  extra;
    GpCoord*    src;
    s32         val;
    s32         arg;
    s32         flag;

    actor                            = arg0->work;
    extra                            = (gameGetPtrSlot(3))->extra.tmd;
    scratch                          = SCRATCH_HEAD_ADDR;
    head                             = SCRATCH_HEAD_AT(scratch, VECTOR);
    SCRATCH_HEAD_AT(scratch, VECTOR) = head - 1;
    pos                              = (VECTOR3*)(head - 1);
    node                             = actor->field_90C;
    src                              = extra->coords;
    if (node != NULL) {
        if (!(node->state.b.flags & 1)) {
            Gp_GetLockPos(node, pos);
        } else {
            actor->field_95E = 2;
        }
    } else {
        pos->vx = src->coord.t[0];
        pos->vy = src->coord.t[1];
        pos->vz = src->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            if (func_8010BCF4((Task*)arg0, pos) < 0) {
                actor->field_93E = -1;
                arg              = 5;
            } else {
                actor->field_93E = 1;
                arg              = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_975 = (u8)actor->field_93E;
            val              = func_8010BCF4((Task*)arg0, pos);
            if (val < 0) {
                val = -val;
            }
            if ((val < 0x81) || (actor->field_95E == 2)) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)src->coord.t);
    SCRATCH_POP(VECTOR);
}

/// Second arm of the lock-on drive: builds the lock position at
/// `G_SCRATCH_HEAD - 0x10` (`Gp_GetLockPos`, or `Gp_FindLockNodePad` when
/// `field_90C` is flagged) and measures the distance to it with
/// `func_8010BCF4`. Close enough latches `field_95E` to 1 and plays the slot-7
/// child animation; otherwise the target is handed to `Gp_TrackAllyLockTarget`
/// with 1. `field_95E` 2/3 waits for the chain to reach 3, which resets the
/// move fields and plays the slots 9/6 pair.
///
/// `track:` sits between the state store and `case 1` so the hand-off is
/// emitted after the store; the store's fall into case 1 is therefore a jump.
void func_actor_800100_80164580(Task* arg0)
{
    void**     scratch;
    VECTOR*    head;
    VECTOR3*   pos;
    GameActor* actor;
    s32        flag;
    s32        arg;
    s32        val;

    actor                            = arg0->work;
    scratch                          = SCRATCH_HEAD_ADDR;
    head                             = SCRATCH_HEAD_AT(scratch, VECTOR);
    SCRATCH_HEAD_AT(scratch, VECTOR) = head - 1;
    pos                              = (VECTOR3*)(head - 1);
    flag                             = 1;

    switch (actor->field_95E) {
        case 0:
            if (actor->field_90C != NULL) {
                if (actor->field_90C->state.b.flags & 1) {
                    actor->field_90C = Gp_FindLockNodePad(arg0);
                }
                Gp_GetLockPos(actor->field_90C, pos);
                val = func_8010BCF4((Task*)arg0, pos);
                if (val < 0) {
                    val = -val;
                }
                if (val >= 0x201) {
                    goto track;
                }
            }
            actor->field_95E = flag;
            goto caseOne;
        track:
            Gp_TrackAllyLockTarget(arg0, 1);
            break;
        caseOne:
        case 1:
            arg               = 7;
            actor->field_95C  = arg;
            actor->field_95E += 1;
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 3);
            /* fallthrough */
        case 2:
        case 3:
            Gp_TrackAllyLockTarget(arg0, 3);
            if (actor->field_95E == 3) {
                GameActor* actor2 = arg0->work;
                actor2->field_954 = 0;
                actor2->field_956 = 4;
                actor2->field_95C = 0;
                actor2->field_95E = 0;
                actor2->field_973 = 0;
                actor2->field_975 = 0;
                Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
            }
            break;
    }
    SCRATCH_POP(VECTOR);
}

void func_actor_800100_80164710(Task* arg0)
{
    GameActor*              actor;
    GameActor*              actor2;
    GameActor*              actor3;
    GpActorD4*              d4;
    GpLinkNode*             node;
    GpLinkNode*             lock;
    GpCoord*                coord;
    Actor800100LockScratch* scratch;
    Actor800100LockScratch* head;
    void**                  scratchHead;
    s32                     dist;
    u16                     state;

    head               = SCRATCH_HEAD(Actor800100LockScratch);
    actor              = arg0->work;
    scratch            = head - 1;
    SCRATCH_HEAD(void) = scratch;
    d4                 = actor->field_910;
    Gp_TrackAllyLockTarget(arg0, 3);
    state = actor->field_95E;
    if (state != 0) {
        if (state != 1) {
            scratchHead = SCRATCH_HEAD_ADDR;
        } else {
            goto block_10;
        }
    } else {
        lock = actor->field_90C;
        if ((lock == NULL) || (coord = arg0->extra.tmd->coords, Gp_GetLockPos(lock, &scratch->lock), func_80103C74(coord, &scratch->lock, &(head - 1)->rot), ((func_80103D8C(scratch->rot.vx, scratch->rot.vz) < 0x301) != 0))) {
            actor2            = arg0->work;
            actor2->field_954 = 0;
            actor2->field_956 = 4;
            actor2->field_95C = 0;
            actor2->field_95E = 0;
            actor2->field_973 = 0;
            actor2->field_975 = 0;
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
        } else {
            dist = func_8010BCF4((Task*)arg0, &scratch->lock);
            if (dist < 0) {
                dist = -dist;
            }
            if (dist < 0x181) {
                actor->field_95E += 1;
            block_10:
                if (((s8)d4->repeatCount <= 0) || (node = actor->field_90C, node == NULL) || (node->state.b.flags & 1)) {
                    // Stored through a plain pointer: the member-access spelling schedules differently.
                    *(GpLinkNode**)&actor->field_90C = NULL;
                    actor->field_97E                 = 1;
                    actor->field_12A                &= 0x3FFF;
                    if ((u8)Mc_SaveData.companionVariant == 4) {
                        func_80106350(arg0, D_actor_800100_80167218[Mc_SaveData.companionVariant], 0);
                    }
                    actor3            = arg0->work;
                    actor3->field_954 = 0;
                    actor3->field_956 = 4;
                    actor3->field_95C = 0;
                    actor3->field_95E = 0;
                    actor3->field_973 = 0;
                    actor3->field_975 = 0;
                    Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
                } else if (actor->field_940 == 0) {
                    func_actor_800100_80166EE8(arg0);
                }
            }
        }
        scratchHead = SCRATCH_HEAD_ADDR;
    }
    SCRATCH_POP_AT(scratchHead, Actor800100LockScratch);
}

/// Third arm of the lock-on drive, running the actor's `field_95E` state
/// machine over a `VECTOR3` carved from `G_SCRATCH_HEAD`. Case 0 latches the
/// state and `field_95A`, aims at the lock node and plays the 5/6 child-slot
/// animation with a random `field_934` hold; it falls into case 1, which
/// mirrors `field_93E` into `field_975` and, once the aim distance reaches
/// `field_934`, advances the state and plays slot 4. Case 2 counts
/// `field_934` down and, while it runs, asks `func_actor_800100_8016709C` for
/// the angle to what the ally block's `field_910->contact` recorded: under
/// 0x281 the actor hands over to the `0xA` / slot-1 chain (`field_97E` set,
/// `field_956` kept in `field_960`), otherwise `field_975` is cleared; a spent
/// counter resets the state to slot 9.
void func_actor_800100_80164940(Task* arg0)
{
    void**     scratch;
    VECTOR*    head;
    VECTOR3*   pos;
    GameActor* actor;
    GpCoord*   coord;
    GpActorD4* d4;
    u16        old;
    s16        angle;
    s32        flag;
    s32        arg;
    s32        val;
    s32        count;
    s32        dist;

    scratch                          = SCRATCH_HEAD_ADDR;
    head                             = SCRATCH_HEAD_AT(scratch, VECTOR);
    SCRATCH_HEAD_AT(scratch, VECTOR) = head - 1;
    pos                              = (VECTOR3*)(head - 1);
    actor                            = arg0->work;
    coord                            = arg0->extra.tmd->coords;
    flag                             = 1;
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = flag;
            actor->field_95A = flag;
            Gp_GetLockPos(actor->field_90C, pos);
            if (func_8010BCF4((Task*)arg0, pos) < 0) {
                actor->field_93E = flag;
                arg              = 6;
            } else {
                actor->field_93E = -1;
                arg              = 5;
            }
            actor->field_934 = (rand() & 0x1FF) + 0x400;
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_975 = (u8)actor->field_93E;
            Gp_GetLockPos(actor->field_90C, pos);
            val  = func_8010BCF4((Task*)arg0, pos);
            dist = actor->field_934;
            if (val < 0) {
                val = -val;
            }
            if (val >= dist) {
                actor->field_958  = 3;
                actor->field_95E += 1;
                actor->field_934  = (rand() & 0x1F) + 0x14;
                Gp_AnimPlayChildSlotsEx(arg0, 4, 0, 5);
            }
            break;
        case 2:
            actor->field_973 = flag;
            count            = actor->field_934 - 1;
            actor->field_934 = count;
            if (count <= 0) {
                GameActor* actor2 = arg0->work;
                actor2->field_954 = 0;
                actor2->field_956 = 4;
                actor2->field_95C = 0;
                actor2->field_95E = 0;
                actor2->field_973 = 0;
                actor2->field_975 = 0;
                Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
            } else {
                angle = func_actor_800100_8016709C(coord, &actor->field_910->contact, NULL);
                if (angle != 0) {
                    if (angle < 0x281) {
                        s16        anim   = 1;
                        GameActor* actor3 = arg0->work;
                        old               = actor3->field_956;
                        actor3->field_956 = 0xA;
                        actor3->field_97E = anim;
                        d4                = actor3->field_910;
                        actor3->field_954 = 0;
                        actor3->field_95A = flag;
                        actor3->field_95C = 0;
                        actor3->field_95E = 0;
                        actor3->field_960 = old;
                        d4->scanDist      = -1;
                        d4->scanAngle     = 0;
                        Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 6);
                    }
                } else {
                    actor->field_975 = 0;
                }
            }
            break;
    }
    SCRATCH_POP(VECTOR);
}

/// Aim/lock drive for the actor's `field_95E` phase machine. While the angle
/// to what the ally block's `field_910->contact` recorded is nonzero and under
/// `0x301`, `field_93E` counts up and the LCG decides the next aim window:
/// once the step passes `((Gp_LcgState >> 16) & 0x3F) + 0x28` the actor
/// latches into the `0xA` / child-slot-1 chain, keeping the old `field_956` in
/// `field_960` and clearing the aim offset on `field_910`. Otherwise it carves
/// a 0x20-byte `Actor800100LockScratch` off `G_SCRATCH_HEAD`, fills `lock`
/// either from the lock node (`Gp_GetLockPos`) or from the player's model
/// coordinate, runs the `field_95E` switch, measures the aim spread across
/// `rot`, and drops back to child slot 9 when the roll loses. Both arms end by
/// handing `lock` to `func_8010BD88` / `func_8010BE5C` and returning the
/// scratch.
void func_actor_800100_80164B9C(Task* arg0)
{
    GameActor*              actor;
    GameActor*              actor2;
    GameActor*              actor3;
    GpActorD4*              d4;
    GpCoord*                coord;
    GpCoord*                target;
    Actor800100LockScratch* block;
    GpLinkNode*             node;
    void**                  scratch;
    u8*                     head;
    u16                     step;
    u16                     old;
    s16                     anim;
    u32                     random;
    s32                     angle;
    s32                     val;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    actor  = arg0->work;
    angle  = func_actor_800100_8016709C(coord, &actor->field_910->contact, NULL);
    if (angle != 0 && angle < 0x301) {
        step             = actor->field_93E + 1;
        actor->field_93E = step;
        random           = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState      = random;
        if ((s16)step >= (s32)(((random >> 16) & 0x3F) + 0x28)) {
            anim              = 1;
            actor2            = arg0->work;
            old               = actor2->field_956;
            actor2->field_956 = 0xA;
            actor2->field_95A = anim;
            actor2->field_97E = anim;
            d4                = actor2->field_910;
            actor2->field_954 = 0;
            actor2->field_95C = 0;
            actor2->field_95E = 0;
            actor2->field_960 = old;
            d4->scanDist      = -1;
            d4->scanAngle     = 0;
            Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 6);
            return;
        }
    }
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    SCRATCH_HEAD_AT(scratch, void) = head - 0x20;
    block                          = (Actor800100LockScratch*)(head - 0x20);
    node                           = actor->field_90C;
    if (node != NULL) {
        if ((node->state.b.flags & 1) == 0) {
            Gp_GetLockPos(node, &block->lock);
        } else {
            actor->field_95E = 2;
        }
    } else {
        block->lock.vx = target->coord.t[0];
        block->lock.vy = target->coord.t[1];
        block->lock.vz = target->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = 1;
            actor->field_934 = 0;
            actor->field_958 = 3;
            Gp_AnimPlayChildSlotsEx(arg0, 0xC, 0, 5);
        case 1:
        case 2:
            break;
        default:
            goto tail;
    }
    actor->field_973 = 1;
    func_80103C74(coord, &block->lock, &block->rot);
    angle = func_80103D8C(block->rot.vx, block->rot.vz);
    if (actor->field_90C != NULL) {
        val = (rand() & 0x3FF) + 0xB00;
    } else {
        val = 0xB00;
    }
    if (val >= angle || actor->field_95E == 2) {
        actor3            = arg0->work;
        actor3->field_954 = 0;
        actor3->field_956 = 4;
        actor3->field_95C = 0;
        actor3->field_95E = 0;
        actor3->field_973 = 0;
        actor3->field_975 = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
    }
tail:
    func_8010BD88(arg0, &block->lock);
    func_8010BE5C(arg0, &block->lock);
    SCRATCH_POP_BYTES(0x20);
}

void func_actor_800100_80164E60(Task* arg0)
{
    GameActor* actor;
    GameActor* target;
    GpActorD4* d4;
    GpAnimRec* rec;
    GpCoord*   coord;
    s16        sel;

    actor = arg0->work;
    d4    = actor->field_910;
    rec   = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, actor->field_438 + 1);
    coord = actor->field_91C->extra.tmd->coords;
    sel   = D_actor_800100_80167218[Mc_SaveData.companionVariant];

    switch (sel) {
        case 3:
            if (rec != NULL) {
                if (rec != actor->field_92C) {
                    actor->field_92C = rec;
                    if ((rec->flags & 0x30) == 0x30) {
                        if (actor->field_95E == 0) {
                            actor->field_95E = 1;
                        }
                    }
                }
            }
            break;
        case 12:
            if (actor->field_95E == 0) {
                actor->field_95E = 1;
                Gp_SpawnEff(0x6006E, coord, 0xC, NULL);
            }
            if (rec != NULL) {
                if (rec != actor->field_92C) {
                    actor->field_92C = rec;
                }
            }
            break;
        default:
            if (actor->field_95E == 0) {
                actor->field_95E = 1;
            } else {
                if (rec != NULL) {
                    if (rec != actor->field_92C) {
                        actor->field_92C = rec;
                    }
                }
            }
            break;
    }

    d4->actionCount = D_actor_800100_80167230[Mc_SaveData.companionVariant];
    if (rec != NULL && func_80105894(arg0, 1, 0, 0) == 0) {
        target            = arg0->work;
        target->field_954 = 0;
        target->field_956 = 4;
        target->field_95C = 0;
        target->field_95E = 0;
        target->field_973 = 0;
        target->field_975 = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 6);
    }
}

void func_actor_800100_80165010(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GpCoord*   coord;
    GpCoord*   target;
    s32        dist;
    u32        rng;
    s32        arg;
    s32        slot;
    s32        diff;
    s32        flag;
    s32        turn;
    s32        state;
    s32        heading;
    u16        angle;

    coord  = arg0->extra.tmd->coords;
    target = ((Task*)gameGetPtrSlot(3))->extra.tmd->coords;
    actor  = arg0->work;
    d4     = actor->field_910;
    dist   = func_actor_800100_8016709C(coord, &d4->contact, NULL);
    state  = actor->field_95E;
    flag   = 1;

    switch (state) {
        case 0:
            if (d4->scanAngle < 0x1000) {
                if (d4->scanDist != 0 && (d4->scanDist < dist || dist == 0)) {
                    d4->scanDist      = dist;
                    d4->targetHeading = d4->scanAngle;
                }
                d4->scanAngle += 0x80;
            } else {
                actor->field_95E  = flag;
                angle             = (d4->targetHeading + actor->field_52) & 0xFFF;
                d4->targetHeading = angle;
                turn              = func_80103E7C(actor->field_52, angle);
                arg               = 5;
                if (turn > 0) {
                    arg         = 6;
                    d4->turnDir = flag;
                } else {
                    d4->turnDir = -1;
                }
                Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 3);
            }
            break;
        case 1:
            actor->field_975 = (u8)d4->turnDir;
            do {
                heading = actor->field_52;
                state   = d4->targetHeading;
                diff    = heading - state;
                if (diff < 0) {
                    diff = -diff;
                }
            } while (0);
            if (diff < 0x40) {
                actor->field_95E++;
                actor->field_52  = d4->targetHeading;
                actor->field_975 = 0;
                if (Gp_StateF0.field_0 == 1) {
                    slot             = 4;
                    actor->field_958 = 3;
                    rng              = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState      = rng;
                    actor->field_934 = ((rng >> 16) & 0x3F) + 0x14;
                    Gp_AnimPlayChildSlotsEx(arg0, slot, 0, 3);
                } else {
                    slot             = 2;
                    actor->field_958 = 1;
                    rng              = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState      = rng;
                    actor->field_934 = ((rng >> 16) & 0x7F) + 0x28;
                    Gp_AnimPlayChildSlotsEx(arg0, slot, 0, 3);
                }
            }
            break;
        case 2:
            if (dist >= 0x301 || dist == 0) {
                if (--actor->field_934 > 0) {
                    goto setFlag;
                }
            }
            Gp_ResetActorMove(arg0, 0);
            break;
        setFlag:
            actor->field_973 = 1;
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
}

void func_actor_800100_801652B0(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GameActor* target;
    GpActorD4* targetD4;
    s32        flag;
    s32        arg;
    s32        val;
    s32        dist;
    s32        targetDist;
    s32        turn;
    s32        state;
    s16        ang;
    s16        anim;
    u16        old;

    actor      = arg0->work;
    d4         = actor->field_910;
    targetDist = func_actor_800100_8016709C(arg0->extra.tmd->coords, &d4->contact, NULL);
    state      = actor->field_95E;
    flag       = 1;
    switch (state) {
        case 0:
            actor->field_95E  = flag;
            ang               = (actor->field_52 + (rand() & 0xFFF)) & 0xFFF;
            d4->targetHeading = ang;
            turn              = func_80103E7C(actor->field_52, ang);
            arg               = 5;
            if (turn << 16 > 0) {
                arg         = 6;
                d4->turnDir = flag;
            } else {
                d4->turnDir = -1;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 3);
        case 1:
            val              = (u8)d4->turnDir;
            actor->field_975 = val;
            val              = actor->field_52;
            state            = d4->targetHeading;
            dist             = val - state;
            if (dist < 0) {
                dist = -dist;
            }
            if (dist < 0x40) {
                actor->field_95E += 1;
                actor->field_52   = (u16)d4->targetHeading;
                actor->field_975  = 0;
                actor->field_958  = 1;
                val               = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState       = val;
                val               = (((u32)val >> 16) & 0x7F) + 0x1E;
                actor->field_934  = val;
                Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 3);
                break;
            }
            break;
        case 2:
            val              = actor->field_934 - 1;
            actor->field_934 = val;
            if (val != 0) {
                break;
            }
            if (targetDist < 0x401 && targetDist != 0) {
                anim                = 1;
                target              = arg0->work;
                old                 = target->field_956;
                target->field_956   = 0xA;
                target->field_97E   = anim;
                targetD4            = target->field_910;
                target->field_954   = 0;
                target->field_95A   = flag;
                target->field_95C   = 0;
                target->field_95E   = 0;
                target->field_960   = old;
                targetD4->scanDist  = -1;
                targetD4->scanAngle = 0;
                Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 6);
                break;
            }
            actor->field_95E += 1;
            actor->field_934  = (rand() & 0x3F) + 0x3C;
            Gp_AnimPlayChildSlotsEx(arg0, 2, 0, 3);
            break;
        case 3:
            if (targetDist < 0x301 && targetDist != 0) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            val              = actor->field_934 - 1;
            actor->field_934 = val;
            if (val <= 0) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            actor->field_973 = flag;
            break;
    }
}

void func_actor_800100_80165528(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable3 sp;

    sp    = D_actor_800100_80161E4C;
    actor = arg0->work;
    if (actor->field_940 > 0) {
        actor->field_940--;
    }
    if ((s8)actor->field_97A > 0) {
        actor->field_97A--;
    }
    actor->field_973 = 0;
    actor->field_975 = 0;
    sp.funcs[actor->field_954](arg0);
    actor->field_986 = 0;
}

void func_actor_800100_801655C0(Task* arg0)
{
    GameActor* actor;

    actor                         = arg0->work;
    actor->field_956              = 3;
    actor->field_954              = 0;
    actor->field_95C              = 0;
    actor->field_95E              = 0;
    actor->field_910->repeatCount = 0;
    actor->field_97E              = 2;
    actor->field_90C              = Gp_FindLockNode(arg0);
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
}

void func_actor_800100_80165630(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 7;
    actor->field_954 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_93E = 0;
    actor->field_97E = 1;
    actor->field_973 = 0;
    actor->field_975 = 0;
}

void func_actor_800100_80165664(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    u16        temp;

    actor            = arg0->work;
    temp             = actor->field_956;
    actor->field_956 = 0xA;
    actor->field_95A = 1;
    actor->field_97E = 1;
    d4               = actor->field_910;
    actor->field_954 = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_960 = temp;
    d4->scanDist     = -1;
    d4->scanAngle    = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
}

void func_actor_800100_801656C8(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 1;
    actor->field_95A = 1;
    actor->field_954 = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
    actor->field_93E = 0x3C;
}

void func_actor_800100_801656F4(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 2;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 1;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
}

void func_actor_800100_80165720(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 0xB;
    actor->field_954 = 0;
    actor->field_95A = 1;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
}

void func_actor_800100_80165748(Task* arg0)
{
    GameActor* actor;

    if (Gp_StateF0.field_0 == 1) {
        actor                         = arg0->work;
        actor->field_956              = 3;
        actor->field_954              = 0;
        actor->field_95C              = 0;
        actor->field_95E              = 0;
        actor->field_910->repeatCount = 0;
        actor->field_97E              = 2;
        actor->field_90C              = Gp_FindLockNode(arg0);
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 6);
        return;
    }
    func_actor_800100_80163D54(arg0);
}

void func_actor_800100_801657D8(Task* arg0)
{
    if (Gp_StateF0.field_0 != 1) {
        func_actor_800100_80166E14(arg0);
        return;
    }
    func_actor_800100_801659EC(arg0);
}

void func_actor_800100_80165818(Task* arg0)
{
    GameActor* actor;

    actor = arg0->work;
    if (actor->field_95E != 0) {
        Gp_ResetActorMove(arg0, 0);
    }
}

/// Handlers `func_actor_800100_80165850` runs, indexed by `field_96C`.
const TaskFuncTable4 D_actor_800100_80161E88 = { {
    func_actor_800100_801658E8,
    func_actor_800100_801658E8,
    func_actor_800100_801658E8,
    func_actor_800100_80165928,
} };

void func_actor_800100_80165850(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable4 sp;

    sp    = D_actor_800100_80161E88;
    actor = arg0->work;
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    sp.funcs[(u16)actor->field_96C](arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

void func_actor_800100_801658E8(Task* arg0)
{
    GameActor* actor;
    u16        value;

    actor = arg0->work;
    value = actor->field_95E;
    if (value == 0) {
        return;
    }
    if (value == 1) {
        func_8010C180(arg0);
    }
}

void func_actor_800100_80165928(Task* arg0)
{
}

/// Gameplay's player-mode handlers `func_actor_800100_80165930` runs,
/// indexed by `field_956`.
const TaskFuncTable7 D_actor_800100_80161E98 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State2,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State4,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State6,
} };

void func_actor_800100_80165930(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable7 sp;

    sp    = D_actor_800100_80161E98;
    actor = arg0->work;
    sp.funcs[(u16)actor->field_956](arg0);
    Gp_TurnPlayer(arg0);
    if (Mc_SaveData.companionHp <= 0) {
        func_8010BFCC(arg0);
        Gp_StopPlayerAnim(arg0, 0);
    }
}

void func_actor_800100_801659EC(Task* arg0)
{
    GameActor*  actor;
    GpActorD4*  d4;
    GpLinkNode* node;
    GpCoord*    coord;
    VECTOR3*    lock;
    VECTOR*     head;
    s32         entry;
    s32         offset;
    s32         kind;
    s32         angle;
    s32         index;
    s32         inRange;
    s32         mode;

    head                  = SCRATCH_HEAD(VECTOR);
    lock                  = (VECTOR3*)(head - 1);
    SCRATCH_HEAD(VECTOR3) = lock;
    actor                 = arg0->work;
    d4                    = actor->field_910;
    coord                 = arg0->extra.tmd->coords;
    node                  = Gp_FindLockNode(arg0);
    actor->field_90C      = node;
    if (node != NULL) {
        Gp_GetLockPos(node, lock);
        func_80103C74(coord, lock, lock);
        kind = func_80103D8C(*(s32*)lock, lock->vz);
        mode = 2;
        if (kind >= 0x381) {
            if (kind < 0) {
                index  = kind;
                index += 0x3FF;
            } else {
                index = kind;
            }
            angle = index >> 0xA;
            if (angle >= 3) {
                if (angle < 5) {
                    angle = 3;
                }
            }
            inRange = angle < 4;
            if (inRange != 0) {
                entry  = (s32)D_actor_800100_801672F8[angle];
                offset = entry + (func_8010C058() * 0x10);
                mode   = *(u8*)(offset + (rand() & 0xF));
            } else {
                mode = 3;
            }
        } else {
            mode = 2;
        }
    } else {
        if ((s8)actor->field_97E == 2) {
            actor->field_97E = 1;
        }
        mode = D_actor_800100_80167308[rand() & 7];
        if (mode == 3) {
            actor->field_90C = NULL;
        }
    }
    switch (mode) {
        case 0:
            break;
        case 1:
            if ((s8)d4->actionCount <= 0) {
                func_actor_800100_80166E94(arg0, 0);
            } else {
                actor->field_97E = 2;
                actor->field_940 = (rand() & 0x1F) + 0xF;
                d4->repeatCount  = D_actor_800100_80167310[rand() & 7];
                func_actor_800100_80166DD0(arg0);
            }
            break;
        case 2:
            func_actor_800100_80166DF0(arg0);
            func_8010BF7C(arg0, 0x14, 0x3F);
            break;
        case 3:
            func_actor_800100_80165630(arg0);
            break;
        case 4:
            func_actor_800100_80165664(arg0);
            break;
    }
    SCRATCH_POP(VECTOR);
}

void func_actor_800100_80165C38(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GpCoord*   coord;
    GpCoord*   place;
    u16        state;

    place                 = (GpCoord*)((u8*)SCRATCH_HEAD(void) - 0x50);
    SCRATCH_HEAD(GpCoord) = place;

    actor = arg0->work;
    d4    = actor->field_910;
    state = actor->field_960;
    coord = actor->field_91C->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->field_954 = 0;
            actor->field_958 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_960 = 1;
            d4->actionCount -= 1;
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            func_80106238(arg0, 0, 0);
            actor->field_12A |= 0xC800;
            Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x40650001, 1);
            Gp_SpawnEff(0x6002B, coord, 0x21, NULL);
            break;

        case 1:
            actor->field_960  = 2;
            actor->field_12A &= 0x3FFF;
            if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            /* fallthrough */

        case 2:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->field_940 = 0xA;
                d4->repeatCount -= 1;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    SCRATCH_POP_BYTES(0x50);
}

void func_actor_800100_80165DE8(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    u16        state;
    GpCoord*   coord;

    actor = arg0->work;
    d4    = actor->field_910;
    state = actor->field_960;
    coord = actor->field_91C->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->field_954 = 0;
            actor->field_958 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            /* fallthrough */
        case 1:
            actor->field_960 = 2;
            d4->actionCount -= 1;
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            Gp_PlayObjSfx(coord, 0x40660001, 1);
            if (coord != NULL) {
                actor->field_940 = 0x28;
                Gp_SpawnEff(0x6006C, coord, D_actor_800100_80167218[Mc_SaveData.companionVariant] | 0x10000, NULL);
                func_80104490(arg0, 1, 2, 0x110C0A);
                return;
            }
            return;
        case 2:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->field_940 = 0x12;
                d4->repeatCount -= 1;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
}

/// Handlers `func_actor_800100_80166EE8` runs, indexed by `Mc_SaveData.companionVariant`.
const TaskFuncTable5 D_actor_800100_80161EC8 = { {
    func_actor_800100_80165C38,
    func_actor_800100_80165C38,
    func_actor_800100_80165DE8,
    func_actor_800100_80165F50,
    func_actor_800100_80166190,
} };

void func_actor_800100_80165F50(Task* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    GpCoord*   coord;
    GpCoord*   place;
    u16        state;
    void**     scratch;
    GpCoord*   head;

    scratch                           = SCRATCH_HEAD_ADDR;
    head                              = SCRATCH_HEAD_AT(scratch, GpCoord);
    SCRATCH_HEAD_AT(scratch, GpCoord) = head - 1;
    place                             = head - 1;

    actor = arg0->work;
    d4    = actor->field_910;
    state = actor->field_960;
    coord = actor->field_91C->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->field_954  = 0;
            actor->field_958  = 0;
            actor->field_95A  = 2;
            actor->field_95C  = 0;
            d4->repeatCount   = (rand() & 7) + 3;
            actor->field_12A |= 0x800;
            /* fallthrough */

        case 1:
        block_4:
            actor->field_960 = 2;
            actor->field_940 = 0;
            actor->field_934 = 3;
            func_80106238(arg0, 0, 0);
            /* fallthrough */

        case 2:
            actor->field_934 -= 1;
            if (actor->field_934 == 0) {
                actor->field_960 += 1;
                d4->actionCount  -= 1;
                actor->field_12A |= 0xC000;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x40670001, 1);
                Gp_SpawnEff(0x6002B, coord, D_actor_800100_80167218[Mc_SaveData.companionVariant] | 0x10000, NULL);
                Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 2);
            }
            break;

        case 3:
            actor->field_960 += 1;
            actor->field_12A &= 0x3FFF;
            if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            /* fallthrough */

        case 4:
            d4->repeatCount -= 1;
            if ((s8)d4->repeatCount > 0) {
                if ((s8)d4->actionCount > 0) {
                    goto block_4;
                }
            }
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    SCRATCH_POP(GpCoord);
}

void func_actor_800100_80166190(Task* arg0)
{
    void**     scratch;
    GpCoord*   head;
    GameActor* actor;
    GpActorD4* d4;
    GpCoord*   coord;
    GpCoord*   place;
    u16        state;

    scratch                           = SCRATCH_HEAD_ADDR;
    head                              = SCRATCH_HEAD_AT(scratch, GpCoord);
    SCRATCH_HEAD_AT(scratch, GpCoord) = head - 1;
    place                             = head - 1;

    actor = arg0->work;
    d4    = actor->field_910;
    state = actor->field_960;
    coord = actor->field_91C->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->field_954  = 0;
            actor->field_958  = 0;
            actor->field_95C  = 0;
            actor->field_960 += 1;
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, 1);
            break;

        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424,
                              actor->field_438 + 1) != NULL) {
                actor->field_960 += 1;
            }
            break;

        case 2:
            Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
            if (((Gp_LcgState >> 16) & 0xFF) < 0x3F) {
                actor->field_960 = 5;
                actor->field_95A = 2;
                actor->field_940 = 0x28;
                actor->field_979 = 0x1C;
                actor->field_93E = 0x14;
                Gp_PlayObjSfx(coord, 0x40680002, 1);
                if (actor->field_914 != NULL) {
                    actor->field_914->spawnArg1 = 2;
                }
                break;
            }
            actor->field_960 = 3;
            actor->field_95A = 0;
            actor->field_934 = 0;
            actor->field_979 = 9;
            actor->field_93E = 3;
            func_80106238(arg0, 0, 1);
            actor->field_12A |= 0x800;
            /* fallthrough */

        case 3:
            if (actor->field_93E != 0) {
                if (actor->field_934 == 0) {
                    actor->field_93E  = (u16)actor->field_93E - 1;
                    actor->field_934  = 3;
                    actor->field_12A |= 0xC000;
                    d4->actionCount  -= 1;
                    if ((s8)d4->actionCount == 0) {
                        actor->field_93E = 0;
                    }
                    Gp_PlayObjSfx(coord, 0x40680001, 1);
                    Gp_SpawnEff(0x6006B, coord, D_actor_800100_80167218[Mc_SaveData.companionVariant] | 0x10000, NULL);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 2);
                    break;
                } else {
                    actor->field_934 -= 1;
                    if (actor->field_934 != 0) {
                        break;
                    }
                }
                actor->field_12A &= 0x3FFF;
                if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                    Gp_PlayObjSfx(place, 0x17, 1);
                }
                break;
            }
            /* fallthrough */

        case 4:
            actor->field_960  = 6;
            actor->field_12A &= 0x3FFF;
            if (func_actor_800100_80166B40(actor->field_32C, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            break;

        case 5:
            if (actor->field_93E == 0) {
                actor->field_960 = 6;
                if (actor->field_914 != NULL) {
                    actor->field_914->spawnArg1 = 3;
                }
                SndEvt_EnqueueType7(0x40680002, 1);
                Gp_AnimPlayChildSlotsEx(arg0, 0xB, 0, 2);
            } else {
                actor->field_93E = (u16)actor->field_93E - 1;
            }
            break;

        case 6:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->field_940 = 0xF;
                d4->repeatCount  = (actor->field_97F == 1) ? d4->repeatCount - 1 : 0;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    SCRATCH_POP(GpCoord);
}

void func_actor_800100_80166514(Task* arg0)
{
    void**                   scratch;
    Actor800100PlaceScratch* head;
    GameActor*               actor;
    GpCoord                  sp10;
    GpCoord*                 src;
    GpObj*                   obj;
    Actor800100PlaceScratch* blk;
    s16                      angle;

    actor       = arg0->work;
    src         = actor->field_91C->extra.tmd->coords;
    obj         = (GpObj*)actor->field_12C;
    sp10        = *src;
    obj->flags |= 0xC000;

    scratch                                           = SCRATCH_HEAD_ADDR;
    head                                              = SCRATCH_HEAD_AT(scratch, Actor800100PlaceScratch);
    blk                                               = head - 1;
    SCRATCH_HEAD_AT(scratch, Actor800100PlaceScratch) = blk;

    Gp_FindRec18(obj->ctx.d4rec->recs, 0);
    Gfx_RotMatrixX(&sp10.workm, 0x400, 0);
    blk->rot.vx = 0;
    blk->rot.vy = 0x120;
    blk->rot.vz = 0x20;
    Gp_PlaceCoordOffset(&sp10, &blk->coord, &(head - 1)->rot);
    angle      = func_actor_800100_8016709C(&blk->coord, (GpRec18*)actor->pad_3BC, NULL);
    blk->angle = angle;
    func_actor_800100_8016666C(&blk->coord, angle);
    blk->rot.vx = 0;
    blk->rot.vz = 0;
    blk->rot.vy = blk->angle + 0x38;
    Gp_PlaceCoordOffset(&blk->coord, &blk->coord, &(head - 1)->rot);
    func_actor_800100_801668C0(&blk->coord);
    Gp_ClearRec18Occupied((GpRec18*)actor->pad_3BC);
    SCRATCH_POP_AT(scratch, Actor800100PlaceScratch);
}

/* The 0x1C bytes are carved off `head` into `newhead` and stored there, but the
   GTE calls address them through the typed `blk` view: the ROM keeps that typed
   pointer as a copy of `newhead` in `$a3`, and one variable for both drops it.
   The post-`rcos` reads go through `newhead` for the same reason - naming `blk`
   there would keep the copy live across the call - and the two `sxy0` reads are
   spelled off `head`, whose folded address is the one the ROM uses. */
void func_actor_800100_8016666C(GpCoord* arg0, s16 arg1)
{
    void**                  scratch;
    u8*                     head;
    u8*                     newhead;
    Actor800100LineScratch* blk;
    LINE_G2*                prim;
    s16                     angle;
    s32                     sy0;
    s32                     sy1;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    newhead                        = head - sizeof(Actor800100LineScratch);
    blk                            = (Actor800100LineScratch*)newhead;
    SCRATCH_HEAD_AT(scratch, void) = newhead;

    angle = arg1;
    if (arg1 == 0) {
        angle = D_80112F60[Player_Status.weapon];
    }
    blk->tip.vy    = angle;
    blk->origin.vx = 0;
    blk->origin.vy = 0;
    blk->origin.vz = 0;
    blk->tip.vx    = 0;
    blk->tip.vz    = 0;

    gte_SetTransMatrix(&arg0->workm);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(&blk->origin);
    gte_rtps();
    gte_stsxy(&blk->sxy0);
    gte_ldv0(&blk->tip);
    gte_rtps();
    gte_stsxy(&blk->sxy1);
    gte_stszotz(&blk->otz);

    if (((Actor800100LineScratch*)newhead)->otz >= 0x20) {
        prim           = (LINE_G2*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineG2(prim);
        prim->x0 = ((Actor800100LineScratch*)(head - sizeof(Actor800100LineScratch)))->sxy0.vx;
        /* Both `vy` loads sign-extend, which needs the `s32` locals: a direct
           16-bit field copy assembles to `lhu` for either of them. */
        sy0      = ((Actor800100LineScratch*)(head - sizeof(Actor800100LineScratch)))->sxy0.vy;
        prim->y0 = sy0;
        prim->x1 = ((Actor800100LineScratch*)newhead)->sxy1.vx;
        sy1      = ((Actor800100LineScratch*)newhead)->sxy1.vy;
        prim->y1 = sy1;
        /* Both ends pulse with the frame counter, the far one 0x50 darker. */
        prim->r0 = (rcos(gDisplayState.gameTick) & 0x1F) - 0x80;
        prim->g0 = 0x20;
        prim->b0 = 0x20;
        prim->r1 = prim->r0 - 0x50;
        prim->g1 = 0;
        prim->b1 = 0;
        addPrim((u_long*)(((((u32)((Actor800100LineScratch*)newhead)->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, ((Actor800100LineScratch*)newhead)->otz);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(Actor800100LineScratch));
}

/// The four (y, z) corners of the quad `func_actor_800100_801668C0` draws,
/// offset off the placed coordinate's world translation. The table sits in the
/// unit's .rodata right after the jump tables, so it is written here rather
/// than left to the split: nothing else refers to it.
const Actor800100QuadCorner D_actor_800100_80161F10[4] = {
    { -62, 0 },
    { -62, 124 },
    { 62, 0 },
    { 62, 124 },
};

/* The beam quad `func_actor_800100_80166514` places: the four
   `D_actor_800100_80161F10` (y, z) offsets, raised to the coordinate's world
   translation, projected through `GsWSMATRIX` and textured with one 0x20
   square of the atlas.
   The 0x44 bytes are carved off the scratch head and written back in one
   chained assignment: the ROM keeps the allocated pointer as a copy of the
   store's temporary in `$t1`, and splitting the two into separate statements
   drops that copy. */
void func_actor_800100_801668C0(GpCoord* arg0)
{
    void**                  scratch;
    Actor800100QuadScratch* blk;
    POLY_FT4*               prim;
    s32                     i;
    s32                     ay;
    s32                     az;
    s32                     sy;

    scratch = SCRATCH_HEAD_ADDR;
    blk     = (SCRATCH_HEAD_AT(scratch, void) = (Actor800100QuadScratch*)((u8*)SCRATCH_HEAD_AT(scratch, void) - sizeof(Actor800100QuadScratch)));

    for (i = 0; i < 4; i++) {
        ay           = D_actor_800100_80161F10[i].vy;
        az           = D_actor_800100_80161F10[i].vz;
        blk->work.vx = 0;
        blk->work.vy = ay;
        blk->work.vz = az;
        blk->v[i].vx = (u16)blk->work.vx + (u16)arg0->workm.t[0];
        blk->v[i].vy = (u16)blk->work.vy + (u16)arg0->workm.t[1];
        blk->v[i].vz = (u16)blk->work.vz + (u16)arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);

    gte_ldv0(&blk->v[0]);
    gte_rtps();

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);

    gte_stsxy2(&blk->sxy[0]);

    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    prim->tpage = 0x27;
    prim->clut  = 0x3CCE;
    setUV4(prim, 0x20, 0x80, 0x3F, 0x80, 0x20, 0x9F, 0x3F, 0x9F);
    prim->code |= 3;

    gte_stsxy3(&blk->sxy[1], &blk->sxy[2], &blk->sxy[3]);
    gte_stszotz(&blk->otz);

    /* Both `vy` loads sign-extend, which the `s32` locals keep: a direct
       16-bit field copy assembles to `lhu` for either of them. */
    prim->x0 = blk->sxy[0].vx;
    sy       = blk->sxy[0].vy;
    prim->y0 = sy;
    prim->x1 = blk->sxy[1].vx;
    sy       = blk->sxy[1].vy;
    prim->y1 = sy;
    prim->x2 = blk->sxy[2].vx;
    sy       = blk->sxy[2].vy;
    prim->y2 = sy;
    prim->x3 = blk->sxy[3].vx;
    sy       = blk->sxy[3].vy;
    prim->y3 = sy;

    addPrim((u_long*)&gGpuCurrentOt[blk->otz >> 4], prim);
    SCRATCH_POP_BYTES(sizeof(Actor800100QuadScratch));
}

s32 func_actor_800100_80166B40(GpRec18* arg0, GpCoord* arg1, GpCoord* arg2)
{
    s32 minDist;

    minDist = 0x7FFFFFFF;
    if (Gp_CountRec18Hi(arg0, 0x30000) == 0) {
        s32               idx;
        s32*              pidx;
        register void**   scratch asm("v1");
        GpPickScratch*    block;
        register GpRec18* rec asm("s2");
        s32               i;
        s32               bestIdx;
        s32               dist;
        GpRec18*          picked;

        scratch = SCRATCH_HEAD_ADDR;
        i       = 0;
        bestIdx = i;
        pidx    = &idx;
        rec     = arg0;
        {
            register GpPickScratch* p asm("v0");
            p                                       = SCRATCH_HEAD_AT(scratch, GpPickScratch);
            p                                       = p - 1;
            block                                   = p;
            SCRATCH_HEAD_AT(scratch, GpPickScratch) = p;
        }
        do {
            if (rec->key & 0x100000) {
                s32 fy;
                s32 dy;
                {
                    register s32 dx asm("v0");
                    dx   = arg1->workm.t[0] - rec->point.vx;
                    fy   = rec->point.vy;
                    dist = dx;
                    if (dx < 0) {
                        dist = -dist;
                    }
                }
                {
                    register s32 t2 asm("v0");
                    register s32 fz asm("a0");
                    dy = arg1->workm.t[1] - fy;
                    t2 = arg1->workm.t[2];
                    if (dy < 0) {
                        dy = -dy;
                    }
                    fz    = rec->point.vz;
                    dist += dy;
                    t2    = t2 - fz;
                    TOUCH_REG2(t2, dist);
                    if (t2 < 0) {
                        t2 = -t2;
                    }
                    dist += t2;
                }
                if (dist < minDist) {
                    func_800E0FEC(rec, (GpDeltaScratch*)block, 1, pidx);
                    idx = func_800E1ACC((u8*)pidx);
                    {
                        GameSession* session = gGameSession;
                        if (Gp_RoomParamTables[session->at4.loc.stage - 1][session->at4.loc.area - 1][idx]->field_2 != 0) {
                            minDist = dist;
                            bestIdx = i;
                        }
                    }
                }
            }
            i++;
            rec++;
        } while (i < 6);
        if (minDist != 0x7FFFFFFF) {
            i                = 1;
            picked           = (GpRec18*)(bestIdx * 0x18 + (s32)arg0);
            block->sub       = 0;
            block->flg       = i;
            block->t[0]      = picked->point.vx;
            block->t[1]      = picked->point.vy;
            block->t[2]      = picked->point.vz;
            block->offset.vx = rand() & 7;
            block->offset.vy = rand() & 7;
            block->offset.vz = rand() & 7;
            if (arg2 != 0) {
                arg2->workm.t[0] = block->t[0] + block->offset.vx;
                arg2->workm.t[1] = block->t[1] + block->offset.vy;
                arg2->workm.t[2] = block->t[2] + block->offset.vz;
            }
            Gp_SpawnEff(0x6003B, (GpCoord*)&block->flg, 0, &block->offset);
        } else {
            i = 0;
        }
        SCRATCH_POP(GpPickScratch);
        return i;
    }
    return 0;
}

void func_actor_800100_80166DD0(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_954 = 0;
    actor->field_956 = 5;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_960 = 0;
}

void func_actor_800100_80166DF0(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 6;
    actor->field_954 = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_97E = 1;
}

void func_actor_800100_80166E14(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 8;
    actor->field_95C = 7;
    actor->field_954 = 0;
    actor->field_95E = 0;
    actor->field_90C = NULL;
    actor->field_97E = 1;
    func_80106350(arg0, D_actor_800100_80167218[Mc_SaveData.companionVariant], 0);
    Gp_AnimPlayChildSlotsEx(arg0, 8, 1, 6);
}

void func_actor_800100_80166E94(Task* arg0, s32 arg1)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 9;
    actor->field_960 = arg1;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_97E = 1;
    Gp_AnimPlayChildSlotsEx(arg0, arg1 + 0xE, 0, 1);
}

void func_actor_800100_80166EE8(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_800100_80161EC8;
    sp.funcs[Mc_SaveData.companionVariant](arg0);
}

void func_actor_800100_80166F50(Task* arg0)
{
    GameActor*    actor;
    GpObj*        obj;
    GpActorD4Rec* rec;
    GpCoord*      src;
    Task*         task;

    actor = arg0->work;
    task  = actor->field_91C;
    if (task != NULL) {
        obj                         = (GpObj*)actor->field_12C;
        rec                         = (GpActorD4Rec*)actor->pad_164;
        src                         = task->extra.tmd->coords;
        *(GpCoord*)actor->field_3D4 = *src;
        Gfx_RotMatrixX(&((GpCoord*)actor->field_3D4)->workm, 0x400, 0);
        obj->coord      = (GpCoord*)actor->field_3D4;
        obj->ctx.d4rec  = (GpActorD4Rec*)actor->pad_164;
        obj->key        = 0x60000;
        obj->flags      = 3;
        obj->pos.vx     = 0;
        obj->pos.vy     = 0;
        obj->pos.vz     = 0;
        rec->end1.vx    = 0;
        rec->end1.vy    = -0x10;
        rec->end0.vx    = rec->end1.vx;
        rec->end1.vz    = 0x20;
        rec->end0.vy    = rec->end1.vy;
        rec->end0.vz    = rec->end1.vz + D_80112F60[Player_Status.weapon];
        rec->end1Radius = 1;
        rec->end0Radius = 1;
        rec->recs       = (GpRec18*)actor->pad_3BC;
        Gp_LinkObj(1, obj);
        Gp_InitRec18Table(rec->recs, 1, 0);
        obj->flags |= 0x800;
    }
}

s32 func_actor_800100_8016709C(GpCoord* arg0, GpRec18* arg1, GpRec18* arg2)
{
    s32 ret;

    if (arg1->key != 0) {
        ret = func_80103D8C(arg0->workm.t[0] - arg1->point.vx, arg0->workm.t[2] - arg1->point.vz);
        if (arg2 != NULL) {
            arg2->flags = arg1->point.vx;
            arg2->depth = arg1->point.vy;
            arg2->flags = arg1->point.vz;
        }
    } else {
        ret = 0;
    }
    return ret;
}
