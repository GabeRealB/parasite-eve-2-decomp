#ifndef ACTOR_401800_H
#define ACTOR_401800_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

/// XZ patrol point in `Actor401800Work.field_C`. Same shape as
/// `Actor01900Waypoint`.
typedef struct Actor401800Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor401800Waypoint;

/// 0xC-byte row of `D_actor_401800_8013E700`; the init body copies the four
/// halfwords of the row picked by the spawn argument's low nibble into
/// `Actor401800Work.field_C08..field_C0E`. Same shape as
/// `Actor01900TintRow`.
typedef struct Actor401800TintRow {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  field_4;
    /* 0x6 */ s16  field_6;
    /* 0x8 */ byte pad_8[4];
} Actor401800TintRow;
STATIC_ASSERT_SIZEOF(Actor401800TintRow, 0xC);

/// Private work block of the actor 401800 task, hanging off `Task::work`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` display nodes the teardown hands
/// back to `Gp_UnlinkObj`, and the two child tasks it kills. The offsets match
/// the same-shaped teardown of actor 01900 (`Actor01900Work`), whose display
/// nodes sit at the same three addresses; only the child-task pair differs.
/// `field_4` is the live-actor flag `func_actor_401800_8013E138` tests, and
/// `field_A08.flags` / `field_B48.flags` are the two masks it writes. The
/// halfwords at 0x898..0x8A2 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes; `func_actor_401800_8013E194` is that body.
typedef struct Actor401800Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Step counter `func_actor_401800_8013E4F0` resets to 0 and bumps once a
    /// frame; the same slot `Actor104000Work.field_6` counts in.
    /* 0x006 */ s16 field_6;
    /// Stride counter the walking body bumps once a frame alongside `field_6`
    /// and clears whenever `func_actor_401800_80133918` reports the actor is on
    /// target; the walking body picks `field_0 = 0x1B` once it reaches 0x5B.
    /// Same slot `Actor01900Work.field_8` counts in.
    /* 0x008 */ s16  field_8;
    /* 0x00A */ byte pad_A[2];
    /// XZ patrol points: the spawn position and one step along its facing.
    /* 0x00C */ Actor401800Waypoint field_C[2];
    /* 0x014 */ s16                 field_14;
    /* 0x016 */ byte                pad_16[0x44];
    /* 0x05A */ u16                 field_5A;
    /* 0x05C */ byte                pad_5C[0xC];
    /* 0x068 */ u16                 field_68;
    /* 0x06A */ byte                pad_6A[0x82A];
    /* 0x894 */ s32                 field_894;
    /* 0x898 */ s16                 field_898;
    /* 0x89A */ s16                 field_89A;
    /* 0x89C */ byte                pad_89C[2];
    /* 0x89E */ s16                 field_89E;
    /* 0x8A0 */ byte                pad_8A0[2];
    /* 0x8A2 */ s16                 field_8A2;
    /* 0x8A4 */ s16                 field_8A4;
    /* 0x8A6 */ byte                pad_8A6[8];
    /* 0x8AE */ s16                 field_8AE;
    /* 0x8B0 */ s16                 field_8B0;
    /* 0x8B2 */ byte                pad_8B2[2];
    /// State the `0x3FF` handler last ran for: `func_actor_401800_8013A034`
    /// sends the actor's 0x200 effect when `field_5A & 0x3FF` is 4 and differs
    /// from this, then stores the mask back. Same slot `Actor01900Work.field_894`
    /// compares against.
    /* 0x8B4 */ s32 field_8B4;
    /// `func_800FDB18` argument record: the coordinate handed to it plus the
    /// effect scale / count pair. Same slot `Actor01900Work.field_8B8` keeps.
    /* 0x8B8 */ GpEffArg field_8B8;
    /* 0x8C0 */ byte     pad_8C0[2];
    /// Frame counter the aim-and-rescale body decrements once per frame while
    /// it is non-zero. Same role `Actor01900Work.field_C37` plays in the
    /// matching chase body.
    /* 0x8C2 */ u8      field_8C2;
    /* 0x8C3 */ byte    pad_8C3[5];
    /* 0x8C8 */ GpObj   field_8C8;
    /* 0x8E8 */ GpRec18 field_8E8;
    /* 0x900 */ byte    pad_900[0x108];
    /* 0xA08 */ GpObj   field_A08;
    /* 0xA28 */ GpRec18 field_A28;
    /* 0xA40 */ byte    pad_A40[0x108];
    /* 0xB48 */ GpObj   field_B48;
    /* 0xB68 */ GpRec18 field_B68;
    /// Light and color matrices the init body binds onto the model object
    /// (`TmdObject.field_1C` / `field_20`). Same pair `Actor01900Work` keeps
    /// at `field_BB0` / `field_BD0`.
    /* 0xB80 */ MATRIX field_B80;
    /// Root-coordinate matrix the live-actor block of the walking body copies
    /// from `field_BC0` back over itself, then re-writes from the pose scratch.
    /* 0xBA0 */ MATRIX field_BA0;
    /// Home of the root-coordinate matrix the live-actor block restores.
    /// Same pair `Actor00100Work.field_BA0` keeps.
    /* 0xBC0 */ MATRIX field_BC0;
    /// Cleared by the init body right after `field_A08` is linked; same slot
    /// `Actor01900Work.field_C10` / `Actor401300Work.field_C88` clears.
    /* 0xBE0 */ s16  field_BE0;
    /* 0xBE2 */ byte pad_BE2[6];
    /// Direction the swing body of `func_actor_401800_80137DDC` rebuilds from
    /// the yaw in the scratch angle with `Gfx_MatrixCol2` / `VectorNormalSS`,
    /// then GPF-scales by `field_C02` into the offset added to the root
    /// coordinate. Same slot `Actor01900Work.field_C18` keeps.
    /* 0xBE8 */ SVECTOR field_BE8;
    /* 0xBF0 */ byte    pad_BF0[8];
    /// Bearing the chase body of `func_actor_401800_80137714` steers 0x89 a
    /// frame toward `field_BFA` and hands to `Gfx_RotMatrixY`. Same slot
    /// `Actor401300Work.field_C94` / `Actor01900Work.field_C20` keep.
    /* 0xBF8 */ s16 field_BF8;
    /// Target bearing the same body stores as the actor's facing plus twice
    /// the wrapped turn toward the player. Same slot
    /// `Actor401300Work.field_C96` / `Actor01900Work.field_C22` keep.
    /* 0xBFA */ s16 field_BFA;
    /// Step the aim-and-rescale body walks the actor along its own local Z
    /// axis while `func_actor_401800_80133558` says the path is clear, and
    /// reloads `field_0 = 9` once it has counted down to zero. Same slot
    /// `Actor401300Work.field_C98` keeps.
    /* 0xBFC */ s16 field_BFC;
    /// Countdown that walks the chase body of `func_actor_401800_80136EAC`
    /// through its step ramp: 8 until `field_8A2` reaches 0x18, then -1 until
    /// it lands on 0x12, then 0 — the state that ticks `field_6` and re-aims
    /// the actor at the player. Added to `field_8A2` every frame. Same slot
    /// `Actor01900Work.field_C26` keeps.
    /* 0xBFE */ s16 field_BFE;
    /// Side the live-actor swing of `func_actor_401800_80137DDC` takes from
    /// the LCG, then flips every frame it runs: 1 adds 0x171 to the yaw and
    /// -1 subtracts it. Same slot `Actor01900Work.field_C28`.
    /* 0xC00 */ s16 field_C00;
    /// The 0xDE the swing body resets its GPF scale to and halves once the
    /// `field_A28` contact test fires. Same slot `Actor01900Work.field_C2A`.
    /* 0xC02 */ s16 field_C02;
    /// Step the actor walks along its local Z axis: `func_actor_401800_80139118`
    /// seeds it with -0x78, hands it to the step helper while the 0x10 clip is
    /// playing, and halves it each time the `field_A28` contact test fires.
    /// Same role `Actor401300Work.field_C98` plays.
    /* 0xC04 */ s16  field_C04;
    /* 0xC06 */ byte pad_C06[2];
    /// Per-variant reload the LCG spreads over the idle step countdown: the
    /// high half of a fresh `Gp_LcgState` draw masked to 3 bits (`& 7`) is
    /// added to it and stored into `field_6`. Same slot `Actor401300Work`
    /// keeps as `field_CA0`, whose counterpart loads it with `& 0xF`.
    /* 0xC08 */ u16 field_C08;
    /// Second and third halfwords of the same `D_actor_401800_8013E700` row the
    /// init body copies; `field_C0A` / `field_C0C` pair with `field_C08` and
    /// `field_C0E` as the row's four halfwords.
    /* 0xC0A */ u16 field_C0A;
    /* 0xC0C */ u16 field_C0C;
    /// Radius `func_actor_401800_8013A034` hands its scratch distance test:
    /// the player is close enough to arm the actor once the squared XZ offset
    /// fits inside it. Same role `Actor01900Work.field_C32` plays.
    /* 0xC0E */ u16 field_C0E;
    /// The three bytes `func_actor_401800_8013DF80` copies out of the room
    /// request record it is handed; same slot as `Actor01900Work.field_C34`.
    /* 0xC10 */ u8   field_C10[3];
    /* 0xC13 */ byte pad_C13[1];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC14 */ Task* field_C14;
    /* 0xC18 */ Task* field_C18;
    /// Contact range the chase body of `func_actor_401800_80137714` tests the
    /// player offset against: under 2 it takes the 8 state outright. Same slot
    /// `Actor401300Work.field_D1C` / `Actor01900Work.field_C40` keep.
    /* 0xC1C */ s16 field_C1C;
    /// Frames the live-actor swing of `func_actor_401800_80137DDC` has run:
    /// it picks the side `field_C00` flips to only while this is zero, and
    /// bumps it once per frame. Same slot `Actor01900Work.field_C42`.
    /* 0xC1E */ s16 field_C1E;
    /// Set to 1 by the actors that own the `0x3F1` message and cleared once
    /// `func_actor_401800_80139118` has sent it. Same slot `Actor01900Work`
    /// keeps at 0xC20.
    /* 0xC20 */ s16  field_C20;
    /* 0xC22 */ byte pad_C22[0x52];
    /// Cleared by the init body once the root coordinate has been rescaled;
    /// same slot `Actor01900Work.field_C98` / `Actor401300Work.field_C98`
    /// clears at the same point.
    /* 0xC74 */ s16 field_C74;
} Actor401800Work;

/// 0x34-byte scratch `func_actor_401800_8013629C` takes from `G_SCRATCH_HEAD`
/// to push the root coordinate away from the kind 0x10000 / 0x30000 records of
/// a `GpRec18` table: `pos` is the world translation, `offset` the push
/// (clamped to length 0x96), `i` the record cursor and `hit` the return value.
/// `dist` gets 0x7FFE at the terminating record.
/// Same shape as `Actor01900PushScratch` / `Actor401300PushScratch`.
typedef struct Actor401800PushScratch {
    /* 0x00 */ SVECTOR offset;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ s32     kind;
    /* 0x14 */ s32     len;
    /* 0x18 */ s16     i;
    /* 0x1A */ s16     hit;
    /* 0x1C */ s16     dist[12];
} Actor401800PushScratch;
STATIC_ASSERT_SIZEOF(Actor401800PushScratch, 0x34);

/// 0x88-byte scratch `func_actor_401800_801323D4` takes from `G_SCRATCH_HEAD`
/// to keep the push-out of the last kind 0x10000 / 0x30000 record of a `GpRec18`
/// table: `pos` is the coordinate's world translation, `offset` the latest push
/// (clamped to length 0x100), `last` its XZ copy, `i` the record cursor and
/// `hit` the return value. `dist` gets 0x7FFE at the terminating record.
/// Same shape as `Actor01900RepelScratch`.
typedef struct Actor401800RepelScratch {
    /* 0x00 */ byte    pad_0[0x20];
    /* 0x20 */ SVECTOR offset;
    /* 0x28 */ SVECTOR last;
    /* 0x30 */ SVECTOR pos;
    /* 0x38 */ s32     kind;
    /* 0x3C */ u32     len;
    /* 0x40 */ s16     dist[32];
    /* 0x80 */ s16     i;
    /* 0x82 */ byte    pad_82[4];
    /* 0x86 */ s16     hit;
} Actor401800RepelScratch;
STATIC_ASSERT_SIZEOF(Actor401800RepelScratch, 0x88);

/// 0x54-byte scratch `func_actor_401800_8013271C` takes from `G_SCRATCH_HEAD` to
/// push a coordinate away from the obstacles in a `GpRec18` table: `angle`/`ok`
/// hold up to eight bearings collected from the records, `i`/`j` are the loop
/// cursors, and `blocked` is set when any record's kind is 0x10000. Same layout
/// as `Actor00100AvoidScratch`, which `Actor00100_Fn00508` carves.
typedef struct Actor401800AvoidScratch {
    /* 0x00 */ MATRIX   m;
    /* 0x20 */ SVECTOR  dir;
    /* 0x28 */ SVECTOR3 eye;
    /* 0x2E */ byte     pad_2E[0x2];
    /* 0x30 */ s32      kind;
    /* 0x34 */ s16      angle[8];
    /* 0x44 */ s8       ok[8];
    /* 0x4C */ s16      face;
    /* 0x4E */ s16      diff;
    /* 0x50 */ u8       i;
    /* 0x51 */ u8       j;
    /* 0x52 */ u8       count;
    /* 0x53 */ u8       blocked;
} Actor401800AvoidScratch;
STATIC_ASSERT_SIZEOF(Actor401800AvoidScratch, 0x54);

/// 0x10-byte scratch the bearing helpers of `func_actor_401800_8013271C` nest
/// inside `Actor401800AvoidScratch`: an obstacle's offset, widened to words.
typedef struct Actor401800AvoidDelta {
    /* 0x0 */ s32  vx;
    /* 0x4 */ s32  vy;
    /* 0x8 */ s32  vz;
    /* 0xC */ byte pad_C[0x4];
} Actor401800AvoidDelta;
STATIC_ASSERT_SIZEOF(Actor401800AvoidDelta, 0x10);

/// 0xC-byte scratch `func_actor_401800_8013A034` takes from `G_SCRATCH_HEAD`
/// for its player-in-radius test: the X/Z offset to the camera target and the
/// radius, each squared in place before `dx + dz < r`. Same shape as
/// `Actor01900RangeScratch` / `Actor401300RangeScratch`.
typedef struct Actor401800RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor401800RangeScratch;
STATIC_ASSERT_SIZEOF(Actor401800RangeScratch, 0xC);

/// Animation view of the same task work block: the pose context at 0x1C and
/// its slot array, then the blend context the actor keeps beside it. The
/// arrays cover the slot indices `func_actor_401800_801337EC` walks, which is
/// the identical body of `Actor01900_Fn01950`; the offsets all match
/// `Actor01900AnimWork`, and the tail overlays the work block's
/// `field_8A2` / `field_8A4` (the state the slot writes step down by 3).
typedef struct Actor401800AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[19];
    /* 0x328 */ byte       pad_328[0x130];
    /* 0x458 */ GpAnimCtx  blendAnim;
    /* 0x46C */ GpAnimSlot blendSlots[19];
    /* 0x764 */ byte       pad_764[0x13E];
    /* 0x8A2 */ s16        field_8A2;
    /* 0x8A4 */ s16        field_8A4;
    /* 0x8A6 */ byte       pad_8A6[4];
    /* 0x8AA */ s16        field_8AA;
    /* 0x8AC */ s16        field_8AC;
} Actor401800AnimWork;

/// 0x10-byte scratch `func_actor_401800_80135F58` takes from `G_SCRATCH_HEAD`:
/// the offset from the actor to the player, then the clamped turn applied to
/// the root coordinate. Same shape as `Actor01900AimScratch`.
typedef struct Actor401800AimScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     angle;
    /* 0xE */ s16     pad_E;
} Actor401800AimScratch;
STATIC_ASSERT_SIZEOF(Actor401800AimScratch, 0x10);

/// 0xC-byte scratch `func_actor_401800_8013AB64` takes from `G_SCRATCH_HEAD`:
/// the offset from the actor to the player, then the clamped turn folded into
/// the root coordinate's Y rotation. Same layout as `Actor401300TurnScratch`.
typedef struct Actor401800TurnScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad_A;
} Actor401800TurnScratch;
STATIC_ASSERT_SIZEOF(Actor401800TurnScratch, 0xC);

/// 0x10-byte scratch the walking and chase bodies take from `G_SCRATCH_HEAD`:
/// the offset from the actor to the player, the player's facing yaw, the yaw
/// back toward the player, the wrapped turn toward the player and the facing
/// yaw the body folds it into. Only `delta` and `turn` are read back, and the
/// chase body uses neither the facing pair nor `angle`. Same shape as
/// `Actor401300ChaseScratch` / `Actor01900ChaseScratch`.
typedef struct Actor401800ChaseScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     playerYaw;
    /* 0xA */ s16     yaw;
    /* 0xC */ s16     turn;
    /* 0xE */ s16     angle;
} Actor401800ChaseScratch;
STATIC_ASSERT_SIZEOF(Actor401800ChaseScratch, 0x10);

/// 0x34-byte scratch the yaw rebuild takes from `G_SCRATCH_HEAD`: a `MATRIX`
/// plus the `VECTOR` handed to `ScaleMatrix` and the yaw stored before
/// `Gfx_RotMatrixY`. Same layout as `Actor01900RotScratch`.
typedef struct Actor401800RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor401800RotScratch;
STATIC_ASSERT_SIZEOF(Actor401800RotScratch, 0x34);

/// 0x20 scratch block the shared coordinate walk takes from
/// `G_SCRATCH_HEAD`. `coord` is the frame the walk is currently standing on
/// (it climbs the `GsCOORDINATE2::sub` parent chain until NULL), `vec` is the
/// vector being carried up into world space, and `out` receives the `MVMVA`
/// result (`MAC1..3`) that is fed back into `vec` each step. `flag` takes the
/// GTE flag register so the block matches what the code stores. Same layout as
/// `RoomsShared80182078Walk`.
typedef struct Actor401800BisectorWalk {
    /* 0x00 */ GsCOORDINATE2* coord;
    /* 0x04 */ SVECTOR        vec;
    /* 0x0C */ s32            out[3];
    /* 0x18 */ s32            pad_18;
    /* 0x1C */ s32            flag;
} Actor401800BisectorWalk;
STATIC_ASSERT_SIZEOF(Actor401800BisectorWalk, 0x20);

/// 0xE4 scratch block `func_actor_401800_80132E0C` takes from
/// `G_SCRATCH_HEAD` while it nudges a coordinate frame away from the
/// obstacles recorded in a `GpRec18` table. `m` is the working matrix handed
/// to `Gfx_RotMatrixY` / `Gfx_MatrixCol2`. `eye` is the frame's own world
/// position and `aim` the world point one unit (0x1000) in front of it, both
/// produced by walking the parent chain; `delta` is the scratch difference fed
/// to `ratan2` and later the GPF-scaled push applied to `coord.t[0]` /
/// `coord.t[2]`. `kind` is the record's `field_4` high halfword, `angle[]` the
/// per-record bearing relative to the facing direction (0x7FFE marks "no more
/// records", 0x7FFF "record does not count"), `i` / `j` the two loop counters,
/// `diff` the wrapped bearing difference between a pair of records and `hit`
/// the value the function returns. Same layout as
/// `RoomsShared80182078Scratch`.
typedef struct Actor401800BisectorScratch {
    /* 0x00 */ MATRIX  m;
    /* 0x20 */ byte    pad_20[0x80];
    /* 0xA0 */ SVECTOR delta;
    /* 0xA8 */ SVECTOR eye;
    /* 0xB0 */ SVECTOR aim;
    /* 0xB8 */ s32     kind;
    /* 0xBC */ s16     angle[0x10];
    /* 0xDC */ s16     i;
    /* 0xDE */ s16     j;
    /* 0xE0 */ s16     diff;
    /* 0xE2 */ s16     hit;
} Actor401800BisectorScratch;
STATIC_ASSERT_SIZEOF(Actor401800BisectorScratch, 0xE4);

/// `mvmva 1, 0, 0, 0, 0`: rotate V0 by the rotation matrix and add the
/// translation vector. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

/// Carries `v` from the local frame `coord` up the `GsCOORDINATE2::sub` parent
/// chain into world space, using a 0x20 scratch block from `G_SCRATCH_HEAD`.
static __inline__ void Actor401800_BisectorToWorld(GsCOORDINATE2* coord, SVECTOR* v)
{
    Actor401800BisectorWalk* blk;

    {
        register GsCOORDINATE2* parent asm("v0");
        parent                                                                                              = coord;
        ((Actor401800BisectorWalk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor401800BisectorWalk)))->coord = parent;
    }
    {
        register u8* tmp asm("v0");
        tmp = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor401800BisectorWalk);
        blk = (Actor401800BisectorWalk*)tmp;
    }
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr_real();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = *(u16*)&blk->out[0];
        blk->vec.vy = *(u16*)&blk->out[1];
        blk->vec.vz = *(u16*)&blk->out[2];
        blk->coord  = blk->coord->sub;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor401800BisectorWalk);
}

/// Same as `Actor401800_BisectorToWorld`, but the walk starts at `coord`
/// itself rather than its parent, so the vector is not carried through a frame
/// until the loop has run at least once.
static __inline__ void Actor401800_BisectorToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
{
    Actor401800BisectorWalk* blk;

    blk         = (Actor401800BisectorWalk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor401800BisectorWalk));
    blk->coord  = coord;
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr_real();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = *(u16*)&blk->out[0];
        blk->vec.vy = *(u16*)&blk->out[1];
        blk->vec.vz = *(u16*)&blk->out[2];
        blk->coord  = blk->coord->sub;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor401800BisectorWalk);
}

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::work` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900`.
typedef struct Actor401800 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401800Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ byte             pad_30[6];
    /// High halfword of `Task::spawnArg1`; the init body picks the starting
    /// state from its low nibble. Same slot `Actor401300.field_36` keeps.
    /* 0x36 */ s16 field_36;
} Actor401800;

/// Payload of the `0x3FF` message `func_actor_401800_80138F5C` sends: the same
/// 0x14-byte animation record other actors keep as `GpAnimArg` data
/// (`D_actor_356100_80173244` and friends); `field_4` is the animation id.
extern GpAnimArg D_actor_401800_80155A0C;

/// Twelve `SVECTOR` hit positions `func_actor_401800_801348A8` picks from by
/// damage magnitude: the low four when the hit is light, the high two when it
/// is heavy, and the last four on the `arg1 > 0` / `arg1 <= 0` split in
/// between. The fourth halfword (`pad`, unused by the effect itself) is the
/// model part index `func_800FDB18` anchors the spawned effect to. Same role
/// `Actor00100_D1B9F4` plays for `Actor00100_Fn03340`.
extern SVECTOR D_actor_401800_80155A20[12];

/// Animation bank both `func_800B3F84` contexts are initialised from. Same
/// role `Actor01900_D17174` plays for actor 01900.
extern s32 D_actor_401800_80155938;

/// Enemy description record the init body copies `field_4` out of into
/// `GpEnemy.field_40` and points `GpEnemy.field_50` at. Same role
/// `D_actor_401300_80141FA0` plays for actor 401300.
extern GpPairSrcE D_actor_401800_8013E6F0;

/// The three `Actor401800TintRow` variants the init body picks from by the
/// spawn argument's low nibble: `[0]` when it is 2, `[2]` when it is 1, `[1]`
/// otherwise. Same table shape as `Actor01900_D0AC64`.
extern Actor401800TintRow D_actor_401800_8013E700[];

/// Handler table the actor's task receives in `Task::field_24`; same role
/// `Actor01900_D1728C` plays for actor 01900.
extern void* D_actor_401800_80155A80;

/// Payload of the `0x3E9` message `func_actor_401800_80138C28` sends: the
/// slot-3 task's root position followed by the heading `ratan2` derives from
/// the direction to the actor. Same 0x18-byte shape the `Actor401300Work`
/// fields `field_CD4` / `field_CE4` form.
typedef struct Actor401800Msg3E9 {
    /* 0x00 */ VECTOR  field_0;
    /* 0x10 */ SVECTOR field_10;
} Actor401800Msg3E9;
STATIC_ASSERT_SIZEOF(Actor401800Msg3E9, 0x18);

/// Payload `func_actor_401800_80138C28` fills and sends with message 0x3E9.
extern Actor401800Msg3E9 D_actor_401800_80155AD8;

/// Frame counter the chase body of `func_actor_80136EAC` accumulates its step
/// `field_BFC` into and the init body clears; the aim-and-rescale body reads it
/// back as the phase of the step it walks. Same role `Actor01900_D172FC` plays
/// for actor 01900.
extern u16 D_actor_401800_80155AC0;

/// Camera-target matrix `func_actor_401800_8013A034` measures the actor's root
/// coordinate against for its proximity test. Same global `Actor401300` reads.
extern MATRIX* D_80073B8C;

/// LCG seed the actor handlers `Gp_LcgState = Gp_LcgState * 5 + 0x71357911`
/// step and read the high half of; `func_actor_401800_8013E5A4` takes 3 bits
/// of it as the extra idle steps it adds to `Actor401800Work.field_6`.
extern u32 Gp_LcgState;

/// The block `func_actor_401800_8013A034` posts into `D_actor_401800_80155978`
/// when the actor's live flag is set, taking over the animation the actor had
/// been running. Same pair `Actor401300` keeps as `D_actor_401300_80158878` /
/// `D_actor_401300_80152BB8`.
extern s32  D_actor_401800_80155124;
extern s32* D_actor_401800_80155978;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401800_8013BB10`.
extern void* D_80114B78[1];

/// Overlay effect model data `func_actor_401800_8013BB10` points `D_80114B78`
/// at before spawning: the 0x60030 debris burst, then the 0xA0005 fan the step
/// counter trips at 3 and 5 and the two 0xA0005 bursts at 7 and 9.
extern char D_actor_401800_80143E9C;
extern char D_actor_401800_80144434;
extern char D_actor_401800_80144F24;

/// Payload of message `0x7D3`, the "set animation state" request the handler
/// table `D_actor_401800_80155A80` routes to `func_actor_401800_8013DCBC`:
/// `field_4` is the requested state, 0..4. The 01900 actor's table entry for
/// the same message id carries the identical record as `Actor01900Msg7D3`.
typedef struct Actor401800Msg7D3 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor401800Msg7D3;

/// Movement is frozen while this is 1. Same flag `Actor401800_MoveForwardNonzero`
/// and the other families' step helpers test.
extern u8 D_80072729;

/// Pushes `coord` away from the obstacles in `recs`, exactly as
/// `Actor00100_Fn00508` does for actor 00100: records of kind 0x10000 (which
/// also raises the returned `blocked` flag) or 0x30000 each give a bearing, at
/// most eight; bearings more than 0x400 apart cancel each other. Each survivor
/// becomes a 10-unit step added to `pos` and to the coordinate's translation.
s32 func_actor_401800_8013271C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos);
s32 func_actor_401800_80132C68(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2);
/// Nudges a coordinate frame away from the obstacles recorded in a `GpRec18`
/// table: it takes the frame's world position and the point one unit in front
/// of it, sorts the records by bearing, and where two of them close to within
/// 0x400 pushes the frame `push` units along the bisector. Returns 1 when a
/// push was applied. Same body as `RoomsShared80182078`, which six acropolis
/// rooms carry.
s32 func_actor_401800_80132E0C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push);
/// Re-seeds the `rec` contact record the aim-and-rescale body arms for the
/// actor's root coordinate. Same role `func_actor_401300_80132910` plays.
s32 func_actor_401800_8013629C(Actor401800* arg0, GpRec18* rec, s16 count);
/// Returns non-zero while `coord` may still travel `arg1` units of its local Z
/// path; the result is read as a signed halfword (`func_actor_401800_80139118`),
/// the way `func_actor_401300_8013267C` is.
s32 func_actor_401800_80133558(GsCOORDINATE2* coord, s16 arg1, s16 arg2);
s32 func_actor_401800_80133918(Actor401800* arg0);
/// Spawns the impact effect for a hit of magnitude `arg1` on `arg0`, effect id
/// `Gp_GetIdParam1(arg2)`. Same body `Actor00100_Fn03340` runs for actor 00100.
void func_actor_401800_801348A8(Actor401800* arg0, s16 arg1, s32 arg2);
s32  func_actor_401800_8013DCBC(Actor401800* arg0, s32 arg1, Actor401800Msg7D3* arg2);
void func_actor_401800_80133EB8(Actor401800* arg0);
/// Enemy init: allocates the work block, binds the model matrices, sets up both
/// animation contexts and the three hit/body `GpObj` nodes, then picks the
/// starting state and tint row from the spawn flags and rescales the model.
/// Same body as `Actor01900_Fn02018` / `func_actor_401300_80134454`.
void func_actor_401800_8013423C(GpEnemy* enemy, Actor401800* actor);
void func_actor_401800_8013E0A0(Task* task);
void func_actor_401800_8013E138(Actor401800* arg0);
void func_actor_401800_8013E194(Actor401800* arg0);
void func_actor_401800_8013E23C(Actor401800* arg0);
void func_actor_401800_8013E2E8(Actor401800* arg0);
void func_actor_401800_8013E394(Actor401800* arg0);
void func_actor_401800_8013E44C(Actor401800* arg0);
void func_actor_401800_8013E4F0(Actor401800* arg0);

#endif // ACTOR_401800_H
