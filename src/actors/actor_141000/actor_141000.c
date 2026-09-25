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
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

typedef struct Actor141000Point {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ byte pad_4[4];
} Actor141000Point;

typedef struct Actor141000Proj {
    /* 0x0 */ DVECTOR sxy;
    /* 0x4 */ s32     z;
} Actor141000Proj;
STATIC_ASSERT_SIZEOF(Actor141000Proj, 0x8);

/// Work block allocated by `func_actor_141000_8013392C` (`memCalloc(0x4CC)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_141000_801339DC` republishes the two matrices
/// onto `TmdObject::lightMtx` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C9 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared. `target` is the world position the
/// actor turns to face, written by the 0x7DD placement handler.
typedef struct Actor141000Work {
    /* 0x000 */ GpAnimCtx  anim;         // animation context the slot helpers are handed
    /* 0x014 */ GpAnimSlot slots[0x13];  // the slot array `func_800B3F84` is handed
    /* 0x30C */ byte       poses[0x130]; // pose buffer `func_800B3F84` is handed
    /* 0x43C */ s8         field_43C;    // non-zero while the animation slots tick
    /* 0x43D */ s8         field_43D;
    /* 0x43E */ s8         field_43E;
    /* 0x43F */ s8         field_43F;
    /* 0x440 */ MATRIX     light;
    /* 0x460 */ MATRIX     color;
    /* 0x480 */ VECTOR3    target;
    /* 0x48C */ byte       pad_48C[0x4];
    /* 0x490 */ VECTOR3    step; // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x49C */ byte       pad_49C[0x4];
    /* 0x4A0 */ s32        field_4A0;
    /* 0x4A4 */ s32        field_4A4;
    /* 0x4A8 */ s32        field_4A8;
    /* 0x4AC */ byte       pad_4AC[0x4];
    /* 0x4B0 */ SVECTOR    limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x4B8 */ u16        field_4B8;
    /* 0x4BA */ u16        field_4BA; // target yaw the turn-to-face body steers toward
    /* 0x4BC */ u16        field_4BC;
    /* 0x4BE */ byte       pad_4BE[0x2];
    /* 0x4C0 */ u16        field_4C0;
    /* 0x4C2 */ u16        field_4C2; // main-body state index; the dispatcher reads it back sign-extending
    /* 0x4C4 */ u16        field_4C4; // source rect the upload countdown at 0x4C6 reloads from
    /* 0x4C6 */ u16        field_4C6; // upload countdown; `func_actor_141000_801335D4` runs it down and an underflow starts the next upload
    /* 0x4C8 */ s8         field_4C8; // variant the 0x7DB handler latches; 0 picks anim 10, non-zero anim 2
    /* 0x4C9 */ s8         field_4C9;
    /* 0x4CA */ s8         field_4CA; // upload step the switch at 0x801335D4 dispatches on
    /* 0x4CB */ byte       pad_4CB;
} Actor141000Work;
STATIC_ASSERT_SIZEOF(Actor141000Work, 0x4CC);

/// Work block of the overlay's controller task -- the one whose three `Task`
/// states are `D_actor_141000_80131E30`, which spawns the actor and then drives
/// its model through the four animation states at 0x80131E3C.
///
/// `func_actor_141000_80132C7C` allocates it with `memCalloc(0x10, 0)` and
/// parks it in that task's `Task::work` slot, so the size below is the
/// allocation and not a guess; the slot is not a `TaskIdMap` here.
///
/// `field_0` is armed at 0xFFF by the spawn state, `frames` is the counter the
/// state at 0x80132EF4 masks with 7 to pace the actor's spawns, `scale` is the
/// Z scale the state at 0x80132E24 ramps by 0x100 a frame up to 0x1000, `state`
/// is the index `func_actor_141000_80132D3C` dispatches through, and `ticks` is
/// the per-state frame counter the state at 0x80132EB0 holds for 0x1F frames.
typedef struct Actor141000CtrlWork {
    /* 0x0 */ s32  field_0; // armed at 0xFFF by the spawn state
    /* 0x4 */ byte pad_4[0x4];
    /* 0x8 */ u16  frames;
    /* 0xA */ u16  scale;
    /* 0xC */ u16  state;
    /* 0xE */ u16  ticks;
} Actor141000CtrlWork;
STATIC_ASSERT_SIZEOF(Actor141000CtrlWork, 0x10);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// its halfword at 0x2 chooses the variant the handler latches.
typedef struct Actor141000Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor141000Msg;
STATIC_ASSERT_SIZEOF(Actor141000Msg, 0x4);

/// 0x14-byte animation preset `func_actor_141000_80133BD8` builds for
/// `func_actor_141000_80133CD8`, which installs it on the task's model through
/// `Gp_AnimResetSlot` / `Gp_AnimTickIndex` and `func_800B3F84` /
/// `func_800B4114`. The turn-to-face body fills `field_0` with 0, `field_4` with
/// the `field_43F` byte, `field_8` with 1, `field_C` with 5 and `field_10` with
/// 0.
typedef struct Actor141000AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor141000AnimPreset;
STATIC_ASSERT_SIZEOF(Actor141000AnimPreset, 0x14);

/// Payload of the two placement messages: a world position and Euler angles.
/// Message 0x7DD (`func_actor_141000_801336DC`) copies it into the work block,
/// the position into `Actor141000Work::target` and the rotation into
/// `field_4B8..field_4BC` (the yaw being the turn-to-face target); message
/// 0x7D4 (`func_actor_141000_80133E10`) applies it to the root coordinate at
/// once.
typedef struct Actor141000Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor141000Placement;
STATIC_ASSERT_SIZEOF(Actor141000Placement, 0x18);

/// `GsCOORDINATE2` at `TmdObject::coords` as the placement code uses it: the
/// libgs `param` slot at 0x44 holds the Euler angles written there and then
/// handed straight to `RotMatrix`.
typedef struct Actor141000Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor141000Coord;
STATIC_ASSERT_SIZEOF(Actor141000Coord, 0x4C);

/// Optional start animation for the 0x7DD placement handler: the preset's
/// `field_4` and the `field_43F` byte. Absent, the defaults are anim 10 (or 2
/// once `field_4C8` is latched) and 1.
typedef struct Actor141000SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor141000SpawnAnim;

/// A `MATRIX`'s word-wise view, for the identity splat
/// `func_actor_141000_80132FD0` writes over the root coordinate: five aligned
/// stores rather than nine halfword ones.
typedef struct Actor141000MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor141000MatWords;
STATIC_ASSERT_SIZEOF(Actor141000MatWords, 0x14);

extern s32 D_80070F70;

/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;

/// The rotation table `func_actor_141000_80132FD0` feeds to `RotMatrix`: 0x5A
/// `SVECTOR` axis triples, one per frame of the ramp the controller's state 2
/// climbs, ending at the entry index 0x59 the function clamps to.
extern SVECTOR D_actor_141000_80134228[];

/// The world positions matching `D_actor_141000_80134228`, same 0x5A entries
/// and same index; the function copies the chosen triple into the root
/// coordinate's translation and then drops X by 40.
extern SVECTOR D_actor_141000_801344F8[];

/// Quad index table: sixteen quads, four point/colour indices each.
extern s8 D_actor_141000_801347C8[][4];

/// Base vertex colours, scaled by the controller's `field_0` each frame.
extern u8 D_actor_141000_80134808[][4];

extern SVECTOR D_actor_141000_80134868[2];
extern SVECTOR D_actor_141000_80134878[];
extern SVECTOR D_actor_141000_801348A8[];

/// The descriptor table the controller spawns from: index 1 is the task its
/// spawn state starts and index 2 the model actor `func_actor_141000_80132EF4`
/// spawns later, every eighth frame.
extern TaskDesc D_actor_141000_801348D8[];

/// The texture uploads `func_actor_141000_801335D4` walks, one per value of
/// `Actor141000Work::field_4CA`, and `func_actor_141000_80133FA8` picks from:
/// each a `GpImgRec` whose own `rect` carries the upload size and whose `data`
/// points at the pixel blob.
extern GpImgRec D_actor_141000_8013CA7C;
extern GpImgRec D_actor_141000_8013CE84;
extern GpImgRec D_actor_141000_8013D28C;
extern GpImgRec D_actor_141000_8013D4DC;
extern GpImgRec D_actor_141000_8013D72C;

/// Animation bank table `func_800B3F84` re-seeds the slots from, indexed by
/// the preset's bank index.
extern void* D_actor_141000_8013D778[];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_141000_8013392C`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_141000_8013D788[];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_141000_80132C7C(Task* task);
void func_actor_141000_80132D3C(Task* task);
void func_actor_141000_80132E04(Task* task);
void func_actor_141000_80132E24(Task* arg0);
void func_actor_141000_80132EB0(Task* arg0);
void func_actor_141000_80132EF4(Task* arg0);
void func_actor_141000_80132FC8(Task* arg0);
s32  func_actor_141000_80132FD0(GsCOORDINATE2* arg0, s32 arg1);
void func_actor_141000_8013308C(GsCOORDINATE2* arg0, s32 arg1);
void func_actor_141000_80133204(Task* task);
void func_actor_141000_80133260(Task* arg0);
void func_actor_141000_801332A0(Task* task);
void func_actor_141000_80133490(Task* arg0);
void func_actor_141000_801335D4(GpActorWork* arg0);
void func_actor_141000_8013392C(Task* arg0);
void func_actor_141000_801339BC(Task* arg0);
void func_actor_141000_801339DC(Task* arg0);
void func_actor_141000_801339F8(Task* arg0);
void func_actor_141000_80133A00(Task* arg0);
void func_actor_141000_80133A68(Task* task);
void func_actor_141000_80133B28(Task* arg0);
void func_actor_141000_80133BD8(Task* arg0);
s32  func_actor_141000_80133CD8(Task* task, s32 arg1, Actor141000AnimPreset* msg, s32 arg3);

/// The model actor's attach states: chain under the spawner, then draw the
/// sixteen quads every frame, then `taskKill`. Dispatched by
/// `func_actor_141000_801331AC`.
const TaskFuncTable3 D_actor_141000_80131E24 = { {
    func_actor_141000_80133204,
    func_actor_141000_80133260,
    taskKill,
} };

/// The controller's three states - spawn, per-frame tick and `taskKill` -
/// dispatched by `func_actor_141000_80132C24`.
const TaskFuncTable3 D_actor_141000_80131E30 = { {
    func_actor_141000_80132C7C,
    func_actor_141000_80132D3C,
    taskKill,
} };

/// The controller's four animation states, dispatched by
/// `func_actor_141000_80132D3C` through the controller work block's `state`
/// halfword.
const TaskFuncTable4 D_actor_141000_80131E3C = { {
    func_actor_141000_80132E24,
    func_actor_141000_80132EB0,
    func_actor_141000_80132EF4,
    func_actor_141000_80132FC8,
} };

/// The model actor's three states - spawn, per-frame tick and exit -
/// dispatched by `func_actor_141000_801338C0`.
const TaskFuncTable3 D_actor_141000_80131E4C = { {
    func_actor_141000_8013392C,
    func_actor_141000_801332A0,
    func_actor_141000_801339BC,
} };

/// The model actor's four main-body states, dispatched by
/// `func_actor_141000_80133A00` through `Actor141000Work::field_4C2`.
const TaskFuncTable4 D_actor_141000_80131E58 = { {
    func_actor_141000_80133A68,
    func_actor_141000_80133B28,
    func_actor_141000_80133490,
    func_actor_141000_80133BD8,
} };

/// The local-space offset the main body's state 1 (`func_actor_141000_80133B28`)
/// rotates into `Actor141000Work::step`: straight ahead along the part's own
/// axis, halved first while `field_4C8` is clear.
const VECTOR D_actor_141000_80131E68 = { 0, 0, 0x300000 };

/// Draws the sixteen gouraud quads, each followed by a semi-transparency
/// tpage change, at `arg2` depth minus 20.
void func_actor_141000_80131E94(Task* arg0, Actor141000Point* arg1, s32 arg2)
{
    CVECTOR   colors[24];
    s8*       quad;
    CVECTOR*  col;
    POLY_G4*  poly;
    DR_TPAGE* tpage;
    s32       i;
    u16       scale;
    s32       a, b, c, d;

    quad  = D_actor_141000_801347C8[0];
    scale = ((Actor141000CtrlWork*)((Task*)arg0->spawnArg2)->work)->field_0;
    if (arg0->killCountdown >= 0x800) {
        arg0->killCountdown = 0;
    }
    rsin(arg0->killCountdown);
    for (i = 0; i < 24; i++) {
        colors[i].r = (D_actor_141000_80134808[i][0] * (s16)scale) >> 12;
        colors[i].g = (D_actor_141000_80134808[i][1] * (s16)scale) >> 12;
        colors[i].b = (D_actor_141000_80134808[i][2] * (s16)scale) >> 12;
    }
    col = colors;
    for (i = 0; i < 6; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = arg1[a].field_0;
        poly->y0   = arg1[a].field_2;
        poly->x1   = arg1[b].field_0;
        poly->y1   = arg1[b].field_2;
        poly->x2   = arg1[c].field_0;
        poly->y2   = arg1[c].field_2;
        poly->x3   = arg1[d].field_0;
        poly->y3   = arg1[d].field_2;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) - 20, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) - 20, tpage);
        quad += 4;
    }
    for (i = 6; i < 16; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = arg1[a].field_0;
        poly->y0   = arg1[a].field_2;
        poly->x1   = arg1[b].field_0;
        poly->y1   = arg1[b].field_2;
        poly->x2   = arg1[c].field_0;
        poly->y2   = arg1[c].field_2;
        poly->x3   = arg1[d].field_0;
        poly->y3   = arg1[d].field_2;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) - 20, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) - 20, tpage);
        quad += 4;
    }
}

void func_actor_141000_801323F0(Task* arg0, Actor141000Point* arg1, s32* arg2, s32* arg3)
{
    SVECTOR              a;
    SVECTOR              b;
    MATRIX               rot;
    Actor141000Proj      proj[2];
    Task*                parent;
    MATRIX*              mtx;
    SVECTOR*             src;
    s16                  t;
    s16                  r;
    s32                  scale;
    Actor141000MatWords* words;
    s32                  i;
    u16                  f;
    u16                  x0;
    s32                  y0;
    u16                  x1;
    s32                  y1;
    s32                  dx;
    s32                  dy;

    parent = arg0->spawnArg2;
    mtx    = &((TmdObject*)parent->extra)->coords->coord;
    f      = ((Actor141000CtrlWork*)parent->work)->field_0;
    a.vx   = D_actor_141000_80134868[0].vx;
    a.vy   = D_actor_141000_80134868[0].vy;
    a.vz   = D_actor_141000_80134868[0].vz;
    src    = &D_actor_141000_80134868[1];
    b.vx   = src->vx;
    b.vy   = src->vy;
    b.vz   = src->vz;
    gte_SetRotMatrix(mtx);
    gte_ldv0(&a);
    gte_rtv0();
    gte_stsv(&a);
    gte_ldv0(&b);
    gte_rtv0();
    gte_stsv(&b);
    t     = (double)(s16)f;
    b.vx  = a.vx + (b.vx - a.vx) * t / 4096;
    b.vy  = a.vy + (b.vy - a.vy) * t / 4096;
    b.vz  = a.vz + (b.vz - a.vz) * t / 4096;
    a.vx += mtx->t[0];
    a.vy += mtx->t[1];
    a.vz += mtx->t[2];
    b.vx += mtx->t[0];
    b.vy += mtx->t[1];
    b.vz += mtx->t[2];
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&a);
    gte_rtps();
    gte_stsxy(&proj[0].sxy);
    gte_stdp(&proj[0].z);
    gte_stflg(arg3);
    gte_stszotz(arg2);
    gte_ldv0(&b);
    gte_rtps();
    gte_stsxy(&proj[1].sxy);
    gte_stdp(&proj[0].z);
    gte_stflg(arg3);
    gte_stszotz(&proj[1].z);
    dy                                    = proj[0].sxy.vy - proj[1].sxy.vy;
    dx                                    = proj[1].sxy.vx - proj[0].sxy.vx;
    x1                                    = proj[1].sxy.vx;
    x0                                    = proj[0].sxy.vx;
    y0                                    = proj[0].sxy.vy;
    y1                                    = proj[1].sxy.vy;
    i                                     = ratan2(dx, dy);
    scale                                 = gDisplayState.screenDistance;
    ((Actor141000MatWords*)&rot)->m00_m01 = 0x1000;
    ((Actor141000MatWords*)&rot)->m02_m10 = 0;
    words                                 = (Actor141000MatWords*)&rot;
    words->m11_m12                        = 0x1000;
    ((Actor141000MatWords*)&rot)->m20_m21 = 0;
    words->m22                            = 0x1000;
    RotMatrixZ(i, &rot);
    gte_SetRotMatrix(&rot);
    for (i = 0; i < 6; i++) {
        a.vx = D_actor_141000_80134878[i].vx * scale / *arg2;
        a.vy = D_actor_141000_80134878[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        arg1[i].field_0 = b.vx + x0;
        arg1[i].field_2 = b.vy + y0;
    }
    USE_REG(x0);
    for (i = 0; i < 6; i++) {
        a.vx = D_actor_141000_801348A8[i].vx * scale / proj[1].z;
        a.vy = D_actor_141000_801348A8[i].vy * scale / proj[1].z;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        arg1[i + 6].field_0 = b.vx + x1;
        arg1[i + 6].field_2 = b.vy + y1;
    }
    r = 0x2000 - rsin(arg0->killCountdown);
    for (i = 0; i < 6; i++) {
        a.vx = ((D_actor_141000_80134878[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_actor_141000_80134878[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        arg1[i + 12].field_0 = b.vx + x0;
        arg1[i + 12].field_2 = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = ((D_actor_141000_801348A8[i].vx * r) >> 12) * scale / proj[1].z;
        a.vy = ((D_actor_141000_801348A8[i].vy * r) >> 12) * scale / proj[1].z;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        arg1[i + 18].field_0 = b.vx + x1;
        arg1[i + 18].field_2 = b.vy + y1;
    }
}

void func_actor_141000_80132C24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E30;
    sp.funcs[task->state](task);
}

/// Spawn state of the overlay's controller task: takes the display object's
/// root coordinate, allocates the work block the later states read through
/// `Task::work` and arms it at step 0xFFF, un-parks the model (`field_C` bit
/// 0x80 is the flag that keeps a `TmdObject` out of the coordinate update),
/// republishes that coordinate onto the two scale helpers, spawns the attach
/// task from `D_actor_141000_801348D8` and installs `func_actor_141000_80132E04`
/// as the exit callback before advancing to the per-frame state. A failed allocation kills
/// the task instead of leaving a half-built controller behind.
void func_actor_141000_80132C7C(Task* task)
{
    Actor141000CtrlWork* work;
    TmdObject*           obj;
    GsCOORDINATE2*       coord;

    obj   = task->extra;
    coord = obj->coords;
    work  = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = (TaskIdMap*)work;
    work->field_0 = 0xFFF;
    obj->flags   &= 0xFF7F;
    func_actor_141000_80132FD0(coord, 0);
    func_actor_141000_8013308C(coord, 0);
    Task_SpawnFromTable(&D_actor_141000_801348D8, 1, 0, (s32)task);
    task->exitCallback = func_actor_141000_80132E04;
    task->state       += 1;
}

/// Per-frame state of the overlay's controller task: copies the four animation
/// handlers onto the stack and runs the one the controller work block's `state`
/// halfword selects, sign-extended. A pending effect bit spawns the controller's
/// effect through the model's root coordinate, and the session's teardown flag
/// kills the task instead of letting it tick again.
void func_actor_141000_80132D3C(Task* task)
{
    Actor141000CtrlWork* work;
    TaskFuncTable4       sp;

    work = (Actor141000CtrlWork*)task->work;
    sp   = D_actor_141000_80131E3C;
    sp.funcs[(s16)work->state](task);
    if (D_80070F70 & 1) {
        Gp_SpawnEff(0x60070, (GsCOORDINATE2*)((TmdObject*)task->extra)->coords, 0x24200, NULL);
    }
    if (gGameSession->viewReady != 0) {
        taskKill(task);
    }
}

/// `Task::exitCallback` the controller's spawn state installs: it only hands
/// the task to `taskKill`.
void func_actor_141000_80132E04(Task* task)
{
    taskKill(task);
}

/// State 0 of the handler table at 0x80131E3C: ramps the actor's Z scale by
/// 1/16 a frame and, on reaching 1.0, clamps it there and advances the state
/// index `state` the dispatcher at 0x80132D3C walks.
void func_actor_141000_80132E24(Task* arg0)
{
    Actor141000CtrlWork* work;
    u16                  scale;

    work        = (Actor141000CtrlWork*)arg0->work;
    scale       = work->scale + 0x100;
    work->scale = scale;
    if ((s16)scale >= 0x1000) {
        work->scale = 0x1000;
        work->state = work->state + 1;
    }
    func_actor_141000_80132FD0(((TmdObject*)arg0->extra)->coords, 0);
    func_actor_141000_8013308C(((TmdObject*)arg0->extra)->coords, (s16)work->scale);
}

/// State 1 of the handler table at 0x80131E3C: holds for 0x1F frames, then
/// advances the state index `state` the dispatcher at 0x80132D3C walks.
void func_actor_141000_80132EB0(Task* arg0)
{
    Actor141000CtrlWork* work;
    u16                  ticks;

    work        = (Actor141000CtrlWork*)arg0->work;
    ticks       = work->ticks + 1;
    work->ticks = ticks;
    if ((s16)ticks >= 0x1F) {
        work->state = work->state + 1;
    }
}

/// State 2 of the handler table at 0x80131E3C: drives the model's rotation
/// through `func_actor_141000_80132FD0` and, on the frame that runs the ramp's
/// 0x5A entries out, advances the state index `state` the dispatcher at
/// 0x80132D3C walks. Every eighth frame it spawns another actor from index 2
/// of `D_actor_141000_801348D8` and copies this actor's world position onto
/// the new one.
void func_actor_141000_80132EF4(Task* arg0)
{
    Actor141000CtrlWork* work;
    TmdObject*           obj;
    Task*                spawned;
    GsCOORDINATE2*       src;
    GsCOORDINATE2*       dst;
    u16                  frames;

    work         = (Actor141000CtrlWork*)arg0->work;
    obj          = arg0->extra;
    frames       = work->frames + 1;
    work->frames = frames;

    if (func_actor_141000_80132FD0(obj->coords, (s16)frames) != 0) {
        work->state = work->state + 1;
        return;
    }

    if (!(work->frames & 7)) {
        spawned = Task_SpawnFromTable(D_actor_141000_801348D8, 2, 0, 0);
        if (spawned != NULL) {
            src             = ((TmdObject*)arg0->extra)->coords;
            dst             = ((TmdObject*)spawned->extra)->coords;
            dst->coord.t[0] = src->coord.t[0];
            dst->coord.t[1] = src->coord.t[1];
            dst->coord.t[2] = src->coord.t[2];
        }
    }
}

void func_actor_141000_80132FC8(Task* arg0)
{
}

/// Drives the model root one frame along the ramp the rotation table at
/// 0x80134228 and its position table at 0x801344F8 hold: splat an identity
/// matrix, let `RotMatrix` replace it with the frame's triple -- entry 0x59
/// once `arg1` runs past the table's 0x5A entries -- copy that entry's
/// position into the root's translation, drop X by 40 and clear `flg`.
/// Returns non-zero on the frame that ran past the table, which is what the
/// state-2 handler at 0x80132EF4 advances `state` on.
s32 func_actor_141000_80132FD0(GsCOORDINATE2* arg0, s32 arg1)
{
    Actor141000MatWords* words;
    SVECTOR*             pos;
    s32                  idx;
    s32                  ret;

    if (arg1 < 0x5A) {
        idx = arg1;
        ret = 0;
    } else {
        idx = 0x59;
        ret = 1;
    }
    words          = (Actor141000MatWords*)&arg0->coord;
    words->m00_m01 = 0x1000;
    words->m02_m10 = 0;
    words->m11_m12 = 0x1000;
    words->m20_m21 = 0;
    words->m22     = 0x1000;
    RotMatrix(&D_actor_141000_80134228[idx], &arg0->coord);
    pos               = &D_actor_141000_801344F8[idx];
    arg0->coord.t[0]  = pos->vx;
    arg0->coord.t[1]  = pos->vy;
    arg0->coord.t[2]  = pos->vz;
    arg0->coord.t[0] -= 0x28;
    arg0->flg         = 0;
    return ret;
}

void func_actor_141000_8013308C(GsCOORDINATE2* arg0, s32 arg1)
{
    VECTOR scale;

    scale.vz = arg1;
    scale.vx = 0x1000;
    scale.vy = 0x1000;
    ScaleMatrix(&arg0->coord, &scale);
}

/// Callback of the model actor the controller's state 2 spawns every eighth
/// frame -- index 2 of `D_actor_141000_801348D8`, pointed at the controller's
/// own position. The first frame splats an identity matrix over the task's root
/// coordinate and clears its `flg`: the rotation part only, so the translation
/// the spawner copied in survives. Every frame then runs the 5-frame countdown
/// in `killCountdown`, spawning effect 0x60070 from that same coordinate
/// (spawn arg 0x14200, no offset vector) each time it completes. The countdown
/// is held while the freeze byte is set, and a room change or a script event
/// taking over kills the task outright.
void func_actor_141000_801330C0(Task* arg0)
{
    GsCOORDINATE2*       coord;
    Actor141000MatWords* words;
    u16                  count;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (arg0->state == 0) {
        words          = (Actor141000MatWords*)&coord->coord;
        words->m00_m01 = 0x1000;
        words->m02_m10 = 0;
        words->m11_m12 = 0x1000;
        words->m20_m21 = 0;
        words->m22     = 0x1000;
        coord->flg     = 0;
        arg0->state   += 1;
    }
    if (D_801153F4 == 0) {
        count               = arg0->killCountdown + 1;
        arg0->killCountdown = count;
        if ((s16)count >= 5) {
            arg0->killCountdown = 0;
            Gp_SpawnEff(0x60070, coord, 0x14200, NULL);
        }
    }
    if ((gGameSession->viewReady != 0) || (gGameSession->evtSkipped != 0)) {
        taskKill(arg0);
    }
}

void func_actor_141000_801331AC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E24;
    sp.funcs[task->state](task);
}

/// Chains this actor's root coordinate under the spawner's root coordinate,
/// hands the task to the spawner with `Task_Reparent`, arms `killCountdown` at
/// 0x7FF and advances the state.
void func_actor_141000_80133204(Task* task)
{
    ((TmdObject*)task->extra)->coords->sub = ((TmdObject*)((Task*)task->spawnArg2)->extra)->coords;
    Task_Reparent((Task*)task->spawnArg2, task);
    task->killCountdown = 0x7FF;
    task->state        += 1;
}

void func_actor_141000_80133260(Task* arg0)
{
    Actor141000Point sp10[24];
    s32              spD0;
    s32              spD4;

    func_actor_141000_801323F0(arg0, sp10, &spD0, &spD4);
    func_actor_141000_80131E94(arg0, sp10, spD0);
}

/// Per-frame tick of the model actor: runs the motion handler `field_4C0` selects, steps the 16.16 accumulators
/// by `step` and moves the coordinate by their integer part, ticks the
/// animation slots and ground shadow while visible, runs the texture-upload
/// state, and counts `field_4C9` down to the buffer free.
void func_actor_141000_801332A0(Task* task)
{
    TmdObject*       ext      = task->extra;
    Actor141000Work* work     = (Actor141000Work*)task->work;
    TaskFunc         funcs[2] = { func_actor_141000_801339F8, func_actor_141000_80133A00 };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s32              i;

    funcs[(s16)work->field_4C0](task);
    coord              = ((TmdObject*)task->extra)->coords;
    work->field_4A0   += work->step.vx;
    work->field_4A4   += work->step.vy;
    work->field_4A8   += work->step.vz;
    coord->coord.t[0] += (s16)(work->field_4A0 >> 16);
    coord->coord.t[1] += (s16)(work->field_4A4 >> 16);
    coord->coord.t[2] += (s16)(work->field_4A8 >> 16);
    coord->flg         = 0;
    work->field_4A0    = (u16)work->field_4A0;
    work->field_4A4    = (u16)work->field_4A4;
    work->field_4A8    = (u16)work->field_4A8;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
    }
    func_actor_141000_801335D4((GpActorWork*)task);
    if (work->field_4C9 >= 0) {
        if (work->field_4C9 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C9--;
    }
}

/// State 2 of the main-body table `D_actor_141000_80131E58`: the approach
/// test. Once the X/Z distance from the root coordinate to `target` stops
/// shrinking below `limit`, plays anim 0x7D3 with a preset carrying the
/// `field_43F` byte, clears `step` and advances the state; otherwise records
/// the distance as the new `limit`.
void func_actor_141000_80133490(Task* arg0)
{
    Actor141000Work*      work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor141000AnimPreset preset;

    work  = (Actor141000Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->target.vx;
    }
    d.vx = dx;
    if (work->target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->limit.vx && d.vz >= work->limit.vz) {
        preset.field_0  = 0;
        preset.field_4  = work->field_43F;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_141000_80133CD8(arg0, 0x7D3, &preset, 0);
        work->step.vx = 0;
        work->step.vy = 0;
        work->step.vz = 0;
        work->field_4C2++;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Texture-upload state of the enemy actor: runs the countdown at
/// `Actor141000Work::field_4C6` down one a frame while `field_4CA` names the
/// upload in progress, and on the frame it underflows posts that step's image
/// over the 0x19x0x14 rect at 0x10 -- reloading the countdown from `field_4C4`
/// and advancing `field_4CA` for steps 1 and 2, or clearing `field_4CA` and
/// starting over for step 3. Steps 1 and 2 share their whole tail, which is
/// what makes the compiler emit one copy of it that step 1 jumps into; step 3
/// only differs in clearing the step instead of advancing it.
void func_actor_141000_801335D4(GpActorWork* arg0)
{
    Actor141000Work* work;
    RECT             rect;

    work   = (Actor141000Work*)arg0->actor;
    rect.x = 0;
    rect.y = 0x40;
    rect.w = 0x19;
    rect.h = 0x14;

    switch (work->field_4CA) {
        case 1:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013D28C, &rect);
                work->field_4C6 = work->field_4C4;
                work->field_4CA = work->field_4CA + 1;
            }
            break;
        case 2:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013CE84, &rect);
                work->field_4C6 = work->field_4C4;
                work->field_4CA = work->field_4CA + 1;
            }
            break;
        case 3:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013CA7C, &rect);
                work->field_4CA = 0;
            }
            break;
    }
}

/// Placement handler: stores the spawn position and rotation, resets the body
/// state, then applies a start preset exactly as `func_actor_141000_80133CD8`
/// does (inlined here). The default anim id is chosen by the `field_4C8`
/// variant; writing it as an if/else into the preset (not a ternary) is what
/// keeps CSE from reusing the earlier constant 1 for the `field_43F` store.
s32 func_actor_141000_801336DC(Task* task, s32 arg1, Actor141000Placement* place, Actor141000SpawnAnim* anim)
{
    Actor141000Work*       work;
    Actor141000Work*       w;
    Actor141000AnimPreset  preset;
    Actor141000AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor141000Work*)task->work;
    w->field_4C0   = 1;
    w->field_4C2   = 0;
    w->target.vx   = place->pos.vx;
    w->target.vy   = place->pos.vy;
    w->target.vz   = place->pos.vz;
    w->field_4B8   = place->rot.vx;
    w->field_4BA   = place->rot.vy;
    w->field_4BC   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_43F   = anim->field_4;
    } else {
        if (w->field_4C8 != 0) {
            preset.field_4 = 2;
        } else {
            preset.field_4 = 0xA;
        }
        w->field_43F = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor141000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_141000_8013D778[work->field_43E], ext, work->poses,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
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
    return 0;
}

/// Per-frame dispatcher of the model actor: runs its spawn, tick or exit
/// state from `D_actor_141000_80131E4C`, skipping the frame while the global
/// freeze byte is set.
void func_actor_141000_801338C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E4C;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4CC-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and three cleared words the work's own init expects, republishes the light
/// and colour matrices onto the display object, then installs the message
/// table and the exit callback `func_actor_141000_801339BC`. An allocation failure ends the task
/// instead of leaving a half-built actor behind.
void func_actor_141000_8013392C(Task* arg0)
{
    Actor141000Work* work;

    work = memCalloc(sizeof(Actor141000Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C9 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_141000_801339DC(arg0);

    arg0->msgTable     = D_actor_141000_8013D788;
    arg0->exitCallback = func_actor_141000_801339BC;
    arg0->state++;
}

/// `Task::exitCallback` the model actor's spawn state installs: it only hands
/// the task to `Gp_EnemyTaskExit`.
void func_actor_141000_801339BC(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_141000_801339DC(Task* arg0)
{
    TmdObject*       ext;
    Actor141000Work* work;

    ext           = arg0->extra;
    work          = (Actor141000Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

void func_actor_141000_801339F8(Task* arg0)
{
}

/// Dispatches the actor's four main-body handlers by state.
void func_actor_141000_80133A00(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor141000Work* work;

    work = (Actor141000Work*)arg0->work;
    sp   = D_actor_141000_80131E58;
    sp.funcs[(s16)work->field_4C2](arg0);
}

/// State 0 of the main-body table `D_actor_141000_80131E58`: turns the root
/// part to face `work->target`, the position the placement handler stored.
/// Normalises the offset from the part's own translation, takes its yaw with
/// `ratan2` and rebuilds the local matrix from that yaw alone, then clears
/// `flg` so the world matrix is recomputed and advances the state.
void func_actor_141000_80133A68(Task* task)
{
    Actor141000Work*  work;
    Actor141000Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor141000Work*)task->work;
    coord = (Actor141000Coord*)((TmdObject*)task->extra)->coords;

    delta.vx = work->target.vx - coord->coord.t[0];
    delta.vy = work->target.vy - coord->coord.t[1];
    delta.vz = work->target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    work->field_4C2++;
}

/// State 1 of the actor's main-body table `D_actor_141000_80131E58`, the step
/// after the turn-to-face state. Rotates the constant
/// local-space offset `D_actor_141000_80131E68` through the root part's matrix
/// into `work->step`, halving it first while `field_4C8` is clear -- the
/// variant `func_actor_141000_80133F6C` latches through message 0x7DB -- then
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances the state.
void func_actor_141000_80133B28(Task* arg0)
{
    Actor141000Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor141000Work*)arg0->work;

    vec = D_actor_141000_80131E68;
    if (work->field_4C8 == 0) {
        vec.vx >>= 1;
        vec.vy >>= 1;
        vec.vz >>= 1;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx  = 0x7FFF;
    work->limit.vy  = 0x7FFF;
    work->limit.vz  = 0x7FFF;
    work->field_4C2 = work->field_4C2 + 1;
}

/// State 3 of the main-body table `D_actor_141000_80131E58`, after the approach
/// test: turns the root part to the placement yaw. Euler-extracts the root coordinate into `vec`, and
/// while the yaw gap to the target `work->field_4BA` is at least 0x41 it steps
/// `vec.vy` toward it by 0x40 -- the step is taken on an `s32` widening of the
/// extracted yaw, which the common tail adds to -- and otherwise snaps the yaw
/// to the target and plays anim 0x7D3 with a preset carrying the `field_43F`
/// byte, clearing the two body counters. Either way the root coordinate is
/// rebuilt as the identity matrix rotated by `vec`.
void func_actor_141000_80133BD8(Task* arg0)
{
    Actor141000Work*      work;
    Actor141000MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor141000AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor141000Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4BA - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy          = work->field_4BA;
        preset.field_0  = 0;
        preset.field_4  = work->field_43F;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_141000_80133CD8(arg0, 0x7D3, &preset, 0);
        work->field_4C0 = 0;
        work->field_4C2 = 0;
    }

    words          = (Actor141000MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message-0x7D3 handler, also called directly by the turn-to-face and
/// approach states with a preset of their own. A changed bank index re-seeds
/// the whole animation slot array through `func_800B3F84` from the bank table
/// and forgets the current animation id. A changed animation id is then stored
/// and installed on every slot - through `func_800B4114` when the preset's
/// `field_8` is set and the slots have already been started, through
/// `Gp_AnimResetSlot` otherwise - after which every slot is ticked once and
/// `field_43C` latches. An unchanged id skips all of that. Returns 0.
s32 func_actor_141000_80133CD8(Task* task, s32 arg1, Actor141000AnimPreset* msg, s32 arg3)
{
    Actor141000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor141000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_141000_8013D778[work->field_43E], ext, work->poses, work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
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
    return 0;
}

/// Message-0x7D4 handler: places the model at once. Writes the payload's
/// translation into the root coordinate, keeps its Euler angles in the
/// coordinate's `rot` slot and rebuilds the rotation from them, then clears
/// `flg` so the world matrix is recomputed.
s32 func_actor_141000_80133E10(Task* task, s32 arg1, Actor141000Placement* args)
{
    Actor141000Coord* coord;

    coord             = (Actor141000Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 shows the model and clears the 4 flag, 1 hides it, frees the aux
/// buffers and clears the flag, 2 does both plus latching the mode into the
/// work block's `field_4C9`, and 3 hides it while setting the flag. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
s32 func_actor_141000_80133E8C(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                               |= 0x80;
            ((Actor141000Work*)task->work)->field_4C9 = mode;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_141000_80133F6C(Task* task, s32 arg1, Actor141000Msg* msg)
{
    Actor141000Work* work;

    work = (Actor141000Work*)task->work;
    switch (msg->field_2) {
        case 1:
            work->field_4C8 = 0;
            break;
        case 2:
            work->field_4C8 = 1;
            break;
    }
    return 0;
}

/// Image-load handler: picks one of the overlay's texture uploads by `mode`
/// and posts it through `Gp_LoadActorImage` over a scratch `RECT` -- the
/// 0x19x0x14 rect at (0, 0x40) for modes 0-3, the 0xEx0x14 rect at (0xC, 0x60)
/// for 4 and 5. Mode 3 also arms the work block's upload step and countdown
/// source. Unknown modes load nothing and return 0.
s32 func_actor_141000_80133FA8(Task* task, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret = 0;
    switch (mode) {
        case 0:
            img = &D_actor_141000_8013CA7C;
            goto small;
        case 1:
            img = &D_actor_141000_8013D28C;
            goto small;
        case 2:
            img = &D_actor_141000_8013D4DC;
        small:
            rect.y = 0x40;
            rect.w = 0x19;
            rect.x = 0;
            rect.h = 0x14;
            break;
        case 3:
            img                                       = &D_actor_141000_8013D72C;
            rect.y                                    = 0x40;
            rect.w                                    = 0x19;
            rect.x                                    = 0;
            rect.h                                    = 0x14;
            ((Actor141000Work*)task->work)->field_4CA = 1;
            ((Actor141000Work*)task->work)->field_4C4 = 1;
            break;
        case 4:
            img = &D_actor_141000_8013D72C;
            goto big;
        case 5:
            img = &D_actor_141000_8013D4DC;
        big:
            rect.x = 0xC;
            rect.y = 0x60;
            rect.w = 0xE;
            rect.h = 0x14;
            break;
        default:
            img = NULL;
            break;
    }
    if (img != NULL) {
        ret = Gp_LoadActorImage((GpActorWork*)task, img, &rect);
    }
    return ret;
}
