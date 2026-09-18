#ifndef ACTOR_401300_H
#define ACTOR_401300_H

#include "common.h"

#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/task.h"

#include <psyq/inline_c.h>

/// XZ patrol point in `Actor401300Work::field_C`. Same shape as
/// `Actor01900Waypoint`.
typedef struct Actor401300Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor401300Waypoint;

/// Payload `func_actor_401300_80138B24` sends with message 0x3FF, seeded from
/// `D_actor_401300_80158914` at init; `field_4` is the sub-code.
typedef struct Actor401300Msg3FF {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8[3];
} Actor401300Msg3FF;

/// Private work block of the actor 401300 task, hanging off `Task::work`.
///
/// Only the fields the matched code touches are named so far: `yaw` at 0x18
/// (the heading `func_actor_401300_80141614` reads back from the root
/// coordinate, one halfword later than `ActorsShared80169f74Work::yaw`), the
/// three `GpObj` display nodes `func_actor_401300_80141758` hands back to
/// `Gp_UnlinkObj`, the two child tasks it kills, and the halfword the
/// teardown-ish `func_actor_401300_80141EF8` tests before it stamps the enemy's
/// `field_40` with the -999 sentinel. The block is a good deal larger - sibling
/// `func_actor_401300_80141C88` reads animation state at 0x89C..0xC0E of the
/// same pointer - so the struct stays open-ended.
///
/// The display nodes do *not* sit at the same addresses as the same-shaped
/// teardown of actor 01900/401800, which keeps its three at 0x8C8/0xA08/0xB48.
/// Animation halfwords sit 4 bytes later than `Actor01900Work` (0x89C vs
/// 0x898); the three `GpObj` nodes sit 0xA8 later (0x970/0xAB0/0xBF0).
typedef struct Actor401300Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ s16  field_2;
    /* 0x004 */ s16  field_4;
    /* 0x006 */ s16  field_6;
    /* 0x008 */ s16  field_8;
    /* 0x00A */ byte pad_A[2];
    /// XZ patrol points: the spawn position and one step along its facing.
    /* 0x00C */ Actor401300Waypoint field_C[2];
    /// Lunge step length: `func_actor_401300_8013E930`'s clamped player
    /// distance over 18.
    /* 0x014 */ s16  field_14;
    /* 0x016 */ s16  field_16;
    /* 0x018 */ s16  yaw;
    /* 0x01A */ byte pad_1A[0x44];
    /* 0x05E */ u16  field_5E;
    /* 0x060 */ byte pad_60[0xC];
    /* 0x06C */ u16  field_6C;
    /* 0x06E */ byte pad_6E[0x82A];
    /* 0x898 */ s32  field_898;
    /* 0x89C */ s16  field_89C;
    /* 0x89E */ s16  field_89E;
    /* 0x8A0 */ s16  field_8A0;
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ s16  field_8A6;
    /* 0x8A8 */ s16  field_8A8;
    /* 0x8AA */ s16  field_8AA;
    /* 0x8AC */ s16  field_8AC;
    /* 0x8AE */ s16  field_8AE;
    /* 0x8B0 */ s16  field_8B0;
    /* 0x8B2 */ s16  field_8B2;
    /* 0x8B4 */ s16  field_8B4;
    /* 0x8B6 */ s16  field_8B6;
    /* 0x8B8 */ s16  field_8B8;
    /* 0x8BA */ s16  field_8BA;
    /* 0x8BC */ s32  field_8BC;
    /// Effect anchor `func_actor_401300_80139134` places at the actor's
    /// view-space position before spawning effect 0x600A5.
    /* 0x8C0 */ GsCOORDINATE2 field_8C0;
    /* 0x910 */ GpEffArg      field_910;
    /* 0x918 */ byte          pad_918[8];
    /// Fixed pose `func_actor_401300_80134454` anchors above the root
    /// coordinate (identity rotation, 0x15E up) for the `field_AB0` node.
    /* 0x920 */ GsCOORDINATE2 field_920;
    /* 0x970 */ GpObj         field_970;
    /* 0x990 */ byte          field_990[0x120];
    /* 0xAB0 */ GpObj         field_AB0;
    /* 0xAD0 */ byte          field_AD0[0x120];
    /* 0xBF0 */ GpObj         field_BF0;
    /* 0xC10 */ byte          pad_C10[0x18];
    /// Light matrix `func_actor_401300_80134454` binds to the model's
    /// `TmdObject::lightMtx` (the color matrix is `field_C48`).
    /* 0xC28 */ MATRIX field_C28;
    /// Saved at 0xC48 and copied over 0xC68 when
    /// `func_actor_401300_80139520` enters its state.
    /* 0xC48 */ MATRIX  field_C48;
    /* 0xC68 */ MATRIX  field_C68;
    /* 0xC88 */ s16     field_C88;
    /* 0xC8A */ s16     field_C8A;
    /* 0xC8C */ SVECTOR field_C8C;
    /* 0xC94 */ s16     field_C94;
    /* 0xC96 */ s16     field_C96;
    /* 0xC98 */ s16     field_C98;
    /* 0xC9A */ byte    pad_C9A[2];
    /* 0xC9C */ s16     field_C9C;
    /* 0xC9E */ s16     field_C9E;
    /* 0xCA0 */ u16     field_CA0;
    /* 0xCA2 */ s16     field_CA2;
    /* 0xCA4 */ s16     field_CA4;
    /* 0xCA6 */ byte    pad_CA6[2];
    /// Copy of the first three bytes of the last event
    /// `func_actor_401300_80132554` handled.
    /* 0xCA8 */ u8                field_CA8[3];
    /* 0xCAB */ byte              pad_CAB;
    /* 0xCAC */ Actor401300Msg3FF field_CAC;
    /* 0xCC0 */ s32               field_CC0[3];
    /* 0xCCC */ byte              pad_CCC[4];
    /* 0xCD0 */ s16               field_CD0;
    /* 0xCD2 */ u8                field_CD2;
    /* 0xCD3 */ byte              pad_CD3;
    /// Player position and facing sent with message 0x3E9 by
    /// `func_actor_401300_80138800`.
    /* 0xCD4 */ VECTOR  field_CD4;
    /* 0xCE4 */ SVECTOR field_CE4;
    /// Payload `func_actor_401300_80138160` sends with message 0x3F8.
    /* 0xCEC */ byte field_CEC[0x14];
    /* 0xD00 */ s32  field_D00;
    /// Root position saved by `func_actor_401300_8013F628` on entry.
    /* 0xD04 */ s16  field_D04;
    /* 0xD06 */ s16  field_D06;
    /* 0xD08 */ s16  field_D08;
    /* 0xD0A */ byte pad_D0A[2];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xD0C */ Task* field_D0C;
    /* 0xD10 */ Task* field_D10;
    /* 0xD14 */ byte  pad_D14[8];
    /* 0xD1C */ s16   field_D1C;
    /* 0xD1E */ s16   field_D1E;
    /* 0xD20 */ s16   field_D20;
    /* 0xD22 */ s16   field_D22;
    /* 0xD24 */ byte  pad_D24[4];
    /// Ring of the last seven view-space positions of coordinate 2, written by
    /// `func_actor_401300_801405DC`; `field_D78` is the write cursor.
    /* 0xD28 */ SVECTOR field_D28[7];
    /* 0xD60 */ byte    pad_D60[0x18];
    /* 0xD78 */ s16     field_D78;
    /* 0xD7A */ byte    pad_D7A[2];
} Actor401300Work;
STATIC_ASSERT_SIZEOF(Actor401300Work, 0xD7C);

/// The actor's state handlers, indexed by `Actor401300Work::field_0`.
/// `func_actor_401300_801405DC` copies the table to its frame before
/// dispatching.
typedef struct Actor401300StateTable {
    void (*fn[41])(struct Actor401300*);
} Actor401300StateTable;
STATIC_ASSERT_SIZEOF(Actor401300StateTable, 0xA4);

/// 0x18-byte scratch `func_actor_401300_801405DC` takes from
/// `G_SCRATCH_HEAD`; `pos` receives a model coordinate in view space.
typedef struct Actor401300ViewScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR pos;
} Actor401300ViewScratch;
STATIC_ASSERT_SIZEOF(Actor401300ViewScratch, 0x18);

/// Animation view of the same work block, as `func_actor_401300_80133324`
/// reads it: the `Actor01900AnimWork` layout shifted 4 bytes later, like the
/// rest of this overlay's animation fields.
typedef struct Actor401300AnimWork {
    /* 0x000 */ byte       pad_0[0x20];
    /* 0x020 */ GpAnimCtx  anim;
    /* 0x034 */ GpAnimSlot slots[19];
    /* 0x32C */ byte       pad_32C[0x130];
    /* 0x45C */ GpAnimCtx  blendAnim;
    /* 0x470 */ GpAnimSlot blendSlots[19];
    /* 0x768 */ byte       pad_768[0x138];
    /* 0x8A0 */ s16        field_8A0;
    /* 0x8A2 */ s16        field_8A2;
    /* 0x8A4 */ byte       pad_8A4[2];
    /* 0x8A6 */ s16        field_8A6;
    /* 0x8A8 */ byte       pad_8A8[4];
    /* 0x8AC */ s16        field_8AC;
    /* 0x8AE */ s16        field_8AE;
    /* 0x8B0 */ s16        field_8B0;
} Actor401300AnimWork;

/// Per-task actor context: `field_1C` is the work block (`Task::work`) and
/// `field_2C` is the actor's `TmdObject`. Same shape as `Actor01900`.
typedef struct Actor401300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401300Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ byte             pad_30[6];
    /// High halfword of `Task::spawnArg1`; `func_actor_401300_8014192C` only
    /// stamps the -999 sentinel when it is 2.
    /* 0x36 */ s16 field_36;
} Actor401300;

/// Payload of the message `func_actor_401300_80141494` handles; `field_4`
/// selects the animation id written to `Actor401300Work::field_8A2`. Same
/// shape as `Actor01900Msg7D3`.
typedef struct Actor401300Msg {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u32 field_4;
} Actor401300Msg;

/// Event record `func_actor_401300_80132554` dispatches on: `w[0]` is the
/// event kind (0x301, 0xB05, 0x1D05) and `w[1]` its sub-code, and the first
/// three bytes are also copied raw into `Actor401300Work::field_CA8`.
typedef union Actor401300Event {
    u8  b[3];
    u16 w[2];
} Actor401300Event;

/// Height-clamp row `func_actor_401300_80132BE4` scans: `field_0` / `field_2`
/// are matched against `GpAreaKey::stage` / `area`, and when a row
/// matches the coordinate's Y is clamped to [`lo`, `hi`]. Same shape as
/// `Actor01900HeightClamp`.
typedef struct Actor401300HeightClamp {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  lo;
    /* 0x6 */ s16  hi;
    /* 0x8 */ byte pad_8[8];
} Actor401300HeightClamp;
STATIC_ASSERT_SIZEOF(Actor401300HeightClamp, 0x10);

extern Actor401300HeightClamp D_actor_401300_801589C8[];

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_401300_80132C78`.
/// Same shape as `Actor01900Delta`: the `GpDeltaScratch` filled by
/// `func_800E0C10`, its integer `step` (scaled to length 0xAF when longer),
/// the XZ length `len`, and `moved`, the return value.
typedef struct Actor401300Delta {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ SVECTOR        step;
    /* 0x18 */ s32            len;
    /* 0x1C */ s32            moved;
} Actor401300Delta;
STATIC_ASSERT_SIZEOF(Actor401300Delta, 0x20);

/// Halfword table in the overlay's data; element 0 is the value the 0xB05/0xC
/// event writes into `GpEnemy::field_40`. Declared as an array: a scalar lets
/// the scheduler hoist its load above the preceding store.
extern u16 D_actor_401300_80141FA4[];

/// Per-animation reset argument for `func_800B4114`, indexed by the previous
/// and the new animation id (`field_8A0`, `field_8A2`).
extern s8 D_actor_401300_8015804C[][45];

/// Two rest/target rotation pairs `func_actor_401300_80133834` blends by
/// `0x200 - t` (in 1/512ths) into coord 7 and coord 8.
extern SVECTOR D_actor_401300_801589F8[2];
extern SVECTOR D_actor_401300_80158A08[2];

/// Data `func_actor_401300_80134454` wires up at init: the enemy parameter
/// record (`GpEnemy::field_50`), the three per-variant `field_CA0..CA4`
/// triples selected by `spawnArg1 & 0xF`, the animation bank passed to
/// `Gp_AnimInitCtxSlots`, the 0x3FF message seed, and the task's `field_24`.
extern GpPairSrcE D_actor_401300_80141FA0;
extern SVECTOR    D_actor_401300_80141FB0[3];
extern s32        D_actor_401300_80158838;
/// Handler table `func_actor_401300_80138160` points `field_CAC.field_0` at.
extern s32               D_actor_401300_801588F0;
extern Actor401300Msg3FF D_actor_401300_80158914;
extern s32               D_actor_401300_80158988;

/// Twelve vectors `func_actor_401300_80134BA4` picks from by LCG, grouped by
/// `|arg1|`: 0-4 below 0x200, 5-7 above 0x600, else 8-9 / 10-11 by sign.
/// `pad` is the model coordinate index passed to `func_800FDB18`.
extern SVECTOR D_actor_401300_80158928[12];

/// 0xC-byte `G_SCRATCH_HEAD` block `func_actor_401300_801397F8` borrows for
/// its player-in-radius test: the X/Z offset to the camera target and the
/// radius, each squared in place before `dx + dz < r`. Same shape as
/// `Actor01900RangeScratch`.
typedef struct Actor401300RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor401300RangeScratch;

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_401300_8013AAE8` takes: the
/// offset from the actor to the player, then the clamped turn applied to the
/// root coordinate. Same shape as `Actor01900AimScratch`.
typedef struct Actor401300AimScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     angle;
    /* 0xE */ s16     pad_E;
} Actor401300AimScratch;

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_401300_801376E4` takes: the
/// offset from the actor to the player, the wrapped turn toward the player
/// and the facing yaw. Same shape as `Actor01900ChaseScratch`.
typedef struct Actor401300ChaseScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     turn;
    /* 0xE */ s16     angle;
} Actor401300ChaseScratch;

/// 0x54-byte `G_SCRATCH_HEAD` block `func_actor_401300_80134F90` takes while
/// applying a hit: the model matrix rotated to the hit yaw, the offset to the
/// player, the knockback direction and hit position, the hit record id and the
/// derived damage, distance, yaw, critical flag and effect. Same shape as
/// `Actor01900HitScratch`.
typedef struct Actor401300HitScratch {
    /* 0x00 */ MATRIX  m;
    /* 0x20 */ s32     dx;
    /* 0x24 */ s32     dy;
    /* 0x28 */ s32     dz;
    /* 0x2C */ s32     pad_2C;
    /* 0x30 */ SVECTOR dir;
    /* 0x38 */ SVECTOR hitPos;
    /* 0x40 */ s32     id;
    /* 0x44 */ s32     damage;
    /* 0x48 */ s32     dist;
    /* 0x4C */ s16     yaw;
    /* 0x4E */ s16     crit;
    /* 0x50 */ s16     effect;
    /* 0x52 */ s16     pad_52;
} Actor401300HitScratch;
STATIC_ASSERT_SIZEOF(Actor401300HitScratch, 0x54);
STATIC_ASSERT_SIZEOF(Actor401300AimScratch, 0x10);

/// 0xC-byte `G_SCRATCH_HEAD` block `func_actor_401300_8013A208` takes: the
/// offset from the actor to the player, then the clamped turn. Same shape as
/// `Actor01900TurnScratch`.
typedef struct Actor401300TurnScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad_A;
} Actor401300TurnScratch;
STATIC_ASSERT_SIZEOF(Actor401300TurnScratch, 0xC);

/// 0x34-byte `G_SCRATCH_HEAD` block `func_actor_401300_80132910` takes to push
/// the root coordinate away from kind 0x10000 / 0x30000 records: `pos` is the
/// world translation, `offset` the push (clamped to length 0x140), `i` the
/// record cursor and `hit` the return value. Same shape as `Actor01900PushScratch`.
typedef struct Actor401300PushScratch {
    /* 0x00 */ SVECTOR offset;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ s32     kind;
    /* 0x14 */ s32     len;
    /* 0x18 */ s16     i;
    /* 0x1A */ s16     hit;
    /* 0x1C */ s16     dist[12];
} Actor401300PushScratch;
STATIC_ASSERT_SIZEOF(Actor401300PushScratch, 0x34);

/// 0x34-byte `G_SCRATCH_HEAD` block `Actor401300_RescaleYaw` builds its scaled
/// Y rotation in. Same shape as `Actor01900RotScratch`.
typedef struct Actor401300RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor401300RotScratch;
STATIC_ASSERT_SIZEOF(Actor401300RotScratch, 0x34);

/// 0x24-byte `G_SCRATCH_HEAD` block `func_actor_401300_8013E930` takes: the
/// offset to the player (full width for the distance, halfwords for the yaw),
/// the clamped lunge range and the wrapped turn.
typedef struct Actor401300LungeScratch {
    /* 0x00 */ VECTOR  dist;
    /* 0x10 */ SVECTOR delta;
    /* 0x18 */ s32     range;
    /* 0x1C */ s32     pad_1C;
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     pad_22;
} Actor401300LungeScratch;
STATIC_ASSERT_SIZEOF(Actor401300LungeScratch, 0x24);

/// 0x18-byte `G_SCRATCH_HEAD` block `Actor401300_ScaleMatrix` scales a matrix
/// and its translation in. Same shape as `Actor00100ScaleScratch`.
typedef struct Actor401300ScaleScratch {
    /* 0x00 */ VECTOR  scale;
    /* 0x10 */ SVECTOR trans;
} Actor401300ScaleScratch;
STATIC_ASSERT_SIZEOF(Actor401300ScaleScratch, 0x18);

/// Word-wise view of a `MATRIX` used to splat an identity rotation: five
/// aligned stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries. Same shape as `Actor206100MatrixWords`.
typedef struct Actor401300MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor401300MatWords;

extern MATRIX* D_80073B8C;

/// Movement freeze flag: `Actor401300_MoveForward` skips its step when it is 1.
extern u8  D_80072729;
extern u32 Gp_LcgState;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401300_8013B6E8`.
extern void* D_80114B78[1];

/// Overlay effect model data `func_actor_401300_8013B6E8` points
/// `D_80114B78` at before spawning.
extern char D_actor_401300_80147894;
extern char D_actor_401300_80148808;
extern char D_actor_401300_80148A14;
extern u8   D_801153F2[2];

/// Overlay-data word `func_actor_401300_801397F8` points
/// `D_actor_401300_80158878` at on entering its state.
extern s32  D_actor_401300_80152BB8;
extern s32* D_actor_401300_80158878;

s32 func_actor_401300_80132554(Actor401300* arg0, s32 arg1, Actor401300Event* arg2);

s32 func_actor_401300_80141494(Actor401300* arg0, s32 arg1, Actor401300Msg* arg2);

s32 func_actor_401300_80141614(Task* task, s32 arg1, ActorShared80169f74Placement* placement);

void func_actor_401300_80133A3C(Actor401300* arg0);

void func_actor_401300_8013B6E8(Actor401300* arg0);

void func_actor_401300_80141758(Task* task);

void func_actor_401300_8014192C(Actor401300* arg0);

void func_actor_401300_801419B8(Actor401300* arg0);

void func_actor_401300_80141A60(Actor401300* arg0);

void func_actor_401300_80141EF8(Task* task);

void func_actor_401300_801365F8(Actor401300* arg0);
void func_actor_401300_80136CE8(Actor401300* arg0);
void func_actor_401300_80141B0C(Actor401300* arg0);
void func_actor_401300_80141BC8(Actor401300* arg0);
void func_actor_401300_80141C80(void);
void func_actor_401300_80141C88(Actor401300* arg0);
void func_actor_401300_80141D50(Actor401300* arg0);
void func_actor_401300_80141DF4(Actor401300* arg0);

/// Walks `p` up its parent chain to `Gfx_ViewCoord`, transforming `out` by each
/// coordinate; `out` is left unchanged if the chain ends before the view.
static __inline__ void Actor401300_TransformToView(GsCOORDINATE2* p, SVECTOR* out)
{
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp   = &sv;
    GsCOORDINATE2* view  = &Gfx_ViewCoord;
    VECTOR*        vecp  = &vec;
    s32*           flagp = &flag;
    sv.vx                = out->vx;
    sv.vy                = out->vy;
    sv.vz                = out->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            __asm__ volatile("nop; nop; .word 0x4A480012");
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        out->vx = sv.vx;
        out->vy = sv.vy;
        out->vz = sv.vz;
    }
}

#endif // ACTOR_401300_H
