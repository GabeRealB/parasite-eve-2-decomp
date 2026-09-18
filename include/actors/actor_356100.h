#ifndef ACTOR_356100_H
#define ACTOR_356100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include <psyq/inline_c.h>

/// `mvmva 1, 0, 0, 0, 0`: rotate V0 by the rotation matrix and add the
/// translation vector. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

/// One XZ pair of `Actor356100Work::field_C`; same shape as
/// `Actor01900Waypoint`.
typedef struct Actor356100Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor356100Waypoint;

/// 8-byte row of the `D_actor_356100_8016A994` table `func_actor_356100_8016382C`
/// picks the re-entry pair from on the spawn argument; the same role
/// `Actor01900TintRow` has for `Actor01900_D0AC64`.
typedef struct Actor356100TintRow {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} Actor356100TintRow;
STATIC_ASSERT_SIZEOF(Actor356100TintRow, 0x8);

/// 0xC-byte `G_SCRATCH_HEAD` block `func_actor_356100_8016804C` takes: the
/// offset from the actor to the player, then the clamped turn applied to the
/// root coordinate. Same shape as `Actor401300TurnScratch` /
/// `Actor01900TurnScratch`.
typedef struct Actor356100TurnScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad_A;
} Actor356100TurnScratch;
STATIC_ASSERT_SIZEOF(Actor356100TurnScratch, 0xC);

/// Head of the work block this overlay hangs off `Task::work`. `field_4` is
/// the live-actor flag `func_actor_356100_8016A1D8` tests, where
/// `Actor00100Work::field_4` sits. `field_0` / `field_5A` / `field_68` are the
/// same state, clip-id and flag halfwords `Actor01900Work` keeps at those
/// offsets; `field_974` is the `field_5A & 0x3FF` snapshot
/// `func_actor_356100_8016A468` stores (same role as `Actor01900Work.field_8B4`).
/// The halfwords at 0x978..0x982 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes at 0x898..0x8A2; `field_984` is the halfword
/// `func_actor_356100_8016A5DC` snapshots into `field_982`, where
/// `Actor01900Work.field_8A4` sits, and `field_98E` / `field_990` the zero-pair
/// that function clears at +0x8AE / +0x8B0. `field_9BC` holds the 0x180 it
/// writes; `Actor01900_Fn0AA78` puts the same constant in the neighbouring
/// `GpObj` field it names `field_8C8.field_1C` (+0x8E4), not this one.
/// `field_B5C` / `field_B60` are the two helper tasks the exit callback
/// kills; same pair as `Actor01900Work` at +0xC38 / +0xC3C, without the three
/// `GpObj` nodes that teardown unlinks.
typedef struct Actor356100Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Countdown `func_actor_356100_8016A668` decrements every frame and
    /// tests with `(s16)` — the 0x0F / 0x10 state it picks when the counter
    /// wraps is the transition into the state 0xB / 0xC clip it is running.
    /// Signed, like `Actor01900Work.field_6` / `Actor401300Work.field_6`;
    /// `func_actor_356100_80167584` reads the same slot as a `u16` when it
    /// increments it, so it casts there.
    /* 0x006 */ s16 field_6;
    /// Latch `func_actor_356100_80164158` raises together with the `field_97E = 9`
    /// reseed once the wrapped turn from the actor's own facing to the player's
    /// exceeds 0x200, and tests before raising so the state change fires once.
    /// Cleared on the state-7 entry next to `field_6`; same slot as
    /// `Actor01900Work.field_8`.
    /* 0x008 */ s16 field_8;
    /* 0x00A */ s16 field_A;
    /// The two XZ pairs `func_actor_356100_8016382C` seeds on entering state
    /// 0x10: the model root's X/Z, then the same pair pushed one normalised
    /// unit along the facing. Same role as `Actor01900Work.field_C`.
    /* 0x00C */ Actor356100Waypoint field_C[2];
    /// Zeroed on the same entry, next to the pair above; same slot as
    /// `Actor01900Work.field_14`.
    /* 0x014 */ s16  field_14;
    /* 0x016 */ byte pad_16[0x44];
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x90A];
    /* 0x974 */ s32  field_974;
    /* 0x978 */ s16  field_978;
    /* 0x97A */ s16  field_97A;
    /* 0x97C */ byte pad_97C[2];
    /* 0x97E */ s16  field_97E;
    /* 0x980 */ byte pad_980[2];
    /* 0x982 */ s16  field_982;
    /* 0x984 */ s16  field_984;
    /* 0x986 */ byte pad_986[8];
    /* 0x98E */ s16  field_98E;
    /* 0x990 */ s16  field_990;
    /* 0x992 */ byte pad_992[2];
    /// Snapshot of `field_5A & 0x3FF` `func_actor_356100_80167818` tests
    /// before re-storing it, so the clip-4 branch fires once per change.
    /// Distinct from `field_974`, which the state 0xE tick snapshots.
    /* 0x994 */ s32  field_994;
    /* 0x998 */ byte pad_998[0x24];
    /* 0x9BC */ s16  field_9BC;
    /* 0x9BE */ byte pad_9BE[2];
    /// First `GpRec18` of the body-part record table, the address
    /// `func_actor_356100_8016382C` publishes in the enemy's `field_54` slot
    /// and the exit callback drops. Same slot as `Actor01900Work.field_8E8`.
    /* 0x9C0 */ GpRec18 field_9C0;
    /* 0x9D8 */ byte    pad_9D8[0x80];
    /// Collision record `func_actor_356100_801668FC` hands `func_800E0C10`,
    /// 0x98 past `field_9C0` — one body-part record rather than one `GpRec18`,
    /// the stride `Actor00100Obj` gives the same table in the 00100 overlay.
    /* 0xA58 */ GpRec18 field_A58;
    /* 0xA70 */ byte    pad_A70[0x68];
    /// Light matrix `func_actor_356100_8016382C` binds to the model's
    /// `TmdObject::field_1C` (the color matrix is `field_AF8`).
    /* 0xAD8 */ MATRIX field_AD8;
    /// Color matrix bound to the model's `TmdObject::field_20`.
    /* 0xAF8 */ MATRIX field_AF8;
    /// Copy `func_actor_356100_80167584` saves `field_AF8` into when it enters
    /// its state; same role as `Actor401300Work.field_C68` has for `field_C48`.
    /* 0xB18 */ MATRIX field_B18;
    /* 0xB38 */ byte   pad_B38[2];
    /// Threshold `func_actor_356100_8016A834` tests once the enemy is still
    /// alive (`field_40 > 0`) to choose clip 4 or 0x11; the analogue of the
    /// `Actor00100Ctx.field_4C & 2` bit `Actor00100_Fn0BB2C` tests there.
    /* 0xB3A */ s16  field_B3A;
    /* 0xB3C */ byte pad_B3C[4];
    /// Direction `Gfx_MatrixCol2` takes off the model's root colour-matrix Y
    /// column and `VectorNormalSS` normalises; `func_actor_356100_80165B30`
    /// then GPF-scales it by `field_B52` into its aim scratch, the same
    /// normalise-then-scale pair `Actor01900_MoveForward` runs.
    /* 0xB40 */ SVECTOR field_B40;
    /// The yaw pair `func_actor_356100_801653F4` walks: `field_B48` is the
    /// angle fed to `Gfx_RotMatrixY` and slewed 0x89 at a time toward
    /// `field_B4A`, the target it re-derives each entry as the root's facing
    /// plus twice the wrapped turn. Same slots as `Actor01900Work.field_C20` /
    /// `field_C22`.
    /* 0xB48 */ s16 field_B48;
    /* 0xB4A */ s16 field_B4A;
    /// Push distance `func_actor_356100_8016804C` normalises the root's own
    /// colour-matrix column by and walks down by 0xA per frame until the
    /// state moves on; same role as `Actor401300Work.field_C98`.
    /* 0xB4C */ s16  field_B4C;
    /* 0xB4E */ byte pad_B4E[2];
    /// Side `func_actor_356100_80165B30`'s turn settles on: the zero it draws
    /// once from `Gp_LcgState` at entry, then -1 / 1 for the two clip directions
    /// it alternates each time it re-enters with the same clip.
    /* 0xB50 */ s16 field_B50;
    /// Uniform GTE scale `func_actor_356100_80165B30` applies to the normalised
    /// direction: 0xDE whole while `field_97A` is clear, halved when it is set.
    /* 0xB52 */ u16 field_B52;
    /// Random reload `func_actor_356100_8016A668` adds a 4-bit `Gp_LcgState`
    /// draw to when the work block's `field_4` flag is set.
    /* 0xB54 */ u16 field_B54;
    /// Second half of the pair: the re-entry delay `func_actor_356100_8016382C`
    /// copies off `D_actor_356100_8016A994` next to `field_B54`.
    /* 0xB56 */ u16 field_B56;
    /// Copy of the first three bytes of the last event
    /// `func_actor_356100_8016A0B8` handled.
    /* 0xB58 */ u8    field_B58[3];
    /* 0xB5B */ byte  pad_B5B;
    /* 0xB5C */ Task* field_B5C;
    /* 0xB60 */ Task* field_B60;
    /// Halfword `func_actor_356100_80166CF0` clears after its state entry has
    /// run, next to the `field_B66` / `field_B68` latch pair the state-4 tick
    /// clears; same slot as `Actor401300Work.field_D1C`.
    /* 0xB64 */ s16 field_B64;
    /// Cleared next to `field_B68` on the state-4 entry; same slot as
    /// `Actor401300Work.field_D1E`.
    /* 0xB66 */ s16 field_B66;
    /// Latch `func_actor_356100_801668FC` and `func_actor_356100_80166018`
    /// clear after dispatching message 0x3F1, and set so that the next tick
    /// dispatches it once.
    /* 0xB68 */ s16  field_B68;
    /* 0xB6A */ byte pad_B6A[2];
    /// Ring buffer of the model root's world positions, one slot per frame:
    /// `func_actor_356100_80169854` writes the transformed coordinate there at
    /// the `field_BBC` index and reads the slot back as the enemy's local
    /// position unless `field_97E` is 0x14 or 0x15.
    /* 0xB6C */ SVECTOR field_B6C[7];
    /* 0xBA4 */ byte    pad_BA4[0x18];
    /// Write index into the `field_B6C` ring above; `func_actor_356100_8016382C`
    /// zeroes it on the state-0x10 entry and the per-frame tick wraps it at 7.
    /// Same tail slot as `Actor01900Work.field_C98`.
    /* 0xBBC */ s16  field_BBC;
    /* 0xBBE */ byte pad_BBE[2];
} Actor356100Work;
STATIC_ASSERT_SIZEOF(Actor356100Work, 0xBC0);

/// Event record `func_actor_356100_8016A0B8` dispatches on: the first three
/// bytes are copied raw into `Actor356100Work::field_B58`, `w[0]` is the
/// event kind and `w[1]` its sub-code. Same shape as `Actor401300Event`.
typedef union Actor356100Event {
    u8  b[3];
    u16 w[2];
} Actor356100Event;

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::work` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900` / `Actor401000`. The sibling teardown
/// `func_actor_356100_8016A158` reaches those same slots as a `Task*`.
typedef struct Actor356100 {
    /* 0x00 */ byte pad_0[0x18];
    /// Teardown `func_actor_356100_8016382C` installs; same slot as
    /// `Task::exitCallback` / `Actor01900.field_18`.
    /* 0x18 */ TaskFunc         field_18;
    /* 0x1C */ Actor356100Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /// Event-handler table installed on entry; same slot as
    /// `Task::field_24` / `Actor01900.field_24`.
    /* 0x24 */ void*      field_24;
    /* 0x28 */ byte       pad_28[4];
    /* 0x2C */ TmdObject* field_2C;
    /// State index `func_actor_356100_8016382C` advances once its entry has
    /// run; same slot as `Task::state` / `Actor01900.field_30`.
    /* 0x30 */ s32  field_30;
    /* 0x34 */ byte pad_34[2];
    /// Spawn sub-type `func_actor_356100_8016382C` switches on to pick the
    /// re-entry pair; same slot and role as `Actor01900.field_36`, the high
    /// half of `Task::spawnArg1`.
    /* 0x36 */ s16 field_36;
} Actor356100;

/// Binds the model's light and colour matrices to the pair kept in the work
/// block. Same body as `Actor01900_BindMatrices`.
static __inline__ void Actor356100_BindMatrices(Actor356100* actor)
{
    Actor356100Work* work;
    TmdObject*       obj;

    work          = actor->field_1C;
    obj           = actor->field_2C;
    obj->field_1C = &work->field_AD8;
    obj->field_20 = &work->field_AF8;
}

/// Animation view of the work block above, as `func_actor_356100_801633DC`
/// reads it: the `Actor01900AnimWork` layout 0xE0 bytes later, so the two
/// `GpAnimCtx` blocks at 0x1C / 0x4C8 each sit 0x14 bytes before their
/// 0x28-byte slot array. `field_982` is the clip id the slot loop copies
/// minus 3 into `slots[i].field_9`, `field_98A` the clip written whole into
/// `blendSlots[i].field_9`, and `field_98C` the blend weight, the same three
/// roles `Actor01900AnimWork.field_8A2` / `field_8AA` / `field_8AC` have.
typedef struct Actor356100AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[24];
    /* 0x3F0 */ byte       pad_3F0[0xD8];
    /* 0x4C8 */ GpAnimCtx  blendAnim;
    /* 0x4DC */ GpAnimSlot blendSlots[24];
    /* 0x89C */ byte       pad_89C[0xDC];
    /// The animation-state halfwords `func_actor_356100_80163508` drives, the
    /// same slots `Actor356100Work` names: `field_978` is the state it leaves
    /// at 3 once the 1/2 entry has been served, `field_97C` the clip id the
    /// slots were last seeded with, `field_97E` the one being seeded now and
    /// `field_980` the per-frame counter it bumps. The 0x97C / 0x97E pair is
    /// what makes the state-1 reseed fire once per clip change.
    /* 0x978 */ s16 field_978;
    /* 0x97A */ s16 field_97A;
    /* 0x97C */ s16 field_97C;
    /* 0x97E */ s16 field_97E;
    /* 0x980 */ u16 field_980;
    /* 0x982 */ s16 field_982;
    /* 0x984 */ s16 field_984;
    /// Blend-context state `func_actor_356100_80163508` restarts at 2 with
    /// speed 0x30 / weight 0x800: `field_986` the state, `field_988` the clip
    /// id its slots are reset to, and `field_98A` / `field_98C` the pair the
    /// reset copies into the primary slots and the blend weight.
    /* 0x986 */ s16 field_986;
    /* 0x988 */ s16 field_988;
    /* 0x98A */ s16 field_98A;
    /* 0x98C */ s16 field_98C;
    /// The eased yaw pair `func_actor_356100_80163508` walks: `field_98E` is
    /// the aim the root coordinates are turned to and `field_990` the running
    /// value moved toward it by 0x100 per frame.
    /* 0x98E */ s16  field_98E;
    /* 0x990 */ s16  field_990;
    /* 0x992 */ byte pad_992[2];
    /* 0x994 */ s32  field_994;
} Actor356100AnimWork;

/// Blends pose slots 1..0x14: the first eleven copy the two clip ids into
/// their slot records and are written from both animation contexts with
/// `0x1000 - field_98C` as the blend weight, the rest only tick. Same body as
/// `Actor01900_Fn01950` / `func_actor_403000_801336B4` with this overlay's
/// slot count.
void func_actor_356100_801633DC(Actor356100* arg0);

void func_actor_356100_80163508(Actor356100* arg0);

/// The player's coordinate, as `Actor401300` / `Actor01900` name it. The
/// overlay keeps its own copy like those two do.
extern MATRIX* D_80073B8C;

extern u32 Gp_LcgState;

/// Movement-freeze flag: when it is 1 the root coordinate is left alone, so
/// `func_actor_356100_8016804C`'s two collision pushes are skipped entirely.
/// Same slot and role as `Actor00100_MoveForwardNonzero`'s guard.
extern u8 D_80072729;

/// Two-byte mode pair read by the per-frame tick: `[1] == 1` re-maps the
/// 0x18 state onto 6, and `D_801153F4` below picks the tick's three-arm switch
/// (0 leaves the model flag alone, 1 and 2 clear it outright).
extern u8 D_801153F2[2];
extern u8 D_801153F4;

/// Player-to-`coord` vector, in the 16-bit `SVECTOR` view of both matrices.
static __inline__ void Actor356100_PositionDelta(GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = D_80073B8C->t[0] - coord->coord.t[0];
    pos->vy = D_80073B8C->t[1] - coord->coord.t[1];
    pos->vz = D_80073B8C->t[2] - coord->coord.t[2];
}

/// 0xC-byte 3D squared-radius block walked off `G_SCRATCH_HEAD`, same shape
/// `Actor401300RangeScratch` has. Handed out whole by `Actor356100_OutOfRange`.
typedef struct Actor356100RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor356100RangeScratch;
STATIC_ASSERT_SIZEOF(Actor356100RangeScratch, 0xC);

/// Whether `d` is further than `r` from the origin, compared on squared
/// lengths. Takes the block off `G_SCRATCH_HEAD`, publishes it for the
/// duration of the multiply chain and gives it back, then adds `dx*dx` to
/// `dz*dz` and tests `>= r*r`. Same body as `Actor401300_OutOfRange`; the two
/// `G_SCRATCH_HEAD` stores bracketing the chain are what the scratch block is
/// reserved against (`func_actor_356100_80167818` and the tick above it pass
/// 3000).
static __inline__ s32 Actor356100_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor356100RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor356100RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor356100RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor356100RangeScratch*)(head - 0xC))->dx *= ((Actor356100RangeScratch*)(head - 0xC))->dx;
    *(Actor356100RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor356100RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_356100_80168E44` takes: the
/// offset from the actor to the player, then the facing yaw it settles on.
/// Same shape as `Actor401300AimScratch` / `Actor01900AimScratch`.
typedef struct Actor356100AimScratch {
    /* 0x0 */ SVECTOR delta;
    /// The two facing yaws `func_actor_356100_80164158` compares: `target` is the
    /// player's root facing (`ratan2` of its matrix column) and `current` this
    /// actor's own, re-derived from `delta` and wrapped 0x800 off it. Their
    /// wrapped difference is the turn onto the player, which is what separates
    /// the state-0xA and state-0xB transitions.
    /* 0x8 */ s16 target;
    /* 0xA */ s16 current;
    /* 0xC */ s16 angle;
    /// The root's facing yaw `func_actor_356100_801653F4` reads back to seed
    /// `Actor356100Work::field_B48`; `Actor01900ChaseScratch` names the same
    /// pair `turn` / `angle`.
    /* 0xE */ s16 facing;
} Actor356100AimScratch;
STATIC_ASSERT_SIZEOF(Actor356100AimScratch, 0x10);

/// 0x34-byte `G_SCRATCH_HEAD` block `Actor356100_RescaleYaw` builds its scaled
/// Y rotation in. Same shape as `Actor401300RotScratch` / `Actor01900RotScratch`.
typedef struct Actor356100RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor356100RotScratch;
STATIC_ASSERT_SIZEOF(Actor356100RotScratch, 0x34);

/// 0x14-byte `G_SCRATCH_HEAD` block `func_actor_356100_801668FC` gives
/// `func_800E0C10`: the `GpDeltaScratch` it fills plus the returned flag, set
/// when the X or Z delta is nonzero. Same shape as `Actor01900DeltaFlag` /
/// `Actor00100DeltaFlag`.
typedef struct Actor356100DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor356100DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor356100DeltaFlag, 0x14);

/// 0x68-byte `G_SCRATCH_HEAD` block `func_actor_356100_80169854` takes while it
/// builds the ground coordinate it draws an effect quad on: the coordinate the
/// function fills (`coord.sub` parented to `Gfx_ViewCoord`) plus the world
/// position `v` its two parent walks leave there.
typedef struct Actor356100GroundCoord {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad_50[0x10];
    /* 0x60 */ SVECTOR       v;
} Actor356100GroundCoord;
STATIC_ASSERT_SIZEOF(Actor356100GroundCoord, 0x68);

/// The world root every coordinate chain in this overlay hangs off; same
/// declaration `gameplay/3CD8.h` carries.
extern GsCOORDINATE2 Gfx_ViewCoord;

/// Walks `p` up its parent chain to `Gfx_ViewCoord`, transforming `out` by each
/// coordinate; `out` is left unchanged if the chain ends before the view.
/// Same body as `Actor01900_TransformToView`.
static __inline__ void Actor356100_TransformToView(GsCOORDINATE2* p, SVECTOR* out)
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
            gte_rtv0tr_real();
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

/// Wraps a 12-bit angle difference into `[-0x800, 0x800]`.
static __inline__ s16 Actor356100_NormalizeYaw(s16 input)
{
    s16 value = input;

    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

/// `Actor356100_PositionDelta` against an explicit config block rather than
/// the overlay's own `D_80073B8C` copy of the player coordinate.
static __inline__ void Actor356100_ConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the
/// offset. Same body as `Actor401300_PositionYaw` / `Actor01900_PositionYaw`.
static __inline__ s16 Actor356100_PositionYaw(Actor356100* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;

    Actor356100_ConfigPositionDelta(config, actor->field_2C->field_8, pos);
    coord = actor->field_2C->field_8;
    angle = ratan2(pos->vx, pos->vz);
    return Actor356100_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// `Actor356100_PositionDelta` against an explicit matrix rather than the
/// overlay's own `D_80073B8C` copy of the player coordinate.
static __inline__ void Actor356100_MatrixPositionDelta(MATRIX* m, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = m->t[0] - coord->coord.t[0];
    pos->vy = m->t[1] - coord->coord.t[1];
    pos->vz = m->t[2] - coord->coord.t[2];
}

/// `Actor356100_PositionYaw` towards the translation of `m`. Same body as
/// `Actor401300_MatrixPositionYaw`.
static __inline__ s16 Actor356100_MatrixPositionYaw(Actor356100* actor, SVECTOR* pos, MATRIX* m)
{
    GsCOORDINATE2* coord;
    s32            angle;

    Actor356100_MatrixPositionDelta(m, actor->field_2C->field_8, pos);
    coord = actor->field_2C->field_8;
    angle = ratan2(pos->vx, pos->vz);
    return Actor356100_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Rebuilds `coord`'s Y rotation from the yaw it already faces, uniformly
/// scaled by `scale`. Same body as `Actor401300_RescaleYaw` / `Actor01900_RescaleYaw`.
static __inline__ void Actor356100_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                 scratch;
    void*                  head;
    Actor356100RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor356100RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor356100RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Payload of message 0x3E9 `func_actor_356100_801666B4` sends the player:
/// the player's own position, then the heading away from this actor, so the
/// player ends up moved one normalised unit along that direction. Same shape
/// and roles as `Actor403000Msg3E9`, whose `field_10` / `field_12` are the
/// flag and angle pair its sender writes there.
typedef struct Actor356100Msg3E9 {
    /* 0x00 */ s32  x;
    /* 0x04 */ s32  y;
    /* 0x08 */ s32  z;
    /* 0x0C */ byte pad_C[0x4];
    /* 0x10 */ s16  field_10;
    /// Heading `ratan2` produces from the normalised player-to-actor vector,
    /// where `Actor356100Work::field_5A` holds this actor's own facing.
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[0x2];
} Actor356100Msg3E9;
STATIC_ASSERT_SIZEOF(Actor356100Msg3E9, 0x18);

/// The overlay's only message-0x3E9 instance; all eight words are zero in the
/// image, so it is a work area rather than a table.
extern Actor356100Msg3E9 D_actor_356100_801732B0;

/// Reply buffer `func_actor_356100_80166018` passes with message 0x3F8; only
/// `field_14` is seeded (to 8) before the query. Same shape as
/// `Actor400600Msg3F8`.
typedef struct Actor356100Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor356100Msg3F8;
STATIC_ASSERT_SIZEOF(Actor356100Msg3F8, 0x18);

/// Reply buffer for the message-0x3F8 query above; the six words after it are
/// zero in the image. Same shape as `Actor400600Msg3F8`.
extern Actor356100Msg3F8 D_actor_356100_801732D0;

/// Player-character flag selecting which animation block
/// `func_actor_356100_80166018` points `D_actor_356100_80173244.field_0` at:
/// the second block when it is 1, the first otherwise.
extern s32 D_actor_356100_80173228;
extern s32 D_actor_356100_80173230;

/// `s8` selector for those two blocks; same slot and role as
/// `Actor403100AnimTable`'s base index.
extern s8 D_8007218A;

/// Zeroed word `func_actor_356100_80167818` clears when the actor goes live.
/// The 0x74 bytes after it are zero in the image too, so the whole run is a
/// work area rather than a table.
extern s32 D_actor_356100_801731B0;

/// Effect record `func_actor_356100_80167818` fills for `func_800FDB18`:
/// coordinate index 5 of the model, scale 0x100 and count 2. Same shape and
/// roles as `Actor401300Work.field_910`.
extern GpEffArg D_actor_356100_801732A8;

/// Animation bank `func_800B3F84` seeds both of the work block's slot arrays
/// from. Same role as `Actor01900_D17174`.
extern u8 D_actor_356100_801730B8[];

/// Enemy descriptor `func_actor_356100_8016382C` publishes in the enemy's
/// `field_50` slot and takes `field_40` off. Same role as `Actor01900_D0AC54`.
extern GpPairSrcE D_actor_356100_8016A984;

/// The three rows `func_actor_356100_8016382C` picks its re-entry pair from on
/// the spawn sub-type. Same role as `Actor01900_D0AC64`.
extern Actor356100TintRow D_actor_356100_8016A994[];

/// Event-handler table `func_actor_356100_8016382C` hands the task as
/// `Task::field_24`. Same shape and role as `Actor01900_D1728C`.
extern void* D_actor_356100_80173258;

/// Initialisation for the state-0x10 clip run: allocates the work block, binds
/// the light / colour matrices, re-seeds the enemy descriptor and both
/// animation contexts, copies the model root's XZ pair into the work block and
/// rebuilds the root's Y rotation as a uniform 0x1194 scale. The spawn
/// sub-type picks the clip and re-entry pair, and the finished entry advances
/// the state.
void func_actor_356100_8016382C(GpEnemy* enemy, Actor356100* actor);

/// Separation tick: when the work block's `field_4` flag is set, pushes this
/// actor one normalised unit away from the player along the player-to-actor
/// direction in XZ (recentring it on the player first), clears the model's
/// root `flg`, and sends the player message 0x3E9 with its own position and
/// the resulting heading. Bit 0 of `field_68` then forces `field_0` to 0xD.
void func_actor_356100_801666B4(Actor356100* arg0);

/// Approach tick, and the sibling of `func_actor_356100_80167584` above it. Going
/// live clears `D_actor_356100_801731B0` and re-seeds the animation slots at
/// clip 2 / speed 0x10 with the enemy's link node cleared; otherwise a single
/// sound 0x51030008 is queued the first time through, keyed on the enemy's
/// `field_8 >> 12` bank. Each frame then snapshots `field_5A & 0x3FF` into
/// `field_994`, and the frame that first lands on clip 4 spawns the
/// `D_actor_356100_801732A8` effect at model coordinate 5. Once the player is
/// further than 3000 away it plays 0x51030008 as a type-7 event and enters
/// state 6. Same shape as `func_actor_401300_801397F8`.
void func_actor_356100_80167818(Actor356100* arg0);

/// Event handler: copies the event's first three bytes into the work block's
/// `field_B58`, then dispatches on `w[0] == 0xB05` and `w[1]` — sub-code 1
/// puts the actor in state 0x1E, 0 and 2 in state 0. Anything else returns 0.
s32 func_actor_356100_8016A0B8(Actor356100* arg0, s32 arg1, Actor356100Event* arg2);

/// `Task::exitCallback` teardown: kill the two helper tasks, drop the
/// enemy's `field_54` slot, then `Gp_DestroyEnemy`. Same shape as
/// `Actor01900_Fn0A6CC` without the three `Gp_UnlinkObj` calls.
void func_actor_356100_8016A158(Task* task);

/// When the work block's `field_4` flag is set, flags the enemy's link node
/// and raises bit 0x80 of the model's `field_C`. Same shape as
/// `ActorsShared80164c20` / `Actor00100_Fn0B4D8` without extra flag masks.
void func_actor_356100_8016A1D8(Actor356100* arg0);

/// When the work block's `field_4` flag is set, clears the enemy's link node,
/// reallocates the model buffers and writes the 0x978..0x982 animation
/// slots; otherwise clears the model's root `flg`. Same shape as
/// `Actor01900_Fn0A7C0` without the two `GpObj` flag masks.
void func_actor_356100_8016A21C(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 3`.
void func_actor_356100_8016A2AC(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A340(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A3D4(Actor356100* arg0);

/// Message 0x3FF payload `func_actor_356100_8016A468` sends the slot-3 task.
/// `field_0` points at `D_actor_356100_80173228`; the function overwrites
/// `field_4` with 2 before the dispatch.
extern GpAnimArg D_actor_356100_80173244;

/// When the work block's `field_4` flag is set, writes the 0x978..0x982
/// animation slots, sends message 0x3FF then 0x3F9 at slot 3, and snapshots
/// `field_5A & 0x3FF` into `field_974`. Bit 1 of `field_68` forces `field_0`
/// to 0xE.
void func_actor_356100_8016A468(Actor356100* arg0);

/// When the work block's `field_4` flag is set, clears the model's `field_C`,
/// clears the enemy's link node and writes the 0x978..0x982 animation slots
/// with `field_9BC` forced to 0x180. Bit 0 of `field_68` forces `field_0` to 7.
/// Same shape as `Actor01900_Fn0AA78` without its two `GpObj` flag masks.
void func_actor_356100_8016A5DC(Actor356100* arg0);

/// Per-frame tick run under the death-throes clip 0xB / 0xC pair.
void func_actor_356100_80163CD4(Actor356100* arg0);

/// Per-frame tick of the state-6 clip run.
void func_actor_356100_80163E2C(Actor356100* arg0);

/// Per-frame tick of the state-7 clip run.
void func_actor_356100_80164158(Actor356100* arg0);

/// Per-frame tick of the state-8 clip run.
void func_actor_356100_80164ACC(Actor356100* arg0);

/// Turn tick that slews the root yaw 0x89 at a time onto `field_B4A`.
void func_actor_356100_801653F4(Actor356100* arg0);

/// Aim tick: normalises a root colour-matrix column and GPF-scales it by
/// `field_B52` into the aim scratch.
void func_actor_356100_80165B30(Actor356100* arg0);

/// Tick of the state-0xB aim run.
void func_actor_356100_80166018(Actor356100* arg0);

/// Tick that hands `func_800E0C10` the `field_A58` collision record.
void func_actor_356100_801668FC(Actor356100* arg0);

/// Tick that dispatches message 0x3F1 and clears the `field_B68` latch.
void func_actor_356100_8016A550(Actor356100* arg0);

/// Tick that decrements `field_6` and reloads it from `field_B54` plus a
/// 4-bit `Gp_LcgState` draw.
void func_actor_356100_8016A668(Actor356100* arg0);

/// Tick that runs the `field_978` clip and halves `field_982` once the actor
/// is no longer live.
void func_actor_356100_80166CF0(Actor356100* arg0);

/// Tick of the state-0x13 clip run.
void func_actor_356100_8016A710(Actor356100* arg0);

/// Tick that picks clip 4 or 0x11 off `field_B3A` once the enemy is still
/// alive.
void func_actor_356100_8016A834(Actor356100* arg0);

/// Per-frame tick of the state-0x15 clip run.
void func_actor_356100_80167358(Actor356100* arg0);

/// Per-frame tick of the state-0x16 clip run.
void func_actor_356100_80167584(Actor356100* arg0);

/// Per-frame tick of the state-0x18 clip run.
void func_actor_356100_80167A7C(Actor356100* arg0);

/// Per-frame tick of the state-0x19 clip run.
void func_actor_356100_801684F0(Actor356100* arg0);

/// Tick that pushes the actor off any collision record and turns it onto the
/// player.
void func_actor_356100_8016804C(Actor356100* arg0);

/// Turn tick that slews the root yaw onto the player 0x28 at a time.
void func_actor_356100_80168AFC(Actor356100* arg0);

/// Turn tick that slews the root yaw onto the player in one step and rescales
/// the root coordinate to 0x1194.
void func_actor_356100_80168E44(Actor356100* arg0);

/// The death-throes tick: runs the per-frame clip, walks part 1's coordinate
/// and fires the 0x600FB effect burst over the model's part coordinates.
void func_actor_356100_80169180(Actor356100* arg0);

/// One entry of the state dispatch table `D_actor_356100_80161EC4`; every
/// handler takes the actor alone.
typedef void (*Actor356100StateFn)(Actor356100* arg0);

/// The 31-entry state table the per-frame tick copies onto its own stack
/// before dispatching `f[work->field_0]`. Entry 0x1D is a null hole.
typedef struct Actor356100StateTable {
    /* 0x00 */ Actor356100StateFn f[31];
} Actor356100StateTable;
STATIC_ASSERT_SIZEOF(Actor356100StateTable, 0x7C);

/// Draws the ground-shadow quad at `arg0` (a coordinate's `workm.t`), shaded
/// by `arg2` and scaled by `arg1`.
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// The per-frame tick: rebinds the model colour matrix from part 1's world
/// translation, draws the ground quad under the model unless the actor is
/// already dying, walks part 1's parent chain into a scratch coordinate and
/// draws the second quad there, advances the state and dispatches it through
/// the table copy, then walks part 2's chain and rings the result into
/// `Actor356100Work::field_B6C` as the enemy's next local position.
void func_actor_356100_80169854(GpEnemy* arg0, Actor356100* arg1);

#endif
