#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>
#include <psyq/stdio.h>

#include "actors/actor.h"
#include "actors/actors_shared_8013411c.h"
#include "actors/actors_shared_80135a60.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// 0x2C-byte scratch frame `func_actor_110600_80133778` opens on
/// `G_SCRATCH_HEAD` to lay one patrol node out: `m` receives a copy of the
/// walker coordinate's matrix, `v` the facing column `Gfx_MatrixCol2` reads
/// out of it once it has been rotated and scaled by the GTE, and `i` the node
/// index the two loops below walk.
typedef struct Actor110600TsvScratch {
    /* 0x00 */ SVECTOR v;
    /* 0x08 */ MATRIX  m;
    /* 0x28 */ s16     i;
    /* 0x2A */ byte    pad_2A[0x2];
} Actor110600TsvScratch;
STATIC_ASSERT_SIZEOF(Actor110600TsvScratch, 0x2C);

/// Returns the patrol node nearest the walker: the squared XZ distance between
/// each node and the low halfwords of the walker coordinate's translation,
/// with the running best and the cursor staged in an `OverlayWalkerNearScratch`.
u8 func_actor_110600_80132958(OverlayWalker* work);

/// Returns the node the walker's route cursor steps onto, reseeding the scan's
/// stored node byte for the `actor` variant of the walker. Same body as the
/// acropolis bridge room's `func_acropolis_bridge_801843A0`.
u8 func_actor_110600_801327EC(OverlayWalker* work, s32 actor);

/// Re-resolves the walker's patrol node against the route's byte table once
/// the state or the node bytes have moved. Same body as the acropolis bridge
/// room's `func_acropolis_bridge_80184638`.
void func_actor_110600_80132A84(OverlayWalker* work, s16 actor);

/// One per-frame behaviour step the walker runs while its `field_6C` gate is
/// clear: steps it toward its current patrol node. `func_800E0C10` produces the
/// 16.16 delta; the high half of each component becomes the whole-unit step,
/// rounded away from zero whenever a fraction is left over. While `field_6B` is
/// set the walker is pinned vertically, otherwise Y also carries a constant
/// 0x10 fall. Y is applied in three bands: a +8 hop above 0x20, a -0x20 drop
/// below -0x20, and the plain step in between. `moving` records whether the
/// frame produced any XZ motion at all. Same body as the acropolis bridge
/// room's `func_acropolis_bridge_80184908`.
void func_actor_110600_80132D54(OverlayWalker* work);

/// The second per-frame behaviour step, gated on `field_6D`. Same body as the
/// acropolis bridge room's `func_acropolis_bridge_80184B94`.
void func_actor_110600_80132FE0(OverlayWalker* work);

/// Turns the walker towards `pos` by at most `field_5A` angle units a frame.
/// The wrapped relative bearing drives the consecutive-turn counter, then
/// becomes the absolute yaw the model's saved scale matrix is rebuilt around.
void func_actor_110600_80133550(OverlayWalker* work, SVECTOR3* pos);

/// Event record `func_actor_110600_80134040` dispatches on: `w[0]` is the event
/// kind (0x301, 0x401) and `w[1]` its sub-code, and the first three bytes are
/// also copied raw into `Actor110600Work::field_BDC`. Same shape as
/// `Actor401300Event`, which is the same body one overlay over.
typedef union Actor110600Event {
    u8  b[3];
    u16 w[2];
    u32 raw;
} Actor110600Event;

/// Work block this overlay parks in the task's `Task::work` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_110600_80134AB4` allocates it
/// with `memCalloc(0xBEC, 0)`, so the size below is the allocation.
///
/// `field_0` is the state index `func_actor_110600_801387C0` writes. `field_4`
/// is the live-actor flag `func_actor_110600_801388A4` tests, and
/// `field_A90.flags` / `field_950.flags` are the two masks it writes. The three
/// `GpObj` display nodes are the ones the spawn handler links (with `GpRec18`
/// tables of 5 / 12 / 1 records filling the gaps) and the exit callback
/// `func_actor_110600_801387F4` hands back to `Gp_UnlinkObj`. `field_BD4` /
/// `field_BD8` are optional helpers the spawn clears; teardown increments
/// `Task::state` when they are non-NULL.
typedef struct Actor110600Work {
    /* 0x000 */ s16 field_0;
    /// Second halfword of the state word above: the `0x7D3` display handler
    /// clears it to -1 whenever it parks the actor in state 0x11.
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// The `field_5C`-style tick `func_actor_110600_80138A70` counts down once
    /// the actor is live, and reseeds from `Gp_LcgState` when `field_4` is set.
    /* 0x006 */ u16  field_6;
    /* 0x008 */ s16  field_8;
    /* 0x00A */ byte pad_A[0x2];
    /// The two hit-sphere radii the aiming stage measures the player
    /// delta against in the XZ plane: `field_C` only while the target is
    /// within 0x3E8 angle units of the model's heading, `field_E` always.
    /* 0x00C */ u16  field_C;
    /* 0x00E */ u16  field_E;
    /* 0x010 */ byte pad_10[0x3E];
    /// Pose the model has reached, the same `& 0x3FF` frame index
    /// `func_actor_110600_80137DB0` tests for 4 before it advances the
    /// `field_BE2` stage from 0 to 1.
    /* 0x04E */ u16  field_4E;
    /* 0x050 */ byte pad_50[0xC];
    /// Flag halfword the state handlers test on entry: bit 0 moves the actor
    /// on (`field_0 = 3`), bit 1 is the timer gate
    /// `func_actor_110600_80136888` retimes on.
    /* 0x05C */ u16  field_5C;
    /* 0x05E */ byte pad_5E[0x82A];
    /* 0x888 */ s32  field_888;
    /* 0x88C */ s16  field_88C;
    /* 0x88E */ s16  field_88E;
    /* 0x890 */ byte pad_890[2];
    /* 0x892 */ s16  field_892;
    /* 0x894 */ s16  field_894;
    /* 0x896 */ s16  field_896;
    /* 0x898 */ u16  field_898;
    /* 0x89A */ s16  field_89A;
    /* 0x89C */ s16  field_89C;
    /* 0x89E */ byte pad_89E[4];
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ u16  field_8A6;
    /* 0x8A8 */ byte pad_8A8[2];
    /// Timer `func_actor_110600_80137F2C` counts down once the actor is live,
    /// handing off to `func_actor_110600_80136210` on the frame it reaches
    /// zero; the same slot the `Actor01900Work` dispatcher keeps at 0xC10.
    /* 0x8AA */ s16 field_8AA;
    /* 0x8AC */ s32 field_8AC;
    /* 0x8B0 */ s8  field_8B0;
    /// Live flag `func_actor_110600_80137F2C` raises once the model root's
    /// coordinate has been cleared at the end of the tick.
    /* 0x8B1 */ s8   field_8B1;
    /* 0x8B2 */ byte pad_8B2[2];
    /* 0x8B4 */ s16  field_8B4;
    /// Per-state walk speed the spawn handler seeds (0xA / 8 / 0xE); the aiming
    /// stage hands it to the walker as its `field_5E` ramp target.
    /* 0x8B6 */ u16   field_8B6;
    /* 0x8B8 */ GpObj field_8B8;
    /// The `GpRec18` table the spawn handler links behind `field_8B8`: five
    /// records, cleared first by the tick's teardown.
    /* 0x8D8 */ GpRec18 recs_8D8[5];
    /// The second display node, whose `pos`
    /// carry a model position: the spawn handler seeds the three halfwords
    /// from a stack `SVECTOR` and the tick restamps the model root's
    /// translation onto them every frame.
    /* 0x950 */ GpObj field_950;
    /// The walker's own collision table, the twelve records its `recs` pointer
    /// names; cleared alongside the two tables around it.
    /* 0x970 */ GpRec18 recs_970[12];
    /* 0xA90 */ GpObj   field_A90;
    /// The actor's own `GpRec18` table, the one the spawn handler links behind
    /// `field_A90` and initialises with a count of **one** record, so it spans
    /// 0xAB0..0xAC8 and the three matrices below start where it ends.
    /// `func_actor_110600_80135B84` reads the only record's `key` as the
    /// 0x10000 kind tag the sound cue is gated on.
    /* 0xAB0 */ GpRec18 recs[1];
    /// The light matrix the spawn handler binds to the model object's
    /// `field_1C`.
    /* 0xAC8 */ MATRIX field_AC8;
    /// The colour matrix the spawn handler binds to the model object's
    /// `field_20`; the tick shifts its translation down by the shrink amount
    /// `field_BE4` as the actor dies.
    /* 0xAE8 */ MATRIX field_AE8;
    /// Saved copy of `field_AE8` the state-12 handler `func_actor_110600_80136B20`
    /// swaps in and out around each `ScaleMatrix` call.
    /* 0xB08 */ MATRIX            field_B08;
    /* 0xB28 */ OverlayWalker     walker;
    /* 0xBBC */ OverlayWalkerNode field_BBC[2];
    /* 0xBCC */ u8                field_BCC[4];
    /* 0xBD0 */ u8                field_BD0[4];
    /* 0xBD4 */ Task*             field_BD4;
    /* 0xBD8 */ Task*             field_BD8;
    /// Copy of the first three bytes of the last event
    /// `func_actor_110600_80134040` handled.
    /* 0xBDC */ Actor110600Event field_BDC;
    /* 0xBE0 */ s16              field_BE0;
    /// Death-shrink stage: `func_actor_110600_80137DB0` runs its idle half at
    /// 0 and its halving tail at 1, advancing from 0 once `field_4E` lands on
    /// pose 4.
    /* 0xBE2 */ s16  field_BE2;
    /* 0xBE4 */ s16  field_BE4;
    /* 0xBE6 */ s16  field_BE6;
    /* 0xBE8 */ s16  field_BE8;
    /* 0xBEA */ byte pad_BEA[2];
} Actor110600Work;
STATIC_ASSERT_SIZEOF(Actor110600Work, 0xBEC);

/// Animation view of the same block `Actor110600Work` describes: a primary
/// `GpAnimCtx` at 0x10 and a blend context at 0x44C, each followed by its own
/// 24-entry `GpAnimSlot` array (0x24 / 0x460). `func_actor_110600_80134438`
/// drives both for clip ids 1..0x12: the blend weight is the halfword at 0x8A0,
/// the two clip ids the bytes at 0x896 / 0x89E. Same view as
/// `Actor403000AnimWork`, which is the same body one overlay over; the two
/// views overlap `Actor110600Work` because the overlay reads the block through
/// whichever shape a function needs.
///
/// `func_actor_110600_80134728` drives this view's slots and contexts and reads
/// the state words below it through the same pointer, so the view carries the
/// state halfwords `Actor110600Work` names too: `field_88C` is the stage it
/// dispatches on, `field_890` / `field_892` the clip pair `func_800B4114` is
/// armed with, `field_894` the frame counter it bumps, and `field_8AC` the
/// word its stage setup clears.
typedef struct Actor110600AnimWork {
    /* 0x000 */ byte       pad_0[0x10];
    /* 0x010 */ GpAnimCtx  anim;
    /* 0x024 */ GpAnimSlot slots[24];
    /* 0x3E4 */ byte       pad_3E4[0x68];
    /* 0x44C */ GpAnimCtx  blendAnim;
    /* 0x460 */ GpAnimSlot blendSlots[24];
    /* 0x820 */ byte       pad_820[0x6C];
    /* 0x88C */ s16        field_88C;
    /* 0x88E */ s16        field_88E;
    /* 0x890 */ s16        field_890;
    /// Clip id the slots are armed with; read as an unsigned halfword into the
    /// clamped halfword `field_890` is assigned from.
    /* 0x892 */ s16 field_892;
    /* 0x894 */ u16 field_894;
    /// Clip id written into every slot's `field_9`, read as a byte.
    /* 0x896 */ s16  field_896;
    /* 0x898 */ byte pad_898[2];
    /* 0x89A */ s16  field_89A;
    /// Blend-animation clip id, which the `field_89A == 2` stage arms the blend
    /// slots with.
    /* 0x89C */ s16  field_89C;
    /* 0x89E */ s16  field_89E;
    /* 0x8A0 */ s16  field_8A0;
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ byte pad_8A6[6];
    /* 0x8AC */ s32  field_8AC;
} Actor110600AnimWork;
STATIC_ASSERT_SIZEOF(Actor110600AnimWork, 0x8B0);

/// Damage and hit-direction values in the 0x30-byte scratchpad frame used by
/// func_actor_110600_80136210.
typedef struct Actor110600HitScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR direction;
    /* 0x18 */ SVECTOR point;
    /* 0x20 */ s32     key;
    /* 0x24 */ u32     damage;
    /* 0x28 */ s32     distance;
    /* 0x2C */ s16     angle;
    /* 0x2E */ s16     pad;
} Actor110600HitScratch;

STATIC_ASSERT_SIZEOF(Actor110600HitScratch, 0x30);

extern u16 D_actor_110600_80138F18;

/// The actor's state handlers, indexed by `Actor110600Work::field_0`.
/// `func_actor_110600_80137F2C` copies the table to its frame before
/// dispatching, the same local jump table `Gp_EnemyDispatch` builds for the
/// shared `Gp_EnemyWaitFuncs`; four of the 25 slots are still unused.
typedef struct Actor110600StateTable {
    TaskFunc fn[0x19];
} Actor110600StateTable;
STATIC_ASSERT_SIZEOF(Actor110600StateTable, 0x64);

extern const Actor110600StateTable D_actor_110600_80131F3C;

/// One of the model objects `func_actor_110600_80134040` parks in the four
/// display slots below. They live in main's data; this overlay only ever takes
/// their addresses, so their layout is not modelled here.
typedef struct Actor110600Display Actor110600Display;

extern Actor110600Display D_8015BD7C;
extern Actor110600Display D_8015C064;
extern Actor110600Display D_8015C950;
extern Actor110600Display D_8015D2E8;

/// Twelve `SVECTOR` hit positions `func_actor_110600_80135E20` picks from by
/// damage magnitude. The fourth halfword (`pad`, unused by the effect) is the
/// model part index the spawned effect anchors to.
extern SVECTOR D_actor_110600_801485C4[12];

/// The actor's four display slots, which the 0x401 events repoint at one of the
/// objects above; `D_actor_110600_80148598` is the one events 2 and 6 swap.
extern Actor110600Display* D_actor_110600_80148594;
extern Actor110600Display* D_actor_110600_80148598;
extern Actor110600Display* D_actor_110600_8014859C;
extern Actor110600Display* D_actor_110600_801485A0;

/// Event handler: saves the event's first three bytes in the work block's
/// `field_BDC`, then dispatches on the event kind. Kind 0x301 with sub-code 1
/// enters state 0x14; kind 0x401 picks a display slot and a `field_892` state
/// per sub-code — 1, 8 and 9 only set the state, and 9 shares its tail with the
/// five sub-codes that repoint a slot — parking the actor in state 0x11 with
/// `field_2` cleared. Returns 1 when it handled the event, 0 otherwise. `arg1`
/// is unused; it exists because the dispatch passes three arguments.
s32 func_actor_110600_80134040(Task* arg0, s32 arg1, Actor110600Event* arg2);

/// The `0x7D3` display handler: parks the actor in state 0x11 with
/// `field_892` set from the requested state.
s32 func_actor_110600_8013839C(Task* arg0, s32 arg1, GpAnimArg* arg2);

/// Rebuilds `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled independently on each axis through a
/// 0x34-byte block borrowed from the scratchpad. Marks the coordinate dirty.
void func_actor_110600_80138680(GpCoord* coord, s16 sx, s16 sy, s16 sz);

s32  func_actor_110600_801387C0(Task* arg0);
void func_actor_110600_801388A4(Task* arg0);

/// The remaining entries of `D_actor_110600_80131F3C` that are still only
/// present as assembly, so that the table can name them. `func_actor_110600_80135A18`
/// (state 4) and the ones above carry their own documentation.
void func_actor_110600_80135454(Task* arg0);
void func_actor_110600_80136B20(Task* arg0);
void func_actor_110600_801372CC(Task* arg0);
void func_actor_110600_801377FC(Task* arg0);
void func_actor_110600_80138980(Task* arg0);
void func_actor_110600_80138A70(Task* arg0);
void func_actor_110600_80138AFC(Task* arg0);
void func_actor_110600_80138BD0(Task* arg0);

/// The actor's per-tick model update, driven from `Task::work` /
/// `Task::spawnArg2` off the pointer it is handed.
void func_actor_110600_80134728(Task* arg0);

/// Reports the sound cue the model is currently owed — 0 while there is none —
/// which `func_actor_110600_80134728` queues as the id's low byte. Watches the
/// pose of the animation slot the `field_892` state selects, reporting the cue
/// once per pose and remembering it in `field_8AC`.
s32 func_actor_110600_80134564(Actor110600AnimWork* anim);

/// Aiming stage: wraps the yaw from the model's root coordinate to the camera
/// target `Player_Status.coordMtx` against the coordinate's own yaw into `field_8A2`, ticks
/// the model, and moves the actor to state 3 once the walker's `field_5C` bit 0
/// arrives.
void func_actor_110600_80135A18(Task* arg0);

/// Picks one of twelve hit positions out of `D_actor_110600_801485C4` by damage
/// magnitude `arg1`, then spawns effect `Gp_GetIdParam1(arg2)` on the model
/// part that entry names.
void func_actor_110600_80135E20(Task* arg0, s16 arg1, s32 arg2);

/// Per-tick walker step: advances the animation the `field_68` byte selects,
/// resolves the patrol node the `field_6E` byte names against `D_80073B08`,
/// and ramp-scales the model matrix between `field_5E` and `field_5C`.
void func_actor_110600_80133A94(OverlayWalker* walker);

/// Measures the walker's node against the coordinate it is moving towards,
/// leaving the three per-axis deltas in the scratchpad, and reports whether it
/// has arrived: 1 while the delta is inside either of two radii -- the walker's
/// own `field_5C * 4`, or a flat 300 -- and 0 once it is outside both.
s16 func_actor_110600_80132470(OverlayWalker* walker);

/// Steers the walker along its patrol route: resolves the node the route
/// cursor names, and on the frame `func_actor_110600_80132470` reports arrival
/// it raises the route's `arrived` flag, clears the movement deltas and steps
/// the cursor onto the next node — wrapping back to the first at the 0xFF
/// terminator. `pos` receives the position of the node it is heading for, so
/// on the arrival frame it already describes the new node.
void func_actor_110600_80132654(OverlayWalker* work, SVECTOR3* pos);

/// Enters work state 2 (`field_88C`) on a live actor: clear the model object,
/// clear bit 0x8000 of `field_A90.flags` and set 0x4000 of `field_950.flags`,
/// tag the enemy's link node, arm the `field_892` / `field_896` timers, then run
/// 20 update ticks before parking `field_896` at -8 and ticking once more.
void func_actor_110600_80138CA4(Task* arg0);

/// Re-enters work state 2 on a live actor: clear the model root coordinate,
/// re-allocate its TMD buffers, tag the enemy's link node, arm `field_88C` /
/// `field_892` / `field_896` and the `field_950.flags` 0x4000 / `field_A90.flags`
/// 0x8000 masks, then tick twice. On a dead one it is the model-shrink tail:
/// halves `field_896` each tick — parking at -0x10 when the halving lands on 1
/// and bouncing -1 back to 0x10 — and once `Gp_TickObjFlag2` reports 1, drops
/// bit 1 of the enemy node's flags and moves the actor to state 3.
void func_actor_110600_80138D7C(Task* arg0);

/// Placement opcode: seeds the model's root coordinate from `placement`, then
/// rebuilds and rescales it from the actor's own heading.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, GpXformArg* placement);

/// Five-frame shake counter. Incremented each call, wraps at 5, and drives
/// `Display_ClampField126` with the low bit (0 or 1). Returns 1 on wrap.
extern s16 D_actor_110600_8014865C;
s32        func_actor_110600_80138900(void);

/// Recoil push stage `func_actor_110600_80137AF4` indexes for the speed it
/// moves the actor by, and bumps once that push has landed. Reset to 0 first,
/// so the push only starts on the frame a live actor arrives.
extern s16 D_actor_110600_80148688;

/// Argument record `func_actor_110600_80135E20` fills for `func_800FDB18`:
/// model part 1's coordinate, scale 0x100 and count 3.
extern GpEffArg D_actor_110600_80148698;

/// `Task::exitCallback` installed by the spawn handler: bump the two helper
/// tasks' `state` if present, unlink the three display nodes, drop the enemy's
/// `recs` slot, clear the screen shake, then `Gp_DestroyEnemy`.
void func_actor_110600_801387F4(Task* task);

/// Global freeze flag the walker's turn step bails out on: 1 while the game is
/// paused.
extern u8 D_80072728;

/// Table of 0x80-byte actor config blocks the walker's `field_6E` byte indexes
/// for the position state 1 steers towards.
extern PlayerStatus D_80073B08[];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Applies a rotation of `angle` about Y to `matrix` (main executable).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// The complaint the route re-plan prints when the two node lists share no
/// slot at all. The string is spelled out rather than left a literal so the
/// re-plan reaches it by name, the way the original object does.
const char D_actor_110600_80131E24[] = "s->root_cnt == 0xff about \n";

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it. The actor's body update turns two joints of its
/// model with it, the second by a quarter of the angle.
void func_actor_110600_80131FC0(GpCoord* coord, s16 yaw)
{
    MATRIX*  rotation;
    GpCoord* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Whole-unit step `func_actor_110600_801322CC` last applied to its coordinate.
extern SVECTOR D_actor_110600_80148690;

/// Moves `coord` in X/Z by the push the first `count` records of `movement`
/// resolve to through `func_800E0C10`, rounding a fractional part away from
/// zero, and stores the whole-unit step taken in `D_actor_110600_80148690`.
/// Returns 1 when the X or Z delta is nonzero. Nothing in the actor calls it.
s32 func_actor_110600_801322CC(GpCoord* coord, GpRec18* movement, s16 count)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    p                              = head - 0x14;
    s                              = p;
    SCRATCH_HEAD_AT(scratch, void) = p;
    s->moved                       = 0;
    if (func_800E0C10(movement, &s->delta, (s32)count, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_110600_80148690.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_110600_80148690.vy = s->delta.vy.w >> 16;
        D_actor_110600_80148690.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_110600_80148690.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_110600_80148690.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_110600_80148690.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_110600_80148690.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

s16 func_actor_110600_80132470(OverlayWalker* walker)
{
    OverlayWalkerArrivalDelta* d;
    u8*                        head;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x8;
    d                = (OverlayWalkerArrivalDelta*)(head - 0x8);

    d->x = walker->nav->nodes[walker->node].x;
    d->y = walker->nav->nodes[walker->node].y;
    d->z = walker->nav->nodes[walker->node].z;
    d->x = d->x - *(u16*)&walker->coord->coord.t[0];
    d->y = 0;
    d->z = d->z - *(u16*)&walker->coord->coord.t[2];

    if (!overlayWalkerOutOfRange(d, walker->field_5C * 4) ||
        !overlayWalkerOutOfRange(d, 300)) {
        SCRATCH_POP_BYTES(0x8);
        return 1;
    }
    SCRATCH_POP_BYTES(0x8);
    return 0;
}

void func_actor_110600_80132654(OverlayWalker* work, SVECTOR3* pos)
{
    OverlayWalkerRoute* route;
    OverlayWalkerRoute* step;
    OverlayWalkerRoute* wrap;
    OverlayWalkerRoute* next;
    u8                  node;

    route      = work->route;
    work->node = route->nodes[route->cursor];
    if (func_actor_110600_80132470(work) == 0) {
        pos->vx              = work->nav->nodes[work->node].x;
        pos->vy              = work->nav->nodes[work->node].y;
        pos->vz              = work->nav->nodes[work->node].z;
        work->route->arrived = 0;
        return;
    }

    work->route->arrived = 1;
    step                 = work->route;
    work->field_62       = 0;
    work->field_64       = 0;
    step->cursor++;

    wrap = work->route;
    if (wrap->nodes[wrap->cursor] == 0xFF) {
        wrap->cursor = 0;
    }

    next       = work->route;
    node       = next->nodes[next->cursor];
    work->node = node;
    pos->vx    = work->nav->nodes[node].x;
    pos->vy    = work->nav->nodes[work->node].y;
    pos->vz    = work->nav->nodes[work->node].z;
}

/// Scans the walker's patrol node table for the node nearest actor `actor` and
/// returns its index. Same scan as `func_actor_110600_80132958`, but measured
/// from the translation of the actor config's matrix rather than from the
/// walker's own coordinate; the walker uses it with the player (entry 1) to
/// pick the node it retreats to.
u8 func_actor_110600_801327EC(OverlayWalker* work, s32 actor)
{
    OverlayWalkerNearCfgScratch* block;
    u8*                          head;
    s16                          dz;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x18;
    block            = (OverlayWalkerNearCfgScratch*)SCRATCH_HEAD(u8);

    block->cfg  = &D_80073B08[(s16)actor];
    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = *(u16*)&block->cfg->coordMtx->t[0] - work->nav->nodes[block->node].x;
        block->dy   = *(u16*)&block->cfg->coordMtx->t[1] - work->nav->nodes[block->node].y;
        dz          = *(u16*)&block->cfg->coordMtx->t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    SCRATCH_POP_BYTES(0x18);
    return block->nearest;
}

/// Returns the patrol node nearest the walker: the squared XZ distance between
/// each node and the low halfwords of the walker coordinate's translation,
/// with the running best and the cursor staged in a scratch block. Same body
/// as the acropolis bridge room's `func_acropolis_bridge_8018450C`.
u8 func_actor_110600_80132958(OverlayWalker* work)
{
    OverlayWalkerNearScratch* block;
    u8*                       head;
    s16                       dz;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x14;
    block            = (OverlayWalkerNearScratch*)SCRATCH_HEAD(u8);

    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = *(u16*)&work->coord->coord.t[0] - work->nav->nodes[block->node].x;
        dz          = *(u16*)&work->coord->coord.t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    SCRATCH_POP_BYTES(0x14);
    return block->nearest;
}

/// Re-plans the walker's position in the room's route byte table so that it
/// heads towards actor `actor`. It collects every slot of that table naming
/// the node nearest the actor and every slot naming the node nearest the
/// walker, then picks the pair of slots that are closest together: the
/// walker's cursor becomes the slot on its own side, `field_75` records the
/// slot on the actor's side, and `field_73` becomes the +1 / -1 direction the
/// cursor has to travel along the table to close the gap -- which the caller
/// then applies, as does the last line here. Both lists hold at most eight
/// slots, so a table with more matches than that is silently truncated; if no
/// pair was found at all the routine only complains and leaves the cursor
/// where it was.
void func_actor_110600_80132A84(OverlayWalker* work, s16 actor)
{
    OverlayWalkerRouteScratch* s;
    u8*                        head;
    s32                        diff;
    s32                        best;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x1C;
    s                = (OverlayWalkerRouteScratch*)(head - 0x1C);

    s->nodeA  = func_actor_110600_801327EC(work, actor);
    s->nodeB  = func_actor_110600_80132958(work);
    s->countA = 0;
    s->countB = 0;
    for (s->i = 0; s->i < work->nav->field_9; s->i++) {
        if (work->nav->field_4[s->i] == s->nodeA && s->countA < 8) {
            s->listA[s->countA] = s->i;
            s->countA++;
        }
        if (work->nav->field_4[s->i] == s->nodeB && s->countB < 8) {
            s->listB[s->countB] = s->i;
            s->countB++;
        }
    }

    s->listA[s->countA] = 0xFF;
    s->listB[s->countB] = 0xFF;
    s->best             = 0xFF;
    for (s->i = 0; s->i < 8; s->i++) {
        if (s->listA[s->i] == 0xFF) {
            break;
        }
        for (s->j = 0; s->j < 8; s->j++) {
            if (s->listB[s->j] == 0xFF) {
                break;
            }
            diff    = s->listA[s->i] - s->listB[s->j];
            best    = s->best;
            s->diff = diff;
            diff    = ABS(diff);
            if (diff < best) {
                s->best        = diff;
                work->cursor   = s->listB[s->j];
                work->field_75 = s->listA[s->i];
                if (s->diff < 0) {
                    work->field_73 = -1;
                } else {
                    work->field_73 = 1;
                }
            }
        }
    }

    if (s->best == 0xFF) {
        printf(D_actor_110600_80131E24);
    }
    work->cursor += (u8)work->field_73;
    SCRATCH_POP_BYTES(0x1C);
}

void func_actor_110600_80132D54(OverlayWalker* work)
{
    u8*                       head;
    OverlayWalkerMoveScratch* s;
    s32                       valx;
    s32                       valy;
    s32                       valz;
    s32                       dx;
    s32                       dy;
    s32                       dz;
    s32                       y;
    s32                       mag;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x18;
    s                = (OverlayWalkerMoveScratch*)(head - 0x18);
    if (func_800E0C10(work->recs, &s->delta, work->field_56, NULL) != 0) {
        dx         = ((OverlayWalkerMoveScratch*)(head - 0x18))->delta.vx.h.hi;
        dz         = s->delta.vz.h.hi;
        s->move.vx = dx;
        s->move.vz = dz;
        valx       = ((OverlayWalkerMoveScratch*)(head - 0x18))->delta.vx.w;
        if ((valx & 0xFFFF) != 0) {
            if (valx > 0) {
                s->move.vx++;
            } else {
                s->move.vx--;
            }
        }
        valz = s->delta.vz.w;
        if ((valz & 0xFFFF) != 0) {
            if (valz > 0) {
                s->move.vz++;
            } else {
                s->move.vz--;
            }
        }
        if (work->field_6B == 0) {
            dy         = s->delta.vy.h.hi;
            valy       = s->delta.vy.w;
            s->move.vy = s->move.vy + dy;
            if ((valy & 0xFFFF) != 0) {
                if (valy > 0) {
                    s->move.vy++;
                } else {
                    s->move.vy--;
                }
            }
        } else {
            s->move.vy = 0;
        }
    } else {
        s->move.vx = 0;
        s->move.vy = 0;
        s->move.vz = 0;
    }
    if (work->field_6B == 0) {
        s->move.vy += 0x10;
    }
    work->moveDelta          = s->move;
    work->coord->coord.t[0] += s->move.vx;
    if (s->move.vy >= 0x21) {
        work->coord->coord.t[1] += 8;
    }
    if (s->move.vy < -0x20) {
        work->coord->coord.t[1] -= 0x20;
    }
    y   = s->move.vy;
    mag = y;
    if (y < 0) {
        SOFT_TOUCH_REG(mag);
        mag = -mag;
    }
    if (mag < 0x20) {
        work->coord->coord.t[1] += y;
    }
    work->coord->coord.t[2] += s->move.vz;
    if (work->coord->coord.t[0] != 0 || work->coord->coord.t[2] != 0) {
        work->moving = 1;
    } else {
        work->moving = 0;
    }
    SCRATCH_POP_BYTES(0x18);
}

void func_actor_110600_80132FE0(OverlayWalker* work)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (Mc_SaveData.field_5C1 == 1) {
        return;
    }

    work->blocked = 0;
    work->push.vz = 0;
    work->push.vy = 0;
    work->push.vx = 0;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(OverlayAvoidScratch);
    s                = (OverlayAvoidScratch*)SCRATCH_HEAD(u8);

    Gfx_MatrixCol1(&work->coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-work->coord->workm.m[2][0], work->coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-work->coord->workm.m[0][2], work->coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&work->coord->workm.t[0];
    s->eye.vy = *(u16*)&work->coord->workm.t[1];
    s->eye.vz = *(u16*)&work->coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < work->avoidCount; s->i++) {
        if (work->avoidRecs[s->i].key == 0) {
            break;
        }
        s->kind = work->avoidRecs[s->i].key & 0xFFFF0000;
        if (s->kind != 0x10000) {
            if (s->kind != 0x30000 && (u16)work->avoidRecs[s->i].key != 0) {
                continue;
            }
        } else {
            work->blocked = 1;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] =
                overlayBearingXZ((SVECTOR3*)&work->avoidRecs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] =
                overlayBearingXY((SVECTOR3*)&work->avoidRecs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-work->coord->coord.m[2][0], work->coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            work->push.vx           += s->dir.vx;
            work->push.vz           += s->dir.vz;
            work->coord->coord.t[0] += s->dir.vx;
            work->coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_HEAD(u8) =
        (u8*)SCRATCH_HEAD(u8) + sizeof(OverlayAvoidScratch);
}

/// Turns the walker towards `pos` by at most `field_5A` angle units a frame.
/// The wrapped relative bearing drives the consecutive-turn counter, then
/// becomes the absolute yaw the model's saved scale matrix is rebuilt around.
void func_actor_110600_80133550(OverlayWalker* work, SVECTOR3* pos)
{
    OverlayWalkerTurnScratch* s;
    GpCoord*                  coord;
    u8*                       head;
    s16                       diff, t;
    s32                       angle;
    u16                       frames;

    if (D_80072728 == 1)
        return;
    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x1C;
    s                = (OverlayWalkerTurnScratch*)(head - 0x1C);
    coord            = work->coord;
    diff             = overlayCoordBearingXZ(pos, coord) -
           ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    t = diff;
    if (diff < 0) {
    wrapUp:
        if (t < -0x800) {
            t += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (t > 0x800) {
            t -= 0x1000;
            goto wrapDown;
        }
    }
    angle    = t;
    s->angle = angle;
    if (angle != 0)
        work->field_62++;
    else
        work->field_62 = 0;
    frames = (u16)work->field_62;
    CLOBBER_REG(v0);
    SOFT_USE_REG(frames);
    work->field_64 = 0;
    SOFT_COMPILER_BARRIER();
    if ((u16)work->field_5A + (u16)work->field_64 < s->angle)
        s->angle = (u16)work->field_5A + (u16)work->field_64;
    if (s->angle < -((u16)work->field_5A + (u16)work->field_64))
        s->angle = -((u16)work->field_5A + (u16)work->field_64);
    if ((u16)work->field_5A == 0)
        s->angle = 0;
    s->angle += ratan2(-work->coord->coord.m[2][0], work->coord->coord.m[2][2]);
    __builtin_memcpy(work->coord->coord.m, work->scaleMtx.m, sizeof(work->scaleMtx.m));
    Gfx_RotMatrixY(&work->coord->coord, s->angle, 0);
    SCRATCH_POP_BYTES(0x1C);
}

/// Debug rebuild of the walker's patrol table. Node 0 takes the walker's own
/// coordinate translation; every node above it takes that translation plus the
/// coordinate's facing column, rotated to `angle` and scaled by `scale` through
/// the GTE, and each node laid is logged as it is built. The route is then
/// re-seeded from the node count -- one node index per step with the 0xFF
/// terminator after the last -- with `field_4` and the cursor cleared, and the
/// scratch frame released.
void func_actor_110600_80133778(OverlayWalker* work, s16 scale, s16 angle)
{
    Actor110600TsvScratch* blk;
    u8*                    head;

    if (work->nav->count < 2)
        return;
    SOFT_COMPILER_BARRIER();
    work->nav->nodes[0].x = *(u16*)&work->coord->coord.t[0];
    work->nav->nodes[0].y = *(u16*)&work->coord->coord.t[1];
    work->nav->nodes[0].z = *(u16*)&work->coord->coord.t[2];
    head                  = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8)      = head - 0x2C;
    blk                   = (Actor110600TsvScratch*)(head - 0x2C);
    work->nav->field_4[0] = 0;
    blk->m                = work->coord->coord;
    for (blk->i = 1; blk->i < work->nav->count; blk->i++) {
        Gfx_RotMatrixY(&blk->m, (s16)angle, 0);
        Gfx_MatrixCol2(&blk->m, &blk->v);
        gte_lddp(scale);
        gte_ldsv(&blk->v);
        gte_gpf12();
        gte_stsv(&blk->v);
        work->nav->nodes[blk->i].x = *(u16*)&work->coord->coord.t[0] + *(u16*)&blk->v.vx;
        work->nav->nodes[blk->i].y = *(u16*)&work->coord->coord.t[1] + *(u16*)&blk->v.vy;
        work->nav->nodes[blk->i].z = *(u16*)&work->coord->coord.t[2] + *(u16*)&blk->v.vz;
        work->nav->field_4[blk->i] = blk->i;
        printf("emc_m->tsv[%d]( %d, %d, %d )\n", blk->i, work->nav->nodes[blk->i].x, work->nav->nodes[blk->i].y, work->nav->nodes[blk->i].z);
    }
    work->route->field_4 = 0;
    work->route->cursor  = 0;
    for (blk->i = 0; blk->i < work->nav->count; blk->i++) {
        work->route->nodes[blk->i] = blk->i;
    }
    work->route->nodes[blk->i] = 0xFF;
    SCRATCH_POP_BYTES(0x2C);
}

/// The walker's per-tick body, open on the scratch frame `func_actor_110600_80133A94`
/// hands it. State 1 heads straight for the position the `D_80073B08` motion
/// config indexed by `field_6E` holds, state 2 re-runs the patrol steering and
/// re-reads `nav`'s byte table at `cursor` whenever the step or one of the
/// three node bytes changed, and state 3 follows the patrol route proper. The
/// scalar at `field_5E` then ramps towards `field_5C` by `field_60` a frame;
/// while it is non-zero it scales (`GPF`) the normalised facing column of the
/// model matrix into the per-frame world step, which is added to the
/// coordinate's translation and kept in `moveStep`. `Mc_SaveData.field_5C1` (a global
/// freeze flag) zeroes the step instead. Written as an inline so the two
/// scratch-head accesses inside one frame stay absolute; see
/// `func_acropolis_bridge_8018532C` in `acropolis_bridge_12.c`, the same body.
static __inline__ void Actor110600_WalkerStep(OverlayWalker* walker, u8* head,
                                              OverlayWalkerTickScratch* block)
{
    u8*           head2;
    SVECTOR3*     pos;
    PlayerStatus* cfg;
    SVECTOR*      sv;
    SVECTOR*      gsv;
    SVECTOR*      step;
    GpCoord*      coord;
    s16           sdiff;
    s32           diff;
    s16           speed;
    s32           cur;
    s32           target;
    s32           result;

    switch (walker->state) {
        case 0:
            break;
        case 1:
            cfg                            = &D_80073B08[walker->field_6E];
            pos                            = (SVECTOR3*)(head - 0x24);
            ((SVECTOR3*)(head - 0x24))->vx = *(u16*)&cfg->coordMtx->t[0];
            pos->vy                        = *(u16*)&cfg->coordMtx->t[1];
            pos->vz                        = *(u16*)&cfg->coordMtx->t[2];
            break;
        case 2:
            SCRATCH_PUSH_BYTES(4);
            walker->field_6F = func_actor_110600_801327EC(walker, 1);
            walker->field_70 = func_actor_110600_80132958(walker);
            if (walker->field_69 != walker->state || walker->field_70 != walker->field_72 ||
                walker->field_6F != walker->field_71) {
                func_actor_110600_80132A84(walker, 1);
                walker->node = walker->nav->field_4[walker->cursor];
            }
            walker->field_69 = walker->state;
            walker->field_72 = walker->field_70;
            walker->field_71 = walker->field_6F;
            if (func_actor_110600_80132470(walker) != 0) {
                walker->cursor += (u8)walker->field_73;
                walker->node    = walker->nav->field_4[walker->cursor];
                SCRATCH_POP_BYTES(4);
            }
            break;
        case 3:
            func_actor_110600_80132654(walker, (SVECTOR3*)(head - 0x24));
            break;
    }
    func_actor_110600_80133550(walker, &block->pos);

    cur    = walker->field_5C;
    target = walker->field_5E;
    if (cur != target) {
        diff  = cur - target;
        sdiff = diff;
        if (sdiff > walker->field_60) {
            result = target + walker->field_60;
        } else if (sdiff < -walker->field_60) {
            result = target - walker->field_60;
        } else {
            result = target + diff;
        }
        walker->field_5E = result;
    }

    coord = walker->coord;
    speed = walker->field_5E;
    step  = &walker->moveStep;
    if (Mc_SaveData.field_5C1 == 1) {
        step->vz            = 0;
        step->vy            = 0;
        walker->moveStep.vx = 0;
    } else {
        head2            = SCRATCH_HEAD(u8);
        sv               = (SVECTOR*)(head2 - 8);
        SCRATCH_HEAD(u8) = (u8*)sv;
        gsv              = sv;
        if (speed != 0) {
            Gfx_MatrixCol2(&coord->coord, sv);
            VectorNormalSS(sv, sv);
            gte_lddp(speed);
            gte_ldsv(gsv);
            gte_gpf12();
            gte_stsv(gsv);
            coord->coord.t[0] += ((SVECTOR*)(head2 - 8))->vx;
            coord->coord.t[1] += sv->vy;
            coord->coord.t[2] += sv->vz;
            walker->moveStep   = *(SVECTOR*)(head2 - 8);
            coord->flg         = 0;
        }
        SCRATCH_POP_BYTES(8);
    }
    if (walker->field_6C == 0) {
        func_actor_110600_80132D54(walker);
    }
    if (walker->field_6D == 0) {
        func_actor_110600_80132FE0(walker);
    }
}

/// Per-tick walker step: advances the animation the `field_68` byte selects,
/// resolves the patrol node the `field_6E` byte names against `D_80073B08`,
/// and ramp-scales the model matrix between `field_5E` and `field_5C`. The
/// working frame is carved off `G_SCRATCH_HEAD` and handed back once the
/// coordinate has been rebuilt. Same body as the acropolis bridge room's
/// `func_acropolis_bridge_8018532C`.
void func_actor_110600_80133A94(OverlayWalker* walker)
{
    u8*                       head;
    OverlayWalkerTickScratch* block;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x28;
    block            = (OverlayWalkerTickScratch*)SCRATCH_HEAD(u8);
    Actor110600_WalkerStep(walker, head, block);
    walker->coord->flg = 0;
    SCRATCH_POP_BYTES(0x28);
}

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// rescales it uniformly: `ratan2` of the rotation's Z basis gives the yaw,
/// `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// `scale` on all three axes. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. Written as an inline so the four scratch-head accesses
/// stay absolute; see `Actor444000_ShrinkRotation` in `actor_444000_5.c`.
static __inline__ void Actor110600_ScaleRotation(Task* task, s16 scale)
{
    ActorScaleRotScratch* blk;
    GpCoord*              coord;
    u8*                   head;
    s16                   ang;
    u16                   m22;

    head                               = SCRATCH_HEAD(u8);
    coord                              = task->extra.tmd->coords;
    blk                                = (ActorScaleRotScratch*)(head - 0x34);
    SCRATCH_HEAD(ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]         = *(u16*)&((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]         = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]         = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]         = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]         = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]         = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]         = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]         = *(u16*)&blk->m.m[2][1];
    m22                          = *(u16*)&blk->m.m[2][2];
    coord->flg                   = 0;
    coord->coord.m[2][2]         = m22;
    task->extra.tmd->coords->flg = 0;
    SCRATCH_POP_BYTES(0x34);
}

/// Placement opcode: drops the model's root coordinate onto `placement` (the
/// three longs become its translation, the Euler angles go through
/// `Gfx_RotMatrixX` / `Y` / `Z`), then rebuilds and rescales that coordinate
/// from the actor's own heading and caches the resulting yaw in the work
/// block's `field_8`. The rescale `func_actor_110600_80138568` performs is
/// inlined behind the placement.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, GpXformArg* placement)
{
    Actor110600Work* work;

    work = (Actor110600Work*)task->work;

    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    Actor110600_ScaleRotation(task, (s16)work->walker.scale);
    work->field_8 = ratan2(-task->extra.tmd->coords->coord.m[2][0],
                           task->extra.tmd->coords->coord.m[2][2]);
    return 1;
}

/// Event handler: saves the event's first three bytes in the work block's
/// `field_BDC`, then dispatches on the event kind. Kind 0x301 with sub-code 1
/// enters state 0x14; kind 0x401 picks a display slot and a `field_892` state
/// per sub-code — 1, 8 and 9 only set the state, and 9 shares its tail with the
/// five sub-codes that repoint a slot — parking the actor in state 0x11 with
/// `field_2` cleared. Written with the share as a `goto` because the sub-codes
/// fall through into it from case 9. `arg1` is unused; it exists because the
/// dispatch passes three arguments.
s32 func_actor_110600_80134040(Task* arg0, s32 arg1, Actor110600Event* arg2)
{
    Actor110600Work* work = arg0->work;

    work->field_BDC.b[0] = arg2->b[0];
    work->field_BDC.b[1] = arg2->b[1];
    work->field_BDC.b[2] = arg2->b[2];
    if (arg2->w[0] == 0x301) {
        if (arg2->w[1] == 1) {
            work->field_0 = 0x14;
            return 1;
        }
        return 0;
    }
    if (arg2->w[0] == 0x401) {
        switch (arg2->w[1]) {
            default:
                return 0;
            case 1:
                work->field_0 = 0x17;
                work->field_2 = -1;
                return 1;
            case 2:
                work->field_892         = 0x23;
                D_actor_110600_80148598 = &D_8015BD7C;
                goto state_11;
            case 3:
                work->field_892         = 0x24;
                D_actor_110600_8014859C = &D_8015BD7C;
                goto state_11;
            case 5:
                work->field_892         = 0x22;
                D_actor_110600_80148594 = &D_8015C950;
                goto state_11;
            case 6:
                work->field_892         = 0x23;
                D_actor_110600_80148598 = &D_8015D2E8;
                goto state_11;
            case 4:
            case 7:
                work->field_892         = 0x25;
                D_actor_110600_801485A0 = &D_8015C064;
                goto state_11;
            case 8:
                work->field_0 = 0xC;
                work->field_2 = -1;
                return 1;
            case 9:
                work->field_892 = 0x11;
            state_11:
                work->field_0 = 0x11;
                work->field_2 = -1;
                return 1;
        }
    }
    return 0;
}

/// Tests the player (task slot 3) against `coord`: when the player's bearing
/// relative to the facing of `coord` is outside +/-0x400 for a non-negative
/// `offset`, or inside it for a negative one, returns 1 outright. Otherwise
/// returns whether the player stands at least `range + 0x96` from the point
/// `offset` units ahead of `coord` along its facing.
s32 func_actor_110600_801341A4(GpCoord* coord, s16 range, s16 offset)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = gameGetPtrSlot(3);
    d.vx   = ((GpCoordPos*)player->extra.tmd->coords)->x - ((GpCoordPos*)coord)->x;
    d.vy   = (u16)player->extra.tmd->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = ((GpCoordPos*)player->extra.tmd->coords)->z - ((GpCoordPos*)coord)->z;
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
    if (offset >= 0) {
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
    gte_lddp(offset);
    gte_ldsv(pv);
    gte_gpf12();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = player->extra.tmd->coords->coord.t[0] - v.vx;
    e.vy  = player->extra.tmd->coords->coord.t[1] - v.vy;
    e.vz  = player->extra.tmd->coords->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= range + 0x96;
}

/// Per-tick animation pass: for each clip id 1..0x12, the first ten (`i < 0xB`)
/// seed their slot's `rate` from the two work bytes and tick the primary and
/// blend contexts through `func_800B3448`, then hand both poses to
/// `Gp_AnimWritePoseCopy` with `weight` at 0x8A0 and its complement; the rest
/// only rewrite the primary slot and `Gp_AnimTickIndex` it. Same body as
/// `func_actor_403000_801336B4`, which walks 24 slots instead of 19.
void func_actor_110600_80134438(Task* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor110600AnimWork* work;

    work   = (Actor110600AnimWork*)((Actor110600Work*)arg0->work);
    weight = work->field_8A0;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = (u8)work->field_89E;
            work->slots[i].rate      = (u8)(work->field_896 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (u8)(work->field_896 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Sound cue the pose the model has reached has earned this tick, 0 for none:
/// each state watches one clip id of the animation slot `field_892` selects and
/// reports its `0x401D00NN` cue the first time that id is held, remembering it
/// in `field_8AC` so the cue is not repeated. State 3 reads slots 14 and 18
/// (cues 4 and 3, the second only while `field_8AC` is not already 0xFC);
/// states 2, 21, 4 and 5 read slot 1, with state 2 the only one watching two
/// ids (cues 2 and 1) and clearing the memory when neither is held. Every other
/// way out re-reads the slot-1 pose into `field_8AC`.
s32 func_actor_110600_80134564(Actor110600AnimWork* anim)
{
    s32 id14;
    s32 id18;
    s32 id2;
    s32 id21;
    s32 id4;
    s32 id5;
    s32 prev;
    s16 state;

    state = (u16)anim->field_892 - 2;
    switch (state) {
        case 1:
            id14 = anim->slots[14].curRec & 0x3FF;
            if (id14 == 0xC5) {
                prev = anim->field_8AC;
                if (prev != id14) {
                    anim->field_8AC = id14;
                    return 0x401D0004;
                }
                anim->field_8AC = prev;
                return 0;
            }
            id18 = anim->slots[18].curRec & 0x3FF;
            if (id18 == 0xFD) {
                if (anim->field_8AC != 0xFC) {
                    anim->field_8AC = id18;
                    return 0x401D0003;
                }
                anim->field_8AC = id18;
                break;
            }
            anim->field_8AC = 0;
            break;
        case 0:
            id2 = anim->slots[1].curRec & 0x3FF;
            if (id2 == 0x33) {
                if (anim->field_8AC != id2) {
                    anim->field_8AC = id2;
                    return 0x401D0002;
                }
                anim->field_8AC = id2;
            } else if (id2 == 0x26) {
                prev = anim->field_8AC;
                if (prev != id2) {
                    anim->field_8AC = id2;
                    return 0x401D0001;
                }
                anim->field_8AC = prev;
            } else {
                anim->field_8AC = 0;
            }
            break;
        case 19:
            id21 = anim->slots[1].curRec & 0x3FF;
            if (id21 == 4 && anim->field_8AC != id21) {
                anim->field_8AC = id21;
                return 0x401D0006;
            }
            anim->field_8AC = anim->slots[1].curRec & 0x3FF;
            break;
        case 2:
            id4 = anim->slots[1].curRec & 0x3FF;
            if (id4 == 9 && anim->field_8AC != id4) {
                anim->field_8AC = id4;
                return 0x401D000C;
            }
            anim->field_8AC = anim->slots[1].curRec & 0x3FF;
            break;
        case 3:
            id5 = anim->slots[1].curRec & 0x3FF;
            if (id5 == 0xB && anim->field_8AC != id5) {
                anim->field_8AC = id5;
                return 0x401D000C;
            }
            anim->field_8AC = anim->slots[1].curRec & 0x3FF;
            break;
    }
    return 0;
}

/// Reset argument `func_800B4114` is handed for the clip `field_892` of the
/// `field_890` stage: the `0x2D`-byte row of the animation table this overlay's
/// data carries at `D_actor_110600_80147D20`, indexed by the clip id. The row
/// stride is the row's own length, so the load is a signed byte.
extern s8 D_actor_110600_80147D20[][0x2D];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Per-tick animation stage machine driven off the task's work block.
///
/// Stages 1, 2 and 6 arm every slot 1..0x12 and then park the stage at 3 with
/// the frame counter `field_894` and `field_8AC` cleared: stage 1 plays the
/// clip at `field_892` through `func_800B4114`, taking each slot's reset
/// argument out of the `field_890` row of `D_actor_110600_80147D20`; stage 2
/// arms the same clip with `Gp_AnimResetSlot`; stage 6 arms clip 0x10 and then
/// ticks the context 99 times so the pose settles before it is shown. A
/// `field_89A` of 2 is the blend stage instead — it arms the blend context with
/// clip `field_89C` at weight `field_8A0` and parks `field_89A` at 3.
///
/// Every tick then steps all slots, either directly or — while `field_88E` is
/// set — through the blend pass `func_actor_110600_80134438`, which ends the
/// wait once blend slot 1 reports its clamp. `field_8A4` walks towards
/// `field_8A2` in 0x100 steps, and the turn that leaves, clamped to ±0x400, is
/// handed to joints 5 and 3 of the model root (the second a quarter of it).
/// Finally the id `func_actor_110600_80134564` reports is queued through
/// `SndEvt_EnqueueType6` with the model root's pan and depth; the bits 12..15
/// of the enemy's `field_8` are appended to it.
void func_actor_110600_80134728(Task* arg0)
{
    Actor110600AnimWork* work;
    Actor110600AnimWork* seekWork;
    Actor110600AnimWork* resetWork;
    Actor110600AnimWork* warmWork;
    Actor110600AnimWork* blendWork;
    Actor110600AnimWork* tickWork;
    GpEnemy*             enemy;
    u32                  table;
    s32                  index;
    s32                  animation;
    s32                  seekIndex;
    s32                  resetIndex;
    s32                  warmIndex;
    s32                  blendIndex;
    s32                  tickIndex;
    s32                  targetAngle;
    s32                  currentAngle;
    s32                  targetAngleBits;
    s32                  currentAngleBits;
    s32                  turn;
    s16                  turnNow;
    s32                  sound;
    s32                  soundId;
    s32                  pan;
    s16                  state;

    work  = (Actor110600AnimWork*)((Actor110600Work*)arg0->work);
    state = work->field_88C;
    enemy = arg0->spawnArg2;
    if (state == 1) {
        seekWork  = work;
        seekIndex = 1;
        table     = (u32)D_actor_110600_80147D20;
        do {
            work->slots[seekIndex].rate = (u8)seekWork->field_896;
            animation                   = seekWork->field_892;
            index                       = seekWork->field_890 * 0x2D;
            func_800B4114(&seekWork->anim, seekIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
            seekIndex += 1;
        } while (seekIndex < 0x13);
        seekWork->field_890 = (u16)seekWork->field_892;
        work->field_88C     = 3;
        work->field_894     = 0;
        work->field_8AC     = 0;
    } else if (state == 2) {
        resetWork  = work;
        resetIndex = 1;
        do {
            work->slots[resetIndex].rate = (u8)resetWork->field_896;
            Gp_AnimResetSlot(&resetWork->anim, resetIndex, (s32)resetWork->field_892);
            resetIndex += 1;
        } while (resetIndex < 0x13);
        resetWork->field_890 = (u16)resetWork->field_892;
        work->field_88C      = 3;
        work->field_894      = 0;
        work->field_8AC      = 0;
    } else if (state == 6) {
        warmWork  = work;
        warmIndex = 1;
        do {
            work->slots[warmIndex].rate = 0x10;
            Gp_AnimResetSlot(&warmWork->anim, warmIndex, (s32)warmWork->field_892);
            warmIndex += 1;
        } while (warmIndex < 0x13);
        warmWork->field_890 = (u16)warmWork->field_892;
        warmIndex           = 1;
        do {
            tickWork  = (Actor110600AnimWork*)((Actor110600Work*)arg0->work);
            tickIndex = 1;
            do {
                tickWork->slots[tickIndex].rate = (u8)tickWork->field_896;
                Gp_AnimTickIndex(&tickWork->anim, tickIndex);
                tickIndex += 1;
            } while (tickIndex < 0x13);
            warmIndex += 1;
        } while (warmIndex < 0x64);
        work->field_88C = 3;
        work->field_894 = 0;
        work->field_8AC = 0;
    }
    if (work->field_89A == 2) {
        blendWork            = (Actor110600AnimWork*)((Actor110600Work*)arg0->work);
        blendIndex           = 1;
        blendWork->field_89E = 0x30;
        blendWork->field_8A0 = 0xB78;
        do {
            blendWork->slots[blendIndex].rate = (u8)blendWork->field_89E;
            Gp_AnimResetSlot(&blendWork->blendAnim, blendIndex, (s32)blendWork->field_89C);
            blendIndex += 1;
        } while (blendIndex < 0x13);
        work->field_89A = 3;
    }
    work->field_894 = (u16)(work->field_894 + 1);
    if (work->field_88E == 0) {
        tickWork  = (Actor110600AnimWork*)((Actor110600Work*)arg0->work);
        tickIndex = 1;
        do {
            tickWork->slots[tickIndex].rate = (u8)tickWork->field_896;
            Gp_AnimTickIndex(&tickWork->anim, tickIndex);
            tickIndex += 1;
        } while (tickIndex < 0x13);
    } else {
        func_actor_110600_80134438(arg0);
        if (work->blendSlots[1].flags & 0x1) {
            work->field_88E = 0;
        }
    }
    targetAngle      = work->field_8A2;
    currentAngle     = work->field_8A4;
    targetAngleBits  = (u16)work->field_8A2;
    currentAngleBits = (u16)work->field_8A4;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x101) {
            work->field_8A4 = (s16)(currentAngleBits + 0x100);
        } else {
            goto block_31;
        }
    } else if ((currentAngle - targetAngle) >= 0x101) {
        work->field_8A4 = (s16)(currentAngleBits - 0x100);
    } else {
    block_31:
        work->field_8A4 = (s16)targetAngleBits;
    }
    turnNow = work->field_8A4;
    turn    = (u16)work->field_8A4;
    if (turnNow != 0) {
        if (turnNow >= 0x401) {
            turn = 0x400;
        }
        if (turnNow < -0x400) {
            turn = -0x400;
        }
        func_actor_110600_80131FC0(&arg0->extra.tmd->coords[5], (s16)turn);
        func_actor_110600_80131FC0(&arg0->extra.tmd->coords[3], (s16)((s32)(turn << 0x10) >> 0x12));
    }
    sound = func_actor_110600_80134564(work);
    if (sound != 0) {
        soundId = sound | ((enemy->placeKey >> 12) << 8);
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s32)(s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
}

extern GpPairSrcE D_actor_110600_80138F14;
extern GpAnimSet* D_actor_110600_8014850C[];
extern GpMsgEntry D_actor_110600_80148624[];

static __inline__ void Actor110600_InitBodyObj(GpObj* obj, GpCoord* coord, GpRec18* recs, SVECTOR* pos, s16 enabled)
{
    obj->ctx.recs = recs;
    obj->coord    = coord;
    obj->pos.vx   = (u16)pos->vx;
    obj->pos.vy   = (u16)pos->vy;
    obj->pos.vz   = (u16)pos->vz;
    obj->radius   = 0x200;
    obj->flags    = enabled;
    Gp_LinkObj(3, obj);
}

static __inline__ void Actor110600_InitScale(OverlayWalker* walker)
{
    VECTOR *head, *scale;
    s32     amount;
    head                     = SCRATCH_HEAD(VECTOR);
    scale                    = head - 1;
    SCRATCH_HEAD(VECTOR)     = scale;
    walker->scaleMtx.m[0][0] = walker->scaleMtx.m[1][1] = walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[0][1] = walker->scaleMtx.m[0][2] = walker->scaleMtx.m[1][0] = walker->scaleMtx.m[1][2] = walker->scaleMtx.m[2][0] = walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.t[0] = walker->scaleMtx.t[1] = walker->scaleMtx.t[2] = 0;
    amount                                                                = walker->scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vx = scale->vy = scale->vz = amount;
        ScaleMatrix(&walker->scaleMtx, scale);
    }
    SCRATCH_POP(VECTOR);
}

void func_actor_110600_80134AB4(GpEnemy* enemy, Task* task)
{
    SVECTOR          pos;
    VECTOR           world;
    GpRec18*         savedRecs;
    GpObj*           obj;
    GpObj*           bodyObj;
    GpRec18*         contactRecs;
    GpRec18*         walkRecs;
    GpCoord*         coord;
    TmdObject*       model;
    s16              enabled;
    u32              placement;
    TmdObject*       boundModel;
    Actor110600Work* work;
    Actor110600Work* boundWork;

    model = task->extra.tmd;
    coord = model->coords;
    if (((task->spawnArg1 >> 16) & 0xF) != 2) {
        model->flags = 0;
        Tmd_AllocBuffers(model);
    }
    work       = memCalloc(0xBECU, false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_110600_801387F4;
    boundWork            = task->work;
    boundModel           = task->extra.tmd;
    boundModel->lightMtx = &boundWork->field_AC8;
    boundModel->colorMtx = &boundWork->field_AE8;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->bodyPos.vx    = 0;
    enemy->bodyPos.vy    = 0;
    enemy->bodyPos.vz    = 0;
    enemy->coord         = task->extra.tmd->coords + 3;
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->reactionFlags      = 0;
    enemy->param              = &D_actor_110600_80138F14;
    enemy->hpMax = enemy->hp = D_actor_110600_80138F14.hpMax;
    contactRecs              = work->recs_8D8;
    enemy->recs              = contactRecs;
    func_800B3F84(&((Actor110600AnimWork*)work)->anim, &D_actor_110600_8014850C, model, &((Actor110600AnimWork*)work)->slots[19], ((Actor110600AnimWork*)work)->slots);
    func_800B3F84(&((Actor110600AnimWork*)work)->blendAnim, &D_actor_110600_8014850C, model, &((Actor110600AnimWork*)work)->blendSlots[19], ((Actor110600AnimWork*)work)->blendSlots);
    work->field_88C = 2;
    work->field_88E = 0;
    work->field_892 = 2;
    work->field_8A4 = 0;
    work->field_8A2 = 0;
    func_actor_110600_80134728(task);
    walkRecs                 = work->recs_970;
    work->field_BE4          = 0;
    work->field_BE6          = 0;
    work->field_950.coord    = &gGfxViewCoord;
    savedRecs                = walkRecs;
    work->field_950.ctx.recs = walkRecs;
    work->field_950.pos.vx   = (u16)task->extra.tmd->coords->coord.t[0];
    work->field_950.pos.vy   = (s16)((u16)task->extra.tmd->coords->coord.t[1] - 0x124);
    work->field_950.pos.vz   = (u16)task->extra.tmd->coords->coord.t[2];
    work->field_950.key      = 0x3000D;
    work->field_950.radius   = 0x1A4;
    work->field_950.flags = enabled = 1;
    Gp_LinkObj(2, &work->field_950);
    work->field_950.flags = (u16)(work->field_950.flags | 0xC000);
    Gp_InitRec18Table(work->field_950.ctx.recs, 0xC, 0);
    obj           = &work->field_8B8;
    obj->coord    = task->extra.tmd->coords;
    obj->ctx.recs = contactRecs;
    obj->pos.vx   = 0;
    obj->pos.vy   = 0;
    obj->pos.vz   = 0;
    obj->key      = 0x30000;
    obj->radius   = 0x14A;
    obj->flags    = enabled;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->ctx.recs, 5, 0);
    work->field_8B8.pos.vy = -0x2BC;
    bodyObj                = &work->field_A90;
    work->field_8B8.key    = 0x30000;
    work->field_8B8.pos.vx = 0;
    work->field_8B8.pos.vz = 0x190;
    pos.vx                 = 0;
    pos.vy                 = 0;
    pos.vz                 = 0;
    Actor110600_InitBodyObj(bodyObj, task->extra.tmd->coords + 3, work->recs, &pos, enabled);
    Gp_InitRec18Table(bodyObj->ctx.recs, 1, 0);
    task->msgTable  = &D_actor_110600_80148624;
    work->field_BD4 = 0;
    work->field_BD8 = 0;
    work->field_8B0 = 0;
    work->field_8B1 = enabled;
    coord->sub      = &gGfxViewCoord;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    world.vx = coord->workm.t[0];
    world.vy = coord->workm.t[1];
    world.vz = coord->workm.t[2];
    func_800D7A9C(task->extra.tmd, &world, 0, 3);
    work->walker.coord      = coord;
    work->walker.recs       = savedRecs;
    work->walker.field_56   = 0xC;
    work->walker.avoidCount = 5;
    work->walker.state      = 0;
    work->walker.avoidRecs  = contactRecs;
    work->walker.scale      = 0;
    work->walker.field_5A   = 0x20;
    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x01030000) {
        work->walker.field_6B = 0;
    } else {
        work->walker.field_6B = enabled;
    }
    work->walker.field_6C          = 0;
    work->walker.field_6D          = 1;
    work->walker.field_6E          = (u8)Mc_SaveData.characterId;
    work->walker.nav               = &work->walker.navData;
    work->walker.route             = &work->walker.routeData;
    work->walker.navData.count     = 2;
    work->walker.navData.field_9   = 2;
    work->walker.routeData.field_4 = 2;
    work->walker.navData.nodes     = work->field_BBC;
    work->walker.navData.field_4   = work->field_BCC;
    work->walker.routeData.nodes   = work->field_BD0;
    work->walker.field_6E          = (u8)Mc_SaveData.characterId;
    switch (task->spawnArg1 & 0xF0) {
        case 0:
            work->field_896    = 20;
            work->field_898    = 20;
            work->walker.scale = 0x1000;
            work->field_8B6    = 10;
            work->field_8B4    = 75;
            break;
        case 0x10:
            work->field_896    = 16;
            work->field_898    = 16;
            work->walker.scale = 4500;
            work->field_8B6    = 8;
            work->field_8B4    = 66;
            break;
        case 0x20:
            work->field_896    = 16;
            work->field_898    = 16;
            work->walker.scale = 6500;
            work->field_8B6    = 14;
            work->field_8B4    = 63;
            break;
        default:
            work->field_896    = 20;
            work->walker.scale = 0x1000;
            break;
    }
    placement = enemy->placeKey >> 0xC;
    if ((s16)(placement & 1) == 1) {
        work->field_896 += placement >> 1;
    } else {
        work->field_896 -= placement >> 1;
    }
    switch (task->spawnArg1 & 0xF00) {
        case 0:
            work->field_C = 3000;
            work->field_E = 500;
            break;
        case 0x100:
            work->field_C = 4000;
            work->field_E = 1000;
            break;
        case 0x200:
            work->field_C = 6000;
            work->field_E = 2000;
            break;
        default:
            work->field_C = 8000;
            work->field_E = 2000;
            break;
    }
    switch (task->spawnArg1 & 0xF000) {
        case 0:
            func_actor_110600_80133778(&work->walker, 1500, 0x764);
            break;
        case 0x1000:
            func_actor_110600_80133778(&work->walker, 3000, 0x764);
            break;
        case 0x2000:
            func_actor_110600_80133778(&work->walker, 4000, 0x764);
            break;
        default:
            func_actor_110600_80133778(&work->walker, 5000, 0x764);
            break;
    }
    switch ((task->spawnArg1 >> 16) & 0xF) {
        case 1:
            if (enemy->spawnState == 0) {
                work->field_0 = 0;
            } else {
                work->field_0 = 1;
            }
            break;
        case 2:
            work->field_0 = 0;
            break;
        case 3:
            break;
        case 4:
            work->field_0 = 6;
            break;
        case 0:
        default:
            if (enemy->spawnState == 0) {
                work->field_0 = 2;
            } else {
                work->field_0 = 1;
            }
            break;
    }
    Actor110600_InitScale(&work->walker);
    work->field_2 = -1;
    task->state  += 1;
}

/// Aiming stage: re-arms the aim on a live actor — clear the model object, drop
/// bit 0x8000 of `field_A90.flags` and set 0x4000 of `field_950.flags`, tag the
/// enemy's link node, reload `field_896` from `field_898`, park the stage at 2
/// (`field_88C` / `field_892`) and the walker at state 3 with its turn limit at
/// 0x10. The aim itself is one bearing: the yaw of the player delta from
/// the model's root coordinate, minus that coordinate's own yaw, wrapped into
/// [-0x800, 0x800]. While it is under 0x3E8 and again unconditionally, the XZ
/// delta is measured against the `field_C` / `field_E` hit spheres, and falling
/// inside either moves the actor to state 4. Every tick the walker is stepped
/// first and the model ticked last.
void func_actor_110600_80135194(Task* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    GpCoord*         coord;
    GpCoord*         facing;
    SVECTOR          delta;
    SVECTOR*         d;
    s16              angle;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        work->field_896           = work->field_898;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_88C           = 2;
        work->field_892           = 2;
        work->walker.state        = 3;
        work->walker.field_5A     = 0x10;
    }
    work->walker.field_5E = work->field_8B6;
    func_actor_110600_80133A94(&work->walker);
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)Player_Status.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->extra.tmd->coords;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    if (abs(angle) < 0x3E8) {
        if (actorOutsideRadius(&delta, work->field_C) == 0)
            work->field_0 = 4;
    }
    if (actorOutsideRadius(&delta, work->field_E) == 0)
        work->field_0 = 4;
    func_actor_110600_80134728(arg0);
}

static __inline__ s16 Actor110600_WrapHitAngle(s16 angle)
{
    if (angle < 0) {
    neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto neg;
        }
    } else {
    pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto pos;
        }
    }
    return angle;
}

static __inline__ s32 Actor110600_TickShake(void)
{
    D_actor_110600_8014865C++;
    if (D_actor_110600_8014865C == 5)
        D_actor_110600_8014865C = 0;
    if (!(D_actor_110600_8014865C & 1))
        Display_ClampField126(0);
    else
        Display_ClampField126(1);
    if (D_actor_110600_8014865C == 0) {
        Display_ClampField126(0);
        return 1;
    }
    return 0;
}

void func_actor_110600_80135454(Task* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    GpCoord*         coord;
    GpCoord*         facing;
    OverlayWalker*   walker;
    SVECTOR          delta;
    SVECTOR*         d;
    s16              angle;
    u16              ramp;
    u16              timer;
    s32              mode;
    s32              pose;
    s32              nextPose;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        work->field_A90.flags    &= 0x7FFF;
        work->field_950.flags    |= 0x4000;
        enemy->node.state.b.flags = 8;
        timer                     = work->field_898;
        work->field_88C           = 1;
        do {
            CLOBBER_REG(v0);
            work->walker.state = 1;
            mode               = (s16)work->field_898;
            work->field_892    = 2;
        } while (0);
        work->field_896 = timer;
        if (mode == 0x38)
            work->walker.field_5A = 0x30;
        work->walker.field_5A = 0x1C;
        work->field_BE0       = 0;
    }
    if (work->field_88E == 0) {
        work->walker.field_5E = (s16)work->field_8B6 * work->field_896 / 16;
        if ((s16)func_actor_110600_801341A4(arg0->extra.tmd->coords, 0x1A4, (s16)work->walker.field_5E) == 0)
            work->walker.field_5E = 0;
    } else {
        work->walker.field_5E = (u16)(work->field_8B4 * work->field_896 / 1520) / 2;
        if ((s16)func_actor_110600_801341A4(arg0->extra.tmd->coords, 0x1A4, (s16)work->walker.field_5E) == 0)
            work->walker.field_5E = 0;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)Player_Status.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    if (actorOutsideRadius(&delta, 900) == 0)
        work->walker.field_5E = 0;
    walker           = &work->walker;
    ramp             = work->walker.field_5E;
    walker->field_60 = 0;
    walker->field_5C = ramp;
    walker->field_5E = ramp;
    func_actor_110600_80133A94(walker);
    work->field_BE0++;
    facing = arg0->extra.tmd->coords;
    angle  = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    angle  = Actor110600_WrapHitAngle(angle);
    if (abs(angle) < 0x80) {
        if (actorOutsideRadius(&delta, 500) != 0) {
            if (actorOutsideRadius(&delta, 1000) == 0 && work->field_BE0 >= 25)
                work->field_0 = 5;
        }
    }
    if (actorOutsideRadius(&delta, 1000) == 0 && work->field_BE0 >= 91)
        work->field_0 = 5;
    work->field_8A2 = angle;
    func_actor_110600_80134728(arg0);
    pose = work->field_4E & 0x3FF;
    if ((pose == 0x33) && (work->field_888 != pose)) {
        work->field_BE8 = 1;
    }
    nextPose = work->field_4E & 0x3FF;
    if ((nextPose == 0x26) && (work->field_888 != nextPose)) {
        work->field_BE8 = 2;
    }
    work->field_888 = (s32)(work->field_4E & 0x3FF);
    if (work->field_BE8 != 0) {
        if (Actor110600_TickShake() != 0)
            work->field_BE8 = 0;
    }
}

/// Aiming stage: points the model at the player. Entering on a live
/// actor re-arms it — clear the model object, drop bit 0x8000 of
/// `field_A90.flags` and set 0x4000 of `field_950.flags`, tag the enemy's link
/// node, then park the stage timer at 0x15 with `field_896` reloaded from
/// `field_898`. The yaw of the delta from the model's root coordinate to
/// `Player_Status.coordMtx`'s translation goes through `ratan2`, has the coordinate's own
/// yaw (`ratan2` of `-m[2][0]`, `m[2][2]`) subtracted, and is wrapped into
/// [-0x800, 0x800] before it lands in `field_8A2`; the model is ticked and the
/// actor moves on (state 3) once the `field_5C` bit the walker sets arrives.
/// Both translations are measured in their low 16 bits, so all three delta
/// reads are `u16`.
void func_actor_110600_80135A18(Task* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    GpCoord*         coord;
    GpCoord*         facing;
    SVECTOR          delta;
    SVECTOR*         d;
    s16              angle;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_88C           = 1;
        work->field_892           = 0x15;
        work->field_896           = work->field_898;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)Player_Status.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->extra.tmd->coords;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->field_8A2 = angle;
    func_actor_110600_80134728(arg0);
    if (work->field_5C & 1) {
        work->field_0 = 3;
    }
}

/// 1 when the first of `recs` carries the kind 0x10000 tag: the walk breaks on
/// an empty slot and reports 0.
static __inline__ s32 Actor110600_HasRec10000(GpRec18* recs)
{
    s16 i;

    for (i = 0; i < 1; i++) {
        if (!recs[i].key) {
            break;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Firing stage. Entering on a live actor re-arms it: clear the model object,
/// take 0x8000 off `field_A90.flags` and put 0x4000 on `field_950.flags`, tag
/// the enemy's link node, and pick one of the two patrol modes off
/// `field_BE6` — a zeroed one packs the model pair with 1 and holds the stage
/// at `field_892` 5 for 0x10 ticks, a set one packs it with 0 and holds mode 4
/// for 0x1A. `walker.state` is raised and `walker` is re-armed
/// for a fresh patrol (`field_5C` cleared, `field_5E` reloaded from
/// `walker.field_5E`, `field_60` = 8) with `field_8A2` / `field_8A4` cleared and
/// `walker.field_5A` parked at 0x10 to cover the first ten ticks. Every tick after
/// that raises `field_BE0`, which retires `walker.field_5A` once it passes 0xB, and
/// ticks the model; the pose `field_4E` then drives the pair of flag edges the
/// mode owns — 0xF raises and 0x15 drops 0x8000 in mode 4, 0x10 / 0x13 the
/// same in mode 5. The `field_5C` bit 0 the walker sets moves the actor on
/// (state 3). Finally, while the first `GpRec18` record still carries the
/// 0x10000 kind tag, the model root's pan and depth are played as sound
/// 0x401D000D and 0x8000 comes off `field_A90.flags`.
void func_actor_110600_80135B84(Task* arg0)
{
    Actor110600Work* work;
    OverlayWalker*   walker;
    GpEnemy*         enemy;
    TmdObject*       obj;
    u16              ramp;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_88C           = 1;
        if (((Actor110600Work*)arg0->work)->field_BE6 != 0) {
            work->field_A90.key = Gp_PackObjPair(enemy, 0);
            work->field_892     = 4;
            work->field_896     = 0x1A;
        } else {
            work->field_A90.key = Gp_PackObjPair(enemy, 1);
            work->field_892     = 5;
            work->field_896     = 0x10;
        }
        work->walker.state    = 1;
        walker                = &work->walker;
        ramp                  = work->walker.field_5E;
        walker->field_5C      = 0;
        walker->field_60      = 8;
        walker->field_5E      = ramp;
        work->walker.field_5A = 0x10;
        work->field_8A4       = 0;
        work->field_8A2       = 0;
        work->field_BE0       = 0;
    }
    work->field_BE0++;
    if (work->field_BE0 >= 0xB) {
        work->walker.field_5A = 0;
    }
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 4) {
        switch (work->field_4E & 0x3FF) {
            case 0xF:
                work->field_A90.flags = (u16)(work->field_A90.flags | 0x8000);
                break;
            case 0x15:
                work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
                break;
        }
    }
    if (work->field_892 == 5) {
        switch (work->field_4E & 0x3FF) {
            case 0x10:
                work->field_A90.flags = (u16)(work->field_A90.flags | 0x8000);
                break;
            case 0x13:
                work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
                break;
        }
    }
    if (work->field_5C & 1) {
        work->field_0 = 3;
    }
    if (Actor110600_HasRec10000(work->recs)) {
        SndEvt_EnqueueType6(0x401D000D, (s8)Gp_GetObjPan(arg0->extra.tmd->coords),
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
    }
}

void func_actor_110600_80135E20(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR* sc;
    s32      mag;

    sc  = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    mag = (arg1 >= 0) ? arg1 : -arg1;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_110600_801485C4[0];
                break;
            case 1:
                *sc = D_actor_110600_801485C4[1];
                break;
            case 2:
                *sc = D_actor_110600_801485C4[2];
                break;
            case 3:
                *sc = D_actor_110600_801485C4[3];
                break;
            default:
                *sc = D_actor_110600_801485C4[4];
                break;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_110600_801485C4[5];
                break;
            case 1:
                *sc = D_actor_110600_801485C4[6];
                break;
            default:
                *sc = D_actor_110600_801485C4[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_110600_801485C4[8];
        } else {
            *sc = D_actor_110600_801485C4[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_110600_801485C4[10];
        } else {
            *sc = D_actor_110600_801485C4[11];
        }
    }
    D_actor_110600_80148698.coord      = &arg0->extra.tmd->coords[1];
    D_actor_110600_80148698.spawnArgLo = 0x100;
    D_actor_110600_80148698.spawnArgHi = 3;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], sc, &D_actor_110600_80148698);
    SCRATCH_POP_BYTES(8);
}

static __inline__ s32 Actor110600_FindHit(SVECTOR* point, GpRec18* recs, s16 count)
{
    s16 i;
    for (i = 0; i < count; i++) {
        if (!recs[i].key)
            break;
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            point->vx = recs[i].point.vx;
            point->vy = recs[i].point.vy;
            point->vz = recs[i].point.vz;
            return recs[i].key;
        }
    }
    return 0;
}

void func_actor_110600_80136210(Task* arg0)
{
    Actor110600Work*       work;
    GpEnemy*               enemy;
    GpCoord*               facing;
    s16                    angle;
    s16                    dz;
    s16                    state;
    s32                    magnitude;
    s32                    yaw;
    s32                    x;
    s32                    y;
    s32                    z;
    s32                    distance;
    s32                    pan;
    u32                    kind;
    Actor110600HitScratch* sc;
    PlayerStatus*          player;

    enemy  = arg0->spawnArg2;
    work   = arg0->work;
    player = &Player_Status;
    if (player->hp <= 0) {
        work->field_A90.flags &= 0x7FFF;
        return;
    }
    sc      = (Actor110600HitScratch*)SCRATCH_PUSH_BYTES(0x30);
    sc->key = Actor110600_FindHit(&sc->point, work->recs_8D8, 5);
    if (!sc->key) {
        sc->key = Actor110600_FindHit(&sc->point, work->recs_970, 12);
    }
    if (sc->key) {
        state = work->field_0;
        if ((state == 2) || (state == 6) || (state == 0x14)) {
            work->field_0 = 4;
        }
        x            = player->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        sc->delta.vx = x;
        y            = player->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        sc->delta.vy = y;
        z            = player->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        sc->delta.vz = z;
        distance     = SquareRoot0((x * x) + (y * y) + (z * z));
        sc->distance = distance;
        sc->damage   = Gp_ComputeDamage((u32)sc->key, (u32)distance, 0, 0);
        if (Gp_RollEnemyChance(enemy, (u32)sc->key, 0) != 0) {
            sc->damage = (u32)(sc->damage * 5);
            Gp_SpawnEff(0x6009C, arg0->extra.tmd->coords + 2, 0, NULL);
        }
        magnitude = sc->angle;
        if (magnitude < 0) {
            magnitude = -magnitude;
        }
        if (magnitude >= 0x501) {
            sc->damage = (u32)(sc->damage * 2);
        }
        if (work->field_BE6 == 1) {
            sc->damage = (u32)((u32)sc->damage >> 1);
        }
        func_800E2C78(enemy, sc->key, (s32)sc->damage, 0);
        enemy->hp = (u16)enemy->hp - (u16)sc->damage;
        func_800DA6E8(&enemy->node, (s32)sc->damage, 0);
        arg0->extra.tmd->coords->flg = 0;
        Gp_UpdateCoord(arg0->extra.tmd->coords);
        sc->direction.vx = (s16)(sc->point.vx - *(u16*)&arg0->extra.tmd->coords->workm.t[0]);
        sc->direction.vy = (s16)(sc->point.vy - *(u16*)&arg0->extra.tmd->coords->workm.t[1]);
        dz               = sc->point.vz - *(u16*)&arg0->extra.tmd->coords->workm.t[2];
        sc->direction.vz = dz;
        yaw              = ratan2((s32)sc->direction.vx, (s32)dz);
        facing           = arg0->extra.tmd->coords;
        angle            = yaw - ratan2((s32)-facing->workm.m[2][0], (s32)facing->workm.m[2][2]);
        sc->angle        = angle;
        sc->angle        = Actor110600_WrapHitAngle(sc->angle);
        func_actor_110600_80135E20(arg0, sc->angle, sc->key);
        work->field_8A4 = 0;
        work->field_8A2 = 0;
        if ((work->field_BE6 == 0) && (enemy->hp < (s32)((u16)D_actor_110600_80138F18 >> 1))) {
            work->field_0   = 0x18;
            work->field_BE6 = 1;
        }
        if (enemy->hp <= 0) {
            work->field_8B8.pos.vx = 0;
            work->field_8B8.pos.vy = 0;
            work->field_8B8.pos.vz = 0;
            work->field_A90.flags &= 0x7FFF;
            work->field_8B8.coord  = arg0->extra.tmd->coords + 2;
            work->field_8A4        = 0;
            work->field_8A2        = 0;
            Display_ClampField126(0);
        } else {
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(0x401D0007, (s32)pan, (s32)(s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        work->field_8AA = Gp_GetIdParam2(sc->key);
        kind            = Gp_GetIdParam0(sc->key) & 0xFFFF;
        switch (kind) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
                work->field_88E = 1;
                work->field_89C = 0xB;
                work->field_89A = 2;
                break;
            case 1:
            case 8:
            case 9:
                break;
            case 2:
                Gp_SetObjFlag2(enemy, sc->key, 0);
                work->field_0 = 0xE;
                break;
            case 3:
                Gp_SetObjFlag4(enemy, sc->key, 0);
                break;
        }
    }
    if (enemy->reactionFlags & 0xC) {
        sc->damage = Gp_TickObjFlag4(enemy);
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
        if (sc->damage != 0) {
            enemy->hp = (u16)enemy->hp - (u16)sc->damage;
            func_800DA6E8(&enemy->node, (s32)sc->damage, 0);
            if (work->field_0 != 0xE) {
                work->field_88E = 1;
                work->field_89C = 0xB;
                work->field_89A = 2;
            } else {
                work->field_2 = -1;
            }
        }
    }
    SCRATCH_POP_BYTES(0x30);
}

/// Timer stage that walks between the two long `field_892` values. Entering on
/// a live actor re-arms it: clear the model object, take 0x8000 off
/// `field_A90.flags` and put 0x4000 on `field_950.flags`, tag the enemy's link
/// node, set the stage timer to 0x18 and `field_896` from `field_898`, then
/// re-arm `walker` for a fresh patrol (`field_5C` cleared,
/// `field_5E` reloaded from `walker.field_5E`, `field_60` = 8) with `walker.state` /
/// `walker.field_5A` / `field_8A4` / `field_8A2` cleared. Every tick after that steps
/// the walker and the model, then retimes: at 0x18 a draw of `Gp_LcgState`
/// whose seventh bit is clear drops it to 0xE, and at 0xE the `field_5C` bit 0
/// the walker sets on arrival — or on hitting something — puts it back to 0x18.
/// Both retimes re-enter state 1 (`field_88C`) and tick once more.
void func_actor_110600_80136888(Task* arg0)
{
    Actor110600Work* work;
    OverlayWalker*   walker;
    GpEnemy*         enemy;
    TmdObject*       obj;
    u32              rng;
    u16              ramp;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_88C           = 1;
        work->field_892           = 0x18;
        work->field_896           = work->field_898;
        ramp                      = work->walker.field_5E;
        walker                    = &work->walker;
        work->walker.state        = 0;
        walker->field_5C          = 0;
        walker->field_5E          = ramp;
        walker->field_60          = 8;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_8A2           = 0;
    }
    func_actor_110600_80133A94(&work->walker);
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 0x18) {
        if (work->field_5C & 2) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng;
            if (!((rng >> 16) & 7)) {
                work->field_892 = 0xE;
                work->field_88C = 1;
                func_actor_110600_80134728(arg0);
            }
        }
    }
    if ((work->field_892 == 0xE) && (work->field_5C & 1)) {
        work->field_892 = 0x18;
        work->field_88C = 1;
        func_actor_110600_80134728(arg0);
    }
}

/// The walker's handoff stage. Entering on a live actor re-arms it: clear the
/// model object, take 0x8000 off `field_A90.flags` and put 0x4000 on
/// `field_950.flags`, tag the enemy's link node, park the stage timer at 0x1D
/// with `field_896` at 0x10, then re-arm `walker` to run its
/// patrol out (`field_5C` = 0xFFFE, `field_5E` reloaded from `walker.field_5E`,
/// `field_60` = 2) with `walker.state` / `walker.field_5A` / `field_8A4` / `field_8A2`
/// cleared. Every tick after that steps the walker and the model; at 0x1D the
/// `field_5C` bit 0 the walker sets on arrival moves the stage to 0x1E and
/// re-seeds the walker block, and at 0x1E that same bit picks what the actor
/// does next: 0xB while the enemy's `field_40` is still positive, 0xC once it
/// has run out.
void func_actor_110600_801369D8(Task* arg0)
{
    Actor110600Work* work;
    OverlayWalker*   walker;
    OverlayWalker*   walker2;
    GpEnemy*         enemy;
    u16              ramp;
    u16              ramp2;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_88C           = 2;
        work->field_892           = 0x1D;
        work->field_896           = 0x10;
        ramp                      = work->walker.field_5E;
        walker                    = &work->walker;
        work->walker.state        = 0;
        walker->field_5C          = 0xFFFE;
        walker->field_5E          = ramp;
        walker->field_60          = 2;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_8A2           = 0;
    }
    walker2 = &work->walker;
    func_actor_110600_80133A94(walker2);
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 0x1D) {
        if (work->field_5C & 1) {
            ramp2             = work->walker.field_5E;
            work->field_892   = 0x1E;
            work->field_88C   = 2;
            walker2->field_5C = 0;
            walker2->field_60 = 2;
            walker2->field_5E = ramp2;
            return;
        }
    }
    if ((work->field_892 == 0x1E) && (work->field_5C & 1)) {
        if (enemy->hp > 0) {
            work->field_0 = 0xB;
        } else {
            work->field_0 = 0xC;
        }
    }
}

static __inline__ void Actor110600_ApplyShrink(Task* arg0, Actor110600Work* work, s16 y)
{
    TmdObject*            obj;
    GpCoord*              coord;
    ActorScaleRotScratch* blk;
    ActorScaleRotScratch* head;
    s16                   page;
    s16                   ang;
    u16                   m22;
    ActorScaleRotScratch* restoredHead;

    head                               = SCRATCH_HEAD(ActorScaleRotScratch);
    obj                                = arg0->extra.tmd;
    coord                              = obj->coords;
    page                               = work->walker.scale;
    blk                                = head - 1;
    SCRATCH_HEAD(ActorScaleRotScratch) = blk;
    y                                 -= (work->field_BE0 - 0x12C) * 2;
    ang                                = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle                         = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = page;
    blk->scale.vy = y;
    blk->scale.vz = page;
    ScaleMatrix(&blk->m, &blk->scale);
    coord->coord.m[0][0]               = *(u16*)&(head - 1)->m.m[0][0];
    coord->coord.m[0][1]               = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]               = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]               = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]               = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]               = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]               = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]               = *(u16*)&blk->m.m[2][1];
    restoredHead                       = SCRATCH_HEAD(ActorScaleRotScratch);
    m22                                = *(u16*)&blk->m.m[2][2];
    coord->flg                         = 0;
    SCRATCH_HEAD(ActorScaleRotScratch) = restoredHead + 1;
    coord->coord.m[2][2]               = m22;
}

void func_actor_110600_80136B20(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    SVECTOR          pos;
    VECTOR           scale;
    s16              y;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        enemy->node.state.b.flags = 1;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags & 0xBFFF);
        work->field_8A4           = 0;
        work->field_8A2           = 0;
        work->field_BE0           = 0;
        work->field_8A6           = work->walker.scale;
    }
    if (work->field_BE0 < 0x3E8) {
        work->field_BE0 = (u16)work->field_BE0 + 1;
    }
    switch (work->field_BE0) {
        case 0xE6:
            arg0->extra.tmd->flags = 2;
            work->field_B08        = work->field_AE8;
            break;
        case 0xC8:
        case 0x190:
            pos.vx = 0x12C;
            pos.vy = 0;
            pos.vz = 0;
            Gp_SpawnEff(0x600A5, &arg0->extra.tmd->coords[3], 3, &pos);
            break;
        case 0xFA:
        case 0x1A4:
            pos.vx = 0x190;
            pos.vy = 0;
            pos.vz = 0;
            break;
        case 0x258:
            arg0->extra.tmd->flags = 0x80;
            break;
    }
    if (work->field_BE0 >= 0xE6) {
        scale.vx = scale.vy = scale.vz = 0xBB8 + work->field_BE0 * -4;
        work->field_AE8                = work->field_B08;
        ScaleMatrix(&work->field_AE8, &scale);
        gte_lddp(scale.vz);
        gte_ldlvl(&work->field_AE8.t[0]);
        gte_gpf12();
        gte_stlvl(&work->field_AE8.t[0]);
    }
    if (work->field_BE0 == 0xFD) {
        arg0->extra.tmd->flags = 2;
    }
    if (work->field_BE0 >= 0xC9) {
        y = work->field_8A6;
        if ((s16)work->field_8A6 >= 0x801) {
            y              -= 8;
            work->field_8A6 = y;
            Actor110600_ApplyShrink(arg0, work, y);
        }
    }
}

/// Re-dresses a live actor: take the model object out of draw, drop bit 0x8000
/// of `field_A90.flags` and bit 0x4000 of `field_950.flags`, tag the enemy's
/// link node, clear the `walker.field_5A` / `field_8A4` / `field_8A2` timers and hand
/// the model the 0x80 texture page, then spawn five effects off its part
/// coordinates 6, 8, 10, 11 and 15 (`Gp_SpawnEff` bank 0xA0005, buffer sizes
/// 0x200 / 0x200 / 0x200 / 0x300 / 0x300). Each spawned model object takes its
/// texture page and CLUT from the nested area record the actor's own area key
/// resolves to, and is streamed twice once its aux buffer exists.
void func_actor_110600_80136ECC(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GpAreaKey        key;
    u8               areaByte0;
    u32              raw1, index1;
    GpEffWork*       effect1;
    TmdObject*       model1;
    GpAreaPlace*     entry1;
    GpAreaKey*       sessionKey1;
    u32              raw2, index2;
    GpEffWork*       effect2;
    TmdObject*       model2;
    GpAreaPlace*     entry2;
    GpAreaKey*       sessionKey2;
    u32              raw3, index3;
    GpEffWork*       effect3;
    TmdObject*       model3;
    GpAreaPlace*     entry3;
    GpAreaKey*       sessionKey3;
    u32              raw4, index4;
    GpEffWork*       effect4;
    TmdObject*       model4;
    GpAreaPlace*     entry4;
    GpAreaKey*       sessionKey4;
    u32              raw5, index5;
    GpEffWork*       effect5;
    TmdObject*       model5;
    GpAreaPlace*     entry5;
    GpAreaKey*       sessionKey5;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        obj                       = arg0->extra.tmd;
        obj->flags                = 0;
        work->field_A90.flags    &= 0x7FFF;
        work->field_950.flags    &= 0xBFFF;
        enemy->node.state.b.flags = 1;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_8A2           = 0;
        obj->flags                = 0x80;

        effect1 = Gp_SpawnEff(0xA0005, &arg0->extra.tmd->coords[6], 0x200, NULL);
        if (effect1 != NULL) {
            sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
            raw1        = enemy->placeKey;
            model1      = effect1->task->extra.tmd;
            key.stage   = sessionKey1->stage;
            key.area    = sessionKey1->area;
            key.room    = sessionKey1->room;
            areaByte0   = gGameSession->at4.loc.view;
            index1      = raw1 >> 12;
            key.view    = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model1->tpage = entry1->tpage;
            model1->clut  = entry1->clut;
            if (model1->buffer != NULL) {
                tmdProcessStream(model1);
                tmdProcessStream(model1);
            }
        }

        effect2 = Gp_SpawnEff(0xA0005, &arg0->extra.tmd->coords[8], 0x200, NULL);
        if (effect2 != NULL) {
            sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
            raw2        = enemy->placeKey;
            model2      = effect2->task->extra.tmd;
            key.stage   = sessionKey2->stage;
            key.area    = sessionKey2->area;
            key.room    = sessionKey2->room;
            areaByte0   = gGameSession->at4.loc.view;
            index2      = raw2 >> 12;
            key.view    = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model2->tpage = entry2->tpage;
            model2->clut  = entry2->clut;
            if (model2->buffer != NULL) {
                tmdProcessStream(model2);
                tmdProcessStream(model2);
            }
        }

        effect3 = Gp_SpawnEff(0xA0005, &arg0->extra.tmd->coords[10], 0x200, NULL);
        if (effect3 != NULL) {
            sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
            raw3        = enemy->placeKey;
            model3      = effect3->task->extra.tmd;
            key.stage   = sessionKey3->stage;
            key.area    = sessionKey3->area;
            key.room    = sessionKey3->room;
            areaByte0   = gGameSession->at4.loc.view;
            index3      = raw3 >> 12;
            key.view    = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry3        = (GpAreaPlace*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model3->tpage = entry3->tpage;
            model3->clut  = entry3->clut;
            if (model3->buffer != NULL) {
                tmdProcessStream(model3);
                tmdProcessStream(model3);
            }
        }

        effect4 = Gp_SpawnEff(0xA0005, &arg0->extra.tmd->coords[11], 0x300, NULL);
        if (effect4 != NULL) {
            sessionKey4 = (GpAreaKey*)&gGameSession->at4.loc;
            raw4        = enemy->placeKey;
            model4      = effect4->task->extra.tmd;
            key.stage   = sessionKey4->stage;
            key.area    = sessionKey4->area;
            key.room    = sessionKey4->room;
            areaByte0   = gGameSession->at4.loc.view;
            index4      = raw4 >> 12;
            key.view    = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry4        = (GpAreaPlace*)((index4 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model4->tpage = entry4->tpage;
            model4->clut  = entry4->clut;
            if (model4->buffer != NULL) {
                tmdProcessStream(model4);
                tmdProcessStream(model4);
            }
        }

        effect5 = Gp_SpawnEff(0xA0005, &arg0->extra.tmd->coords[15], 0x300, NULL);
        if (effect5 != NULL) {
            sessionKey5 = (GpAreaKey*)&gGameSession->at4.loc;
            raw5        = enemy->placeKey;
            model5      = effect5->task->extra.tmd;
            key.stage   = sessionKey5->stage;
            key.area    = sessionKey5->area;
            key.room    = sessionKey5->room;
            areaByte0   = gGameSession->at4.loc.view;
            index5      = raw5 >> 12;
            key.view    = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry5        = (GpAreaPlace*)((index5 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model5->tpage = entry5->tpage;
            model5->clut  = entry5->clut;
            if (model5->buffer != NULL) {
                tmdProcessStream(model5);
                tmdProcessStream(model5);
            }
        }
    }
}

/// Offset, 100 units along Z, that `func_actor_110600_801372CC` hands
/// `func_800FDB18` with the model's seventh coordinate when it spawns its
/// three effects.
const SVECTOR D_actor_110600_80131F1C = { 0, 0, 100, 0 };

static __inline__ void Actor110600_RescaleRoot(Task* arg0, s16 scale)
{
    ActorScaleRotScratch* blk;
    GpCoord*              coord;
    u8*                   head;
    s16                   ang;
    u16                   m22;

    head                               = SCRATCH_HEAD(u8);
    coord                              = arg0->extra.tmd->coords;
    blk                                = (ActorScaleRotScratch*)(head - 0x34);
    SCRATCH_HEAD(ActorScaleRotScratch) = blk;
    ang                                = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle                         = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = (s16)scale;
    blk->scale.vy = (s16)scale;
    blk->scale.vx = (s16)scale;
    ScaleMatrix(&blk->m, &blk->scale);
    coord->coord.m[0][0] = *(u16*)&((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
    SCRATCH_POP_BYTES(0x34);
}

void func_actor_110600_801372CC(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    SVECTOR          vec;
    GpEffArg*        d;
    GpEffArg*        tailEffect;
    GpCoord*         effectCoord;
    GpCoord*         effectCoord2;
    GpCoord*         effectCoord3;
    u32              rng;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        enemy->node.state.b.flags = 1;
        work->field_88C           = 2;
        work->field_896           = 0x10;
        work->field_8A2           = 0;
        work->field_8A4           = 0;
        if ((work->field_BDC.raw & 0xFFFFFF) == 0x60401) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng;
            if ((rng >> 16) & 1) {
                vec.vx                             = -0xE;
                vec.vy                             = 0;
                vec.vz                             = 0;
                effectCoord                        = arg0->extra.tmd->coords;
                D_actor_110600_80148698.spawnArgLo = 0x100;
                D_actor_110600_80148698.spawnArgHi = 3;
                D_actor_110600_80148698.coord      = effectCoord;
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[9], &vec, &D_actor_110600_80148698);
            } else {
                d             = &D_actor_110600_80148698;
                vec.vx        = -0x19;
                vec.vy        = 0;
                vec.vz        = 0;
                effectCoord2  = arg0->extra.tmd->coords;
                d->spawnArgLo = 0x100;
                d->spawnArgHi = 3;
                d->coord      = effectCoord2;
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[2], &vec, &D_actor_110600_80148698);
            }
        }
    }
    work->field_88E = 0;
    func_actor_110600_80134728(arg0);

    Actor110600_RescaleRoot(arg0, work->walker.scale);

    if (((work->field_BDC.raw & 0xFFFFFF) == 0x30401) && (work->field_892 != 0x1E)) {
        if ((work->field_4E & 0x3FF) == 0xB) {
            D_actor_110600_80147D20[0x24][0x1E] = 6;
            work->field_892                     = 0x1E;
            work->field_88C                     = 1;
        }
        if (work->field_892 != 0x1E) {
            if (((work->field_4E & 0x3FF) == 4) && (work->field_8AC != (work->field_4E & 0x3FF))) {
                vec                    = D_actor_110600_80131F1C;
                tailEffect             = &D_actor_110600_80148698;
                effectCoord3           = arg0->extra.tmd->coords;
                tailEffect->spawnArgLo = 0x100;
                tailEffect->spawnArgHi = 3;
                tailEffect->coord      = effectCoord3;
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
            }
        }
    }
}

/// The turn-away stage, the pick-up twin of the `field_892` == 0x16 leg of
/// `func_actor_110600_80136888`: entering on a live actor clears the model
/// object, takes 0x8000 off `field_A90.flags` and 0x4000 off
/// `field_950.flags`, tags the enemy's link node with 1 and parks the timer at
/// `field_896` = 0x20 with `field_88C` re-armed, `walker.field_5A` / `field_8A4` /
/// `field_8A2` cleared. Every tick after that steps the shared handler and, at
/// 0x16, rolls `Gp_LcgState` and turns the model's root coordinate by the yaw
/// the roll's low nibble picks — 0x32 while it is under 0xA, -0x78 past it —
/// clearing the coordinate's `flg`. Once `field_894` has run up to 0x1F the
/// stage drops the timer to 0x10, re-arms `field_88C` and steps to 0x21, where
/// the `field_5C` bit 0 the actor sets walks it on to state 0xC.
void func_actor_110600_80137684(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    u32              rng;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        arg0->extra.tmd->flags    = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags & 0xBFFF);
        enemy->node.state.b.flags = 1;
        work->field_896           = 0x20;
        work->field_88C           = 1;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_8A2           = 0;
        work->field_892           = 0x16;
    }
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 0x16) {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if (((rng >> 16) & 0xF) < 0xA) {
            Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, 0x32, 0);
        } else {
            Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, -0x78, 0);
        }
        arg0->extra.tmd->coords->flg = 0;
        if ((work->field_892 == 0x16) && (work->field_894 >= 0x1F)) {
            work->field_896 = 0x10;
            work->field_88C = 1;
            work->field_892 = 0x21;
        }
    }
    if ((work->field_892 == 0x21) && (work->field_5C & 1)) {
        work->field_0 = 0xC;
    }
}

static __inline__ s32 Actor110600_OutOfRange(SVECTOR* d)
{
    OverlayRangeScratch* blk;
    s32                  dz2;
    s32                  dz;
    u8*                  head;

    head                              = SCRATCH_HEAD(u8);
    blk                               = (OverlayRangeScratch*)(head - 0xC);
    SCRATCH_HEAD(OverlayRangeScratch) = blk;
    blk->dx                           = d->vx;
    blk->dz                           = d->vz;
    blk->r                            = 0xBB8;
    blk->dx                          *= blk->dx;
    dz                                = blk->dz;
    dz2                               = dz * dz;
    blk->dz                           = dz2;
    blk->r                           *= blk->r;
    SCRATCH_HEAD(u8)                  = head;
    __asm__("" : "=r"(dz), "+r"(dz2) : "m"(blk->r));
    return (blk->dx + dz2) >= blk->r;
}

void func_actor_110600_801377FC(Task* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    GpCoord*         coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        obj                       = arg0->extra.tmd;
        work->field_892           = 0x22;
        work->field_88C           = 2;
        obj->flags                = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_896           = 0x10;
        work->field_8A2           = 0;
    }
    func_actor_110600_80134728(arg0);
    work->field_8AC = work->field_4E & 0x3FF;
    coord           = arg0->extra.tmd->coords;
    d               = &delta;
    delta.vx        = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy           = (u16)Player_Status.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz           = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    if (!Actor110600_OutOfRange(d)) {
        work->field_0 = 0x15;
    }
}

/// Aiming stage that re-arms the model behaviour on a live actor — `field_892`
/// = 0x15 with `field_88C` = 1, the model object's `field_C` cleared, bit 0x8000
/// off `field_A90.flags` and 0x4000 on `field_950.flags`, the enemy's link node
/// tagged 1 with `walker.field_5A` / `field_8A4` cleared and `field_896` = 0x10 — then
/// wraps the yaw from the model's root coordinate to the player
/// `Player_Status.coordMtx` against the coordinate's own yaw (`ratan2` of `-m[2][0]`,
/// `m[2][2]`) into `field_8A2`. Ticks the model and moves the actor to state 3
/// once the `field_5C` bit the walker sets arrives. Same wrap as
/// `func_actor_110600_80135A18`.
void func_actor_110600_80137980(Task* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    GpCoord*         coord;
    GpCoord*         facing;
    SVECTOR          delta;
    SVECTOR*         d;
    s16              angle;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        obj                       = arg0->extra.tmd;
        work->field_892           = 0x15;
        work->field_88C           = 1;
        obj->flags                = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 1;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_896           = 0x10;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)Player_Status.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->extra.tmd->coords;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->field_8A2 = angle;
    func_actor_110600_80134728(arg0);
    if (work->field_5C & 1) {
        work->field_0 = 3;
    }
}

/// Stage-driven recoil push, one stage per tick. On a live actor it clears the
/// model's flags, drops bit 0x8000 of `field_A90.flags` and sets 0x4000 of
/// `field_950.flags`, tags the enemy's link node and restarts the stage at 0.
/// The push takes column 0 of the model root coordinate, normalises it out of
/// place and scales it by the stage — 0x320, -0x3E8, 0x190, -0x190, 0xC8,
/// through the GTE's interpolation register. Only the X and Z components are
/// added to the coordinate's translation, and the coordinate is marked dirty so
/// the tree is recomputed. Stage 5 pushes nothing: it moves the actor to state
/// 3 and leaves the counter parked.
void func_actor_110600_80137AF4(Task* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    SVECTOR          vec;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        D_actor_110600_80148688   = 0;
    }
    Gfx_MatrixCol0(&arg0->extra.tmd->coords->coord, &vec);
    VectorNormalSS(&vec, &vec);
    switch (D_actor_110600_80148688) {
        case 0:
            gte_lddp(0x320);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 1:
            gte_lddp(-0x3E8);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 2:
            gte_lddp(0x190);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 3:
            gte_lddp(-0x190);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 4:
            gte_lddp(0xC8);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 5:
            work->field_0 = 3;
            return;
    }
    arg0->extra.tmd->coords->coord.t[0] += vec.vx;
    arg0->extra.tmd->coords->coord.t[2] += vec.vz;
    D_actor_110600_80148688++;
    arg0->extra.tmd->coords->flg = 0;
}

/// Death stage machine, entering on a live actor: take the model out of draw,
/// drop bit 0x8000 of `field_A90.flags` and set 0x4000 of `field_950.flags`,
/// tag the enemy's link node, arm `field_892` / `field_88C` and the `field_896`
/// timer, tick once and clear both `field_BE0` and the `field_BE2` stage. Stage
/// 0 idles on that timer — once the pose `field_4E` reaches 4 it parks
/// `field_896` at -0x10 and steps to stage 1. Stage 1 is the shrink tail:
/// halves `field_896` each tick, parking at -0xC when the halving lands on the
/// stage value and bouncing -1 back to 8, and after 0x35 ticks parks
/// `field_896` / `field_898` at 0x38 and moves the actor to state 3. Every
/// stage-1 tick also adds 0x27 to `field_BE4`.
void func_actor_110600_80137DB0(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    s16              step;
    s32              state;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        obj                       = arg0->extra.tmd;
        obj->flags                = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_892           = 0xC;
        work->field_88C           = 2;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_896           = 6;
        func_actor_110600_80134728(arg0);
        work->field_BE0 = 0;
        work->field_BE2 = 0;
    }
    state           = work->field_BE2;
    work->field_88E = 0;
    switch (state) {
        case 0:
            func_actor_110600_80134728(arg0);
            if ((work->field_4E & 0x3FF) == 4) {
                work->field_896 = -0x10;
                work->field_BE2 = (s16)((u16)work->field_BE2 + 1);
                return;
            }
            return;
        case 1:
            step            = (s16)work->field_896 / 2;
            work->field_896 = step;
            work->field_BE0++;
            if (work->field_896 == state) {
                work->field_896 = -0xC;
            }
            if (work->field_896 == -1) {
                work->field_896 = 8;
            }
            func_actor_110600_80134728(arg0);
            if (work->field_BE0 >= 0x35) {
                work->field_896 = 0x38;
                work->field_898 = 0x38;
                work->field_0   = 3;
            }
            work->field_BE4 += 0x27;
            break;
    }
}

/// Remaining-enemy count: once it has run out the tick hands the enemy 1 HP
/// back rather than letting the death it just took stand. Same "the fight is
/// over" global `func_actor_105100_80136318` gates on.
extern s16 D_80073BA0;

/// Per-frame step the tick hands off to once the `field_8AA` countdown reaches
/// zero.
void func_actor_110600_80136210(Task* arg0);

/// The actor's state handlers, indexed by `Actor110600Work::field_0`. splat
/// migrates the table into the `.s` of the function that reads it, so it is
/// written out here to keep the block in the unit's `.rodata` now that
/// `func_actor_110600_80137F2C` is decompiled.
const Actor110600StateTable D_actor_110600_80131F3C = {
    func_actor_110600_801388A4,
    NULL,
    func_actor_110600_80135194,
    func_actor_110600_80135454,
    func_actor_110600_80135A18,
    func_actor_110600_80135B84,
    func_actor_110600_80136888,
    func_actor_110600_801369D8,
    func_actor_110600_80138980,
    func_actor_110600_80138AFC,
    func_actor_110600_80138BD0,
    func_actor_110600_80138A70,
    func_actor_110600_80136B20,
    func_actor_110600_80136ECC,
    func_actor_110600_80138D7C,
    NULL,
    NULL,
    func_actor_110600_801372CC,
    NULL,
    func_actor_110600_80137684,
    func_actor_110600_801377FC,
    func_actor_110600_80137980,
    func_actor_110600_80137AF4,
    func_actor_110600_80138CA4,
    func_actor_110600_80137DB0,
};

/// The actor's enemy tick, the middle entry of the `D_actor_110600_80131FA0`
/// triple `func_actor_110600_80134AB4` / this / `Gp_DestroyEnemy`: copies
/// `D_actor_110600_80131F3C` onto its frame, rebuilds the model root's
/// coordinate and hands its translation to `Gp_UpdateActorColor`, then switches
/// on `Gp_StateF0.field_4`.
///
/// Modes 1 and 2 skip the state handler entirely — each clears the three
/// `GpRec18` tables and returns, mode 2 stamping `field_C` to 0x80 for the
/// hidden pose first, and mode 1 drawing the ground quad on the way unless the
/// model sits in the death or hit pose. Mode 0 draws the quad the same way with
/// `field_C` zeroed and then falls through.
///
/// The fall-through stages the model root's translation into the `field_950`
/// display node, runs the handler `field_0` selects out of the stack copy,
/// restages the same three halfwords with Y dropped by 0x124 for the pose it
/// just advanced into, and gives the enemy 1 HP back once the remaining-enemy
/// count has run out. The tail clears the three tables again, marks the root
/// clean, shifts the colour matrix's translation down by the shrink `field_BE4`
/// — the matrix state 12 scales — and keeps `field_8B8` out of the ground
/// effect's way by clearing bit 0x8000 while the actor is in a death or hit
/// pose.
void func_actor_110600_80137F2C(GpEnemy* arg0, Task* arg1)
{
    VECTOR                pos;
    Actor110600StateTable states;
    Actor110600Work*      work;

    work   = arg1->work;
    states = D_actor_110600_80131F3C;

    arg1->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);

    switch (Gp_StateF0.field_4) {
        case 0:
            if ((work->field_0 != 0) && (work->field_0 != 0xC)) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->extra.tmd->coords->workm.t, 0x280, Gp_State1C->groundShade);
            }
            break;
        case 1:
            if ((work->field_0 != 0xC) && (work->field_0 != 0)) {
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->extra.tmd->coords->workm.t, 0x280, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied(work->recs_970);
            Gp_ClearRec18Occupied(work->recs_8D8);
            Gp_ClearRec18Occupied(work->recs);
            return;
        case 2:
            arg1->extra.tmd->flags = 0x80;
            Gp_ClearRec18Occupied(work->recs_970);
            Gp_ClearRec18Occupied(work->recs_8D8);
            Gp_ClearRec18Occupied(work->recs);
            return;
    }

    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;

    work->field_950.pos.vx = *(u16*)&arg1->extra.tmd->coords->coord.t[0];
    work->field_950.pos.vy = *(u16*)&arg1->extra.tmd->coords->coord.t[1];
    work->field_950.pos.vz = *(u16*)&arg1->extra.tmd->coords->coord.t[2];
    states.fn[work->field_0](arg1);
    work->field_950.pos.vx = *(u16*)&arg1->extra.tmd->coords->coord.t[0];
    work->field_950.pos.vy = (u16)(*(u16*)&arg1->extra.tmd->coords->coord.t[1] - 0x124);
    work->field_950.pos.vz = *(u16*)&arg1->extra.tmd->coords->coord.t[2];

    if (arg0->hp > 0) {
        if (work->field_8AA > 0) {
            work->field_8AA = (s16)((u16)work->field_8AA - 1);
        } else {
            func_actor_110600_80136210(arg1);
        }
        if (arg0->hp > 0) {
            goto block_24;
        }
    }
    if (D_80073BA0 <= 0) {
        arg0->hp = 1;
    }
block_24:
    Gp_ClearRec18Occupied(work->recs_970);
    Gp_ClearRec18Occupied(work->recs_8D8);
    Gp_ClearRec18Occupied(work->recs);
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->flg = 0;
    }
    if (arg1->extra.tmd->coords->flg == 0) {
        work->field_8B1 = 1;
    } else {
        work->field_8B1 = 0;
    }
    arg1->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    work->field_AE8.t[1] -= work->field_BE4;
    work->field_AE8.t[2] -= work->field_BE4;
    work->field_AE8.t[0] -= (work->field_BE4 * 2) / 3;
    if ((work->field_0 == 0xC) || (work->field_0 == 0) || (work->field_0 == 0xD) || (work->field_0 == 0x13)) {
        work->field_8B8.flags &= 0x7FFF;
    } else {
        work->field_8B8.flags |= 0x8000;
    }
}

void func_actor_110600_80138394(void)
{
}

/// The enemy task's three state handlers - spawn, per-frame tick and teardown -
/// that `func_actor_110600_80138EA8` dispatches through by `Task::state`.
const GpEnemyTaskFuncTable3 D_actor_110600_80131FA0 = {
    func_actor_110600_80134AB4,
    func_actor_110600_80137F2C,
    Gp_DestroyEnemy,
};

/// The `0x7D3` handler of the display-opcode table `D_actor_110600_80148624`:
/// maps the requested state onto the work block's `field_892` (0x22..0x28) and
/// parks the actor in state 0x11 with `field_2` cleared. States 0 and 4 also
/// stamp the enemy's occupancy tag and re-save its pose; state 0 writes its own
/// `field_892` ahead of those calls, so it skips the store the other four share,
/// which is the tail the compiler merged out of the four `break`s.
///
/// The table GCC emits for this switch is what pins the package's
/// `rodata_head`: it lands at 0x18C, 8-aligned only if this unit's `.rodata`
/// starts at 0x4 rather than 0x0 — the package id ahead of it is prepended, not
/// compiled — and behind the id it picks up `.align 3`'s 4-byte pad instead.
s32 func_actor_110600_8013839C(Task* arg0, s32 arg1, GpAnimArg* arg2)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    s32              state;

    state = arg2->field_4;
    work  = arg0->work;
    enemy = arg0->spawnArg2;
    switch (state) {
        case 0:
            work->field_892   = 0x22;
            enemy->spawnState = 1;
            Gp_SaveEnemyPose(enemy);
            break;
        case 1:
            work->field_892 = 0x23;
            break;
        case 2:
            work->field_892 = 0x24;
            break;
        case 3:
            work->field_892 = 0x25;
            break;
        case 4:
            enemy->spawnState = 1;
            Gp_SaveEnemyPose(enemy);
            work->field_892 = 0x28;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// Display-object handler: `arg2` selects the mode and `GpEnemy.spawnState` -- the occupancy tag
/// `Gp_SaveEnemyPose` writes -- decides whether mode 1 shows the object again.
/// Mode 0 hides it (bit 0x80 of `TmdObject.flags`) and reinstates its buffers;
/// 1 hides it and restarts the work block's `field_0` while the tag reads 4, and
/// otherwise clears `field_C` and reinstates the buffers; 2 only sets bit 0x4;
/// 3 clears `field_C`, restarts `field_0` and then sets bit 0x4. `arg1` is
/// unused; it exists because the dispatch passes three arguments.
s32 func_actor_110600_80138448(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor110600Work* work;
    GpEnemy*         enemy;

    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    work  = arg0->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            if (enemy->spawnState == 0) {
                obj->flags = 0;
                Tmd_AllocBuffers(obj);
            } else if (enemy->spawnState == 4) {
                obj->flags    = 0x80;
                work->field_0 = 0;
            } else {
                obj->flags = 0;
                Tmd_AllocBuffers(obj);
            }
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            if (enemy->spawnState == 4) {
                obj->flags = 0x80;
            } else {
                obj->flags = 0;
            }
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

s32 func_actor_110600_80138538(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor110600Work*)arg0->work;
    if (enemy->hp > 0) {
        return 1;
    }
    work->field_BE4      = 0;
    enemy->reactionFlags = 0;
    work->field_BE6      = 0;
    return 0;
}

/// Rebuilds `coord`'s rotation as a pure yaw (read back from its current
/// matrix with `ratan2(-m[2][0], m[2][2])`) uniformly scaled by `scale`, using
/// a 0x34-byte block taken from the scratchpad head, and marks the coordinate
/// for refresh.
void func_actor_110600_80138568(GpCoord* coord, s16 scale)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&(head - 1)->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

void func_actor_110600_80138680(GpCoord* coord, s16 sx, s16 sy, s16 sz)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = sx;
    blk->scale.vy = sy;
    blk->scale.vz = sz;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&(head - 1)->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

s32 func_actor_110600_801387C0(Task* arg0)
{
    Actor110600Work* work;

    work = (Actor110600Work*)arg0->work;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_0 = 4;
    return 1;
}

void func_actor_110600_801387F4(Task* task)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    Task*            helper;
    Task*            helper2;

    work  = (Actor110600Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        helper = work->field_BD4;
        if (helper != NULL) {
            helper->state++;
        }
        helper2 = work->field_BD8;
        if (helper2 != NULL) {
            helper2->state++;
        }
        Gp_UnlinkObj(&work->field_A90);
        Gp_UnlinkObj(&work->field_8B8);
        Gp_UnlinkObj(&work->field_950);
        enemy->recs = 0;
    }
    Display_ClampField126(0);
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_110600_801388A4(Task* arg0)
{
    TmdObject*       obj;
    Actor110600Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 1;
        obj->flags                                      = (u16)(obj->flags | 0x80);
        work->field_A90.flags                           = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags                           = (u16)(work->field_950.flags & 0xBFFF);
    }
}

s32 func_actor_110600_80138900(void)
{
    s16* p;
    s16  next;
    s32  cur;

    p    = &D_actor_110600_8014865C;
    next = (u16)*p + 1;
    *p   = next;
    if (next == 5) {
        *p = 0;
    }
    cur = (u16)*p;
    if ((cur & 1) == 0) {
        Display_ClampField126(0);
    } else {
        Display_ClampField126(1);
    }
    if (D_actor_110600_8014865C != 0) {
        return 0;
    }
    Display_ClampField126(0);
    return 1;
}

void func_actor_110600_80138980(Task* arg0)
{
    Actor110600Work* work;
    OverlayWalker*   walker;
    GpEnemy*         enemy;
    u16              ramp;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_88C           = 2;
        work->field_892           = 0xC;
        work->field_896           = 0x10;
        ramp                      = work->walker.field_5E;
        walker                    = &work->walker;
        work->walker.state        = 0;
        walker->field_5C          = 2;
        walker->field_5E          = ramp;
        walker->field_60          = 8;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_8A2           = 0;
    }
    func_actor_110600_80133A94(&work->walker);
    func_actor_110600_80134728(arg0);
    if (work->field_5C & 1) {
        if (enemy->hp > 0) {
            work->field_0 = 0xB;
        } else {
            work->field_0 = 0xC;
        }
    }
}

void func_actor_110600_80138A70(Task* arg0)
{
    Actor110600Work* work;
    u32              rng;
    s16              timer;

    work = arg0->work;
    if (work->field_4 != 0) {
        rng           = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng;
        work->field_6 = (rng >> 16) & 0x1F;
    }
    timer         = work->field_6 - 1;
    work->field_6 = timer;
    if (timer < 0) {
        switch (work->field_892) {
            case 30:
                work->field_0 = 9;
                return;
            case 12:
                work->field_0 = 0xA;
                break;
        }
    }
}

void func_actor_110600_80138AFC(Task* arg0)
{
    Actor110600Work* work;
    OverlayWalker*   walker;
    GpEnemy*         enemy;
    u16              ramp;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_88C           = 2;
        work->field_892           = 0xF;
        work->field_896           = work->field_898;
        ramp                      = work->walker.field_5E;
        walker                    = &work->walker;
        work->walker.state        = 0;
        walker->field_5C          = 0;
        walker->field_5E          = ramp;
        walker->field_60          = 8;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_8A2           = 0;
    }
    func_actor_110600_80133A94(&work->walker);
    func_actor_110600_80134728(arg0);
    if (work->field_5C & 1) {
        work->field_0 = 3;
    }
}

void func_actor_110600_80138BD0(Task* arg0)
{
    Actor110600Work* work;
    OverlayWalker*   walker;
    GpEnemy*         enemy;
    u16              ramp;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 8;
        work->field_88C           = 2;
        work->field_892           = 0x10;
        work->field_896           = work->field_898;
        ramp                      = work->walker.field_5E;
        walker                    = &work->walker;
        work->walker.state        = 0;
        walker->field_5C          = 0;
        walker->field_5E          = ramp;
        walker->field_60          = 8;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_8A2           = 0;
    }
    func_actor_110600_80133A94(&work->walker);
    func_actor_110600_80134728(arg0);
    if (work->field_5C & 1) {
        work->field_0 = 3;
    }
}

void func_actor_110600_80138CA4(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    s16              i;

    work = arg0->work;
    i    = 0;
    if (work->field_4 != 0) {
        enemy                     = arg0->spawnArg2;
        obj                       = arg0->extra.tmd;
        obj->flags                = 0;
        work->field_A90.flags     = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags     = (u16)(work->field_950.flags | 0x4000);
        enemy->node.state.b.flags = 1;
        work->field_892           = 0x15;
        work->field_88C           = 2;
        work->walker.field_5A     = 0;
        work->field_8A4           = 0;
        work->field_896           = 0x10;
        work->field_88E           = 0;
        func_actor_110600_80134728(arg0);
        work->field_BE0 = 0;
        for (i = 0; i < 0x14; i++) {
            func_actor_110600_80134728(arg0);
        }
    }
    work->field_896 = -8;
    func_actor_110600_80134728(arg0);
}

void func_actor_110600_80138D7C(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    s16              step;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                       = arg0->extra.tmd;
        enemy->node.state.b.flags = 8;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_88C        = 2;
        work->field_892        = 5;
        work->field_896        = 0x30;
        work->field_950.flags |= 0x4000;
        work->field_A90.flags &= 0x7FFF;
        func_actor_110600_80134728(arg0);
        func_actor_110600_80134728(arg0);
        return;
    }
    arg0->extra.tmd->coords->flg = 0;
    step                         = (s16)work->field_896 / 2;
    work->field_896              = step;
    if (step == 1) {
        work->field_896 = -0x10;
    }
    if (work->field_896 == -1) {
        work->field_896 = 0x10;
    }
    func_actor_110600_80134728(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= 0xFD;
        work->field_0         = 3;
    }
}

/// Runs the enemy task's current state handler from the actor's three-entry
/// table (spawn, per-frame tick, teardown), copying the table onto the stack
/// before the call.
void func_actor_110600_80138EA8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_110600_80131FA0;
    sp.funcs[task->state](task->spawnArg2, task);
}
