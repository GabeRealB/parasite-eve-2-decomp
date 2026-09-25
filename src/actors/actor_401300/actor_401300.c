#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include "psyq/abs.h"

#include "actors/actor.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// XZ patrol point in `Actor401300Work::field_C`. Same shape as
/// `Actor01900Waypoint`.
typedef struct Actor401300Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor401300Waypoint;

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
    /// Contact records, walked twelve at a time by the movement helpers.
    /* 0x990 */ GpRec18 field_990[12];
    /* 0xAB0 */ GpObj   field_AB0;
    /// Contact records the `field_AB0` node's context points at.
    /* 0xAD0 */ GpRec18 field_AD0[12];
    /* 0xBF0 */ GpObj   field_BF0;
    /* 0xC10 */ byte    pad_C10[0x18];
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
    /* 0xCA8 */ u8        field_CA8[3];
    /* 0xCAB */ byte      pad_CAB;
    /* 0xCAC */ GpAnimArg field_CAC;
    /* 0xCC0 */ s32       field_CC0[3];
    /* 0xCCC */ byte      pad_CCC[4];
    /* 0xCD0 */ s16       field_CD0;
    /* 0xCD2 */ u8        field_CD2;
    /* 0xCD3 */ byte      pad_CD3;
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
    /* 0xD0C */ Task*   field_D0C;
    /* 0xD10 */ Task*   field_D10;
    /* 0xD14 */ SVECTOR home;
    /* 0xD1C */ s16     field_D1C;
    /* 0xD1E */ s16     field_D1E;
    /* 0xD20 */ s16     field_D20;
    /* 0xD22 */ s16     field_D22;
    /* 0xD24 */ byte    pad_D24[4];
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
    TaskFunc fn[41];
} Actor401300StateTable;
STATIC_ASSERT_SIZEOF(Actor401300StateTable, 0xA4);

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

/// Event record `func_actor_401300_80132554` dispatches on: `w[0]` is the
/// event kind (0x301, 0xB05, 0x1D05) and `w[1]` its sub-code, and the first
/// three bytes are also copied raw into `Actor401300Work::field_CA8`.
typedef union Actor401300Event {
    u8  b[3];
    u16 w[2];
} Actor401300Event;

extern ActorHeightClamp D_actor_401300_801589C8[];

/// Halfword table in the overlay's data; element 0 is the value the 0xB05/0xC
/// event writes into `GpEnemy::hp`. Declared as an array: a scalar lets
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
/// record (`GpEnemy::param`), the three per-variant `field_CA0..CA4`
/// triples selected by `spawnArg1 & 0xF`, the animation bank passed to
/// `Gp_AnimInitCtxSlots`, the 0x3FF message seed, and the task's `field_24`.
extern GpPairSrcE D_actor_401300_80141FA0;
extern SVECTOR    D_actor_401300_80141FB0[3];
extern s32        D_actor_401300_80158838;
/// The animation block the 0x3FF payload in `field_CAC` hands the player.
extern s32       D_actor_401300_801588F0;
extern GpAnimArg D_actor_401300_80158914;
extern s32       D_actor_401300_80158988;

/// Twelve vectors `func_actor_401300_80134BA4` picks from by LCG, grouped by
/// `|arg1|`: 0-4 below 0x200, 5-7 above 0x600, else 8-9 / 10-11 by sign.
/// `pad` is the model coordinate index passed to `func_800FDB18`.
extern SVECTOR D_actor_401300_80158928[12];

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_401300_801376E4` takes: the
/// offset from the actor to the player, the wrapped turn toward the player
/// and the facing yaw.
typedef struct Actor401300ChaseScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     turn;
    /* 0xE */ s16     angle;
} Actor401300ChaseScratch;

/// Offset to the player, distance and turn for the pursuit state.
typedef struct Actor401300PursuitScratch {
    /* 0x00 */ s32     dx;
    /* 0x04 */ s32     dy;
    /* 0x08 */ s32     dz;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ s32     dist;
    /* 0x14 */ SVECTOR delta;
    /* 0x1C */ s32     pad_1C;
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     pad_22;
} Actor401300PursuitScratch;
STATIC_ASSERT_SIZEOF(Actor401300PursuitScratch, 0x24);

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

extern MATRIX* D_80073B8C;

/// Movement freeze flag: `actorMoveForward` skips its step when it is 1.
extern u8 D_80072729;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401300_8013B6E8`.
extern void* D_80114B78[1];

/// The records closing three of the overlay's model streams, which
/// `func_actor_401300_8013B6E8` points `D_80114B78` at before spawning.
extern TmdSource D_actor_401300_80147894;
extern TmdSource D_actor_401300_80148808;
extern TmdSource D_actor_401300_80148A14;

/// Overlay-data word `func_actor_401300_801397F8` points
/// `D_actor_401300_80158878` at on entering its state.
extern s32  D_actor_401300_80152BB8;
extern s32* D_actor_401300_80158878;

void func_actor_401300_80141758(Task* task);
void func_actor_401300_8014192C(Task* arg0);
void func_actor_401300_801419B8(Task* arg0);
void func_actor_401300_80141A60(Task* arg0);
void func_actor_401300_80141B0C(Task* arg0);
void func_actor_401300_80141BC8(Task* arg0);
void func_actor_401300_80141C80(Task* arg0);
void func_actor_401300_80141C88(Task* arg0);
void func_actor_401300_80141D50(Task* arg0);
void func_actor_401300_80141DF4(Task* arg0);
void func_actor_401300_80141EF8(Task* task);

/// Declared locally with a signed `arg2`; see the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

extern SVECTOR D_actor_401300_80158A24;

void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it. The actor turns two joints of its chain with it, by
/// two thirds and one half of the same clamped angle.
void func_actor_401300_801320A4(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

/// Moves `coord` in X and Z by the delta `func_800E0C10` resolves from the
/// first `count` records of `recs`, and keeps the integer part of the full
/// delta in `D_actor_401300_80158A24`. A nonzero fractional part rounds both
/// the coordinate and the kept step one unit away from zero. Returns 1 when the
/// X or Z delta is nonzero.
s32 func_actor_401300_801323B0(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(recs, &s->delta, (s32)count, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_401300_80158A24.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_401300_80158A24.vy = s->delta.vy.w >> 16;
        D_actor_401300_80158A24.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_401300_80158A24.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_401300_80158A24.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_401300_80158A24.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_401300_80158A24.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->moved;
}

s32 func_actor_401300_80132554(Task* arg0, s32 arg1, Actor401300Event* arg2)
{
    Actor401300Work* work  = arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;

    work->field_CA8[0] = arg2->b[0];
    work->field_CA8[1] = arg2->b[1];
    work->field_CA8[2] = arg2->b[2];
    if (arg2->w[0] == 0x301) {
        if (arg2->w[1] == 1) {
            work->field_0 = 0x17;
            return 1;
        }
    } else if (arg2->w[0] == 0xB05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
            case 0xC:
                if ((enemy->placeKey >> 12) == 0) {
                    work->field_0 = 6;
                    enemy->hp     = D_actor_401300_80141FA4[0];
                    ((void (*)(s32))Gp_IncStateF0Ref)(0);
                }
                return 1;
        }
    } else if (arg2->w[0] == 0x1D05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
        }
    }
    return 0;
}

s32 func_actor_401300_8013267C(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = gameGetPtrSlot(3);
    d.vx   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18 - ((GpCoordXZ*)coord)->field_18;
    d.vy   = (u16)((TmdObject*)player->extra)->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20 - ((GpCoordXZ*)coord)->field_20;
    angle  = ratan2(d.vx, d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    x = angle << 16;
    if (arg2 >= 0) {
        if (abs(x >> 16) > 0x400) {
            return 1;
        }
    } else {
        if (abs(x >> 16) < 0x400) {
            return 1;
        }
    }
    Gfx_MatrixCol2(&coord->coord, &v);
    pv = &v;
    VectorNormalSS(pv, pv);
    gte_lddp(arg2);
    gte_ldsv(pv);
    gte_gpf12();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = ((TmdObject*)player->extra)->coords->coord.t[0] - v.vx;
    e.vy  = ((TmdObject*)player->extra)->coords->coord.t[1] - v.vy;
    e.vz  = ((TmdObject*)player->extra)->coords->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= arg1 + 0x96;
}

/// Pushes the root coordinate by a quarter of each kind 0x10000 / 0x30000 record's
/// offset (skipping 0x3000D), walking `recs` until `count` or a zero `key`.
/// The duplicated coordinate update keeps `count`'s sign extension in the loop,
/// as in `Actor01900_Fn03FF8`.
s32 func_actor_401300_80132910(Task* arg0, GpRec18* recs, s16 count)
{
    ActorPushScratch* head;
    ActorPushScratch* s;
    ActorPushScratch* blk;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    ((TmdObject*)arg0->extra)->coords[1].flg = 0;
    head                                     = *(ActorPushScratch**)G_SCRATCH_HEAD;
    blk                                      = head - 1;
    *(ActorPushScratch**)G_SCRATCH_HEAD      = blk;
    s                                        = blk;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
    s->pos.vx = ((TmdObject*)arg0->extra)->coords[1].workm.t[0];
    s->pos.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    s->pos.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if ((s->kind == 0x10000 || s->kind == 0x30000) && recs[s->i].key != 0x3000D) {
            if (s->kind == 0x10000) {
                s->hit = 1;
            }
            Gp_MakeDirOffset(&s->pos, (GpDirSrc*)&recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x140) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x140);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->offset.vx >> 2;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->offset.vz >> 2;
            } else {
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->offset.vx >> 2;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->offset.vz >> 2;
            }
            ((TmdObject*)arg0->extra)->coords->flg = 0;
        }
    }
    *(ActorPushScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

void func_actor_401300_80132BE4(GpAreaKey* session, GsCOORDINATE2* coord)
{
    ActorHeightClamp* row;
    s32               offset;
    s32               lo;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            return;
        }
    }
}

static __inline__ s32 Actor401300_HasHeightClamp(GpAreaKey* session)
{
    ActorHeightClamp* row;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

s32 func_actor_401300_80132C78(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2, s16 arg3)
{
    ActorStepDelta* head;
    ActorStepDelta* s;
    ActorStepDelta* blk;
    s16             vy;
    SVECTOR*        step;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    head                              = *(ActorStepDelta**)G_SCRATCH_HEAD;
    blk                               = head - 1;
    *(ActorStepDelta**)G_SCRATCH_HEAD = blk;
    s                                 = blk;
    s->moved                          = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.w >> 16;
        s->step.vy = s->delta.vy.w >> 16;
        s->step.vz = s->delta.vz.w >> 16;
        if (Actor401300_HasHeightClamp(&gGameSession->at4.loc.view)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x15E) {
                s->step.vy = (vy <= 0) ? -0x15E : 0x15E;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0xAF) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xAF);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.w & 0xFFFF) {
            if (s->delta.vx.w > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.w & 0xFFFF) {
            if (s->delta.vz.w > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor401300_HasHeightClamp(&gGameSession->at4.loc.view)) {
        func_actor_401300_80132BE4(&gGameSession->at4.loc.view, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(ActorStepDelta**)G_SCRATCH_HEAD += 1;
    return s->moved;
}

/// Rotates the player's and this actor's root positions, each raised by 1000,
/// into world space and returns `func_800E0308` on the pair.
s32 func_actor_401300_80132FF4(Task* arg0)
{
    Task*              player;
    u8*                head;
    ActorSightScratch* s;
    SVECTOR*           local;
    SVECTOR*           v;
    SVECTOR*           out;

    player                = gameGetPtrSlot(3);
    head                  = *(u8**)G_SCRATCH_HEAD;
    local                 = (SVECTOR*)(head - 0xC);
    s                     = (ActorSightScratch*)(head - 0x1C);
    s->local.vx           = ((TmdObject*)player->extra)->coords->coord.t[0];
    s->local.vy           = ((TmdObject*)player->extra)->coords->coord.t[1] - 1000;
    *(u8**)G_SCRATCH_HEAD = (u8*)s;
    s->local.vz           = ((TmdObject*)player->extra)->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    v = local;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(&s->out);
    s->out.vx += gGfxViewCoord.workm.t[0];
    s->out.vy += gGfxViewCoord.workm.t[1];
    s->out.vz += gGfxViewCoord.workm.t[2];

    s->local.vx = ((TmdObject*)arg0->extra)->coords->coord.t[0];
    s->local.vy = ((TmdObject*)arg0->extra)->coords->coord.t[1] - 1000;
    s->local.vz = ((TmdObject*)arg0->extra)->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    out = (SVECTOR*)(head - 0x14);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
    s->from.vx           += gGfxViewCoord.workm.t[0];
    s->from.vy           += gGfxViewCoord.workm.t[1];
    s->from.vz           += gGfxViewCoord.workm.t[2];
    s->hit                = func_800E0308(&s->out, out);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
    return s->hit;
}

void func_actor_401300_80133254(Task* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->work;
    TOUCH_REG(work);

    if (work->field_8A0 != work->field_8A2) {
        for (i = 1; i < 0x13; i++) {
            work->slots[i].rate = work->field_8A6;
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            func_800B4114(&work->anim, i, work->field_8A2, 0,
                          D_actor_401300_8015804C[work->field_8A0][work->field_8A2]);
        }
        work->field_8A0 = work->field_8A2;
    }
}

void func_actor_401300_80133324(Task* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    s16                  weight;
    s16                  i;
    Actor401300AnimWork* work;

    work   = (Actor401300AnimWork*)arg0->work;
    weight = work->field_8B0;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = (u8)work->field_8AE;
            work->slots[i].rate      = (u8)(work->field_8A6 - 3);
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            do {
                func_800B3448(&work->anim, i, (s32)&pose, 0);
                func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
                Gp_AnimWritePoseCopy(&work->anim, i, &pose, &blendPose, weight, 0x1000 - weight);
            } while (0);
        } else {
            work->slots[i].rate = (u8)(work->field_8A6 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Returns the sound to play when the current animation (`field_8A2`) reaches
/// one of its cue frames, once per frame reached; `field_8BC` holds the last cue
/// frame seen. The frame is re-read at every use: caching it in a local moves
/// CSE's choice of register for the repeat-frame store.
s32 func_actor_401300_8013346C(Actor401300Work* work)
{
    switch ((s16)(work->field_8A2 - 2)) {
        case 1:
            if ((work->field_5E & 0x3FF) == 0xF) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x15) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0003;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 0:
            if ((work->field_5E & 0x3FF) == 0x11) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x1A) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 23:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000C;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0xE) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 24:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000C;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 10:
            if ((work->field_5E & 0x3FF) == 0x7) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 32:
            if ((work->field_5E & 0x3FF) == 0x4) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 9:
            if ((work->field_5E & 0x3FF) == 0x5) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 7:
            if ((work->field_5E & 0x3FF) == 0x7) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0006;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 30:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000A;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 31:
            if ((work->field_5E & 0x3FF) == 0xD) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000B;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 25:
            if ((work->field_5E & 0x3FF) == 0xE) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x14) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0003;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 26:
            if ((work->field_5E & 0x3FF) == 0x12) {
                if (work->field_8BC != 0xE) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 27:
            if ((work->field_5E & 0x3FF) == 0xD) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0xF) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x11) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x14) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 28:
            if ((work->field_5E & 0x3FF) == 0x9) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0012;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
    }
    return 0;
}

void func_actor_401300_80133834(Task* arg0, s16 arg1)
{
    SVECTOR* sc;

    sc     = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    sc->vx = D_actor_401300_801589F8[1].vx +
             ((D_actor_401300_801589F8[0].vx - D_actor_401300_801589F8[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_801589F8[1].vy +
             ((D_actor_401300_801589F8[0].vy - D_actor_401300_801589F8[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_801589F8[1].vz +
             ((D_actor_401300_801589F8[0].vz - D_actor_401300_801589F8[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &((TmdObject*)arg0->extra)->coords[7].coord);
    sc->vx = D_actor_401300_80158A08[1].vx +
             ((D_actor_401300_80158A08[0].vx - D_actor_401300_80158A08[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_80158A08[1].vy +
             ((D_actor_401300_80158A08[0].vy - D_actor_401300_80158A08[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_80158A08[1].vz +
             ((D_actor_401300_80158A08[0].vz - D_actor_401300_80158A08[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &((TmdObject*)arg0->extra)->coords[8].coord);
    ((TmdObject*)arg0->extra)->coords[7].flg = 0;
    *(u32*)G_SCRATCH_HEAD                   += 8;
    ((TmdObject*)arg0->extra)->coords[8].flg = 0;
}

extern s32 D_80115738;
extern s32 D_8011574C;

/// Spawns effect `id` on `coord` at the offset (`x`, `y`, `z`).
static __inline__ void Actor401300_SpawnEff(s32 id, GsCOORDINATE2* coord, s32 flags, s16 x, s16 y, s16 z)
{
    SVECTOR pos;

    pos.vx = x;
    pos.vy = y;
    pos.vz = z;
    Gp_SpawnEff(id, coord, flags, &pos);
}

static __inline__ void Actor401300_SpawnEffZero(s32 id, GsCOORDINATE2* coord, s32 flags)
{
    SVECTOR pos;

    pos.vx = pos.vy = pos.vz = 0;
    Gp_SpawnEff(id, coord, flags, &pos);
}

/// `Actor401300_SpawnEff` for an effect id held in a global. Taking the
/// global's address rather than its value is a matching requirement: the `lui`
/// is then evaluated with the arguments and the load itself after them, which
/// is the order the scheduler needs.
static __inline__ void Actor401300_SpawnEffVar(s32* id, GsCOORDINATE2* coord, s32 flags, s16 x, s16 y, s16 z)
{
    SVECTOR pos;

    pos.vx = x;
    pos.vy = y;
    pos.vz = z;
    Gp_SpawnEff(*id, coord, flags, &pos);
}

static __inline__ void Actor401300_SpawnEffZeroVar(s32* id, GsCOORDINATE2* coord, s32 flags)
{
    SVECTOR pos;

    pos.vx = pos.vy = pos.vz = 0;
    Gp_SpawnEff(*id, coord, flags, &pos);
}

/// 1 when coordinate 1's view-space Z is in [-299, 2300): the body of
/// `func_actor_401300_801417F0`, with `actorTransformToView` written
/// out so `outp` is initialised after `svp`. The `if` that re-tests `ret` keeps
/// jump from folding the result into a bare `sltiu`.
static __inline__ s32 Actor401300_InRange(Task* arg0)
{
    SVECTOR        out;
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp;
    GsCOORDINATE2* view;
    VECTOR*        vecp;
    s32*           flagp;
    SVECTOR*       outp;
    GsCOORDINATE2* p;
    s32            ret;

    memset(&out, 0, 8);
    p     = &((TmdObject*)arg0->extra)->coords[1];
    svp   = &sv;
    outp  = &out;
    view  = &gGfxViewCoord;
    vecp  = &vec;
    flagp = &flag;
    sv.vx = outp->vx;
    sv.vy = outp->vy;
    sv.vz = outp->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        outp->vx = sv.vx;
        outp->vy = sv.vy;
        outp->vz = sv.vz;
    }
    ret = (u16)(out.vz + 0x12B) < 0xA27;
    if (ret != 0) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

static __inline__ void Actor401300_ResetAnim(Task* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->work;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = work->field_8A6;
        if (i < 7) {
            Gp_AnimResetSlotEx(&work->anim, i, work->field_8A2, i, i);
        } else if (i >= 9) {
            Gp_AnimResetSlotEx(&work->anim, i, work->field_8A2, i - 2, i);
        }
    }
    work->field_8A0 = work->field_8A2;
}

static __inline__ void Actor401300_ResetBlendAnim(Task* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work            = (Actor401300AnimWork*)arg0->work;
    work->field_8AE = 0x30;
    work->field_8B0 = 0x800;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = work->field_8AE;
        if (i < 7) {
            Gp_AnimResetSlotEx(&work->blendAnim, i, work->field_8AC, i, i);
        } else if (i >= 9) {
            Gp_AnimResetSlotEx(&work->blendAnim, i, work->field_8AC, i - 2, i);
        }
    }
}

static __inline__ void Actor401300_TickAnim(Task* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->work;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = work->field_8A6;
        if (i < 7) {
            Gp_AnimTickIndex(&work->anim, i);
        } else if (i >= 9) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Per-frame animation and effect update: restarts or ticks the animation
/// slots, eases the yaw of coordinates 5/2 and the blend weight, then spawns
/// the current animation's effects and plays its cue sound.
void func_actor_401300_80133A3C(Task* arg0)
{
    s32              i;
    s32              snd;
    s16              yaw;
    s32              inRange;
    Actor401300Work* work;
    GpEnemy*         enemy;

    /* Set here so CSE keeps `inRange` distinct from the helper's result. */
    inRange = 0;
    work    = arg0->work;
    enemy   = arg0->spawnArg2;
    if (work->field_89C == 1) {
        func_actor_401300_80133254(arg0);
        work->field_89C = 3;
        work->field_8A4 = 0;
        work->field_8BC = 0;
    } else if (work->field_89C == 2) {
        Actor401300_ResetAnim(arg0);
        work->field_89C = 3;
        work->field_8A4 = 0;
        work->field_8BC = 0;
    }
    if (work->field_8AA == 2) {
        Actor401300_ResetBlendAnim(arg0);
        work->field_8AA = 3;
    }
    work->field_8A4++;
    if (work->field_89E == 0) {
        Actor401300_TickAnim(arg0);
    } else {
        func_actor_401300_80133324(arg0);
        if (((Actor401300AnimWork*)work)->blendSlots[1].flags & 0x100) {
            work->field_89E = 0;
        }
    }
    if (work->field_8B2 > work->field_8B4) {
        if (work->field_8B2 - work->field_8B4 > 0x100) {
            work->field_8B4 += 0x100;
        } else {
            work->field_8B4 = work->field_8B2;
        }
    } else if (-(work->field_8B2 - work->field_8B4) > 0x100) {
        work->field_8B4 -= 0x100;
    } else {
        work->field_8B4 = work->field_8B2;
    }
    if (work->field_8B4 != 0) {
        yaw = work->field_8B4;
        if (work->field_8B4 > 0x400) {
            yaw = 0x400;
        }
        if (work->field_8B4 < -0x400) {
            yaw = -0x400;
        }
        func_actor_401300_801320A4(&((TmdObject*)arg0->extra)->coords[5], (yaw * 2) / 3);
        func_actor_401300_801320A4(&((TmdObject*)arg0->extra)->coords[2], yaw / 2);
        ((TmdObject*)arg0->extra)->coords[5].flg = 0;
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        ((TmdObject*)arg0->extra)->coords[3].flg = 0;
        ((TmdObject*)arg0->extra)->coords[2].flg = 0;
    }
    if (work->field_8B8 != work->field_8B6) {
        if (work->field_8B6 < work->field_8B8) {
            work->field_8B8 -= work->field_8BA;
            if (work->field_8B8 < work->field_8B6) {
                work->field_8B8 = work->field_8B6;
            }
        } else {
            work->field_8B8 += work->field_8BA;
            if (work->field_8B6 < work->field_8B8) {
                work->field_8B8 = work->field_8B6;
            }
        }
    }
    func_actor_401300_80133834(arg0, work->field_8B8);
    snd     = func_actor_401300_8013346C(work);
    inRange = Actor401300_InRange(arg0);
    if (inRange == 1) {
        if (work->field_8A2 == 2) {
            if ((u32)gDisplayState.animFrame % 6 == 0) {
                Actor401300_SpawnEffVar(&D_8011574C, &((TmdObject*)arg0->extra)->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if ((u32)gDisplayState.animFrame % 6 == 3) {
                Actor401300_SpawnEffVar(&D_8011574C, &((TmdObject*)arg0->extra)->coords[15], 0x40, 0, 0x1C2, -100);
            }
        } else if (work->field_8A2 == 3) {
            if ((gDisplayState.animFrame & 1) == inRange) {
                Actor401300_SpawnEffVar(&D_80115738, &((TmdObject*)arg0->extra)->coords[18], 0x1202180, 0, 0x1C2, -100);
                Actor401300_SpawnEffVar(&D_8011574C, &((TmdObject*)arg0->extra)->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if (!(gDisplayState.animFrame & 1)) {
                Actor401300_SpawnEffVar(&D_80115738, &((TmdObject*)arg0->extra)->coords[15], 0x1202180, 0, 0x1C2, -100);
                Actor401300_SpawnEffVar(&D_8011574C, &((TmdObject*)arg0->extra)->coords[15], 0x40, 0, 0x1C2, -100);
            }
        } else if (work->field_8A2 == 9 || work->field_8A2 == 25 || work->field_8A2 == 26) {
            if ((u32)gDisplayState.animFrame % 5 == 0) {
                Actor401300_SpawnEffVar(&D_8011574C, &((TmdObject*)arg0->extra)->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if ((u32)gDisplayState.animFrame % 6 == 3) {
                Actor401300_SpawnEffVar(&D_8011574C, &((TmdObject*)arg0->extra)->coords[15], 0x40, 0, 0x1C2, -100);
            }
        }
        if (snd != 0 && (*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x051D0000) {
            switch (snd) {
                case 0x400D0001:
                case 0x400D0003:
                    Actor401300_SpawnEffVar(&D_80115738, &((TmdObject*)arg0->extra)->coords[18], 0x1202180, 0, 0x1C2, -100);
                    snd = 0x551D0006;
                    break;
                case 0x400D0002:
                case 0x400D0004:
                    Actor401300_SpawnEffVar(&D_80115738, &((TmdObject*)arg0->extra)->coords[15], 0x1202180, 0, 0x1C2, -100);
                    snd = 0x551D0007;
                    break;
                case 0x400D0005:
                case 0x400D000B:
                    Actor401300_SpawnEffZeroVar(&D_80115738, &((TmdObject*)arg0->extra)->coords[1], 0x1202180);
                    Actor401300_SpawnEffZeroVar(&D_80115738, &((TmdObject*)arg0->extra)->coords[1], 0x1202180);
                    Actor401300_SpawnEffZeroVar(&D_80115738, &((TmdObject*)arg0->extra)->coords[1], 0x1202180);
                    snd = 0x551D0005;
                    break;
            }
        }
    }
    if (Gp_State1C->roomEffectMode == 2) {
        switch (snd) {
            case 0x400D0001:
            case 0x400D0003:
                Actor401300_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[18], 0x800022C0, 0, 0x15E, -100);
                break;
            case 0x400D0002:
            case 0x400D0004:
                Actor401300_SpawnEff(0x60054, &((TmdObject*)arg0->extra)->coords[15], 0x800022F0, 0, 0x15E, -100);
                break;
            case 0x400D0005:
            case 0x400D000B:
                Actor401300_SpawnEffZero(0x60054, &((TmdObject*)arg0->extra)->coords[1], 0x80004800);
                Actor401300_SpawnEffZero(0x60054, &((TmdObject*)arg0->extra)->coords[1], 0x80004800);
                break;
        }
    }
    if (snd != 0) {
        i = snd | ((enemy->placeKey >> 12) << 8);
        SndEvt_EnqueueType6(i, (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords),
                            (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
}

/// Points the model's light and color matrices at the work block's copies.
static __inline__ void Actor401300_BindMatrices(Task* actor)
{
    Actor401300Work* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra;
    obj->lightMtx = &work->field_C28;
    obj->colorMtx = &work->field_C48;
}

/// Rebuilds the root coordinate's scaled Y rotation and seeds the combat
/// defaults while the rotation scratch block is still held.
static __inline__ void Actor401300_InitPose(GsCOORDINATE2* coord, Actor401300Work* work)
{
    void*                 top;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    top                   = (void*)*(u32*)G_SCRATCH_HEAD;
    blk                   = (ActorScaleRotScratch*)((u8*)top - 0x34);
    *(u32*)G_SCRATCH_HEAD = (u32)blk;
    ang                   = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle            = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 0x1964;
    blk->scale.vy = 0x1964;
    blk->scale.vx = 0x1964;
    ScaleMatrix(&blk->m, &blk->scale);
    coord->coord.m[0][0]   = *(u16*)&((ActorScaleRotScratch*)((u8*)top - 0x34))->m.m[0][0];
    coord->coord.m[0][1]   = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]   = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]   = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]   = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]   = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]   = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]   = *(u16*)&blk->m.m[2][1];
    m22                    = *(u16*)&blk->m.m[2][2];
    coord->flg             = 0;
    coord->coord.m[2][2]   = m22;
    work->field_D78        = 0;
    work->field_C8A        = 0;
    work->field_CAC        = D_actor_401300_80158914;
    work->field_8B8        = 0x100;
    work->field_8B6        = 0x170;
    work->field_8BA        = 0x20;
    *(u32*)G_SCRATCH_HEAD += 0x34;
}

void func_actor_401300_80134454(GpEnemy* enemy, Task* actor)
{
    SVECTOR          dir;
    VECTOR           pos;
    SVECTOR*         v;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor401300Work* work;
    GpObj*           body;
    GpObj*           head;
    OverlayMatWords* mw;

    root        = ((TmdObject*)actor->extra)->coords;
    obj         = actor->extra;
    work        = memCalloc(0xD7C, 0);
    actor->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    if ((actor->spawnArg1 >> 16) != 2) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
    }
    actor->exitCallback = func_actor_401300_80141758;
    Actor401300_BindMatrices(actor);
    enemy->field_4    = &((TmdObject*)actor->extra)->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)actor->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->reactionFlags      = 0;
    enemy->hp                 = (s16)D_actor_401300_80141FA0.hpMax;
    enemy->param              = &D_actor_401300_80141FA0;
    enemy->recs               = work->field_990;
    Gp_AnimInitCtxSlots(&((Actor401300AnimWork*)work)->anim, &D_actor_401300_80158838, obj,
                        ((Actor401300AnimWork*)work)->pad_32C, ((Actor401300AnimWork*)work)->slots);
    Gp_AnimInitCtxSlots(&((Actor401300AnimWork*)work)->blendAnim, &D_actor_401300_80158838, obj,
                        ((Actor401300AnimWork*)work)->pad_768, ((Actor401300AnimWork*)work)->blendSlots);
    work->field_89C = 2;
    work->field_89E = 0;
    work->field_8A2 = 2;
    work->field_8B4 = 0;
    work->field_8B2 = 0;
    work->field_8A8 = 0x10;
    work->field_8A6 = 0x10;
    if ((s16)((enemy->placeKey >> 12) & 1) == 1) {
        work->field_8A8++;
    } else {
        work->field_8A8--;
    }
    func_actor_401300_80133A3C(actor);

    work->field_920.sub        = &gGfxViewCoord;
    mw                         = (OverlayMatWords*)&work->field_920.coord;
    mw->m00_m01                = 0x1000;
    mw->m02_m10                = 0;
    mw->m11_m12                = 0x1000;
    mw->m20_m21                = 0;
    mw->m22                    = 0x1000;
    work->field_920.coord.t[0] = ((TmdObject*)actor->extra)->coords->coord.t[0];
    work->field_920.coord.t[1] = ((TmdObject*)actor->extra)->coords->coord.t[1] - 0x15E;
    work->field_920.coord.t[2] = ((TmdObject*)actor->extra)->coords->coord.t[2];
    work->field_920.flg        = 0;
    Gp_UpdateCoord(&work->field_920);

    work->field_AB0.ctx.recs = work->field_AD0;
    work->field_AB0.coord    = &work->field_920;
    work->field_AB0.pos.vx   = 0;
    work->field_AB0.pos.vy   = 0;
    work->field_AB0.pos.vz   = 0;
    work->field_AB0.key      = 0x3000D;
    work->field_AB0.radius   = 0x15E;
    work->field_AB0.flags    = 1;
    Gp_LinkObj(2, &work->field_AB0);
    work->field_C88        = 0;
    work->field_AB0.flags |= 0x4000;
    Gp_InitRec18Table(work->field_AB0.ctx.recs, 0xC, 0);

    body           = &work->field_970;
    body->ctx.recs = work->field_990;
    body->key      = 0x30000;
    body->coord    = &gGfxViewCoord;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->radius   = 0x280;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);

    dir.vx         = 0;
    dir.vy         = 0;
    dir.vz         = 0;
    head           = &work->field_BF0;
    head->coord    = &((TmdObject*)actor->extra)->coords[3];
    head->ctx.recs = (GpRec18*)&work->pad_C10;
    v              = &dir;
    head->pos.vx   = v->vx;
    head->pos.vy   = v->vy;
    head->pos.vz   = v->vz;
    head->radius   = 0x200;
    head->flags    = 1;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->ctx.recs, 1, 0);

    work->field_16     = 0;
    work->field_C[0].x = ((TmdObject*)actor->extra)->coords->coord.t[0];
    work->field_C[0].z = ((TmdObject*)actor->extra)->coords->coord.t[2];
    Gfx_MatrixCol2(&((TmdObject*)actor->extra)->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = ((TmdObject*)actor->extra)->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = ((TmdObject*)actor->extra)->coords->coord.t[2] + dir.vz;

    actor->msgTable = &D_actor_401300_80158988;
    root->sub       = &gGfxViewCoord;
    root->flg       = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_910.coord      = &((TmdObject*)actor->extra)->coords[1];
    work->field_910.spawnArgLo = 0x300;
    work->field_910.spawnArgHi = 2;
    switch ((u8)(actor->spawnArg1 >> 16)) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            enemy->hp     = -999;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        case 0x20:
            work->field_2 = -1;
            work->field_0 = 0x27;
            enemy->hp     = 0x50;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1 & 0xF) {
        case 2:
            work->field_CA0 = D_actor_401300_80141FB0[0].vx;
            work->field_CA2 = D_actor_401300_80141FB0[0].vy;
            work->field_CA4 = D_actor_401300_80141FB0[0].vz;
            break;
        case 1:
            work->field_CA0 = D_actor_401300_80141FB0[2].vx;
            work->field_CA2 = D_actor_401300_80141FB0[2].vy;
            work->field_CA4 = D_actor_401300_80141FB0[2].vz;
            break;
        case 0:
        default:
            work->field_CA0 = D_actor_401300_80141FB0[1].vx;
            work->field_CA2 = D_actor_401300_80141FB0[1].vy;
            work->field_CA4 = D_actor_401300_80141FB0[1].vz;
            break;
    }

    Actor401300_InitPose(((TmdObject*)actor->extra)->coords, work);
    actor->state++;
}

void func_actor_401300_80134BA4(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor401300Work* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401300_80158928[0];
                break;
            case 1:
                *sc = D_actor_401300_80158928[1];
                break;
            case 2:
                *sc = D_actor_401300_80158928[2];
                break;
            case 3:
                *sc = D_actor_401300_80158928[3];
                break;
            default:
                *sc = D_actor_401300_80158928[4];
                break;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401300_80158928[5];
                break;
            case 1:
                *sc = D_actor_401300_80158928[6];
                break;
            default:
                *sc = D_actor_401300_80158928[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401300_80158928[8];
        } else {
            *sc = D_actor_401300_80158928[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401300_80158928[10];
        } else {
            *sc = D_actor_401300_80158928[11];
        }
    }
    work->field_910.coord      = &((TmdObject*)arg0->extra)->coords[1];
    work->field_910.spawnArgLo = 0x300;
    work->field_910.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[sc->pad], sc, &work->field_910);
    *(u32*)G_SCRATCH_HEAD += 8;
}

void func_actor_401300_80134F90(Task* arg0)
{
    PlayerStatus*    config = &Player_Status;
    Actor401300Work* work;
    GpEnemy*         enemy;
    ActorHitScratch* head;
    ActorHitScratch* s;
    GsCOORDINATE2*   coord;
    Task*            player;
    SVECTOR*         dir;
    s16              z;
    s32              yaw;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              deathSound;
    s32              deathPan;
    s32              hitSound;
    s32              hitPan;
    s32              mag;
    s16              state;
    s16              effect;
    u32              damage;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    if (enemy->hp > 0 && (work->field_0 != 8 || work->field_8A2 != 0x20)) {
        head  = *(ActorHitScratch**)G_SCRATCH_HEAD;
        s     = (*(ActorHitScratch**)G_SCRATCH_HEAD = head - 1);
        s->id = actorFindHit(&head[-1].hitPos, work->field_990);
        if (s->id == 0) {
            s->id = actorFindHit(&s->hitPos, work->field_AD0);
        }
        if (s->id != 0) {
            work->field_D1C        = 0;
            work->field_D1E        = 0;
            work->field_970.radius = 0x280;
            if (s->id & 0x8000) {
                player       = gameGetPtrSlot(3);
                s->hitPos.vx = ((TmdObject*)player->extra)->coords->workm.t[0];
                s->hitPos.vy = ((TmdObject*)player->extra)->coords->workm.t[1];
                s->hitPos.vz = ((TmdObject*)player->extra)->coords->workm.t[2];
            }
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
            s->dir.vx = s->hitPos.vx - ((TmdObject*)arg0->extra)->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - ((TmdObject*)arg0->extra)->coords->workm.t[1];
            z         = s->hitPos.vz - ((TmdObject*)arg0->extra)->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = ((TmdObject*)arg0->extra)->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = actorNormalizeYaw(s->yaw);
            func_actor_401300_80134BA4(arg0, s->yaw, s->id);
            work->field_8B4 = 0;
            work->field_8B2 = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 0x11 && state != 0xF && state != 0x10 &&
                state != 0x27 && state != 4) {
                s->m = ((TmdObject*)arg0->extra)->coords->coord;
                Gfx_RotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_89E == 1) {
                    gte_lddp(-5);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-10);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->dir.vx;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] += s->dir.vy;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->dir.vz;
                ((TmdObject*)arg0->extra)->coords->flg         = 0;
            }
            dx        = config->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag > 0x500) {
                state = work->field_0;
                if (state != 0x13) {
                    if (state != 0x14 && state != 0x11 && state != 0x25 && state != 0x26 && state != 0xF && state != 0x10 && state != 0x27 && state != 4) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->effect = 4;
                        }
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            effect = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[2], effect, NULL);
            }
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(0x51030008, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) && config->hp > 0 && work->field_D20 == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> 0xC) << 8) | 0x400D0008;
                deathPan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            } else {
                hitSound = ((enemy->placeKey >> 0xC) << 8) | 0x400D0007;
                hitPan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            }
            work->field_C88 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 0x27 && state != 4 && state != 0x11) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x400) {
                            work->field_0 = 0x13;
                        } else {
                            work->field_0 = 0x14;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    if (work->field_0 == 4) {
                        work->field_2 = -1;
                    } else if (work->field_0 != 0xF && work->field_0 != 0x10) {
                        if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0x27 || work->field_0 == 4 || work->field_0 == 0x11) {
                            if (work->field_8A2 == 0xB || work->field_8A2 == 0x17 || work->field_8A2 == 8 || work->field_8A2 == 0xA) {
                                work->field_89E = 1;
                                work->field_8AC = 0xB;
                            } else {
                                work->field_89E = 1;
                                work->field_8AC = 0x22;
                            }
                            work->field_8AA = 2;
                        } else if (s->crit == 1) {
                            if (work->field_0 != 4 && work->field_0 != 0x27 && work->field_0 != 0x11) {
                                work->field_0 = 5;
                            }
                        } else {
                            work->field_8AC = 0xD;
                            work->field_89E = 1;
                            work->field_8AA = 2;
                        }
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state == 0x11 || state == 0x27 || state == 4) {
                        work->field_0 = 4;
                        work->field_2 = -1;
                    } else {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x400) {
                            work->field_0 = 0x13;
                        } else {
                            work->field_0 = 0x14;
                        }
                    }
                    break;
                case 3:
                    state = work->field_0;
                    if (state == 0x18 || state == 0x16 || state == 0x17) {
                        work->field_0 = 5;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= 0xFE;
                    state                 = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x27 && state != 4 && state != 0x25 && state != 0x26 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x25;
                        } else if (work->field_0 == 0x10 && work->field_6 < 0xC) {
                            work->field_0 = 0x26;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            if (mag < 0x400) {
                                work->field_0 = 0x13;
                            } else {
                                work->field_0 = 0x14;
                            }
                        }
                    }
                    break;
            }
        }
        if (enemy->reactionFlags & 0xC) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                if (work->field_0 == 7 || work->field_0 == 0x1E || work->field_0 == 0xB || work->field_0 == 0x1B) {
                    work->field_0 = 5;
                } else if (work->field_0 == 4) {
                    work->field_2 = -1;
                } else {
                    if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0xF || work->field_0 == 0x10 || work->field_0 == 0x27 || work->field_0 == 0x11) {
                        if (work->field_8A2 == 0xB || work->field_8A2 == 0x17 || work->field_8A2 == 8 || work->field_8A2 == 0xA) {
                            work->field_89E = 1;
                            work->field_8AC = 0xB;
                        } else if (work->field_8A2 == 0x22 || work->field_8A2 == 0x18 || work->field_8A2 == 0x16 || work->field_8A2 == 0xC) {
                            work->field_89E = 1;
                            work->field_8AC = 0x22;
                        } else {
                            work->field_89E = 1;
                            work->field_8AC = 0xD;
                        }
                    } else {
                        work->field_89E = 1;
                        work->field_8AC = 0xD;
                    }
                    work->field_8AA = 2;
                }
            }
        }
        if (enemy->hp <= 0) {
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4) {
                    state = work->field_8A2;
                    if (state == 2 || state == 3 || state == 0x1B || state == 0x1C || state == 0x1D) {
                        work->field_0 = 0x28;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else {
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 4 && state != 0x27 && state != 0x25 && state != 0x26 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x25;
                            work->field_2 = -1;
                        } else if (work->field_0 == 0x10 && work->field_6 < 0xC) {
                            work->field_0 = 0x26;
                            work->field_2 = -1;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            if (mag < 0x400) {
                                work->field_0 = 0x13;
                            } else {
                                work->field_0 = 0x14;
                            }
                        }
                    }
                }
            } else {
                state = work->field_0;
                if (state != 0x13 && state != 0x14 && state != 4 && state != 0x27 && state != 0x25 && state != 0x26 && state != 0x11) {
                    work->field_0 = 0x13;
                }
            }
            if (enemy->hp <= 0) {
                enemy->hp       = 0;
                work->field_C8A = 1;
            }
        }
        *(ActorHitScratch**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_80135DDC(Task* arg0)
{
    Actor401300Work* work  = arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd                       = arg0->extra;
        enemy->node.state.b.flags = 0;
        tmd->flags                = 0;
        Tmd_AllocBuffers(tmd);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_AB0.flags |= 0x4000;
        if (work->field_8A2 == 11 || work->field_8A2 == 23) {
            work->field_8A2 = 0x17;
        } else if (work->field_8A2 == 12 || work->field_8A2 == 34 || work->field_8A2 == 24) {
            work->field_8A2 = 0x18;
        }
        if ((u16)(work->field_8A2 - 0x17) >= 2) {
            work->field_8A2 = 0x17;
        }
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->field_8A2 == 0x17 && (work->field_5E & 0x3FF) >= 6) &&
                 !(work->field_8A2 == 0x18 && (work->field_5E & 0x3FF) >= 9));
        work->field_8A6 = 0x20;
        return;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    work->field_8A6                        = work->field_8A6 / 2;
    if (work->field_8A6 == 1) {
        work->field_8A6 = -0x10;
    }
    if (work->field_8A6 == -1) {
        work->field_8A6 = 0x10;
    }
    func_actor_401300_80133A3C(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ~2;
        work->field_8A6       = 0x10;
        if ((arg0->spawnArg1 >> 16) == 0x20) {
            work->field_0 = 0x27;
        } else {
            work->field_0 = 0x11;
        }
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
}

void func_actor_401300_80135FC4(Task* arg0)
{
    Actor401300Work* work  = arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    s16              i     = 0;
    u16              r;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd                       = arg0->extra;
        enemy->node.state.b.flags = 0;
        tmd->flags                = 0;
        Tmd_AllocBuffers(tmd);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x17;
        work->field_8B6        = 0x40;
        work->field_8B8        = 0x40;
        work->field_8BA        = 0x20;
        work->field_AB0.flags |= 0x4000;
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->field_6C & 0x100) && ++i < 0xFF);
        work->field_8A6 = 0x20;
        return;
    }
    if (++work->field_6 == 0) {
        work->field_89C = 2;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        r               = (Gp_LcgState >> 16) % 3;
        switch (r) {
            case 0:
                work->field_8A6 = 0x20;
                break;
            case 1:
                work->field_8A6 = 0x30;
                break;
            case 2:
            default:
                work->field_8A6 = 0x40;
                break;
        }
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->field_8A6 = 0x10;
    } else if (work->field_6 > 0) {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        work->field_8A6                        = work->field_8A6 / 2;
        if (work->field_8A6 == 1) {
            work->field_8A6 = -0x10;
        }
        if (work->field_8A6 == -1) {
            work->field_8A6 = 0x10;
        }
        func_actor_401300_80133A3C(arg0);
    } else if (work->field_89E == 1 || !(work->field_6C & 0x100)) {
        work->field_8A6 = 0x10;
        func_actor_401300_80133A3C(arg0);
    }
    if (work->field_6 >= 7) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = -((Gp_LcgState >> 16) & 0xFF);
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x11;
    }
}

void func_actor_401300_80136238(Task* arg0)
{
    Actor401300Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    ActorAimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 9;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        work->field_970.radius = 0x280;
        Gp_ArmStateF0(1);
        work->field_8B6 = 0x200;
        work->field_8BA = 0x20;
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x200) {
            work->field_8BA = 0x80;
            work->field_8B6 = 0x190;
        } else {
            work->field_8B6 = 0x200;
        }
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD    -= 1;
    aim                                    = *(ActorAimScratch**)G_SCRATCH_HEAD;
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_6C & 0x100) {
        if (func_actor_401300_80132FF4(arg0) == 1 && (*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x051D0000) {
            work->field_0 = 8;
        } else {
            work->field_0 = 7;
        }
    }
    aim->angle      = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8B2 = aim->angle;
    if (aim->angle > 0x10) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_801365F8(Task* arg0)
{
    Actor401300Work*           work;
    GameActor*                 player;
    PlayerStatus*              config;
    TmdObject*                 obj;
    GsCOORDINATE2*             coord;
    GsCOORDINATE2*             c1;
    GsCOORDINATE2*             c2;
    Actor401300PursuitScratch* head;
    Actor401300PursuitScratch* sc;
    SVECTOR*                   delta;
    s32                        angle;
    s32                        dist;
    s32                        dx;
    s32                        dy;
    s32                        dz;
    s32                        mask;
    u16                        speed;

    config = &Player_Status;
    work   = arg0->work;
    player = (GameActor*)gameGetPtrSlot(3)->work;
    mask   = 0xF0;
    if (((arg0->spawnArg1 >> 16) & mask) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        SOFT_BARRIER();
        work->field_8A2        = 3;
        speed                  = work->field_8A8;
        work->field_BF0.flags &= 0x7FFF;
        SOFT_BARRIER();
        work->field_89E        = 0;
        work->field_8A6        = speed;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8B6 = 0x40;
        work->field_8BA = 0x10;
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x40) {
            work->field_8B6 = 0x80;
            work->field_8BA = 0x10;
        } else {
            work->field_8B6 = 0x40;
        }
    }
    work->field_6++;
    head                                         = *(Actor401300PursuitScratch**)G_SCRATCH_HEAD;
    delta                                        = &head[-1].delta;
    c1                                           = ((TmdObject*)arg0->extra)->coords;
    head[-1].delta.vx                            = Player_Status.coordMtx->t[0] - c1->coord.t[0];
    delta->vy                                    = Player_Status.coordMtx->t[1] - c1->coord.t[1];
    delta->vz                                    = Player_Status.coordMtx->t[2] - c1->coord.t[2];
    *(Actor401300PursuitScratch**)G_SCRATCH_HEAD = head - 1;
    sc                                           = head - 1;
    ((TmdObject*)arg0->extra)->coords->flg       = 0;
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    c2              = ((TmdObject*)arg0->extra)->coords;
    angle           = ratan2(head[-1].delta.vx, delta->vz);
    sc->angle       = actorNormalizeYaw(angle - ratan2(-c2->coord.m[2][0], c2->coord.m[2][2]));
    work->field_8B2 = sc->angle;
    if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, (work->field_8A8 + 2) * 30 * 1.5f / 18.0f)) {
        actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, (work->field_8A8 + 2) * 30 * 1.5f / 18.0f);
    }
    if (sc->angle > 0x30) {
        sc->angle = 0x30;
    } else if (sc->angle < -0x30) {
        sc->angle = -0x30;
    } else {
        sc->dx = dx = config->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0];
        sc->dy = dy = config->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1];
        sc->dz = dz = config->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2];
        dist        = SquareRoot0(dx * dx + dy * dy + dz * dz);
        sc->dist    = dist;
        if (player->field_954 != 2 && work->field_6 >= 0x28) {
            if (dist > 4000) {
                work->field_0 = 0x21;
            } else if (dist > 2000) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (((Gp_LcgState >> 16) & 0xF) < 5) {
                    work->field_0 = 0x21;
                } else {
                    work->field_0 = 0x22;
                }
            } else if (dist < 1000) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (((Gp_LcgState >> 16) & 0xF) < 7) {
                    work->field_0 = 0x1F;
                } else {
                    work->field_0 = 0x20;
                }
            }
        }
    }
    coord      = ((TmdObject*)arg0->extra)->coords;
    sc->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, sc->angle, 1);

    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
    *(Actor401300PursuitScratch**)G_SCRATCH_HEAD += 1;
}

static __inline__ void Actor401300_MoveBy(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* v;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        v                          = vec;
        if (amount != 0) {
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(v);
            gte_gpf12();
            gte_stsv(v);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

static __inline__ s32 Actor401300_Abs(s32 x)
{
    if (x < 0) {
        x = -x;
    }
    return x;
}

void func_actor_401300_80136CE8(Task* arg0)
{
    Actor401300Work*           work;
    GpEnemy*                   enemy;
    TmdObject*                 obj;
    GsCOORDINATE2*             coord;
    Actor401300PursuitScratch* head;
    Actor401300PursuitScratch* blk;
    Actor401300PursuitScratch* s;
    s32                        z;
    u16                        speed;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                       = arg0->extra;
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        SOFT_BARRIER();
        work->field_8A2        = 3;
        speed                  = work->field_8A8;
        work->field_BF0.flags &= 0x7FFF;
        SOFT_BARRIER();
        work->field_89E        = 0;
        work->field_8A6        = speed;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x40;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8BA = 0x10;
        z               = ((TmdObject*)arg0->extra)->coords->coord.t[2];
        if (z > 0x1B58) {
            work->home.vx  = -0xB54;
            work->home.vz  = 0x2198;
            work->home.vy  = 0;
            work->home.pad = 0x400;
        } else if (z > 0x1068) {
            work->home.vx  = 0;
            work->home.vy  = 0;
            work->home.vz  = 0x189C;
            work->home.pad = 0;
        } else if (z > 0x384) {
            work->home.vx  = 0x12C0;
            work->home.vy  = 0;
            work->home.vz  = 0x1AF4;
            work->home.pad = 0;
        } else if (z > -0x898) {
            work->home.vx  = 0x12C0;
            work->home.vz  = -0x1388;
            work->home.vy  = 0;
            work->home.pad = 0x800;
        } else {
            work->home.vx  = -0xB4;
            work->home.vy  = 0;
            work->home.vz  = -0x960;
            work->home.pad = 0;
        }
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x40) {
            work->field_8B6 = 0x80;
            work->field_8BA = 0x10;
        } else {
            work->field_8B6 = 0x40;
        }
    }
    head = *(Actor401300PursuitScratch**)G_SCRATCH_HEAD;
    blk  = head - 1;
    work->field_6++;
    *(Actor401300PursuitScratch**)G_SCRATCH_HEAD = blk;
    func_actor_401300_80133A3C(arg0);
    s = blk;
    switch (work->field_8A2) {
        case 3:
            blk->delta.vx = work->home.vx - ((TmdObject*)arg0->extra)->coords->coord.t[0];
            head[-1].dx   = blk->delta.vx;
            blk->delta.vy = work->home.vy - ((TmdObject*)arg0->extra)->coords->coord.t[1];
            blk->dy       = blk->delta.vy;
            blk->delta.vz = work->home.vz - ((TmdObject*)arg0->extra)->coords->coord.t[2];
            blk->dz       = blk->delta.vz;
            blk->dist     = SquareRoot0(head[-1].dx * head[-1].dx + blk->dy * blk->dy + blk->dz * blk->dz);
            if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) != 1) {
                func_actor_401300_80132910(arg0, work->field_990, 0xC);
            }
            coord           = ((TmdObject*)arg0->extra)->coords;
            s->angle        = actorNormalizeYaw(ratan2(head[-1].delta.vx, head[-1].delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            work->field_8B2 = s->angle;
            if (s->angle > 0x30) {
                s->angle = 0x30;
            } else if (s->angle < -0x30) {
                s->angle = -0x30;
            } else if ((s->dist < 0x898 && Actor401300_Abs(actorNormalizeYaw(ratan2(-((TmdObject*)arg0->extra)->coords->coord.m[2][0], ((TmdObject*)arg0->extra)->coords->coord.m[2][2]) - work->home.pad)) < 0x200) || work->field_6 > 0xB4) {
                work->field_8A2 = 0x20;
                work->field_89C = 1;
                SndEvt_EnqueueType6(0x551D0008, (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords), (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords));
                work->field_AB0.flags &= 0x7FFF;
            }
            s->angle += ratan2(-((TmdObject*)arg0->extra)->coords->coord.m[2][0], ((TmdObject*)arg0->extra)->coords->coord.m[2][2]);
            Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->angle, 1);
            if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, (s16)((float)((work->field_8A8 + 2) * 30) * 1.5f / 18.0f)) != 0) {
                Actor401300_MoveBy(((TmdObject*)arg0->extra)->coords, (s16)((float)((work->field_8A8 + 2) * 30) * 1.5f / 18.0f));
            }
            actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
            break;
        case 0x20:
            s->angle = ratan2(-((TmdObject*)arg0->extra)->coords->coord.m[2][0], ((TmdObject*)arg0->extra)->coords->coord.m[2][2]);
            Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->angle, 1);
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 0x12C);
            actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
            if (work->field_6C & 0x100) {
                work->field_0 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, enemy->hp, 0);
            }
            break;
    }
    *(Actor401300PursuitScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_801376E4(Task* arg0)
{
    Actor401300Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    Actor401300ChaseScratch* head;
    Actor401300ChaseScratch* s;

    work = arg0->work;
    if (work->field_4 != 0) {
        head                                            = *(Actor401300ChaseScratch**)G_SCRATCH_HEAD;
        obj                                             = arg0->extra;
        *(Actor401300ChaseScratch**)G_SCRATCH_HEAD      = head - 1;
        s                                               = head - 1;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 3;
        work->field_89E        = 0;
        work->field_8B2        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
        coord                                       = ((TmdObject*)arg0->extra)->coords;
        s->turn                                     = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing                                      = ((TmdObject*)arg0->extra)->coords;
        s->angle                                    = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C94                             = s->angle;
        work->field_C96                             = s->angle + (u16)s->turn * 2;
        *(Actor401300ChaseScratch**)G_SCRATCH_HEAD += 1;
        return;
    }
    head                                       = *(Actor401300ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor401300ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    s                                          = head - 1;
    func_actor_401300_80133A3C(arg0);
    actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
    if (work->field_C94 == work->field_C96) {
        if (work->field_D1C < 2 || overlayOutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        } else {
            work->field_0 = 0xB;
        }
    }
    if (work->field_C94 > work->field_C96) {
        work->field_C94 -= 0x89;
        if (work->field_C94 < work->field_C96) {
            work->field_C94 = work->field_C96;
        }
    }
    if (work->field_C94 < work->field_C96) {
        work->field_C94 += 0x89;
        if (work->field_C94 > work->field_C96) {
            work->field_C94 = work->field_C96;
        }
    }
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, work->field_C94, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89E == 0) {
        if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0x28) != 0) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 0x28);
        }
    } else {
        if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0x14) != 0) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 0x14);
        }
    }
    if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) != 1) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    *(Actor401300ChaseScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_80137D78(Task* arg0)
{
    Actor401300Work* work;
    ActorAimScratch* head;
    ActorAimScratch* aim;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR*         dir;
    MATRIX           mat;
    u16              angle;
    s32              kind;

    kind = (arg0->spawnArg1 >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    head                               = *(ActorAimScratch**)G_SCRATCH_HEAD;
    *(ActorAimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                = head - 1;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x140;
        work->field_6          = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C9C == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C9C = 1;
            } else {
                work->field_C9C = -1;
            }
        }
        if (work->field_C9C == 1) {
            work->field_8A2 = 0x15;
            if (work->field_D1E == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_CA2 + angle;
            } else {
                aim->angle += work->field_CA2;
            }
            work->field_C9C = -1;
        } else {
            work->field_8A2 = 0x14;
            if (work->field_D1E == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_CA2;
            } else {
                aim->angle -= work->field_CA2;
            }
            work->field_C9C = 1;
        }
        work->field_89C = 1;
        work->field_8A6 = 0xC;
        work->field_89E = 0;
        func_actor_401300_80133A3C(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_C8C;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C9E = 0xDE;
        work->field_D1E++;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89E == 0) {
        gte_lddp(work->field_C9E);
        gte_ldsv(&work->field_C8C);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C9E >> 1);
        gte_ldsv(&work->field_C8C);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = ((TmdObject*)arg0->extra)->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = ((TmdObject*)arg0->extra)->coords;
        coord->coord.t[2] += aim->delta.vz;
        func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57);
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_80138160(Task* arg0)
{
    SVECTOR          pos;
    Actor401300Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GameActor*       player;
    PlayerStatus*    config;
    SVECTOR*         p;
    s16              angle;

    enemy  = arg0->spawnArg2;
    work   = arg0->work;
    player = (GameActor*)(gameGetPtrSlot(3))->work;
    config = &Player_Status;
    if (work->field_4 != 0) {
        work->field_970.radius    = 0x280;
        work->field_BF0.flags    &= 0x7FFF;
        work->field_AB0.flags    |= 0x4000;
        enemy->node.state.b.flags = 0;
        work->field_89C           = 1;
        work->field_8A6           = 0x10;
        work->field_8A2           = 4;
        func_actor_401300_80133A3C(arg0);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, actorPositionYaw(arg0, &pos, config), 0);
        actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
        pos.vx                                 = ((TmdObject*)arg0->extra)->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy                                 = 0;
        pos.vz                                 = ((TmdObject*)arg0->extra)->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_8B2                        = 0;
        work->field_8B4                        = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        work->field_D1E                        = 0;
        work->field_D20                        = 0;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_5E & 0x3FF) == 0x10 && player->field_954 != 2) {
        angle = actorMatrixPositionYaw(arg0, &pos, D_80073B8C);
        if (abs(angle) < 0x10 && !overlayOutOfRange(&pos, 0x44C)) {
            work->field_CAC.animBlock.ptr = &D_actor_401300_801588F0;
            work->field_D00               = 8;
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&work->field_CEC, 0) == 0) {
                work->field_0           = 0xC;
                work->field_D20         = 1;
                work->field_CAC.field_4 = 1;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                work->field_CC0[2] = 0;
                work->field_CC0[1] = 0;
                work->field_CC0[0] = 0;
                work->field_CD0    = 7;
                work->field_CD2    = 1;
                work->field_D22    = 0;
            }
        }
    }
    if (work->field_8A2 == 4 && (work->field_6C & 0x100)) {
        work->field_0 = 7;
    }
    if ((work->field_5E & 0x3FF) > 0x10) {
        p      = &pos;
        pos.vx = ((TmdObject*)arg0->extra)->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy = 0;
        pos.vz = ((TmdObject*)arg0->extra)->coords->coord.t[2] - config->coordMtx->t[2];
        if (!overlayOutOfRange(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12();
            gte_stsv(p);
            coord                                  = ((TmdObject*)arg0->extra)->coords;
            coord->coord.t[0]                     += pos.vx;
            coord                                  = ((TmdObject*)arg0->extra)->coords;
            coord->coord.t[2]                     += pos.vz;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
        }
    }
}

void func_actor_401300_80138800(Task* arg0)
{
    SVECTOR          dir;
    Actor401300Work* work  = arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    Task*            player;
    SVECTOR*         pdir;

    if (work->field_4 != 0) {
        player                                   = gameGetPtrSlot(3);
        work->field_970.radius                   = 0x280;
        work->field_BF0.flags                   &= 0x7FFF;
        work->field_AB0.flags                   |= 0x4000;
        enemy->node.state.b.flags                = 0;
        work->field_89C                          = 1;
        work->field_8A6                          = 0x10;
        work->field_8A2                          = 5;
        ((TmdObject*)player->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        work->field_CD4.vx = ((TmdObject*)player->extra)->coords->coord.t[0];
        work->field_CD4.vy = ((TmdObject*)player->extra)->coords->coord.t[1];
        work->field_CD4.vz = ((TmdObject*)player->extra)->coords->coord.t[2];
        pdir               = &dir;
        dir.vx             = ((GpCoordXZ*)((TmdObject*)arg0->extra)->coords)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18;
        dir.vy             = 0;
        dir.vz             = ((GpCoordXZ*)((TmdObject*)arg0->extra)->coords)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12();
        gte_stsv(pdir);
        ((TmdObject*)arg0->extra)->coords->coord.t[0] = ((TmdObject*)player->extra)->coords->coord.t[0] + dir.vx;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] = ((TmdObject*)player->extra)->coords->coord.t[2] + dir.vz;
        ((TmdObject*)arg0->extra)->coords->flg        = 0;
        work->field_CE4.vx                            = 0;
        work->field_CE4.vy                            = ratan2(dir.vx, dir.vz);
        work->field_CE4.vz                            = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&work->field_CD4, 0);
    }
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
    if (work->field_8A2 == 5 && (work->field_6C & 0x100)) {
        work->field_910.coord      = &((TmdObject*)arg0->extra)->coords[1];
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[5], NULL, &work->field_910);
        work->field_0 = 0xD;
    }
}

void func_actor_401300_80138B24(Task* arg0)
{
    Actor401300Work* work   = arg0->work;
    GpEnemy*         enemy  = arg0->spawnArg2;
    Task*            player = gameGetPtrSlot(3);

    if (work->field_4 != 0) {
        work->field_8A6 = 0x10;
        work->field_8A2 = 6;
        work->field_89C = 2;
        if ((s16)Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 0), 0) == 1) {
            ((GameActor*)player->work)->field_956 = 0xA;
        }
        work->field_CAC.field_4 = 2;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
        work->field_D22 = 0;
    }
    if (work->field_6C & 2) {
        work->field_910.coord      = &((TmdObject*)arg0->extra)->coords[1];
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[5], NULL, &work->field_910);
        work->field_0 = 0xE;
    }
    work->field_898 = work->field_5E & 0x3FF;
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
}

void func_actor_401300_80138CF8(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_970.radius           = 0x280;
        work->field_BF0.flags           &= 0x7FFF;
        work->field_AB0.flags           |= 0x4000;
        enemy->node.state.b.flags        = 0;
        work->field_89C                  = 1;
        work->field_8A2                  = 0xA;
        work->field_89E                  = 0;
        work->field_8A6                  = 0x10;
        work->field_8B4                  = 0;
        work->field_8B2                  = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x20;
        work->field_8BA        = 8;
        work->field_970.flags |= 0x4000;
    }
    if (work->field_8A2 == 0xA && (s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, -0x57) != 0) {
        actorMoveForward(((TmdObject*)arg0->extra)->coords, -0x57);
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(((TmdObject*)arg0->extra)->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_6C & 0x100) {
        if (work->field_8A2 == 0xA) {
            work->field_8A2 = 0xB;
            work->field_89C = 2;
            func_actor_401300_80133A3C(arg0);
        }
        if ((work->field_6C & 0x100) && work->field_8A2 == 0xB) {
            work->field_970.flags &= 0xBFFF;
            if (enemy->hp <= 0) {
                work->field_0 = 0x15;
            } else if (enemy->reactionFlags & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        }
    }
}

void func_actor_401300_80138FCC(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_970.radius           = 0x280;
        work->field_BF0.flags           &= 0x7FFF;
        work->field_AB0.flags           |= 0x4000;
        enemy->node.state.b.flags        = 0;
        work->field_89C                  = 1;
        work->field_8A2                  = 0xC;
        work->field_8A6                  = 0x10;
        work->field_8B4                  = 0;
        work->field_8B2                  = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x20;
        work->field_8BA        = 8;
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(((TmdObject*)arg0->extra)->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw, scaled by `xz` on X/Z
/// and `y` on Y. `actorRescaleYaw` with a separate Y scale.
static __inline__ void Actor401300_RescaleYawXZ(GsCOORDINATE2* coord, s32 xz, s16 y)
{
    void*                 head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (ActorScaleRotScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = xz;
    blk->scale.vy = y;
    blk->scale.vz = xz;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]    = *(u16*)&((ActorScaleRotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]    = *(u16*)&blk->m.m[2][1];
    m22                     = *(u16*)&blk->m.m[2][2];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
    coord->flg              = 0;
    coord->coord.m[2][2]    = m22;
}

/// Collapse state: spawns effect 0x600A5 at the actor's view-space position on
/// frame 30, switches the light mode on 30/42, and from frame 26 squashes the
/// root coordinate's Y scale; state 0x24 follows after frame 64.
void func_actor_401300_80139134(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    OverlayMatWords* w;
    SVECTOR          pos;
    s16              t;

    work  = arg0->work;
    obj   = arg0->extra;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        work->field_BF0.flags     = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags     = (u16)(work->field_AB0.flags & 0xBFFF);
        enemy->node.state.b.flags = 1;
        work->field_6             = 0;
        work->field_8A6           = 8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 <= 0x400) {
        switch (++work->field_6) {
            case 30:
                w          = (OverlayMatWords*)&work->field_8C0.coord;
                w->m00_m01 = 0x1000;
                w->m02_m10 = 0;
                w->m11_m12 = 0x1000;
                w->m20_m21 = 0;
                w->m22     = 0x1000;
                pos.vx     = 0;
                pos.vy     = 0;
                pos.vz     = 0;
                actorTransformToView(&((TmdObject*)arg0->extra)->coords[2], &pos);
                work->field_8C0.sub        = &gGfxViewCoord;
                work->field_8C0.coord.t[0] = pos.vx;
                work->field_8C0.coord.t[1] = ((TmdObject*)arg0->extra)->coords->coord.t[1];
                work->field_8C0.coord.t[2] = pos.vz;
                work->field_8C0.flg        = 0;
                Gp_UpdateCoord(&work->field_8C0);
                Gp_SetLightMode(enemy, 1);
                Gp_SpawnEff(0x600A5, &work->field_8C0, 3, NULL);
                break;
            case 48:
                ((TmdObject*)arg0->extra)->flags = 2;
                break;
            case 42:
                Gp_SetLightMode(enemy, 2);
                break;
            case 64:
                ((TmdObject*)arg0->extra)->flags = 0x80;
                break;
        }
        t = work->field_6;
        if (t >= 0x1A) {
            Actor401300_RescaleYawXZ(((TmdObject*)arg0->extra)->coords, 0x1964, 0x1964 - (t - 0x14) * 16);
        }
        if (work->field_6 > 0x40 && work->field_D20 == 0) {
            work->field_0 = 0x24;
        }
    }
}

void func_actor_401300_80139520(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj        = arg0->extra;
        enemy      = arg0->spawnArg2;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius    = 0x280;
        work->field_BF0.flags    &= 0x7FFF;
        work->field_AB0.flags    &= 0xBFFF;
        enemy->node.state.b.flags = 0;
        work->field_6             = 0;
        work->field_C68           = work->field_C48;
        work->field_8A2           = 0xE;
        work->field_89C           = 1;
        work->field_8A6           = work->field_8A8;
    }
    if (work->field_6 > 0x960) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6++;
    }
    coord    = ((TmdObject*)arg0->extra)->coords;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, 3000)) {
        work->field_0 = 6;
    }
    if (Gp_StateF0.field_2 & 1) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_8A2 == 0xE && (work->field_6C & 2)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            work->field_8A2 = 0xF;
            work->field_89C = 1;
            func_actor_401300_80133A3C(arg0);
        }
    }
    if (work->field_8A2 == 0xF && (work->field_6C & 0x100)) {
        work->field_8A2 = 0xE;
        work->field_89C = 1;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_801397F8(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        D_actor_401300_80158878 = &D_actor_401300_80152BB8;
        work->field_8A2         = 0x10;
        work->field_89C         = 2;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius    = 0x280;
        work->field_BF0.flags    &= 0x7FFF;
        work->field_AB0.flags    |= 0x4000;
        enemy->node.state.b.flags = 0;
        work->field_8B4           = 0;
        work->field_8A6           = 0x10;
        work->field_8B2           = 0;
        work->field_6             = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->placeKey >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->field_6 = 1;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_5E & 0x3FF) == 4 && work->field_8BC != (work->field_5E & 0x3FF)) {
        work->field_910.coord      = ((TmdObject*)arg0->extra)->coords + 1;
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), ((TmdObject*)arg0->extra)->coords + 5, NULL, &work->field_910);
    }
    work->field_8BC = work->field_5E & 0x3FF;
    coord           = ((TmdObject*)arg0->extra)->coords;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, 3000)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (Gp_StateF0.field_2 & 1) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
}

void func_actor_401300_80139AB0(Task* arg0)
{
    Actor401300Work*  work;
    TmdObject*        obj;
    GsCOORDINATE2*    coord;
    ActorTurnScratch* s;
    GsCOORDINATE2*    facing;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 2;
        work->field_8B6        = 0x60;
        work->field_8BA        = 8;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x60) {
            work->field_8B6 = 0x20;
        } else {
            work->field_8B6 = 0x60;
        }
    }
    *(ActorTurnScratch**)G_SCRATCH_HEAD -= 1;
    s                                    = *(ActorTurnScratch**)G_SCRATCH_HEAD;
    s->delta.vx                          = work->field_C[work->field_16].x - ((TmdObject*)arg0->extra)->coords->coord.t[0];
    s->delta.vy                          = 0;
    s->delta.vz                          = work->field_C[work->field_16].z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
    if (!overlayOutOfRange(&s->delta, 0xA0)) {
        if (work->field_16 == 0) {
            work->field_16 = 1;
        } else {
            work->field_16 = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    coord           = ((TmdObject*)arg0->extra)->coords;
    s->angle        = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8B2 = s->angle;
    if (s->angle > 0x20) {
        s->angle = 0x20;
    }
    if (s->angle < -0x20) {
        s->angle = -0x20;
    }
    facing    = ((TmdObject*)arg0->extra)->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    if (work->field_89E == 0) {
        if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0xA) != 0) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, 0xA);
        }
    }
    if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
    if (!overlayOutOfRange(&s->delta, 0x7D0)) {
        work->field_0 = 6;
    } else if (!overlayOutOfRange(&s->delta, 0xFA0)) {
        coord    = ((TmdObject*)arg0->extra)->coords;
        s->angle = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        if (ABS(s->angle) < 0x300) {
            work->field_0 = 6;
        }
    }
    if (*(u32*)&Gp_StateF0 & 0xD0000) {
        work->field_0 = 6;
    }
    *(ActorTurnScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013A208(Task* arg0)
{
    Actor401300Work*  work;
    GpEnemy*          enemy;
    TmdObject*        obj;
    GsCOORDINATE2*    coord;
    ActorTurnScratch* turn;
    u16               next;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2;
        obj             = arg0->extra;
        work->field_8A2 = 0x12;
        work->field_89C = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius    = 0x280;
        work->field_BF0.flags    &= 0x7FFF;
        work->field_AB0.flags    |= 0x4000;
        enemy->node.state.b.flags = 0;
        work->field_8B4           = 0;
        work->field_8A6           = 0x1E;
    }
    *(ActorTurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                 = *(ActorTurnScratch**)G_SCRATCH_HEAD;
    turn->angle                          = actorPositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_8B2                      = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = ((TmdObject*)arg0->extra)->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, turn->angle, 1);
    if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, work->field_C98) != 0) {
        actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, work->field_C98);
    }
    if (work->field_C98 > 0) {
        next            = work->field_C98 - 0xA;
        work->field_C98 = next;
        if ((s16)next < 0) {
            work->field_C98 = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_6C & 0x100) || work->field_C98 == 0) {
        work->field_0 = 9;
    }
    *(ActorTurnScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013A5C0(Task* arg0)
{
    Actor401300Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    ActorAimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x16;
        work->field_8A2        = 2;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        return;
    }
    func_actor_401300_80133A3C(arg0);
    *(ActorAimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                 = *(ActorAimScratch**)G_SCRATCH_HEAD;
    aim->angle                          = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8B2                     = aim->angle;
    if (ABS(aim->angle) <= 0x80 && work->field_8A2 == 2) {
        work->field_8A6 = 0x16;
        work->field_8A2 = 0x11;
        work->field_89C = 1;
        work->field_6   = 0;
        func_actor_401300_80133A3C(arg0);
    }
    if (aim->angle > 0x80) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_8A2 == 0x11) {
        work->field_6++;
        if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, -0x10) != 0) {
            actorMoveForward(((TmdObject*)arg0->extra)->coords, -0x10);
        }
        if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) == 0) {
            func_actor_401300_80132910(arg0, work->field_990, 0xC);
        }
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        if (work->field_6 >= 0x13) {
            if (work->field_8B2 <= 0) {
                Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013AAE8(Task* arg0)
{
    Actor401300Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    ActorAimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x13;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    *(ActorAimScratch**)G_SCRATCH_HEAD    -= 1;
    aim                                    = *(ActorAimScratch**)G_SCRATCH_HEAD;
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if ((work->field_6C & 0x100) || work->field_6 >= 0xB) {
        work->field_0 = 0xB;
    }
    aim->angle      = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8B2 = aim->angle;
    if (aim->angle > 0x20) {
        aim->angle = 0x20;
    }
    if (aim->angle < -0x20) {
        aim->angle = -0x20;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013AE48(Task* arg0)
{
    Actor401300Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    ActorAimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 2;
        work->field_8A6        = 8;
        work->field_8A2        = 0x13;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->field_6   = 0;
        work->field_8B4 = 0;
        return;
    }
    work->field_6++;
    *(ActorAimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                 = *(ActorAimScratch**)G_SCRATCH_HEAD;
    aim->angle                          = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    if (work->field_8B2 < aim->angle) {
        if (aim->angle - work->field_8B2 > 0x28) {
            work->field_8B2 += 0x28;
        } else {
            work->field_8B2 = aim->angle;
        }
    } else if (work->field_8B2 - aim->angle > 0x28) {
        work->field_8B2 -= 0x28;
    } else {
        work->field_8B2 = aim->angle;
    }
    coord      = ((TmdObject*)arg0->extra)->coords;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0x32) {
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[1].coord, 0x40, 0);
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, 0x80, 0);
        ((TmdObject*)arg0->extra)->coords[2].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, 0x80, 0);
        ((TmdObject*)arg0->extra)->coords[3].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[4].coord, 0x80, 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[5].coord, 0x100, 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
    } else {
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[1].coord, 0x40 >> ((work->field_6 - 0x31) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, 0x80 >> ((work->field_6 - 0x30) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[2].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, 0x80 >> ((work->field_6 - 0x2F) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[3].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[4].coord, 0x80 >> ((work->field_6 - 0x2E) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[5].coord, 0x100 >> ((work->field_6 - 0x31) / 4), 0);
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[4]);
        aim->angle = actorPositionYaw(arg0, &aim->delta, &Player_Status);
        if (aim->angle > 0x24) {
            aim->angle = 0x24;
        } else if (aim->angle < -0x24) {
            aim->angle = -0x24;
        }
        if (ABS(aim->angle) < 0x24) {
            work->field_0 = 7;
        }
        coord       = ((TmdObject*)arg0->extra)->coords;
        aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
        actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
        ((TmdObject*)arg0->extra)->coords->flg = 0;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013B6E8(Task* arg0)
{
    SVECTOR          vec;
    Actor401300Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0x80;
        work->field_970.radius           = 0x280;
        work->field_AB0.flags            = (u16)(work->field_AB0.flags & 0xBFFF);
        work->field_BF0.flags            = (u16)(work->field_BF0.flags & 0x7FFF);
        enemy->node.state.b.flags        = 1;
        work->field_8B2                  = 0;
        work->field_6                    = 0U;
        vec.vx                           = 0x64;
        vec.vz                           = 0;
        vec.vy                           = 0;
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords + 1, 0x10300, &vec);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0] = &D_actor_401300_80147894;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 9, 0x200, &vec), enemy);
    }
    if (work->field_6 == 5) {
        D_80114B78[0] = &D_actor_401300_80147894;
        vec.vy        = 0;
        vec.vx        = 0;
        actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 12, 0x200, &vec), enemy);
    }
    if (work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401300_80148A14;
        actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 1, 0x200, NULL), enemy);
    }
    if (work->field_6 == 8) {
        D_80114B78[0] = &D_actor_401300_80148808;
        actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 3, 0x200, NULL), enemy);
    }
    if (work->field_6 >= 0x3D && work->field_D20 == 0) {
        work->field_0 = 0x24;
    }
}

void func_actor_401300_8013BB30(Task* arg0)
{
    SVECTOR          vec;
    Actor401300Work* work;
    GpEnemy*         enemy;
    u16              next;
    s16              cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_970.radius    = 0x280;
        work->field_AB0.flags     = (u16)(work->field_AB0.flags | 0x4000);
        work->field_BF0.flags     = (u16)(work->field_BF0.flags & 0x7FFF);
        enemy->node.state.b.flags = 1;
        work->field_8B2           = 0;
        work->field_6             = 0;
        vec.vx                    = 0x64;
        vec.vz                    = 0;
        vec.vy                    = 0;
        work->field_8A2           = 2;
        work->field_89C           = 1;
        work->field_8A6           = 0x10;
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords + 1, 0x10300, &vec);
        work->field_6 = 0;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_8A2) {
        case 2:
            if ((s16)next >= 0x10 && (work->field_6C & 2)) {
                work->field_8A2 = 0x23;
                work->field_89C = 2;
                work->field_8A6 = 0x10;
                work->field_89E = 0;
            }
            if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0xA) != 0) {
                actorMoveForward(((TmdObject*)arg0->extra)->coords, 0xA);
            }
            func_actor_401300_801323B0(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC);
            if (work->field_6 == 3) {
                D_80114B78[0] = &D_actor_401300_80148808;
                vec.vz        = 0x64;
                vec.vy        = 0;
                vec.vx        = 0;
                actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 9, 0x200, &vec), enemy);
            }
            if (work->field_6 == 5) {
                D_80114B78[0] = &D_actor_401300_80148A14;
                actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 1, 0x200, NULL), enemy);
            }
            break;
        case 0x23:
            if (!(work->field_6C & 0x100)) {
                work->field_6 = 0;
            }
            switch (work->field_6) {
                case 3:
                    break;
                case 30:
                    Gp_SetLightMode(enemy, 1);
                    vec.vx = 0;
                    vec.vy = 0;
                    vec.vz = 0;
                    actorTransformToView(((TmdObject*)arg0->extra)->coords + 2, &vec);
                    work->field_8C0.sub        = &gGfxViewCoord;
                    work->field_8C0.coord.t[0] = vec.vx;
                    work->field_8C0.coord.t[1] = ((TmdObject*)arg0->extra)->coords->coord.t[1];
                    work->field_8C0.coord.t[2] = vec.vz;
                    work->field_8C0.flg        = 0;
                    Gp_UpdateCoord(&work->field_8C0);
                    Gp_SpawnEff(0x600A5, &work->field_8C0, 2, NULL);
                    break;
                case 48:
                    ((TmdObject*)arg0->extra)->flags = 2;
                    break;
                case 42:
                    Gp_SetLightMode(enemy, 2);
                    break;
                case 64:
                    ((TmdObject*)arg0->extra)->flags = 0x80;
                    work->field_0                    = 0x24;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                Actor401300_RescaleYawXZ(((TmdObject*)arg0->extra)->coords, 0x1964, 0x1964 - (cur - 0x14) * 16);
            }
            break;
    }
    func_actor_401300_80133A3C(arg0);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 2);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 3);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 4);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 5);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 6);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 7);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 8);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 9);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 10);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 11);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 12);
}

void func_actor_401300_8013CBAC(Task* arg0)
{
    Actor401300Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   facing;
    GsCOORDINATE2*   root;
    GsCOORDINATE2*   root2;
    PlayerStatus*    config;
    ActorAimScratch* head;
    ActorAimScratch* aim;
    s16              yaw;
    s32              angle;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x24;
        work->field_8A2        = 2;
        work->field_89E        = 0;
        work->field_D1C        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
    }
    work->field_6++;
    head                               = *(ActorAimScratch**)G_SCRATCH_HEAD;
    *(ActorAimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                = head - 1;
    if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->field_990, 0xC);
    }
    config                                 = &Player_Status;
    root                                   = ((TmdObject*)arg0->extra)->coords;
    head[-1].delta.vx                      = config->coordMtx->t[0] - root->coord.t[0];
    aim->delta.vy                          = config->coordMtx->t[1] - root->coord.t[1];
    aim->delta.vz                          = config->coordMtx->t[2] - root->coord.t[2];
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    aim->pad_8        = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                               ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
    root2             = ((TmdObject*)arg0->extra)->coords;
    head[-1].delta.vx = config->coordMtx->t[0] - root2->coord.t[0];
    aim->delta.vy     = config->coordMtx->t[1] - root2->coord.t[1];
    aim->delta.vz     = config->coordMtx->t[2] - root2->coord.t[2];
    yaw               = ratan2(head[-1].delta.vx, aim->delta.vz) + 0x800;
    aim->pad_A        = yaw;
    aim->pad_A        = actorNormalizeYaw(yaw);
    coord             = ((TmdObject*)arg0->extra)->coords;
    angle             = ratan2(aim->delta.vx, aim->delta.vz);
    aim->angle        = actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8B2   = aim->angle;
    if (aim->angle < 0x200) {
        if (!overlayOutOfRange(&aim->delta, 0x44C)) {
            work->field_0 = 0xB;
        }
    }
    if (aim->angle > 0x20) {
        aim->angle = 0x20;
    }
    if (aim->angle < -0x20) {
        aim->angle = -0x20;
    }
    facing      = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_8A2 == 2) {
        if (work->field_89E == 0) {
            if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0x16) != 0) {
                actorMoveForward(((TmdObject*)arg0->extra)->coords, 0x16);
            }
        } else {
            if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 5) != 0) {
                actorMoveForward(((TmdObject*)arg0->extra)->coords, 5);
            }
        }
    } else if (work->field_6C & 0x100) {
        work->field_8A2 = 2;
        work->field_89C = 1;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013D2AC(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    ActorAimScratch* aim;
    s32              angle;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        obj                       = arg0->extra;
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x19;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C     = 0;
        work->field_6       = 0;
        work->field_8       = 0;
        work->field_BF0.key = Gp_PackObjPair(enemy, 0);
        work->field_8B6     = 0x200;
        work->field_8BA     = 0x80;
        return;
    }
    func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57);
    if (work->field_6 >= 0x29) {
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
    }
    work->field_6++;
    switch (work->field_6) {
        case 0x19:
            work->field_BF0.flags |= 0x8000;
            break;
        case 0x28:
            work->field_BF0.flags &= 0x7FFF;
            break;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                 = *(ActorAimScratch**)G_SCRATCH_HEAD;
    actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &aim->delta);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0xE) {
        coord           = ((TmdObject*)arg0->extra)->coords;
        angle           = ratan2(aim->delta.vx, aim->delta.vz);
        aim->angle      = actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8B2 = aim->angle;
        if (aim->angle > 0x30) {
            aim->angle = 0x30;
        }
        if (aim->angle < -0x30) {
            aim->angle = -0x30;
        }
        coord2      = ((TmdObject*)arg0->extra)->coords;
        aim->angle += ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
        actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_0 = 6;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_actor_401300_8013D6C4(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    ActorAimScratch* aim;
    s32              angle;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        obj                       = arg0->extra;
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1A;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C     = 0;
        work->field_6       = 0;
        work->field_8       = 0;
        work->field_BF0.key = Gp_PackObjPair(enemy, 1);
        work->field_8B6     = 0x200;
        work->field_8BA     = 0x80;
        return;
    }
    func_actor_401300_801323B0(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC);
    if (work->field_6 >= 0x26) {
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
    }
    work->field_6++;
    switch (work->field_6) {
        case 0x15:
            work->field_BF0.flags |= 0x8000;
            break;
        case 0x25:
            work->field_BF0.flags &= 0x7FFF;
            break;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                 = *(ActorAimScratch**)G_SCRATCH_HEAD;
    actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &aim->delta);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0xE) {
        coord           = ((TmdObject*)arg0->extra)->coords;
        angle           = ratan2(aim->delta.vx, aim->delta.vz);
        aim->angle      = actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8B2 = aim->angle;
        if (aim->angle > 0x30) {
            aim->angle = 0x30;
        }
        if (aim->angle < -0x30) {
            aim->angle = -0x30;
        }
        coord2      = ((TmdObject*)arg0->extra)->coords;
        aim->angle += ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
        actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1964);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_0 = 6;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// `actorRescaleYaw` at 0x1964 on the actor's root coordinate, with
/// the root's `flg` cleared again before the scratch block is released.
static __inline__ void Actor401300_ResetActorYaw(Task* actor)
{
    GsCOORDINATE2*        coord;
    void*                 head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    coord                   = ((TmdObject*)actor->extra)->coords;
    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (ActorScaleRotScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 0x1964;
    blk->scale.vy = 0x1964;
    blk->scale.vx = 0x1964;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]                    = *(u16*)&((ActorScaleRotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]                    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]                    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]                    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]                    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]                    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]                    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]                    = *(u16*)&blk->m.m[2][1];
    m22                                     = *(u16*)&blk->m.m[2][2];
    coord->flg                              = 0;
    coord->coord.m[2][2]                    = m22;
    ((TmdObject*)actor->extra)->coords->flg = 0;
    *(void**)G_SCRATCH_HEAD                 = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
}

/// Facing yaw of `coord`.
static __inline__ s32 Actor401300_Yaw(GsCOORDINATE2* coord)
{
    return ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

/// `actorMoveForward` testing the flag byte through a `McSaveData*`.
static __inline__ void Actor401300_MoveForwardSave(McSaveData* save, GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if ((u8)save->unknown_5C0[1] != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_8013DADC(Task* arg0)
{
    Actor401300Work* work;
    Task*            task;
    GameActor*       player;
    PlayerStatus*    config;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    SVECTOR**        scratch;
    ActorAimScratch* head;
    ActorAimScratch* aim;
    s16              amount;
    s16              ret;
    McSaveData*      save;

    work   = arg0->work;
    task   = gameGetPtrSlot(3);
    player = (GameActor*)task->work;
    enemy  = arg0->spawnArg2;

    if (work->field_4 != 0) {
        obj                       = arg0->extra;
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1B;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
        return;
    }
    scratch = (SVECTOR**)G_SCRATCH_HEAD;
    config  = &Player_Status;
    work->field_6++;
    root              = ((TmdObject*)arg0->extra)->coords;
    head              = (ActorAimScratch*)*scratch;
    head[-1].delta.vx = config->coordMtx->t[0] - root->coord.t[0];
    aim               = (ActorAimScratch*)(*scratch = (SVECTOR*)(head - 1));
    aim->delta.vy     = config->coordMtx->t[1] - root->coord.t[1];
    aim->delta.vz     = config->coordMtx->t[2] - root->coord.t[2];
    save              = &Mc_SaveData;
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x1B:
            if ((aim->pad_E = func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57)) != 0 &&
                work->field_6 >= 0x15) {
                work->field_8++;
            } else {
                if (aim->pad_E != 1) {
                    func_actor_401300_80132910(arg0, work->field_990, 0xC);
                }
                work->field_8 = 0;
            }
            if (work->field_8 >= 7) {
                work->field_8A2 = 0x1E;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            work->field_8B2 = 0;
            aim->angle      = actorYawTo(((TmdObject*)arg0->extra)->coords, aim->delta.vx, aim->delta.vz);
            if (work->field_6 >= 0xB) {
                if (abs(aim->angle) < 0x200) {
                    if (!overlayOutOfRange(&aim->delta, 0x7D0)) {
                        work->field_8A2 = 0x1C;
                        work->field_89C = 1;
                        work->field_6   = 0;
                    }
                }
            }
            if (abs(aim->angle) > 0x400) {
                work->field_8A2 = 0x1C;
                work->field_89C = 1;
                work->field_6   = 0;
            }
            if (aim->angle > 6) {
                aim->angle = 6;
            } else if (aim->angle < -6) {
                aim->angle = -6;
            }
            aim->angle += Actor401300_Yaw(((TmdObject*)arg0->extra)->coords);
            Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
            if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0x70) != 0) {
                actorMoveForward(((TmdObject*)arg0->extra)->coords, 0x70);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x1C:
            if ((aim->pad_E = func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57)) != 0 &&
                work->field_6 >= 0x15) {
                work->field_8++;
            } else {
                if (aim->pad_E != 1) {
                    func_actor_401300_80132910(arg0, work->field_990, 0xC);
                }
                work->field_8 = 0;
            }
            if (work->field_8 >= 7) {
                work->field_8A2 = 0x1E;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            aim->angle = actorYawTo(((TmdObject*)arg0->extra)->coords, aim->delta.vx, aim->delta.vz);
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x1D;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (func_actor_401300_80132910(arg0, work->field_990, 0xC) != 0 && player->field_954 != 2 && abs(aim->angle) < 0x100 &&
                enemy->hp > 0) {
                work->field_D00 = 0x7F;
                if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&work->field_CEC, 0) == 0) {
                    Gp_SpawnPadLerp(0x10, 8, 0xFF);
                    work->field_D20               = 1;
                    work->field_CAC.animBlock.ptr = &D_actor_401300_801588F0;
                    work->field_CC0[2]            = 0;
                    work->field_CC0[1]            = 0;
                    work->field_CC0[0]            = 0;
                    work->field_CD0               = 7;
                    work->field_CD2               = 1;
                    aim->delta.vx                 = -aim->delta.vx;
                    aim->delta.vy                 = -aim->delta.vy;
                    aim->delta.vz                 = -aim->delta.vz;
                    aim->angle                    = actorYawTo(((TmdObject*)task->extra)->coords, aim->delta.vx, aim->delta.vz);
                    if (abs(aim->angle) < 0x400) {
                        amount                  = -0x64;
                        work->field_CAC.field_4 = 4;
                        work->field_CE4.vy      = aim->angle + Actor401300_Yaw(((TmdObject*)task->extra)->coords);
                        ret                     = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 4), 0);
                    } else {
                        amount                  = 0x64;
                        work->field_CAC.field_4 = 5;
                        work->field_CE4.vy      = aim->angle + Actor401300_Yaw(((TmdObject*)task->extra)->coords) + 0x800;
                        ret                     = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 5), 0);
                    }
                    if (ret == 1) {
                        player->field_956 = 0xA;
                    }
                    work->field_CD4.vx = ((TmdObject*)task->extra)->coords->coord.t[0];
                    work->field_CD4.vy = ((TmdObject*)task->extra)->coords->coord.t[1];
                    work->field_CD4.vz = ((TmdObject*)task->extra)->coords->coord.t[2];
                    work->field_CE4.vx = 0;
                    work->field_CE4.vz = 0;
                    Gp_DispatchMsg(task, 0x3E9, (s32)&work->field_CD4, 0);
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                    work->field_D22 = 0;
                    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &aim->delta);
                    aim->delta.vy = 0;
                    VectorNormalSS(&aim->delta, &aim->delta);
                    gte_lddp(amount);
                    gte_ldsv(&aim->delta);
                    gte_gpf12();
                    gte_stsv(&aim->delta);
                    work->field_CC0[0] = aim->delta.vx;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = aim->delta.vz;
                    work->field_CD0    = 7;
                    work->field_CD2    = 1;
                    work->field_8A2    = 0x1E;
                    work->field_89C    = 2;
                    work->field_6      = 0;
                }
            }
            if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0xA8) != 0) {
                actorMoveForward(((TmdObject*)arg0->extra)->coords, 0xA8);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x1E:
            if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) == 0) {
                func_actor_401300_80132910(arg0, work->field_990, 0xC);
            }
            if (work->field_6 < 8) {
                if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, -0x79) != 0) {
                    Actor401300_MoveForwardSave(save, ((TmdObject*)arg0->extra)->coords, -0x79);
                }
            } else if ((u16)(work->field_6 - 8) < 6) {
                if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, -0x19) != 0) {
                    Actor401300_MoveForwardSave(save, ((TmdObject*)arg0->extra)->coords, -0x19);
                }
            }
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        case 0x1D:
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

/// `actorMoveForwardNonzero` testing the same flag byte through a
/// `McSaveData*` (`D_80072729` is `Mc_SaveData.unknown_5C0[1]`).
static __inline__ void Actor401300_MoveForwardNonzeroSave(McSaveData* save, GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if ((u8)save->unknown_5C0[1] != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_8013E930(Task* arg0)
{
    Actor401300Work*         work;
    Task*                    task;
    GameActor*               player;
    PlayerStatus*            config;
    GpEnemy*                 enemy;
    TmdObject*               obj;
    GsCOORDINATE2*           root;
    SVECTOR**                scratch;
    Actor401300LungeScratch* head;
    Actor401300LungeScratch* blk;
    SVECTOR*                 delta;
    SVECTOR*                 vec;
    s16                      cur;
    s16                      amount;
    s16                      ret;
    McSaveData*              save;

    work   = arg0->work;
    task   = gameGetPtrSlot(3);
    player = (GameActor*)task->work;
    config = &Player_Status;
    save   = &Mc_SaveData;
    enemy  = arg0->spawnArg2;

    if (work->field_4 != 0) {
        obj                       = arg0->extra;
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1F;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x200;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8BA = 0x100;
        work->field_D04 = ((TmdObject*)arg0->extra)->coords->coord.t[0];
        work->field_D06 = ((TmdObject*)arg0->extra)->coords->coord.t[1];
        work->field_D08 = ((TmdObject*)arg0->extra)->coords->coord.t[2];
        return;
    }
    scratch = (SVECTOR**)G_SCRATCH_HEAD;
    work->field_6++;
    root              = ((TmdObject*)arg0->extra)->coords;
    head              = (Actor401300LungeScratch*)*scratch;
    head[-1].delta.vx = config->coordMtx->t[0] - root->coord.t[0];
    delta             = &head[-1].delta;
    delta->vy         = config->coordMtx->t[1] - root->coord.t[1];
    blk               = (Actor401300LungeScratch*)(*scratch = (SVECTOR*)(head - 1));
    delta->vz         = config->coordMtx->t[2] - root->coord.t[2];
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x1F:
            work->field_970.radius = 0x280;
            if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) == 0) {
                func_actor_401300_80132910(arg0, work->field_990, 0xC);
            }
            work->field_8B2 = 0;
            blk->angle      = actorYawTo(((TmdObject*)arg0->extra)->coords, head[-1].delta.vx, delta->vz);
            if (work->field_6 >= 0xB) {
                work->field_8A2 = 0x20;
                work->field_89C = 1;
                blk->dist.vx    = config->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0];
                blk->dist.vy    = config->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1];
                blk->dist.vz    = config->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2];
                blk->range      = SquareRoot0(blk->dist.vx * blk->dist.vx + blk->dist.vy * blk->dist.vy + blk->dist.vz * blk->dist.vz) + 1000;
                if (blk->range > 5000) {
                    blk->range = 5000;
                } else if (blk->range < 3000) {
                    blk->range = 3000;
                }
                work->field_14  = blk->range / 18;
                work->field_8B6 = 0;
                work->field_6   = 0;
                work->field_8BA = 0x20;
            }
            if (abs(blk->angle) > 0x400) {
                work->field_0 = 7;
            }
            if (blk->angle > 0x10) {
                blk->angle = 0x10;
            } else if (blk->angle < -0x10) {
                blk->angle = -0x10;
            }
            blk->angle += Actor401300_Yaw(((TmdObject*)arg0->extra)->coords);
            Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, blk->angle, 1);
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x20:
            work->field_970.radius = 0x140;
            func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57);
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x21;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (func_actor_401300_80132910(arg0, work->field_990, 0xC) != 0 && player->field_954 != 2 && work->field_6 >= 8 &&
                enemy->hp > 0) {
                work->field_D00 = 0x7F;
                if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&work->field_CEC, 0) == 0) {
                    Gp_SpawnPadLerp(0x10, 8, 0xFF);
                    SndEvt_EnqueueType6(6, (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords),
                                        (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
                    work->field_D20               = 1;
                    work->field_CAC.animBlock.ptr = &D_actor_401300_801588F0;
                    blk->delta.vx                 = work->field_D04 - ((TmdObject*)task->extra)->coords->coord.t[0];
                    blk->delta.vy                 = work->field_D06 - ((TmdObject*)task->extra)->coords->coord.t[1];
                    blk->delta.vz                 = work->field_D08 - ((TmdObject*)task->extra)->coords->coord.t[2];
                    blk->angle                    = actorYawTo(((TmdObject*)task->extra)->coords, head[-1].delta.vx, delta->vz);
                    if (abs(blk->angle) < 0x400) {
                        amount                  = -0x46;
                        work->field_CAC.field_4 = 4;
                        work->field_CE4.vy      = blk->angle + Actor401300_Yaw(((TmdObject*)task->extra)->coords);
                        ret                     = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 2), 0);
                    } else {
                        amount                  = 0x46;
                        work->field_CAC.field_4 = 5;
                        work->field_CE4.vy      = blk->angle + Actor401300_Yaw(((TmdObject*)task->extra)->coords) + 0x800;
                        ret                     = Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 3), 0);
                    }
                    if (ret == 1) {
                        player->field_956 = 0xA;
                    }
                    work->field_CD4.vx = ((TmdObject*)task->extra)->coords->coord.t[0];
                    work->field_CD4.vy = ((TmdObject*)task->extra)->coords->coord.t[1];
                    work->field_CD4.vz = ((TmdObject*)task->extra)->coords->coord.t[2];
                    work->field_CE4.vx = 0;
                    work->field_CE4.vz = 0;
                    Gp_DispatchMsg(task, 0x3E9, (s32)&work->field_CD4, 0);
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                    work->field_D22 = 0;
                    vec             = &blk->delta;
                    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, vec);
                    blk->delta.vy = 0;
                    VectorNormalSS(vec, vec);
                    gte_lddp(amount);
                    gte_ldsv(vec);
                    gte_gpf12();
                    gte_stsv(vec);
                    work->field_CC0[0] = blk->delta.vx;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = blk->delta.vz;
                    work->field_CD0    = 7;
                    work->field_CD2    = 1;
                }
            }
            if (work->field_D20 == 0) {
                actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, work->field_14);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x21:
            work->field_970.radius = 0x280;
            if (func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57) == 0) {
                cur = work->field_6;
                if (cur < 0x11 && work->field_D20 == 0) {
                    if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, (s16)(0x54 - cur * 0x54 / 16)) != 0) {
                        Actor401300_MoveForwardNonzeroSave(save, ((TmdObject*)arg0->extra)->coords, 0x54 - work->field_6 * 0x54 / 16);
                    }
                }
            }
            func_actor_401300_80132910(arg0, work->field_990, 0xC);
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    *(Actor401300LungeScratch**)G_SCRATCH_HEAD += 1;
}

/// Scale `m` uniformly by `scale` (4.12), translation included.
static __inline__ void Actor401300_ScaleMatrix(MATRIX* m, s16 scale)
{
    ActorScaleMatrixScratch* head;
    ActorScaleMatrixScratch* blk;

    head                                       = *(ActorScaleMatrixScratch**)G_SCRATCH_HEAD;
    blk                                        = head - 1;
    *(ActorScaleMatrixScratch**)G_SCRATCH_HEAD = blk;
    blk->scale.vz                              = scale;
    blk->scale.vy                              = scale;
    head[-1].scale.vx                          = scale;
    ScaleMatrix(m, &blk->scale);
    blk->trans.vx = m->t[0];
    blk->trans.vy = m->t[1];
    blk->trans.vz = m->t[2];
    gte_lddp(scale);
    gte_ldsv(&blk->trans);
    gte_gpf12();
    gte_stsv(&blk->trans);
    m->t[0]                                     = blk->trans.vx;
    m->t[1]                                     = blk->trans.vy;
    *(ActorScaleMatrixScratch**)G_SCRATCH_HEAD += 1;
    m->t[2]                                     = blk->trans.vz;
}

void func_actor_401300_8013F628(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    PlayerStatus*    config;
    GsCOORDINATE2*   root;
    SVECTOR**        scratch;
    SVECTOR*         head;
    SVECTOR*         vec;
    s16              mod;
    s16              cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj = arg0->extra;
        Gp_SetLightMode(enemy, 0);
        enemy->node.state.b.flags = 1;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x20;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x200;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8BA = 0x100;
        work->field_D04 = ((TmdObject*)arg0->extra)->coords->coord.t[0];
        work->field_D06 = ((TmdObject*)arg0->extra)->coords->coord.t[1];
        work->field_D08 = ((TmdObject*)arg0->extra)->coords->coord.t[2];
        Actor401300_ScaleMatrix(&work->field_C48, 0);
        return;
    }
    scratch = (SVECTOR**)G_SCRATCH_HEAD;
    config  = &Player_Status;
    work->field_6++;
    root        = ((TmdObject*)arg0->extra)->coords;
    head        = *scratch;
    head[-1].vx = config->coordMtx->t[0] - root->coord.t[0];
    vec         = head - 1;
    vec->vy     = config->coordMtx->t[1] - root->coord.t[1];
    vec->vz     = config->coordMtx->t[2] - root->coord.t[2];
    *scratch    = head - 3;
    if (work->field_6 < 0x12) {
        Actor401300_ScaleMatrix(&work->field_C48, (work->field_6 << 12) / 30);
        if (gGameSession->at4.loc.area == 0xB) {
            if ((work->field_6 & 7) == 0) {
                Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 3, 0, NULL);
                Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 16, 0, NULL);
                Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 1, 0, NULL);
                Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 18, 0, NULL);
            } else {
                mod = work->field_6 % 8;
                if (mod == 2) {
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 2, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 17, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 3, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 4, 0, NULL);
                } else if (mod == 4) {
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 5, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 16, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 1, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 19, 0, NULL);
                } else if (mod == 6) {
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 17, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 16, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 5, 0, NULL);
                    Gp_SpawnEff(0x600FB, ((TmdObject*)arg0->extra)->coords + 18, 0, NULL);
                }
            }
        } else if (gGameSession->at4.loc.area == 0x1D) {
            if ((work->field_6 & 7) == 0) {
                Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 3, 0, NULL);
                Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 16, 0, NULL);
                Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 1, 0, NULL);
                Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 18, 0, NULL);
            } else {
                mod = work->field_6 % 8;
                if (mod == 2) {
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 2, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 17, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 3, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 4, 0, NULL);
                } else if (mod == 4) {
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 5, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 16, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 1, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 19, 0, NULL);
                } else if (mod == 6) {
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 17, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 16, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 5, 0, NULL);
                    Gp_SpawnEff(0x601C1, ((TmdObject*)arg0->extra)->coords + 18, 0, NULL);
                }
            }
        }
    }
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x20:
            work->field_970.radius = 0x500;
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x21;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (work->field_D20 == 0 && (s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0x78) != 0) {
                actorMoveForward(((TmdObject*)arg0->extra)->coords, 0x78);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x21:
            work->field_970.radius = 0x280;
            cur                    = work->field_6;
            if (cur < 0x11 && work->field_D20 == 0) {
                if ((s16)func_actor_401300_8013267C(((TmdObject*)arg0->extra)->coords, 0x15E, 0x54 - cur * 0x54 / 16) != 0) {
                    actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, 0x54 - work->field_6 * 0x54 / 16);
                }
            }
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    *(ActorScaleMatrixScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_80140300(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_970.radius           = 0x280;
        work->field_BF0.flags           &= 0x7FFF;
        work->field_AB0.flags           |= 0x4000;
        enemy->node.state.b.flags        = 0;
        work->field_89C                  = 2;
        work->field_8A2                  = 0xB;
        work->field_8A6                  = 0x10;
        work->field_8B4                  = 0;
        work->field_8B2                  = 0;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x40;
        work->field_8B8        = 0xC8;
        work->field_8BA        = 0x40;
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(((TmdObject*)arg0->extra)->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

void func_actor_401300_8014046C(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_970.radius           = 0x280;
        work->field_BF0.flags           &= 0x7FFF;
        work->field_AB0.flags           |= 0x4000;
        enemy->node.state.b.flags        = 0;
        work->field_89C                  = 2;
        work->field_8A2                  = 0x22;
        work->field_8A6                  = 0x10;
        work->field_8B4                  = 0;
        work->field_8B2                  = 0;
        work->field_8B6                  = 0x40;
        work->field_8B8                  = 0xC8;
        work->field_8BA                  = 0x40;
        if (enemy->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(((TmdObject*)arg0->extra)->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(((TmdObject*)arg0->extra)->coords, work->field_AD0, 0xC, 0x57);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

// This is a decompilation attempt by the m2c tool.

/// `Actor401300_InRange` with the flag kept apart from the comparison. The
/// dead `ret = cmp` in each arm stops jump from turning the if/else into a
/// store-flag, which would fold `cmp` and `ret` into one register.
static __inline__ s32 Actor401300_InRangeFlag(Task* arg0)
{
    SVECTOR        out;
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp;
    GsCOORDINATE2* view;
    VECTOR*        vecp;
    s32*           flagp;
    SVECTOR*       outp;
    GsCOORDINATE2* p;
    s32            ret;
    s32            cmp;

    memset(&out, 0, 8);
    p     = &((TmdObject*)arg0->extra)->coords[1];
    svp   = &sv;
    outp  = &out;
    view  = &gGfxViewCoord;
    vecp  = &vec;
    flagp = &flag;
    sv.vx = outp->vx;
    sv.vy = outp->vy;
    sv.vz = outp->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        outp->vx = sv.vx;
        outp->vy = sv.vy;
        outp->vz = sv.vz;
    }
    cmp = (u16)(out.vz + 0x12B) < 0xA27;
    if (cmp == 0) {
        ret = cmp;
        ret = 0;
    } else {
        ret = cmp;
        ret = 1;
    }
    return ret;
}

/// 1 when the first of `recs` is a kind 0x10000 record.
static __inline__ s32 Actor401300_HasRec10000(GpRec18* recs)
{
    s16 i;

    for (i = 0; i < 1; i++) {
        if (!recs[i].key)
            break;
        if ((recs[i].key & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Snaps the player's height to the actor's when it is locked (`field_CD0`
/// 7) and has drifted 0x321 or more away.
static __inline__ void Actor401300_SnapPlayerHeight(Task* actor)
{
    Actor401300Work* work;
    Task*            slot;
    GsCOORDINATE2*   playerCoord;
    GsCOORDINATE2*   actorCoord;

    work = actor->work;
    slot = gameGetPtrSlot(3);
    if ((slot != NULL) && (work->field_CD0 == 7)) {
        playerCoord = ((TmdObject*)slot->extra)->coords;
        actorCoord  = ((TmdObject*)actor->extra)->coords;
        if (abs(playerCoord->coord.t[1] - actorCoord->coord.t[1]) >= 0x321) {
            playerCoord->coord.t[1]                = actorCoord->coord.t[1];
            ((TmdObject*)slot->extra)->coords->flg = 0;
        }
    }
}

static const Actor401300StateTable D_actor_401300_80131F34 = { {
    func_actor_401300_8014192C,
    func_actor_401300_801419B8,
    func_actor_401300_80141A60,
    func_actor_401300_80141B0C,
    func_actor_401300_80135DDC,
    func_actor_401300_80141BC8,
    func_actor_401300_80136238,
    func_actor_401300_801365F8,
    func_actor_401300_80136CE8,
    func_actor_401300_801376E4,
    func_actor_401300_80137D78,
    func_actor_401300_80138160,
    func_actor_401300_80138800,
    func_actor_401300_80138B24,
    func_actor_401300_80141C80,
    func_actor_401300_80141C88,
    func_actor_401300_80141D50,
    func_actor_401300_80141DF4,
    NULL,
    func_actor_401300_80138CF8,
    func_actor_401300_80138FCC,
    func_actor_401300_80139134,
    func_actor_401300_80139520,
    func_actor_401300_801397F8,
    func_actor_401300_80139AB0,
    func_actor_401300_8013A5C0,
    func_actor_401300_8013A208,
    func_actor_401300_8013AAE8,
    func_actor_401300_8013AE48,
    func_actor_401300_8013B6E8,
    func_actor_401300_8013CBAC,
    func_actor_401300_8013D2AC,
    func_actor_401300_8013D6C4,
    func_actor_401300_8013DADC,
    func_actor_401300_8013E930,
    func_actor_401300_8013F628,
    func_actor_401300_80141EF8,
    func_actor_401300_80140300,
    func_actor_401300_8014046C,
    func_actor_401300_80135FC4,
    func_actor_401300_8013BB30,
} };

void func_actor_401300_801405DC(GpEnemy* enemy, Task* actor)
{
    VECTOR                pos;
    Actor401300StateTable states;
    Actor401300Work*      work;
    ActorViewScratch*     scratch;
    ActorViewScratch*     head;
    Task*                 player;
    PlayerStatus*         config;
    s32                   state;
    s32                   action;

    work   = actor->work;
    player = gameGetPtrSlot(3);
    config = &Player_Status;
    states = D_actor_401300_80131F34;

    ((TmdObject*)actor->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)actor->extra)->coords);
    pos.vx = ((TmdObject*)actor->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)actor->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)actor->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (Gp_StateF0.field_4) {
        case 0:
            state = work->field_0;
            if ((state != 0) && (state != 0x24) && (state != 0x15) && (state != 0x1D) && (state != 0x28)) {
                ((TmdObject*)actor->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)actor->extra)->coords->workm.t, 0x280, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x28) && (work->field_8A2 == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)actor->extra)->coords->workm.t, 0x280, Gp_State1C->groundShade);
            }
            break;
        case 1:
            state = work->field_0;
            if ((state != 0) && (state != 0x24) && (state != 0x15) && (state != 0x1D) && (state != 0x28)) {
                ((TmdObject*)actor->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)actor->extra)->coords->workm.t, 0x280, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x28) && (work->field_8A2 == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)actor->extra)->coords->workm.t, 0x280, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied(work->field_AD0);
            Gp_ClearRec18Occupied(work->field_990);
            Gp_ClearRec18Occupied((GpRec18*)work->pad_C10);
            return;
        case 2:
            ((TmdObject*)actor->extra)->flags = 0x80;
            Gp_ClearRec18Occupied(work->field_AD0);
            Gp_ClearRec18Occupied(work->field_990);
            Gp_ClearRec18Occupied((GpRec18*)work->pad_C10);
            return;
    }

    head                                = *(ActorViewScratch**)G_SCRATCH_HEAD;
    *(ActorViewScratch**)G_SCRATCH_HEAD = head - 1;
    scratch                             = head - 1;

    if (work->field_C88 > 0) {
        work->field_C88 = (s16)((u16)work->field_C88 - 1);
    } else {
        func_actor_401300_80134F90(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    states.fn[work->field_0](actor);

    state = work->field_0;
    if ((state != 0x15) && (state != 3) && (state != 0) && (state != 0x24) && (state != 0x1D) && (state != 0x28)) {
        scratch->pos.vx = 0;
        scratch->pos.vy = 0;
        scratch->pos.vz = 0;
        actorTransformToView(((TmdObject*)actor->extra)->coords + 1, &scratch->pos);
        work->field_970.pos.vx     = scratch->pos.vx;
        work->field_970.pos.vy     = scratch->pos.vy;
        work->field_970.pos.vz     = scratch->pos.vz;
        work->field_920.coord.t[0] = ((TmdObject*)actor->extra)->coords->coord.t[0];
        work->field_920.coord.t[1] = ((TmdObject*)actor->extra)->coords->coord.t[1] - 0x15E;
        work->field_920.coord.t[2] = ((TmdObject*)actor->extra)->coords->coord.t[2];
        work->field_920.flg        = 0;
        Gp_UpdateCoord(&work->field_920);
        ((TmdObject*)actor->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)actor->extra)->coords);
        state = work->field_0;
    }
    if ((state == 0x15) || (state == 0) || (state == 0x24) || (state == 0x1D) || (state == 0x28)) {
        work->field_970.flags &= 0x7FFF;
        work->field_AB0.flags &= 0x7FFF;
    } else {
        work->field_970.flags |= 0x8000;
        if ((u32)((u16)work->field_0 - 0x21) < 2U) {
            work->field_AB0.flags &= 0x7FFF;
        } else {
            work->field_AB0.flags |= 0x8000;
        }
    }
    if ((Actor401300_HasRec10000((GpRec18*)work->pad_C10) == 1) || (enemy->hp <= 0)) {
        work->field_BF0.flags &= 0x7FFF;
    }
    Gp_ClearRec18Occupied(work->field_AD0);
    Gp_ClearRec18Occupied(work->field_990);
    Gp_ClearRec18Occupied((GpRec18*)work->pad_C10);

    if (work->field_D20 == 1) {
        state = work->field_0;
        if ((state != 0x15) && (state != 3) && (state != 0) && (state != 0x24) && (state != 0x1D) && (state != 0x28)) {
            Actor401300_SnapPlayerHeight(actor);
        }
        action          = work->field_CAC.field_4;
        work->field_D22 = (u16)(work->field_D22 + 1);
        switch (action) {
            case 0:
            case 1:
            case 2:
            case 3:
                break;
            case 4:
                if ((s16)work->field_D22 == 0xF) {
                    if (Actor401300_InRangeFlag(player) == 1) {
                        SndEvt_EnqueueType6(0x551D0005, (s8)Gp_GetObjPan(((TmdObject*)player->extra)->coords),
                                            (s8)gpGetObjDepth(((TmdObject*)player->extra)->coords));
                    } else {
                        SndEvt_EnqueueType6(0x400D0013, (s8)Gp_GetObjPan(((TmdObject*)player->extra)->coords),
                                            (s8)gpGetObjDepth(((TmdObject*)player->extra)->coords));
                    }
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)player->extra)->coords[1], 0x80003A00, NULL);
                    }
                }
                if (Gp_DispatchMsg(player, 0x3FE, (s32)work->field_CC0, 0) == 1) {
                    work->field_CC0[0] = 0;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = 0;
                }
                if (work->field_6 >= 10) {
                    work->field_CC0[1] = 0;
                    work->field_CC0[0] = work->field_CC0[0] >> 1;
                    work->field_CC0[2] = work->field_CC0[2] >> 1;
                }
                break;
            case 5:
                if ((s16)work->field_D22 == 0xD) {
                    if (Actor401300_InRangeFlag(player) == 1) {
                        SndEvt_EnqueueType6(0x551D0005, (s8)Gp_GetObjPan(((TmdObject*)player->extra)->coords),
                                            (s8)gpGetObjDepth(((TmdObject*)player->extra)->coords));
                    } else {
                        SndEvt_EnqueueType6(0x400D0013, (s8)Gp_GetObjPan(((TmdObject*)player->extra)->coords),
                                            (s8)gpGetObjDepth(((TmdObject*)player->extra)->coords));
                    }
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)player->extra)->coords[1], 0x80003A00, NULL);
                    }
                }
                if (Gp_DispatchMsg(player, 0x3FE, (s32)work->field_CC0, 0) == 1) {
                    work->field_CC0[0] = 0;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = 0;
                }
                if (work->field_6 >= 10) {
                    work->field_CC0[1] = 0;
                    work->field_CC0[0] = work->field_CC0[0] >> 1;
                    work->field_CC0[2] = work->field_CC0[2] >> 1;
                }
                break;
            case 6:
            case 7:
                break;
        }
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            switch (work->field_CAC.field_4) {
                case 0:
                    break;
                case 1:
                    if (config->hp > 0) {
                        work->field_CAC.field_4 = 2;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 2:
                    if (config->hp > 0) {
                        work->field_CAC.field_4 = 3;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 4:
                    if (config->hp > 0) {
                        work->field_CAC.field_4 = 6;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 5:
                    if (config->hp > 0) {
                        work->field_CAC.field_4 = 7;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 3:
                case 6:
                case 7:
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                    work->field_D20 = 0;
                    break;
            }
        }
    }
    if ((work->field_C8A == 1) && (work->field_D20 == 0)) {
        Gp_ReleaseStateF0Add(actor, 0xD);
        work->field_C8A = 0;
    }
    if ((Gp_StateF0.field_3 == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(((TmdObject*)actor->extra)->coords + 2, &scratch->pos);

    work->field_D28[work->field_D78].vx = scratch->pos.vx;
    work->field_D28[work->field_D78].vy = scratch->pos.vy;
    work->field_D28[work->field_D78].vz = scratch->pos.vz;

    *(u8**)G_SCRATCH_HEAD += 0x18;
    work->field_D78        = (u16)work->field_D78 + 1;
    if (work->field_D78 == 7) {
        work->field_D78 = 0;
    }
    if ((u32)((u16)work->field_8A2 - 0x14) < 2U) {
        enemy->bodyPos.vx = work->field_D28[work->field_D78].vx;
        enemy->bodyPos.vy = work->field_D28[work->field_D78].vy;
        enemy->bodyPos.vz = work->field_D28[work->field_D78].vz;
    } else {
        enemy->bodyPos.vx = scratch->pos.vx;
        enemy->bodyPos.vy = scratch->pos.vy;
        enemy->bodyPos.vz = scratch->pos.vz;
    }
    enemy->coord = &gGfxViewCoord;
}

void func_actor_401300_8014148C(void)
{
}

/// The task's handlers, indexed by `Task::state` in
/// `func_actor_401300_80141F2C`: the first allocates and sets up the work
/// block, the second runs the per-state logic every frame, and the third tears
/// the enemy down.
const GpEnemyTaskFuncTable3 D_actor_401300_8013201C = { {
    func_actor_401300_80134454,
    func_actor_401300_801405DC,
    Gp_DestroyEnemy,
} };

s32 func_actor_401300_80141494(Task* arg0, s32 arg1, GpAnimArg* arg2)
{
    Actor401300Work* work = arg0->work;

    switch (arg2->field_4) {
        case 0:
            work->field_8A2 = 0x22;
            break;
        case 1:
            work->field_8A2 = 0x23;
            break;
        case 2:
            work->field_8A2 = 0x24;
            break;
        case 3:
            work->field_8A2 = 0x25;
            break;
        case 4:
            work->field_8A2 = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// Applies one of four model settings chosen by `arg2`: 0 sets the model's
/// flags to 0x80 and reallocates its buffers, 1 clears the flags and
/// reallocates them, 2 and 3 set bit 2 (3 clearing the rest first). Case 1
/// moves the actor to state 0x18, the others to state 0. Always returns 0.
s32 func_actor_401300_80141504(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor401300Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor401300Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

/// Returns 1 while the actor's enemy still has HP. Once it is down, returns 0
/// if the model carries bit 0x80 or lacks bit 2, and 1 otherwise.
s32 func_actor_401300_801415C4(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

/// Places the model's root coordinate from `placement`: sets its translation,
/// applies the X, Y and Z rotations in turn, and caches the resulting heading
/// (`ratan2` of the matrix Z axis) in `Actor401300Work::yaw`. Always returns 1.
s32 func_actor_401300_80141614(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    s32              mx;
    s32              mz;
    Actor401300Work* work;

    work                                          = (Actor401300Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    coord                                  = ((TmdObject*)task->extra)->coords;
    mx                                     = coord->coord.m[2][0];
    mz                                     = coord->coord.m[2][2];
    work->yaw                              = ratan2(-mx, mz);
    return 1;
}

/// Leaves state 0xD: to 0xE while the player has HP left, to 0x16 once it is
/// gone. Any other state is left alone.
s32 func_actor_401300_80141714(Task* task)
{
    Actor401300Work* work = (Actor401300Work*)task->work;
    PlayerStatus*    cfg  = &Player_Status;

    if (work->field_0 == 0xD) {
        if (cfg->hp > 0) {
            work->field_0 = 0xE;
        } else {
            work->field_0 = 0x16;
        }
    }
    return 1;
}

void func_actor_401300_80141758(Task* task)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = (Actor401300Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_D0C != NULL) {
            taskKill(work->field_D0C);
        }
        if (work->field_D10 != NULL) {
            taskKill(work->field_D10);
        }
        Gp_UnlinkObj(&work->field_BF0);
        Gp_UnlinkObj(&work->field_970);
        Gp_UnlinkObj(&work->field_AB0);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

s32 func_actor_401300_801417F0(Task* arg0)
{
    SVECTOR out;

    memset(&out, 0, 8);
    actorTransformToView(&((TmdObject*)arg0->extra)->coords[1], &out);
    return (u16)(out.vz + 0x12B) < 0xA27;
}

void func_actor_401300_8014192C(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                       = arg0->extra;
        enemy->node.state.b.flags = 1;
        obj->flags                = (u16)(obj->flags | 0x80);
        work->field_BF0.flags     = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags     = (u16)(work->field_AB0.flags & 0xBFFF);
        return;
    }
    if (enemy->hp != -0x3E7 && work->field_C8A == 0 && (arg0->spawnArg1 >> 16) == 2) {
        enemy->hp = -0x3E7;
    }
}

void func_actor_401300_801419B8(Task* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 2;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        func_actor_401300_80133A3C(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_80141A60(Task* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 3;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        func_actor_401300_80133A3C(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_80141B0C(Task* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 0xB;
        work->field_8B6       = 0x20;
        work->field_8BA       = 8;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        func_actor_401300_80133A3C(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_80141BC8(Task* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x12;
        work->field_8A2       = 0xD;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        work->field_0 = 7;
    }
}

void func_actor_401300_80141C80(Task* arg0)
{
}

void func_actor_401300_80141C88(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_970.radius           = 0x280;
        work->field_BF0.flags           &= 0x7FFF;
        work->field_AB0.flags           |= 0x4000;
        enemy->node.state.b.flags        = 0;
        work->field_89C                  = 2;
        work->field_8A2                  = 8;
        work->field_8B4                  = 0;
        work->field_8B2                  = 0;
        work->field_8A6                  = work->field_8A8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x051D0000) {
            work->field_0 = 8;
        } else {
            work->field_0 = 7;
        }
    }
}

void func_actor_401300_80141D50(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_970.radius           = 0x280;
        work->field_BF0.flags           &= 0x7FFF;
        work->field_AB0.flags           |= 0x4000;
        enemy->node.state.b.flags        = 0;
        work->field_89C                  = 2;
        work->field_8A2                  = 0x16;
        work->field_8B4                  = 0;
        work->field_8B2                  = 0;
        work->field_8A6                  = work->field_8A8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6C & 0x100) {
        work->field_0 = 7;
    }
}

void func_actor_401300_80141DF4(Task* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_8B6 = 0x20;
        work->field_8BA = 8;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_6   = work->field_CA0 + ((Gp_LcgState >> 16) & 0xF);
    }
    func_actor_401300_80133A3C(arg0);
    if (--work->field_6 < 0) {
        switch (work->field_8A2) {
            case 11:
            case 23:
                work->field_0 = 0xF;
                break;
            case 12:
            case 24:
            case 34:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
}

void func_actor_401300_80141EF8(Task* task)
{
    Actor401300Work* work  = (Actor401300Work*)task->work;
    GpEnemy*         enemy = task->spawnArg2;

    if (enemy->hp != -0x3E7 && work->field_C8A == 0) {
        enemy->hp = -0x3E7;
    }
}

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401300_80141F2C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401300_8013201C;
    sp.funcs[task->state](task->spawnArg2, task);
}
