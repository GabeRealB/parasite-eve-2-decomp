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

/// How a screen-wave quad treats the captured frame.
///
/// Stored in `ScreenWaveCtx.modulateTexture`. Zero copies the frame's own
/// colours. Any other value multiplies the frame by the context's `r`, `g`
/// and `b`. Senders that tint store `SCREEN_WAVE_MODULATE_TEXTURE`.
enum {
    /// Copy the captured frame without colour modulation.
    SCREEN_WAVE_TEXTURE_RAW = 0,
    /// Multiply the captured frame by the context's colour.
    SCREEN_WAVE_MODULATE_TEXTURE = 1,
};

/// Phase of a screen-wave ramp, stored in `ScreenWaveCtx.state`.
///
/// Rising counts `frame` up to `span` and holds it there. Falling counts
/// `frame` back toward zero, then stores finished. Finished ends the task.
/// A context may already be finished when the task is spawned; the task
/// resets the phase to rising on its first tick.
enum {
    /// Count `frame` up to `span`.
    SCREEN_WAVE_RAMP_RISING = 0,
    /// Count `frame` down toward zero, then finish.
    SCREEN_WAVE_RAMP_FALLING = 1,
    /// End the screen-wave task.
    SCREEN_WAVE_RAMP_FINISHED = 2,
};

/// Ramp context of a screen-wave task, handed to the task as its spawn argument.
///
/// The spawner seeds `span`, `scale` and the texture colour. The task clears
/// `frame`, sets `state` to rising, and the displacement amplitude each frame
/// is `frame * scale / span`.
typedef struct {
    s16 span;            // Rise length in frames; amplitude peaks when `frame` reaches it
    s16 scale;           // Peak displacement strength, reached at the end of the rise
    s16 state;           // Ramp phase (SCREEN_WAVE_RAMP_RISING, FALLING or FINISHED)
    s16 frame;           // Position along the ramp, in frames
    u8  modulateTexture; // (SCREEN_WAVE_TEXTURE_RAW, or nonzero to tint with r, g, b)
    u8  r;               // Modulation red (0-255), applied while tinting
    u8  g;               // Modulation green (0-255), applied while tinting
    u8  b;               // Modulation blue (0-255), applied while tinting
} ScreenWaveCtx;
STATIC_ASSERT_SIZEOF(ScreenWaveCtx, 0xC);

/// One row's or column's sine wave in the screen-wave grid task's mesh.
///
/// The grid task keeps nine column waves and thirty row waves in arrays of
/// these, which their packages own as `gScreenWaveColumns` and
/// `gScreenWaveRows`. Each frame `phase` advances by `speed`, and a quad
/// corner on the line is pushed by the sine of its position along the line
/// plus `phase` and `offset`. Angles use the `rsin` scale, 4096 to a turn.
/// The 10x30 task's records are the six-byte `ScreenWaveOscillator`.
typedef struct {
    s16 phase;   // Current phase, advanced by `speed` each frame while actors run; starts at 0
    s16 offset;  // Fixed random phase offset, 0 to 4095, set when the task starts
    s16 speed;   // Phase step per frame, 20 to 119
    s16 field_6; // Never read or written by the grid task; role unproven
} ScreenWaveGridOscillator;
STATIC_ASSERT_SIZEOF(ScreenWaveGridOscillator, 0x8);

/// One grid line's sine wave in `screenWaveTask`'s 10x30 screen ripple.
///
/// The task keeps one per vertical grid edge (11 columns) and one per
/// horizontal edge (30 rows), in the arrays their packages own as
/// `gScreenWaveColumns` and `gScreenWaveRows`. A column wave pushes the x of
/// the corners on its edge, a row wave their y; each corner is displaced by
/// the sine of its position along the line plus `phase` and `offset`, scaled
/// by the ramp. Angles use the `rsin` scale, 4096 to a turn. The grid task's
/// records are the eight-byte `ScreenWaveGridOscillator`.
typedef struct {
    s16 phase;  // Current phase, advanced by `speed` each frame; starts at 0
    s16 offset; // Fixed random phase offset, 0 to 4095, set when the task starts
    s16 speed;  // Phase step per frame, 0 to 99, set when the task starts
} ScreenWaveOscillator;
STATIC_ASSERT_SIZEOF(ScreenWaveOscillator, 0x6);

/// The scratch-pad block the padded screen-wave mesh takes from
/// the scratch stack for one frame: a copy of the row and column wave records
/// the mesh's vertices are displaced by.
typedef struct OverlayWaveScratch {
    ScreenWaveGridOscillator rows[30];
    ScreenWaveGridOscillator cols[9];
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

/// Scratch-stack block for projecting two world points and sizing a primitive
/// around each.
///
/// The points are transformed one after the other. Each transform writes that
/// point's screen position as one screen-XY word, so its two halves stay
/// adjacent, and replaces `flag` with the GTE flag word. A negative flag word
/// means that transform reported an error, and the drawer stops. Otherwise the
/// ordering-table depth is stored. The on-screen radius is a caller-chosen
/// numerator divided by the depth stored for that point, and a caller may
/// adjust the stored depth before the division. Code that subtracts two screen
/// halves sign-extends each one: the halves are the raw encoding of signed GTE
/// pixel coordinates.
typedef struct {
    s32 otz0;    // Ordering-table depth of the first point, and the divisor for its radius
    s32 otz1;    // Ordering-table depth of the second point, and the divisor for its radius
    s32 flag;    // GTE flag word of the latest transform; negative means that transform failed
    s32 radius0; // On-screen radius at the first point, in pixels
    s32 radius1; // On-screen radius at the second point, in pixels
    u16 sx0;     // Raw projected X of the first point; first half of its screen-XY word
    u16 sy0;     // Raw projected Y of the first point; second half of that word
    u16 sx1;     // Raw projected X of the second point; first half of its screen-XY word
    u16 sy1;     // Raw projected Y of the second point; second half of that word
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
} BossStrangerNode;
STATIC_ASSERT_SIZEOF(BossStrangerNode, 0x8);

/// Nav record of a Boss Stranger patrol walker: the positions it can steer
/// toward, and the order of node indices the close-in state walks.
///
/// `nodeCount` is how many leading entries of `nodes` the nearest-node scans
/// walk. It is not the allocated length of that table: a patrol route may
/// name a later entry. `nodeOrder` is a second index list, distinct from the
/// route. It holds `orderCount` node indices and has no end marker. The
/// walker's `cursor` indexes it, and the re-plan searches it for the steps
/// that name two nodes.
typedef struct {
    BossStrangerNode* nodes;      // Positions the walker steers toward
    u8*                nodeOrder;  // Node index at each step. The walker's cursor indexes this
    u8                 nodeCount;  // Leading nodes the nearest-node scans walk
    u8                 orderCount; // Live entries in nodeOrder
    byte               pad_A[0x2]; // Unread. Keeps the record twelve bytes
} BossStrangerNav;
STATIC_ASSERT_SIZEOF(BossStrangerNav, 0xC);

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
} BossStrangerRoute;
STATIC_ASSERT_SIZEOF(BossStrangerRoute, 0x8);

/// Values of `BossStrangerWalker::state`. Close-in is implemented; neither
/// carrier stores it.
#define BOSS_STRANGER_WALKER_IDLE   0
#define BOSS_STRANGER_WALKER_CHASE  1
#define BOSS_STRANGER_WALKER_CLOSE  2
#define BOSS_STRANGER_WALKER_PATROL 3

/// Movement record of the Boss Stranger walker.
///
/// Embedded in `actor_110600` and the Acropolis bridge copy. Each tick steers
/// `coord` toward the selected player, along `nav`'s node order, or along the
/// patrol route, then ramps `speed` and turns within `turnLimit`. The ground
/// step and contact avoidance run unless the carrier skips them. `scaleMtx`
/// keeps the model's saved rotation and is rebuilt around `scale`.
typedef struct {
    BossStrangerNav*       nav;           // Node table and order. Usually &navData
    BossStrangerRoute*     route;         // Patrol route. Usually &routeData
    GfxCoord*              coord;         // Coordinate the step writes
    WorldCollisionContact* recs;          // Contacts the ground step measures. NULL on the bridge
    WorldCollisionContact* avoidRecs;     // Contacts the avoidance step backs away from
    byte                   pad_14[0x8];   // Unread
    SVECTOR                moveStep;      // Facing step applied this frame, or zero while actors are frozen
    SVECTOR                moveDelta;     // Whole-unit collision step the ground step applied
    SVECTOR3               push;          // Sum of the avoidance nudges this frame
    byte                   pad_32[0x2];   // Aligns scaleMtx
    MATRIX                 scaleMtx;      // Saved rotation the turn step copies back before applying yaw
    s16                    scale;         // Uniform model scale. 0x1000 is full size
    s16                    recCount;      // Contact count passed with recs. The actor stores 12, the bridge 0
    s16                    avoidCount;    // Leading avoidRecs the avoidance step walks
    u16                    turnLimit;     // Maximum yaw change per frame, in angle units. 0 forces the turn to 0
    u16                    speedTarget;   // Unsigned ramp target. 0xFFFE is the patrol-out sentinel, applied as a step of -2
    u16                    speed;         // Current speed, applied as a signed step along facing
    u16                    speedStep;     // Most that speed may change in one frame
    u16                    turnRun;       // Consecutive frames the relative bearing was nonzero. Cleared at 0 and on route arrival
    u16                    turnBonus;     // Added to turnLimit. Each duration tier stores 0, and route arrival clears it
    byte                   pad_66[0x2];   // Unread
    u8                     state;         // BOSS_STRANGER_WALKER_IDLE, CHASE, CLOSE or PATROL
    u8                     prevState;     // State the previous close-in tick ran. A change re-plans
    u8                     node;          // Nav-node index being steered toward
    u8                     lockHeight;    // Non-zero pins Y: the ground step discards collision Y and skips the fall
    u8                     skipGround;    // Non-zero skips the ground step
    u8                     skipAvoid;     // Non-zero skips contact avoidance
    u8                     playerId;      // One-based player-status selector, copied from the save character id. Chase uses playerId - 1. Only initialization is known to store 1; a second resident record is not established
    u8                     actorNode;     // Nav node nearest the selected actor
    u8                     selfNode;      // Nav node nearest this walker
    u8                     prevActorNode; // Previous actorNode. A change re-plans the close-in
    u8                     prevSelfNode;  // Previous selfNode. A change re-plans the close-in
    s8                     orderStep;     // +1 or -1 along nodeOrder. Added to cursor through a u8 cast, so it wraps
    byte                   pad_74[0x1];   // Unread
    u8                     goalSlot;      // nodeOrder slot on the actor side of the closest pair. Nothing reads it
    u8                     cursor;        // Index into nav->nodeOrder while closing in
    u8                     blocked;       // 1 when an avoidance contact's kind is 0x10000. Nothing reads it
    u8                     offOrigin;     // 1 when local X or Z is nonzero after the ground step. Nothing reads it
    byte                   pad_79[0x7];   // Unread
    BossStrangerNav        navData;       // Storage nav usually points at
    BossStrangerRoute      routeData;     // Storage route usually points at
} BossStrangerWalker;
STATIC_ASSERT_SIZEOF(BossStrangerWalker, 0x94);

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
