#ifndef ACTOR_110600_H
#define ACTOR_110600_H

#include "common.h"

#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// One node of the patrol table `Actor110600WalkerNav::nodes` points at. The
/// three packed coordinates are copied straight out to the caller's `SVECTOR3`
/// and printed by the tsv rebuild, so they are signed halfwords; the walker
/// steps that only carry the bytes over read them through a `u16` cast.
typedef struct Actor110600WalkerNavNode {
    /* 0x0 */ s16  x;
    /* 0x2 */ s16  y;
    /* 0x4 */ s16  z;
    /* 0x6 */ byte pad_6[0x2];
} Actor110600WalkerNavNode;

/// The walker's patrol node table: the array `node` indexes, and the list
/// `func_actor_110600_80132470` measures the current step against. `count` is
/// the bound the nearest-node scan `func_actor_110600_80132958` walks.
/// `field_4` is the second byte table, the one the walker's own `cursor`
/// indexes to resolve the node it heads for next, one entry per patrol step.
/// Same shape as the acropolis bridge room's `AcropolisBridgeNavData`.
typedef struct Actor110600WalkerNav {
    /* 0x0 */ Actor110600WalkerNavNode* nodes;
    /* 0x4 */ u8*                       field_4;
    /* 0x8 */ u8                        count;
    /// Number of entries in `field_4`, the bound the route re-plan
    /// `func_actor_110600_80132A84` walks that byte table with. `count` bounds
    /// the node table, `field_9` the step table that indexes it.
    /* 0x9 */ u8   field_9;
    /* 0xA */ byte pad_A[0x2];
} Actor110600WalkerNav;

/// One patrol route: a 0xFF-terminated list of node indices plus the cursor
/// into it, which `func_actor_110600_80132654` wraps back to the first node at
/// the terminator. `arrived` is the flag that same step raises on the frame the
/// walker reaches the node it was heading for. Same shape as the acropolis
/// bridge room's route type, which drives the identical walker body.
typedef struct Actor110600WalkerRoute {
    /* 0x0 */ u8* nodes;
    /// Byte the tsv rebuild `func_actor_110600_80133778` clears alongside the
    /// cursor; the walker's own steps never read it back.
    /* 0x4 */ u8 field_4;
    /* 0x5 */ u8 cursor;
    /* 0x6 */ u8 arrived;
} Actor110600WalkerRoute;

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

/// View of the walker block `Actor110600Work` carries at 0xB28 — the shape the
/// acropolis bridge room drives, including the `D_80073B08` motion config the
/// byte at 0x6E indexes. `nav` and `route` point at the node tables stored
/// further down the same block (0x80 and 0x8C), `coord` is the coordinate the
/// per-tick step moves, and `field_5E` is the value `field_60` ramps towards
/// `field_5C` once per tick. The work block names the four fields it reads
/// back through its own pointer `field_B7C` / `field_B82` / `field_B86` /
/// `field_B90` instead: same bytes, reached with a constant offset. `node` is
/// the patrol node the walker is heading for and `field_62` / `field_64` the
/// movement deltas `func_actor_110600_80132654` clears once it arrives.
typedef struct Actor110600Walker {
    /* 0x00 */ Actor110600WalkerNav*   nav;
    /* 0x04 */ Actor110600WalkerRoute* route;
    /* 0x08 */ GsCOORDINATE2*          coord;
    /// The collision record table the per-frame behaviour step hands
    /// `func_800E0C10` to measure the walker's movement against: the twelve
    /// `GpRec18` records `Actor110600Work` carries at 0x970, whose count
    /// `field_56` holds, reached from here by address (the spawn writes
    /// `work + 0x970`). The acropolis bridge room's walker type names the same
    /// slot `field_C`.
    /* 0x0C */ GpRec18* recs;
    /* 0x10 */ byte     pad_10[0xC];
    /// How far the walker moves this frame, rebuilt every tick by the
    /// ramp-scaling step and left zeroed while the game is frozen.
    /* 0x1C */ SVECTOR moveStep;
    /// The whole-unit step the per-frame behaviour step applied to `coord` this
    /// frame, kept for the state handlers that follow.
    /* 0x24 */ SVECTOR moveDelta;
    /* 0x2C */ byte    pad_2C[0x8];
    /// The model's saved rotation, which the turn step copies back onto
    /// `coord` before rebuilding it around the yaw it just turned to.
    /* 0x34 */ MATRIX scaleMtx;
    /* 0x54 */ s16    scale;
    /// Number of `GpRec18` records in `recs`, handed to `func_800E0C10`
    /// alongside it and 12 at spawn; the acropolis bridge room's walker type
    /// names the same slot `field_56`.
    /* 0x56 */ s16  field_56;
    /* 0x58 */ byte pad_58[0x2];
    /* 0x5A */ s16  field_5A;
    /* 0x5C */ u16  field_5C;
    /* 0x5E */ u16  field_5E;
    /* 0x60 */ u16  field_60;
    /* 0x62 */ s16  field_62;
    /* 0x64 */ s16  field_64;
    /* 0x66 */ byte pad_66[0x2];
    /* 0x68 */ u8   state;
    /// The state the previous tick ran, which the step compares against
    /// `state` to re-resolve the patrol node whenever it changed.
    /* 0x69 */ u8 field_69;
    /* 0x6A */ u8 node;
    /* 0x6B */ u8 field_6B;
    /// The step's own byte gates: `field_6C` / `field_6D` skip the two
    /// per-frame sub-steps while non-zero.
    /* 0x6C */ u8 field_6C;
    /* 0x6D */ u8 field_6D;
    /// Index into the `D_80073B08` motion config table `Actor110600WalkerNav`
    /// positions are read from.
    /* 0x6E */ u8 field_6E;
    /// The node bytes the step keeps from the previous tick -- the spawn-side
    /// node, the nearest-node scan's answer and the live node -- so it can
    /// tell when any of them moved.
    /* 0x6F */ u8 field_6F;
    /* 0x70 */ u8 field_70;
    /* 0x71 */ u8 field_71;
    /* 0x72 */ u8 field_72;
    /// Per-step advance applied to `cursor` once the walker reaches its node;
    /// signed, so a route can be walked backwards.
    /* 0x73 */ s8   field_73;
    /* 0x74 */ byte pad_74[0x1];
    /// Slot of `nav`'s byte table naming the node nearest the actor the route
    /// re-plan was steered towards; the re-plan pairs it with `cursor` and
    /// leaves `field_73` to carry the cursor across the gap.
    /* 0x75 */ u8 field_75;
    /// Index into `nav`'s byte table of the patrol node the walker heads for.
    /* 0x76 */ u8   cursor;
    /* 0x77 */ byte pad_77[0x1];
    /// Whether the per-frame behaviour step left the coordinate translation
    /// non-zero in XZ, i.e. whether the walker moved at all this frame.
    /* 0x78 */ u8   moving;
    /* 0x79 */ byte pad_79[0x33];
} Actor110600Walker;
STATIC_ASSERT_SIZEOF(Actor110600Walker, 0xAC);

/// 0x10-byte scratch block the turn step carves off the frame it already
/// holds to stage the XZ offset between the position it is turning towards and
/// the walker's own coordinate translation. The offset is kept at full width
/// because the walker's translation is, and the block is released before
/// `ratan2` runs.
typedef struct Actor110600AvoidDelta {
    /* 0x0 */ s32  vx;
    /* 0x4 */ s32  vy;
    /* 0x8 */ s32  vz;
    /* 0xC */ byte pad_C[0x4];
} Actor110600AvoidDelta;
STATIC_ASSERT_SIZEOF(Actor110600AvoidDelta, 0x10);

/// 0x1C-byte scratch frame the turn step opens on `G_SCRATCH_HEAD`, nested
/// over the delta block above. `angle` is the yaw the walker ends the frame
/// facing: the wrapped bearing the turn counter just measured, clamped to the
/// per-frame limit and made absolute against the coordinate's own yaw.
typedef struct Actor110600TurnScratch {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ s16  angle;
    /* 0x1A */ byte pad_1A[0x2];
} Actor110600TurnScratch;
STATIC_ASSERT_SIZEOF(Actor110600TurnScratch, 0x1C);

/// 0xC-byte scratch block the aiming stage carves off `G_SCRATCH_HEAD` to
/// measure the camera-target delta against one hit sphere: the two XZ
/// components and the radius, squared in place and compared in the XZ plane.
/// Same block `Actor00100_OutsideRadius` measures with in
/// `actor_400100_motion.h`, which is the same helper one overlay over.
typedef struct Actor110600RadiusScratch {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 z;
    /* 0x8 */ s32 radius;
} Actor110600RadiusScratch;
STATIC_ASSERT_SIZEOF(Actor110600RadiusScratch, 0xC);

/// Whether the XZ delta in `pos` falls outside the sphere of `radius`: the two
/// components and the radius are squared through the scratch block, and the
/// comparison is returned as a value — the `>=` is what makes the caller test
/// the result against 0. The radius is a halfword so a `u16` cell can be fed
/// in directly.
static __inline__ s32 Actor110600_OutsideRadius(SVECTOR* pos, s16 radius)
{
    Actor110600RadiusScratch* head;
    Actor110600RadiusScratch* scratch;
    head                                         = *(Actor110600RadiusScratch**)G_SCRATCH_HEAD;
    scratch                                      = head - 1;
    *(Actor110600RadiusScratch**)G_SCRATCH_HEAD  = scratch;
    scratch->x                                   = pos->vx;
    scratch->z                                   = pos->vz;
    scratch->radius                              = radius;
    scratch->x                                  *= scratch->x;
    scratch->z                                  *= scratch->z;
    scratch->radius                             *= scratch->radius;
    *(Actor110600RadiusScratch**)G_SCRATCH_HEAD += 1;
    return scratch->x + scratch->z >= scratch->radius;
}

/// 0x14-byte scratch block `func_actor_110600_80132958` carves off
/// `G_SCRATCH_HEAD` to pick the patrol node nearest the walker. `dx` / `dz` are
/// the axis deltas for the node under test and `dist` their squared sum, which
/// is compared against the running `best` -- initialised to `-1` so the first
/// node always wins -- and `nearest` is the winning node's index. Same block
/// the acropolis bridge room's `func_acropolis_bridge_8018450C` scans in.
typedef struct Actor110600NearScratch {
    /* 0x00 */ s16  dx;
    /* 0x02 */ byte pad_2[0x2];
    /* 0x04 */ s16  dz;
    /* 0x06 */ byte pad_6[0x2];
    /* 0x08 */ u32  best;
    /* 0x0C */ u32  dist;
    /* 0x10 */ u8   node;
    /* 0x11 */ u8   nearest;
    /* 0x12 */ byte pad_12[0x2];
} Actor110600NearScratch;
STATIC_ASSERT_SIZEOF(Actor110600NearScratch, 0x14);

/// 0x18-byte scratch block the actor-relative variant of the scan carves off
/// `G_SCRATCH_HEAD`: the same nearest-node scan as above, but measured from the
/// translation of an actor config's matrix instead of the walker's own
/// coordinate. `cfg` is the `D_80073B08` entry being measured from -- the walker
/// runs it with the player (entry 1) to pick the node it retreats to -- `dy` is
/// staged but never enters the distance, and `best` / `nearest` carry the same
/// meaning as in the block above. Same block the acropolis bridge room's
/// `func_acropolis_bridge_801843A0` scans in.
typedef struct Actor110600NearCfgScratch {
    /* 0x00 */ s16           dx;
    /* 0x02 */ s16           dy;
    /* 0x04 */ s16           dz;
    /* 0x06 */ byte          pad_6[0x2];
    /* 0x08 */ PlayerStatus* cfg;
    /* 0x0C */ u32           best;
    /* 0x10 */ u32           dist;
    /* 0x14 */ u8            node;
    /* 0x15 */ u8            nearest;
    /* 0x16 */ byte          pad_16[0x2];
} Actor110600NearCfgScratch;
STATIC_ASSERT_SIZEOF(Actor110600NearCfgScratch, 0x18);

/// Returns the patrol node nearest the walker: the squared XZ distance between
/// each node and the low halfwords of the walker coordinate's translation,
/// with the running best and the cursor staged in the scratch block above.
u8 func_actor_110600_80132958(Actor110600Walker* work);

/// Returns the node the walker's route cursor steps onto, reseeding the scan's
/// stored node byte for the `actor` variant of the walker. Same body as the
/// acropolis bridge room's `func_acropolis_bridge_801843A0`.
u8 func_actor_110600_801327EC(Actor110600Walker* work, s32 actor);

/// Re-resolves the walker's patrol node against the route's byte table once
/// the state or the node bytes have moved. Same body as the acropolis bridge
/// room's `func_acropolis_bridge_80184638`.
void func_actor_110600_80132A84(Actor110600Walker* work, s16 actor);

/// 0x1C-byte scratch block the route re-plan carves off `G_SCRATCH_HEAD`.
/// `nodeA` is the patrol node nearest the actor the walker is reacting to and
/// `nodeB` the node nearest the walker itself; `listA` / `listB` collect every
/// slot of `nav`'s byte table that names each of them -- terminated by `0xFF`,
/// which is also why each list is only filled to eight entries -- and `i` / `j`
/// walk the two lists. `diff` is the signed step between the pair under test
/// and `best` the smallest absolute step seen so far, starting at `0xFF` so the
/// first pair always wins. Same block the acropolis bridge room's
/// `func_acropolis_bridge_80184638` carves off.
typedef struct Actor110600RouteScratch {
    /* 0x00 */ s16  diff;
    /* 0x02 */ byte pad_2[0x2];
    /* 0x04 */ u8   nodeA;
    /* 0x05 */ u8   nodeB;
    /* 0x06 */ u8   i;
    /* 0x07 */ u8   j;
    /* 0x08 */ u8   best;
    /* 0x09 */ u8   countA;
    /* 0x0A */ u8   countB;
    /* 0x0B */ byte pad_B[0x1];
    /* 0x0C */ u8   listB[8];
    /* 0x14 */ u8   listA[8];
} Actor110600RouteScratch;
STATIC_ASSERT_SIZEOF(Actor110600RouteScratch, 0x1C);

/// 0x18-byte scratch the walker's per-frame behaviour step carves off
/// `G_SCRATCH_HEAD`: the `GpDeltaScratch` `func_800E0C10` fills with the 16.16
/// step toward the current patrol node, followed by the whole-unit step
/// actually applied to the walker's coordinate this frame. Same block the
/// acropolis bridge room's `func_acropolis_bridge_80184908` carves off.
typedef struct Actor110600MoveScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ SVECTOR        move;
} Actor110600MoveScratch;
STATIC_ASSERT_SIZEOF(Actor110600MoveScratch, 0x18);

/// One per-frame behaviour step the walker runs while its `field_6C` gate is
/// clear: steps it toward its current patrol node. `func_800E0C10` produces the
/// 16.16 delta; the high half of each component becomes the whole-unit step,
/// rounded away from zero whenever a fraction is left over. While `field_6B` is
/// set the walker is pinned vertically, otherwise Y also carries a constant
/// 0x10 fall. Y is applied in three bands: a +8 hop above 0x20, a -0x20 drop
/// below -0x20, and the plain step in between. `moving` records whether the
/// frame produced any XZ motion at all. Same body as the acropolis bridge
/// room's `func_acropolis_bridge_80184908`.
void func_actor_110600_80132D54(Actor110600Walker* work);

/// The second per-frame behaviour step, gated on `field_6D`. Same body as the
/// acropolis bridge room's `func_acropolis_bridge_80184B94`.
void func_actor_110600_80132FE0(Actor110600Walker* work);

/// 0x28-byte scratch frame `func_actor_110600_80133A94` opens on
/// `G_SCRATCH_HEAD`: only the `SVECTOR3` at +4 is used, the position the step
/// steers the walker towards, which the turn step consumes and the behaviour
/// steps below overwrite. Same block as the acropolis bridge room's
/// `AcropolisBridgeWalkScratch`.
typedef struct Actor110600WalkScratch {
    /* 0x00 */ s32      field_0;
    /* 0x04 */ SVECTOR3 pos;
    /* 0x0A */ byte     pad_A[0x1E];
} Actor110600WalkScratch;
STATIC_ASSERT_SIZEOF(Actor110600WalkScratch, 0x28);

/// Turns the walker towards `pos` by at most `field_5A` angle units a frame.
/// The wrapped relative bearing drives the consecutive-turn counter, then
/// becomes the absolute yaw the model's saved scale matrix is rebuilt around.
void func_actor_110600_80133550(Actor110600Walker* work, SVECTOR3* pos);

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
    /// The two hit-sphere radii the aiming stage measures the camera-target
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
    /* 0x05E */ byte pad_5E[0x82E];
    /* 0x88C */ s16  field_88C;
    /* 0x88E */ s16  field_88E;
    /* 0x890 */ byte pad_890[2];
    /* 0x892 */ s16  field_892;
    /* 0x894 */ s16  field_894;
    /* 0x896 */ s16  field_896;
    /* 0x898 */ u16  field_898;
    /* 0x89A */ byte pad_89A[8];
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ byte pad_8A6[4];
    /// Timer `func_actor_110600_80137F2C` counts down once the actor is live,
    /// handing off to `func_actor_110600_80136210` on the frame it reaches
    /// zero; the same slot the `Actor01900Work` dispatcher keeps at 0xC10.
    /* 0x8AA */ s16  field_8AA;
    /* 0x8AC */ s32  field_8AC;
    /* 0x8B0 */ byte pad_8B0[1];
    /// Live flag `func_actor_110600_80137F2C` raises once the model root's
    /// coordinate has been cleared at the end of the tick.
    /* 0x8B1 */ s8   field_8B1;
    /* 0x8B2 */ byte pad_8B2[4];
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
    /* 0xB08 */ MATRIX field_B08;
    /* 0xB28 */ byte   pad_B28[0x54];
    /// The walker's own names for these four halfwords are `scale`,
    /// `field_5A`, `field_5E` and `state`; the work side reads them back
    /// through its own pointer, so both spellings are live in the code.
    /* 0xB7C */ u16   field_B7C;
    /* 0xB7E */ byte  pad_B7E[4];
    /* 0xB82 */ s16   field_B82;
    /* 0xB84 */ byte  pad_B84[2];
    /* 0xB86 */ u16   field_B86;
    /* 0xB88 */ byte  pad_B88[8];
    /* 0xB90 */ u8    field_B90;
    /* 0xB91 */ byte  pad_B91[0x43];
    /* 0xBD4 */ Task* field_BD4;
    /* 0xBD8 */ Task* field_BD8;
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
    /* 0xBE8 */ byte pad_BE8[4];
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

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::work` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900` / `Actor401800`.
typedef struct Actor110600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor110600Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor110600;

/// The actor's state handlers, indexed by `Actor110600Work::field_0`.
/// `func_actor_110600_80137F2C` copies the table to its frame before
/// dispatching, the same local jump table `Gp_EnemyDispatch` builds for the
/// shared `Gp_EnemyWaitFuncs`; four of the 25 slots are still unused.
typedef struct Actor110600StateTable {
    void (*fn[0x19])(Actor110600*);
} Actor110600StateTable;
STATIC_ASSERT_SIZEOF(Actor110600StateTable, 0x64);

extern const Actor110600StateTable D_actor_110600_80131F3C;

/// Message payload the `0x7D3` display handler is handed: `field_4` is the
/// requested state, `field_0` unused here. The same message id carries the
/// identical record as `Actor01900Msg7D3` / `Actor401800Msg7D3`.
typedef struct Actor110600Msg7D3 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor110600Msg7D3;

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
s32 func_actor_110600_80134040(Actor110600* arg0, s32 arg1, Actor110600Event* arg2);

/// The `0x7D3` display handler: parks the actor in state 0x11 with
/// `field_892` set from the requested state.
s32 func_actor_110600_8013839C(Actor110600* arg0, s32 arg1, Actor110600Msg7D3* arg2);

/// Rebuilds `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled independently on each axis through a
/// 0x34-byte block borrowed from the scratchpad. Marks the coordinate dirty.
void func_actor_110600_80138680(GsCOORDINATE2* coord, s16 sx, s16 sy, s16 sz);

s32  func_actor_110600_801387C0(Task* arg0);
void func_actor_110600_801388A4(Actor110600* arg0);

/// The remaining entries of `D_actor_110600_80131F3C` that are still only
/// present as assembly, so that the table can name them. `func_actor_110600_80135A18`
/// (state 4) and the ones above carry their own documentation.
void func_actor_110600_80135454(Actor110600* arg0);
void func_actor_110600_80136B20(Actor110600* arg0);
void func_actor_110600_801372CC(Actor110600* arg0);
void func_actor_110600_801377FC(Actor110600* arg0);
void func_actor_110600_80138980(Actor110600* arg0);
void func_actor_110600_80138A70(Actor110600* arg0);
void func_actor_110600_80138AFC(Actor110600* arg0);
void func_actor_110600_80138BD0(Actor110600* arg0);

/// The actor's per-tick model update, driven from `Task::work` /
/// `Task::spawnArg2` off the pointer it is handed.
void func_actor_110600_80134728(Actor110600* arg0);

/// Reports the sound cue the model is currently owed — 0 while there is none —
/// which `func_actor_110600_80134728` queues as the id's low byte. Watches the
/// pose of the animation slot the `field_892` state selects, reporting the cue
/// once per pose and remembering it in `field_8AC`.
s32 func_actor_110600_80134564(Actor110600AnimWork* anim);

/// Aiming stage: wraps the yaw from the model's root coordinate to the camera
/// target `D_80073B8C` against the coordinate's own yaw into `field_8A2`, ticks
/// the model, and moves the actor to state 3 once the walker's `field_5C` bit 0
/// arrives.
void func_actor_110600_80135A18(Actor110600* arg0);

/// Picks one of twelve hit positions out of `D_actor_110600_801485C4` by damage
/// magnitude `arg1`, then spawns effect `Gp_GetIdParam1(arg2)` on the model
/// part that entry names.
void func_actor_110600_80135E20(Actor110600* arg0, s16 arg1, s32 arg2);

/// Per-tick walker step: advances the animation the `field_68` byte selects,
/// resolves the patrol node the `field_6E` byte names against `D_80073B08`,
/// and ramp-scales the model matrix between `field_5E` and `field_5C`.
void func_actor_110600_80133A94(Actor110600Walker* walker);

/// 8-byte scratch block the walker's arrival test carves off `G_SCRATCH_HEAD`
/// to stage the delta between the patrol node the walker is heading for and
/// the walker's own coordinate translation. The node coordinates are copied
/// over as raw halfwords and then have the translation subtracted from them in
/// place, so the cells stay unsigned; `y` is flattened to zero because the test
/// only measures in the XZ plane. Same block the acropolis bridge room's
/// arrival test stages the identical walker body in.
typedef struct Actor110600ArrivalDelta {
    /* 0x0 */ u16  x;
    /* 0x2 */ u16  y;
    /* 0x4 */ u16  z;
    /* 0x6 */ byte pad_6[0x2];
} Actor110600ArrivalDelta;
STATIC_ASSERT_SIZEOF(Actor110600ArrivalDelta, 0x8);

/// 0xC-byte scratch block the arrival test's range check squares its three
/// operands in, nested inside the delta block its caller already holds.
typedef struct Actor110600ArrivalRange {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor110600ArrivalRange;
STATIC_ASSERT_SIZEOF(Actor110600ArrivalRange, 0xC);

/// Reports whether the XZ delta staged in `d` is at least `r` long, squaring
/// both sides in a 0xC-byte scratch block of its own so no comparison is done
/// on a square root. The same test `Actor110600_OutsideRadius` runs, staged
/// straight on the delta block rather than on a vector; the acropolis bridge
/// room's `acropolisBridgeOutOfRange` is the same body.
static __inline__ s32 Actor110600_ArrivalOutOfRange(Actor110600ArrivalDelta* d, s16 r)
{
    Actor110600ArrivalRange* b;
    u8*                      head;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0xC;
    b                     = (Actor110600ArrivalRange*)*(u8**)G_SCRATCH_HEAD;

    b->dx                 = (s16)d->x;
    b->dz                 = (s16)d->z;
    b->r                  = r;
    b->dx                 = b->dx * b->dx;
    b->dz                 = b->dz * b->dz;
    b->r                  = b->r * b->r;
    *(u8**)G_SCRATCH_HEAD = head;
    return b->dx + b->dz >= b->r;
}

/// Measures the walker's node against the coordinate it is moving towards,
/// leaving the three per-axis deltas in the scratchpad, and reports whether it
/// has arrived: 1 while the delta is inside either of two radii -- the walker's
/// own `field_5C * 4`, or a flat 300 -- and 0 once it is outside both.
s16 func_actor_110600_80132470(Actor110600Walker* walker);

/// Steers the walker along its patrol route: resolves the node the route
/// cursor names, and on the frame `func_actor_110600_80132470` reports arrival
/// it raises the route's `arrived` flag, clears the movement deltas and steps
/// the cursor onto the next node — wrapping back to the first at the 0xFF
/// terminator. `pos` receives the position of the node it is heading for, so
/// on the arrival frame it already describes the new node.
void func_actor_110600_80132654(Actor110600Walker* work, SVECTOR3* pos);

/// Enters work state 2 (`field_88C`) on a live actor: clear the model object,
/// clear bit 0x8000 of `field_A90.flags` and set 0x4000 of `field_950.flags`,
/// tag the enemy's link node, arm the `field_892` / `field_896` timers, then run
/// 20 update ticks before parking `field_896` at -8 and ticking once more.
void func_actor_110600_80138CA4(Actor110600* arg0);

/// Re-enters work state 2 on a live actor: clear the model root coordinate,
/// re-allocate its TMD buffers, tag the enemy's link node, arm `field_88C` /
/// `field_892` / `field_896` and the `field_950.flags` 0x4000 / `field_A90.flags`
/// 0x8000 masks, then tick twice. On a dead one it is the model-shrink tail:
/// halves `field_896` each tick — parking at -0x10 when the halving lands on 1
/// and bouncing -1 back to 0x10 — and once `Gp_TickObjFlag2` reports 1, drops
/// bit 1 of the enemy node's flags and moves the actor to state 3.
void func_actor_110600_80138D7C(Actor110600* arg0);

/// Placement opcode: seeds the model's root coordinate from `placement`, then
/// rebuilds and rescales it from the actor's own heading.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);

/// The game's shared 32-bit LCG state: a draw is
/// `Gp_LcgState = Gp_LcgState * 5 + 0x71357911`, read back from the global,
/// with the caller taking the bits it wants out of the high half.
extern u32 Gp_LcgState;

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
extern s8       D_actor_110600_80148392;
extern SVECTOR  D_actor_110600_80131F1C;

/// `Task::exitCallback` installed by the spawn handler: bump the two helper
/// tasks' `state` if present, unlink the three display nodes, drop the enemy's
/// `recs` slot, clear the screen shake, then `Gp_DestroyEnemy`.
void func_actor_110600_801387F4(Task* task);

#endif
