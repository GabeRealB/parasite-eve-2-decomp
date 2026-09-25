#ifndef OVERLAY_H
#define OVERLAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

/*
 * Types that room and actor overlays both carry.
 *
 * Every overlay is linked on its own, so code that several of them share was
 * compiled into each one. The layouts below are the ones that code repeats in
 * rooms and actors alike and that neither the gameplay nor the main executable
 * owns; one declaration serves both families.
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
/// `G_SCRATCH_HEAD` for one frame: a copy of the row and column wave records
/// the mesh's vertices are displaced by.
typedef struct OverlayWaveScratch {
    OverlayWaveRec rows[30];
    OverlayWaveRec cols[9];
} OverlayWaveScratch;
STATIC_ASSERT_SIZEOF(OverlayWaveScratch, 0x138);

/// The scratch-pad block the world-space walk takes from `G_SCRATCH_HEAD`:
/// `coord` is the frame the walk stands on, climbing the `GsCOORDINATE2::sub`
/// parent chain until it runs out, `vec` the vector being carried up, `out`
/// the GTE result it is refreshed from after each frame, and `flag` the GTE
/// flag word.
typedef struct OverlayWalkScratch {
    GsCOORDINATE2* coord;
    SVECTOR        vec;
    s32            out[3];
    s32            pad_18;
    s32            flag;
} OverlayWalkScratch;
STATIC_ASSERT_SIZEOF(OverlayWalkScratch, 0x20);

/// The scratch-pad block of the push that steers a coordinate frame between
/// the obstacles in a `GpRec18` contact table. `eye` is the frame's world
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

/// Carries `v` from the frame of `coord` up the parent chain into world
/// space, walking in an `OverlayWalkScratch` taken from the scratch pad.
static __inline__ void overlayToWorld(GsCOORDINATE2* coord, SVECTOR* v)
{
    OverlayWalkScratch* blk;

    {
        register GsCOORDINATE2* parent asm("v0");
        parent                                                                                    = coord;
        ((OverlayWalkScratch*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(OverlayWalkScratch)))->coord = parent;
    }
    {
        register u8* tmp asm("v0");
        tmp = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(OverlayWalkScratch);
        blk = (OverlayWalkScratch*)tmp;
    }
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
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

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(OverlayWalkScratch);
}

/// The walk of `overlayToWorld` without its register bindings. The callers
/// were compiled from both spellings, and each site matches only its own.
static __inline__ void overlayToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
{
    OverlayWalkScratch* blk;

    blk         = (OverlayWalkScratch*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(OverlayWalkScratch));
    blk->coord  = coord;
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
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

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(OverlayWalkScratch);
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

    head                                      = *(u8**)G_SCRATCH_HEAD;
    ((OverlayRangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                       = (OverlayRangeScratch*)(head - 0xC);
    blk->dz                                   = d->vz;
    blk->r                                    = r;
    ((OverlayRangeScratch*)(head - 0xC))->dx *= ((OverlayRangeScratch*)(head - 0xC))->dx;
    *(OverlayRangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                  *= blk->dz;
    blk->r                                   *= blk->r;
    *(u8**)G_SCRATCH_HEAD                     = head;
    ret                                       = ((OverlayRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// The scratch-pad block of a sprite spun about one projected point: `vec` is
/// the point, and one `RTPS` fills `sx`, `sy`, `flag` and `otz`. `dx` and `dy`
/// are the half extent scaled by the depth and rotated by the spin angle; they
/// are added to and subtracted from the projected point to place the corners,
/// and only their low halves are read back.
typedef struct OverlaySparkScratch {
    s32     otz;
    s32     dx;
    s32     dy;
    s32     flag;
    SVECTOR vec;
    s16     sx;
    s16     sy;
} OverlaySparkScratch;
STATIC_ASSERT_SIZEOF(OverlaySparkScratch, 0x1C);

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
    GpEnemy* enemy;
    s16      frames;
    byte     pad_6[0x2];
} OverlayEncounterSingleWork;
STATIC_ASSERT_SIZEOF(OverlayEncounterSingleWork, 0x8);

/// Work block of an encounter slot task that holds a pair of enemies: the two
/// enemies, the frames counted before the second is released, and a mask with
/// bit 0 set once the first is gone and bit 1 once the second is; the task
/// ends when both are.
typedef struct OverlayEncounterPairWork {
    GpEnemy* enemy0;
    GpEnemy* enemy1;
    s16      frames;
    s16      goneMask;
} OverlayEncounterPairWork;
STATIC_ASSERT_SIZEOF(OverlayEncounterPairWork, 0xC);

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

/// A `MATRIX` that can also be written through `GpMtxWords`.
typedef union OverlayMat {
    MATRIX     mat;
    GpMtxWords ident;
} OverlayMat;
STATIC_ASSERT_SIZEOF(OverlayMat, 0x20);

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

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (OverlayAvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// Bearing of `p` from `eye` on the XY plane, the form the steering walk uses
/// while the coordinate's facing column is close to vertical.
static __inline__ s16 overlayBearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                head;
    OverlayAvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (OverlayAvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vy);
}

/// One node of a patrol walker's node table: a position the walker can steer
/// for.
typedef struct OverlayWalkerNode {
    s16  x;
    s16  y;
    s16  z;
    byte pad_6[0x2];
} OverlayWalkerNode;
STATIC_ASSERT_SIZEOF(OverlayWalkerNode, 0x8);

/// A patrol walker's node table: `nodes` holds `count` positions, and
/// `field_4` is a byte table of `field_9` node indices, one per patrol step,
/// which the walker's own `cursor` walks and the route re-plan searches.
typedef struct OverlayWalkerNav {
    OverlayWalkerNode* nodes;
    u8*                field_4;
    u8                 count;
    u8                 field_9;
    byte               pad_A[0x2];
} OverlayWalkerNav;
STATIC_ASSERT_SIZEOF(OverlayWalkerNav, 0xC);

/// One patrol route: a 0xFF-terminated list of node indices and the cursor
/// into it, which wraps back to the first node at the terminator. `arrived` is
/// raised on the frame the walker reaches the node it was heading for.
typedef struct OverlayWalkerRoute {
    u8* nodes;
    u8  field_4;
    u8  cursor;
    u8  arrived;
} OverlayWalkerRoute;
STATIC_ASSERT_SIZEOF(OverlayWalkerRoute, 0x8);

/// State of a patrol walker: an enemy that steers its coordinate from node to
/// node of a patrol table, backs away from the obstacles among its contact
/// records and scales its model in and out. `nav` and `route` point at the
/// tables it walks, normally `navData` and `routeData`; `node` is the node it
/// is heading for and `cursor` its index in the nav byte table. `recs` is the
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
/// and `field_62` / `field_64` are movement deltas cleared on arrival.
typedef struct OverlayWalker {
    OverlayWalkerNav*   nav;
    OverlayWalkerRoute* route;
    GsCOORDINATE2*      coord;
    GpRec18*            recs;
    GpRec18*            avoidRecs;
    byte                pad_14[0x8];
    SVECTOR             moveStep;
    SVECTOR             moveDelta;
    SVECTOR3            push;
    byte                pad_32[0x2];
    MATRIX              scaleMtx;
    s16                 scale;
    s16                 field_56;
    s16                 avoidCount;
    s16                 field_5A;
    u16                 field_5C;
    u16                 field_5E;
    u16                 field_60;
    s16                 field_62;
    s16                 field_64;
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

/// The scratch-pad block of a patrol walker's route re-plan. `nodeA` is the
/// node nearest the actor the walker reacts to and `nodeB` the node nearest
/// the walker; `listA` and `listB` collect the nav byte-table slots naming
/// each, terminated by 0xFF, and `i` and `j` walk them. `diff` is the signed
/// step between the pair under test and `best` the smallest seen, starting at
/// 0xFF so the first pair always wins.
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
    GpDeltaScratch delta;
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

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0xC;
    b                     = (OverlayRangeScratch*)*(u8**)G_SCRATCH_HEAD;

    b->dx                 = (s16)d->x;
    b->dz                 = (s16)d->z;
    b->r                  = r;
    b->dx                 = b->dx * b->dx;
    b->dz                 = b->dz * b->dz;
    b->r                  = b->r * b->r;
    *(u8**)G_SCRATCH_HEAD = head;
    return b->dx + b->dz >= b->r;
}

/// Bearing of `pos` from the full-width translation of `coord` on the XZ
/// plane. The offset is staged on the scratch pad and released before
/// `ratan2` runs.
static __inline__ s32 overlayCoordBearingXZ(SVECTOR3* pos, GsCOORDINATE2* coord)
{
    u8*                head;
    OverlayAvoidDelta* d;
    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (OverlayAvoidDelta*)(head - 0x10);
    d->vx                 = pos->vx - coord->coord.t[0];
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = pos->vy - coord->coord.t[1];
    d->vz                 = pos->vz - coord->coord.t[2];
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// The scratch-pad block of a step resolved against contact records: the
/// 16.16 deltas `func_800E0C10` resolves, then whether the X or Z delta was
/// nonzero.
typedef struct OverlayDeltaFlag {
    GpDeltaScratch delta;
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

#endif /* OVERLAY_H */
