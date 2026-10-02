#ifndef OVERLAY_H
#define OVERLAY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/enemy.h"
#include "gameplay/geometry.h"

#include "main/coord.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/wipsys_types.h"

/*
 * Types that more than one overlay family carries.
 *
 * Every overlay is linked on its own, so code that several of them share was
 * compiled into each one. The layouts below are the ones that code repeats
 * across families - rooms and actors, and some also weapons and PE - and that
 * neither the gameplay nor the main executable owns; one declaration serves
 * every family.
 */

/// Work block of a full-screen fade task, allocated eight bytes at a time and
/// kept at `Task::work`: the three colour channels the fade overlay is drawn
/// with, stepped toward white or black each frame. The channels are signed,
/// since a fade-in ends when a channel goes negative. The leading halfword is
/// never touched.
typedef struct OverlayFadeWork {
    byte pad_0[0x2];
    s16  r;
    s16  g;
    s16  b;
} OverlayFadeWork;
STATIC_ASSERT_SIZEOF(OverlayFadeWork, 0x8);

/// Ramp context of a screen-wave task, handed to the task as its spawn
/// argument. Whoever spawns the task seeds `span`, `scale` and the tint; the
/// task clears `frame` and `state`, then counts `frame` up to `span` while
/// `state` is 0 and back down to zero while it is 1, and ends itself once
/// `state` is 2.
/// The wave's amplitude is `frame * scale / span`. A nonzero `blend` shades the
/// wave mesh with `r`, `g` and `b`; zero draws the raw frame-buffer copy.
typedef struct OverlayWaveCtx {
    s16 span;
    s16 scale;
    s16 state;
    s16 frame;
    u8  blend;
    u8  r;
    u8  g;
    u8  b;
} OverlayWaveCtx;
STATIC_ASSERT_SIZEOF(OverlayWaveCtx, 0xC);

/// One row's or column's sine wave in a screen-wave mesh whose phase tables
/// are padded to eight bytes: `phase` advances by `speed` every frame, and the
/// displacement a vertex takes is the sine of its position plus `phase` and
/// the fixed `offset`.
typedef struct OverlayWaveRec {
    s16 phase;
    s16 offset;
    s16 speed;
    s16 pad_6;
} OverlayWaveRec;
STATIC_ASSERT_SIZEOF(OverlayWaveRec, 0x8);

/// The same wave record in the meshes whose phase tables are packed six bytes
/// apart.
typedef struct OverlayWaveRec6 {
    s16 phase;
    s16 offset;
    s16 speed;
} OverlayWaveRec6;
STATIC_ASSERT_SIZEOF(OverlayWaveRec6, 0x6);

/// The scratch-pad block the padded screen-wave mesh takes from
/// the scratch stack for one frame: a copy of the row and column wave records
/// the mesh's vertices are displaced by.
typedef struct OverlayWaveScratch {
    OverlayWaveRec rows[30];
    OverlayWaveRec cols[9];
} OverlayWaveScratch;
STATIC_ASSERT_SIZEOF(OverlayWaveScratch, 0x138);

/// The scratch-pad block the world-space walk takes from the scratch stack:
/// `coord` is the frame the walk stands on, climbing the `GfxCoord::parent`
/// parent chain until it runs out, `vec` the vector being carried up, `out`
/// the GTE result it is refreshed from after each frame, and `flag` the GTE
/// flag word.
typedef struct OverlayWalkScratch {
    GfxCoord* coord;
    SVECTOR        vec;
    s32            out[3];
    s32            pad_18;
    s32            flag;
} OverlayWalkScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkScratch, 0x20);

/// The scratch-pad block of the push that steers a coordinate frame between
/// the obstacles in a `WorldCollisionContact` contact table. `eye` is the frame's world
/// position and `aim` the world point one unit ahead of it; `angle` holds each
/// record's bearing relative to the facing, 0x7FFE ending the list and 0x7FFF
/// marking a record that does not count. `kind` is the high half of a record's key,
/// `diff` the wrapped difference between two bearings, and `delta` first the
/// offset handed to `ratan2`, then the push added to the frame. `m` is the
/// working rotation, `i` and `j` the loop counters, and `hit` whether a push
/// was applied.
typedef struct OverlayBisectorScratch {
    MATRIX  m;
    byte    pad_20[0x80];
    SVECTOR delta;
    SVECTOR eye;
    SVECTOR aim;
    s32     kind;
    s16     angle[0x10];
    s16     i;
    s16     j;
    s16     diff;
    s16     hit;
} OverlayBisectorScratch;
STATIC_ASSERT_SIZEOF(OverlayBisectorScratch, 0xE4);

/// Wraps an angle into [-0x800, 0x800], spelled with backward jumps.
static __inline__ s16 overlayWrapAngle(s16 angle)
{
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    return angle;
}

/// Carries `v` from the frame of `coord` up the parent chain into world
/// space, walking in an `OverlayWalkScratch` taken from the scratch pad.
static __inline__ void overlayToWorld(GfxCoord* coord, SVECTOR* v)
{
    OverlayWalkScratch* blk;

    SCRATCH_STACK_CURSOR(OverlayWalkScratch)[-1].coord = coord;
    SCRATCH_STACK_RESERVE_BLOCK(OverlayWalkScratch);
    blk         = SCRATCH_STACK_CURSOR(OverlayWalkScratch);
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = (u16)blk->out[0];
        blk->vec.vy = (u16)blk->out[1];
        blk->vec.vz = (u16)blk->out[2];
        blk->coord  = blk->coord->parent;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(OverlayWalkScratch));
}

/// Carries `v` into world space, reserving the scratch block after copying
/// its initial coordinate and vector.
static __inline__ void overlayToWorld2(GfxCoord* coord, SVECTOR* v)
{
    OverlayWalkScratch* blk;

    blk         = (OverlayWalkScratch*)(SCRATCH_STACK_CURSOR(u8) - sizeof(OverlayWalkScratch));
    blk->coord  = coord;
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    SCRATCH_STACK_CURSOR(void) = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = (u16)blk->out[0];
        blk->vec.vy = (u16)blk->out[1];
        blk->vec.vz = (u16)blk->out[2];
        blk->coord  = blk->coord->parent;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(OverlayWalkScratch));
}

/// The scratch-pad block of an in-radius test on the XZ plane: the two
/// offsets and the radius, each squared in place before `dx + dz` is compared
/// with `r`.
typedef struct OverlayRangeScratch {
    s32 dx;
    s32 dz;
    s32 r;
} OverlayRangeScratch;
STATIC_ASSERT_SIZEOF(OverlayRangeScratch, 0xC);

/// Whether the XZ offset `d` reaches at least `r` from its origin.
static __inline__ s32 overlayOutOfRange(SVECTOR* d, s16 r)
{
    u8*                  head;
    OverlayRangeScratch* blk;
    s32                  ret;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((OverlayRangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                       = (OverlayRangeScratch*)(head - 0xC);
    blk->dz                                   = d->vz;
    blk->r                                    = r;
    ((OverlayRangeScratch*)(head - 0xC))->dx *= ((OverlayRangeScratch*)(head - 0xC))->dx;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch)         = blk;
    blk->dz                                  *= blk->dz;
    blk->r                                   *= blk->r;
    SCRATCH_STACK_CURSOR(u8)                          = head;
    ret                                       = ((OverlayRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// One enemy slot of a scripted encounter, in the table its controller works
/// through in order. `kind` selects the task that holds the slot's enemies
/// (one enemy from either of two tables, or a pair), and `command` is what
/// that task sends them as message 0x7DB once they are released. `status` is
/// 0 while the slot waits, 1 while its enemies are alive and 2 once they are
/// gone.
typedef struct OverlayEncounterSlot {
    s16  kind;
    s16  command;
    byte pad_4[0x2];
    s16  status;
} OverlayEncounterSlot;
STATIC_ASSERT_SIZEOF(OverlayEncounterSlot, 0x8);

/// Work block of a scripted encounter's controller: the frames counted before
/// the encounter is armed, the next slot to start, and a stop request, which
/// an actor's message 0x7DB with command 4 writes and which idles the
/// controller.
typedef struct OverlayEncounterCtrlWork {
    s16 frames;
    s16 nextSlot;
    s16 stop;
} OverlayEncounterCtrlWork;
STATIC_ASSERT_SIZEOF(OverlayEncounterCtrlWork, 0x6);

/// Work block of an encounter slot task that holds one enemy: the enemy, and
/// the frames counted before it is released.
typedef struct OverlayEncounterSingleWork {
    Enemy* enemy;
    s16      frames;
    byte     pad_6[0x2];
} OverlayEncounterSingleWork;
STATIC_ASSERT_SIZEOF(OverlayEncounterSingleWork, 0x8);

/// Work block of an encounter slot task that holds a pair of enemies: the two
/// enemies, the frames counted before the second is released, and a mask with
/// bit 0 set once the first is gone and bit 1 once the second is; the task
/// ends when both are.
typedef struct OverlayEncounterPairWork {
    Enemy* enemy0;
    Enemy* enemy1;
    s16      frames;
    s16      goneMask;
} OverlayEncounterPairWork;
STATIC_ASSERT_SIZEOF(OverlayEncounterPairWork, 0xC);

/// The enemy table the pair slots of a scripted encounter spawn their two
/// enemies from. It lies at a fixed address outside the images of the
/// overlays that reach it.
extern TaskDesc D_80151E60;

/// The scratch-pad block a screen-ripple drawer takes for one call. `mtx` is
/// the transposed view rotation, loaded as the GTE rotation for every
/// projection the drawer makes; each screen row's vector `row` is rotated
/// through it into `rowView`, and `origin` is the view translation brought
/// into the same frame. `depth` is divided by each row's height to find its
/// ordering-table depth. Nothing reads the tail; the block's size is
/// how far the drawer moves the scratch head.
typedef struct OverlayRippleScratch {
    MATRIX  mtx;
    SVECTOR row;
    SVECTOR rowView;
    SVECTOR origin;
    s32     depth;
    byte    pad_3C[0x10];
} OverlayRippleScratch;
STATIC_ASSERT_SIZEOF(OverlayRippleScratch, 0x4C);

/// Working state of the steering walk that nudges a coordinate away from the
/// obstacles among its contact records. `dir` is first the coordinate's
/// normalised facing column and later the push applied; `eye` is its world
/// position and `face` its heading. `angle` and `ok` hold the bearings of up
/// to `count` obstacles and whether each survived the pairwise spread check,
/// `kind` is the high half of the record being read, `diff` the wrapped
/// difference between two bearings, `i` and `j` the loop cursors, and
/// `blocked` is set when a record is of the blocking kind. `m` is the working
/// rotation.
typedef struct OverlayAvoidScratch {
    MATRIX   m;
    SVECTOR  dir;
    SVECTOR3 eye;
    byte     pad_2E[0x2];
    s32      kind;
    s16      angle[8];
    s8       ok[8];
    s16      face;
    s16      diff;
    u8       i;
    u8       j;
    u8       count;
    u8       blocked;
} OverlayAvoidScratch;
STATIC_ASSERT_SIZEOF(OverlayAvoidScratch, 0x54);

/// The scratch-pad block of a flat quad on the ground: its four corners in
/// world space and their projected screen positions.
typedef struct OverlayGroundScratch {
    SVECTOR vec[4];
    DVECTOR sxy0;
    DVECTOR sxy1;
    DVECTOR sxy2;
    DVECTOR sxy3;
} OverlayGroundScratch;
STATIC_ASSERT_SIZEOF(OverlayGroundScratch, 0x30);

/// The scratch-pad block of a sprite drawer projecting and sizing one
/// camera-facing quad: `vec` is the point it projects, `sxy` and `otz` the
/// resulting screen point and depth, and `dx`, `dy` the offsets from `sxy` to
/// the quad's corners, derived from `otz` so the sprite shrinks with distance.
typedef struct OverlaySpriteScratch {
    s32     otz;
    s32     dx;
    s32     dy;
    SVECTOR vec;
    DVECTOR sxy;
} OverlaySpriteScratch;
STATIC_ASSERT_SIZEOF(OverlaySpriteScratch, 0x18);

/// The scratch-pad block of a quad drawer that keeps the GTE flag word: the
/// depth the quad is sorted at, the flag, and the quad's four corners.
typedef struct OverlayFlaggedQuadScratch {
    s32     otz;
    s32     flag;
    SVECTOR v[4];
} OverlayFlaggedQuadScratch;
STATIC_ASSERT_SIZEOF(OverlayFlaggedQuadScratch, 0x28);

/// The scratch-pad block of two points projected one after the other, each
/// with a radius scaled by its own depth: `otz0` and `otz1` are the depths,
/// `flag` the GTE flag word of whichever projection ran last, `r0` and `r1`
/// the two radii and `sx0`, `sy0`, `sx1`, `sy1` the two screen points.
typedef struct OverlayPointPairScratch {
    s32 otz0;
    s32 otz1;
    s32 flag;
    s32 r0;
    s32 r1;
    u16 sx0;
    u16 sy0;
    u16 sx1;
    u16 sy1;
} OverlayPointPairScratch;
STATIC_ASSERT_SIZEOF(OverlayPointPairScratch, 0x1C);

/// One stack slot a body uses twice in a frame: first as the `VECTOR` position
/// `func_800D7A9C` lights a model at, then as the `SVECTOR` offset
/// `Gp_DrawFloorQuad` draws the floor quad with. The two uses never overlap.
typedef union OverlayVecSlot {
    VECTOR  vec;
    SVECTOR rot;
} OverlayVecSlot;
STATIC_ASSERT_SIZEOF(OverlayVecSlot, 0x10);

/// The offset from one position to another, widened to words and staged on
/// the scratch pad just long enough to take its bearing with `ratan2`.
typedef struct OverlayAvoidDelta {
    s32  vx;
    s32  vy;
    s32  vz;
    byte pad_C[0x4];
} OverlayAvoidDelta;
STATIC_ASSERT_SIZEOF(OverlayAvoidDelta, 0x10);

/// Bearing of `p` from `eye` on the XZ plane. The offset is staged on the
/// scratch pad at full width and released before `ratan2` runs.
static __inline__ s16 overlayBearingXZ(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                head;
    OverlayAvoidDelta* d;

    head             = SCRATCH_STACK_CURSOR(u8);
    d                = (OverlayAvoidDelta*)(head - 0x10);
    d->vx            = p->vx - eye->vx;
    SCRATCH_STACK_CURSOR(u8) = (u8*)d;
    d->vy            = p->vy - eye->vy;
    d->vz            = p->vz - eye->vz;
    SCRATCH_STACK_CURSOR(u8) = head;
    return ratan2(d->vx, d->vz);
}

/// Bearing of `p` from `eye` on the XY plane, the form the steering walk uses
/// while the coordinate's facing column is close to vertical.
static __inline__ s16 overlayBearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                head;
    OverlayAvoidDelta* d;

    head             = SCRATCH_STACK_CURSOR(u8);
    d                = (OverlayAvoidDelta*)(head - 0x10);
    d->vx            = p->vx - eye->vx;
    SCRATCH_STACK_CURSOR(u8) = (u8*)d;
    d->vy            = p->vy - eye->vy;
    d->vz            = p->vz - eye->vz;
    SCRATCH_STACK_CURSOR(u8) = head;
    return ratan2(d->vx, d->vy);
}

/// One node of the Boss Stranger's patrol table: a world position the walker
/// steers toward.
///
/// The coordinates are signed game coordinates. Code that copies a live
/// translation keeps its low halfword, and the arrival and nearest-node tests
/// subtract that same halfword. The trailing bytes are never read; they make
/// each node eight bytes.
typedef struct {
    s16  x;          // World X, in signed game-coordinate units
    s16  y;          // World Y, in signed game-coordinate units
    s16  z;          // World Z, in signed game-coordinate units
    byte pad_6[0x2]; // Unread; makes each node eight bytes
} OverlayWalkerNode;
STATIC_ASSERT_SIZEOF(OverlayWalkerNode, 0x8);

/// Nav record of a Boss Stranger patrol walker: the positions it can steer
/// toward, and the order of node indices state 2 walks while closing on a
/// player.
///
/// `nodeCount` is how many leading entries of `nodes` the nearest-node scans
/// walk. It is not the allocated length of that table: a patrol route may
/// name a later entry. `nodeOrder` is a second index list, distinct from the
/// route. It holds `orderCount` node indices and has no end marker. The
/// walker's `cursor` indexes it, and the re-plan searches it for the steps
/// that name two nodes.
typedef struct {
    OverlayWalkerNode* nodes;      // Positions the walker steers toward
    u8*                nodeOrder;  // Node index at each step. The walker's cursor indexes this
    u8                 nodeCount;  // Leading nodes the nearest-node scans walk
    u8                 orderCount; // Live entries in nodeOrder
    byte               pad_A[0x2]; // Unread. Keeps the record twelve bytes
} OverlayWalkerNav;
STATIC_ASSERT_SIZEOF(OverlayWalkerNav, 0xC);

/// End of a Boss Stranger patrol route. The cursor returns to the first index
/// when the entry it lands on has this value.
#define OVERLAY_WALKER_ROUTE_END 0xFF

/// One Boss Stranger patrol route: the order of nav-node indices the walker
/// follows, and the cursor into that order.
///
/// `nodeIndices` runs until `OVERLAY_WALKER_ROUTE_END`. On the frame the
/// walker reaches the node it is heading for, `arrived` is raised and `cursor`
/// advances, returning to the first index at the end marker.
typedef struct {
    u8*  nodeIndices; // Indices into the walker's nav node table, ended by OVERLAY_WALKER_ROUTE_END
    u8   field_4;     // Role unproven. Actor spawn stores 2 and the route rebuild stores 0; nothing reads it
    u8   cursor;      // Index of the current entry. Wraps to 0 at the end marker
    u8   arrived;     // 1 on the frame the current node is reached, otherwise 0
    byte pad_7[0x1];  // Unread. Pointer alignment rounds the route to eight bytes
} OverlayWalkerRoute;
STATIC_ASSERT_SIZEOF(OverlayWalkerRoute, 0x8);

/// State of a patrol walker: an enemy that steers its coordinate from node to
/// node of a patrol table, backs away from the obstacles among its contact
/// records and scales its model in and out. `nav` and `route` point at the
/// tables it walks, normally `navData` and `routeData`; `node` is the node it
/// is heading for and `cursor` its index in `nav`'s `nodeOrder`. `recs` is the
/// collision table the movement step measures the walker against, with
/// `field_56` records, and `avoidRecs` the `avoidCount` contact records the
/// avoidance step backs away from, adding what it moves to `push` and setting
/// `blocked` when a record is of the blocking kind. `moveStep` is how far the
/// walker moves this frame and `moveDelta` the whole-unit step the movement
/// step applied; `moving` says whether that moved it in XZ at all. `scaleMtx`
/// is the model's saved rotation, rebuilt around `scale`. `state` selects the
/// walker's behaviour, `field_69` is the state the previous tick ran, and
/// `field_6C` / `field_6D` skip the movement and avoidance steps while set.
/// `field_6E` indexes the actor configuration table the walker is measured
/// against, `field_73` is the signed advance applied to `cursor` on arrival,
/// `field_5A` is the most the walker may turn in one frame, `field_62` counts
/// the consecutive frames it has been turning and `field_64` is an extra
/// allowance added to that limit; the counter and allowance are cleared on
/// arrival.
typedef struct OverlayWalker {
    OverlayWalkerNav*   nav;
    OverlayWalkerRoute* route;
    GfxCoord*      coord;
    WorldCollisionContact*            recs;
    WorldCollisionContact*            avoidRecs;
    byte                pad_14[0x8];
    SVECTOR             moveStep;
    SVECTOR             moveDelta;
    SVECTOR3            push;
    byte                pad_32[0x2];
    MATRIX              scaleMtx;
    s16                 scale;
    s16                 field_56;
    s16                 avoidCount;
    u16                 field_5A;
    u16                 field_5C;
    u16                 field_5E;
    u16                 field_60;
    u16                 field_62;
    u16                 field_64;
    byte                pad_66[0x2];
    u8                  state;
    u8                  field_69;
    u8                  node;
    u8                  field_6B;
    u8                  field_6C;
    u8                  field_6D;
    u8                  field_6E;
    u8                  field_6F;
    u8                  field_70;
    u8                  field_71;
    u8                  field_72;
    s8                  field_73;
    byte                pad_74[0x1];
    u8                  field_75;
    u8                  cursor;
    u8                  blocked;
    u8                  moving;
    byte                pad_79[0x7];
    OverlayWalkerNav    navData;
    OverlayWalkerRoute  routeData;
} OverlayWalker;
STATIC_ASSERT_SIZEOF(OverlayWalker, 0x94);

/// The scratch-pad block a patrol walker's arrival test stages the offset
/// from the walker to its node in. The node's coordinates are copied over and
/// the walker's translation subtracted in place; `y` is flattened to zero
/// because the test only measures in the XZ plane.
typedef struct OverlayWalkerArrivalDelta {
    u16  x;
    u16  y;
    u16  z;
    byte pad_6[0x2];
} OverlayWalkerArrivalDelta;
STATIC_ASSERT_SIZEOF(OverlayWalkerArrivalDelta, 0x8);

/// The scratch-pad block of a patrol walker's nearest-node scan from its own
/// coordinate: `dx` and `dz` are the offsets to the node under test and
/// `dist` their squared sum, compared with the running `best`, which starts
/// at -1 so the first node always wins; `nearest` is the winner.
typedef struct OverlayWalkerNearScratch {
    s16  dx;
    byte pad_2[0x2];
    s16  dz;
    byte pad_6[0x2];
    u32  best;
    u32  dist;
    u8   node;
    u8   nearest;
    byte pad_12[0x2];
} OverlayWalkerNearScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkerNearScratch, 0x14);

/// The same nearest-node scan measured from the coordinate of the actor
/// configuration `cfg` instead of the walker's own; `dy` is staged but never
/// enters the distance.
typedef struct OverlayWalkerNearCfgScratch {
    s16           dx;
    s16           dy;
    s16           dz;
    byte          pad_6[0x2];
    PlayerStatus* cfg;
    u32           best;
    u32           dist;
    u8            node;
    u8            nearest;
    byte          pad_16[0x2];
} OverlayWalkerNearCfgScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkerNearCfgScratch, 0x18);

/// The scratch-pad block of a patrol walker's re-plan along `nodeOrder`.
/// `nodeA` is the node nearest the actor the walker reacts to and `nodeB` the
/// node nearest the walker; `listA` and `listB` collect the `nodeOrder` slots
/// naming each, terminated by 0xFF, and `i` and `j` walk them. `diff` is the
/// signed step between the pair under test and `best` the smallest seen,
/// starting at 0xFF so the first pair always wins.
typedef struct OverlayWalkerRouteScratch {
    s16  diff;
    byte pad_2[0x2];
    u8   nodeA;
    u8   nodeB;
    u8   i;
    u8   j;
    u8   best;
    u8   countA;
    u8   countB;
    byte pad_B[0x1];
    u8   listB[8];
    u8   listA[8];
} OverlayWalkerRouteScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkerRouteScratch, 0x1C);

/// The scratch-pad block of a patrol walker's movement step: the 16.16 step
/// `func_800E0C10` resolves toward the node, then the whole-unit step applied
/// to the walker's coordinate.
typedef struct OverlayWalkerMoveScratch {
    WorldCollisionDelta delta;
    SVECTOR        move;
} OverlayWalkerMoveScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkerMoveScratch, 0x18);

/// The scratch-pad frame a patrol walker's tick opens: `pos` is the position
/// the walker steers for this frame. Nothing else in the frame is read.
typedef struct OverlayWalkerTickScratch {
    s32      field_0;
    SVECTOR3 pos;
    byte     pad_A[0x1E];
} OverlayWalkerTickScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkerTickScratch, 0x28);

/// The scratch-pad frame a patrol walker's turn step opens, around the
/// bearing delta it stages below `angle`: first the bearing relative to the
/// walker's yaw, then that turn clamped to the per-frame limit, and finally
/// the absolute yaw the walker ends the frame facing.
typedef struct OverlayWalkerTurnScratch {
    byte pad_0[0x18];
    s16  angle;
    byte pad_1A[0x2];
} OverlayWalkerTurnScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkerTurnScratch, 0x1C);

/// Whether the XZ offset staged in `d` is at least `r` long, squaring both
/// sides in a scratch block of its own.
static __inline__ s32 overlayWalkerOutOfRange(OverlayWalkerArrivalDelta* d, s16 r)
{
    OverlayRangeScratch* b;
    u8*                  head;

    head             = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0xC;
    b                = SCRATCH_STACK_CURSOR(OverlayRangeScratch);

    b->dx            = (s16)d->x;
    b->dz            = (s16)d->z;
    b->r             = r;
    b->dx            = b->dx * b->dx;
    b->dz            = b->dz * b->dz;
    b->r             = b->r * b->r;
    SCRATCH_STACK_CURSOR(u8) = head;
    return b->dx + b->dz >= b->r;
}

/// Bearing of `pos` from the full-width translation of `coord` on the XZ
/// plane. The offset is staged on the scratch pad and released before
/// `ratan2` runs.
static __inline__ s32 overlayCoordBearingXZ(SVECTOR3* pos, GfxCoord* coord)
{
    u8*                head;
    OverlayAvoidDelta* d;
    head             = SCRATCH_STACK_CURSOR(u8);
    d                = (OverlayAvoidDelta*)(head - 0x10);
    d->vx            = pos->vx - coord->coord.t[0];
    SCRATCH_STACK_CURSOR(u8) = (u8*)d;
    d->vy            = pos->vy - coord->coord.t[1];
    d->vz            = pos->vz - coord->coord.t[2];
    SCRATCH_STACK_CURSOR(u8) = head;
    return ratan2(d->vx, d->vz);
}

/// The scratch-pad block of a step resolved against contact records: the
/// 16.16 deltas `func_800E0C10` resolves, then whether the X or Z delta was
/// nonzero.
typedef struct OverlayDeltaFlag {
    WorldCollisionDelta delta;
    s32            moved;
} OverlayDeltaFlag;
STATIC_ASSERT_SIZEOF(OverlayDeltaFlag, 0x14);

/// One entry of a 0xFFFF-terminated hotspot table: a screen rectangle `x`,
/// `y`, `w`, `h` the action cursor is tested against. A hit raises `hit` on
/// that entry and clears it on every other; the owner then reads `id`, the
/// script variant the hotspot selects, and `promptKind` from the raised entry.
typedef struct OverlayHotspot {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
    s16 id;
    u8  promptKind;
    s8  hit;
} OverlayHotspot;
STATIC_ASSERT_SIZEOF(OverlayHotspot, 0xC);

/// One window of a caption schedule, a table ordered by descending `upper`
/// and ended by an `upper` of -1. While the session's scene clock lies in
/// (`lower * 30`, `upper * 30`], the first matching window starts caption
/// script `script` at line key `key`.
typedef struct OverlayCapWindow {
    s32 upper;
    s32 lower;
    s32 script;
    s32 key;
} OverlayCapWindow;
STATIC_ASSERT_SIZEOF(OverlayCapWindow, 0x10);

/// The spawn argument of a screen-capture task: `duration` seeds the task's
/// kill countdown, and `done` is cleared when the task starts, set when the
/// countdown runs out or the owner cancels the capture, and ends the task once
/// it is nonzero.
typedef struct OverlayCaptureArgs {
    u16 duration;
    s16 done;
} OverlayCaptureArgs;
STATIC_ASSERT_SIZEOF(OverlayCaptureArgs, 0x4);

/// A model's vertex morph: `vertices` / `normals` are the target shape
/// (`normals` null when the model has no normal pass), `savedVertices` /
/// `savedNormals` the snapshot of its rest shape taken at setup -
/// `vertexCount` and `normalCount` of them - and the blend runs over
/// `blendCount` vertices from `firstVertex`. A room declares one in its data
/// and an actor can blend a room's (actor_323300 the Dryfield toilet's).
typedef struct OverlayMorphTarget {
    /* 0x00 */ SVECTOR* vertices;
    /* 0x04 */ SVECTOR* normals;
    /* 0x08 */ SVECTOR* savedVertices;
    /* 0x0C */ SVECTOR* savedNormals;
    /* 0x10 */ s16      vertexCount;
    /* 0x12 */ s16      normalCount;
    /* 0x14 */ s16      firstVertex;
    /* 0x16 */ s16      blendCount;
} OverlayMorphTarget;
STATIC_ASSERT_SIZEOF(OverlayMorphTarget, 0x18);

#endif /* OVERLAY_H */
