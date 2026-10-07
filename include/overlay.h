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

/*
 * Types that more than one overlay family carries.
 *
 * Every overlay is linked on its own, so code that several of them share was
 * compiled into each one. The layouts below are the ones that code repeats
 * across families - rooms and actors, and some also weapons and PE - and that
 * neither the gameplay nor the main executable owns; one declaration serves
 * every family.
 */

/// Work block of a full-screen fade task: the colour `fadeDrawOverlay` is
/// drawn with, kept at `Task::work`.
///
/// The task allocates it in its first state and then, each frame, draws the
/// overlay (subtractive to darken, additive to whiten) and steps the channels
/// by its rate, ending once the ramp leaves the 0..0xFF range a channel draws
/// with - below zero going down, 0x100 going up. Many tasks draw `r` as the
/// blue channel too, so `b` is stepped without ever being drawn.
typedef struct {
    byte field_0[0x2]; // Never read or written by any fade task; role unproven
    s16  r;            // Red intensity; drawn as its low byte, signed so a ramp down ends below zero
    s16  g;            // Green intensity, stepped with `r`
    s16  b;            // Blue intensity, stepped with `r`
} ScreenFadeWork;
STATIC_ASSERT_SIZEOF(ScreenFadeWork, 0x8);

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

/// Scratch-stack block of the walk that carries a vector up a coordinate's
/// parent chain.
///
/// The walk applies the local matrix of the frame it stands on to the vector,
/// then steps to `GfxCoord::parent`, until the chain runs out. The vector
/// therefore ends in the space above the topmost frame, the one
/// `GfxCoord::workm` maps into when nothing is excluded from its composition:
/// for a node beneath `gGfxViewCoord` the view coordinates are part of the
/// chain and are applied like any other frame. Each component is cut to 16
/// bits after every frame.
typedef struct {
    GfxCoord* coord; // Frame the walk stands on; NULL once the chain has run out
    SVECTOR   vec;   // Vector being carried, in the space of `coord`'s local matrix; `pad` is never written
    VECTOR    out;   // The GTE's transform of `vec` by the current frame, which `vec` is refreshed from; `pad` is never written
    s32       flag;  // GTE flag word of the latest transform; stored and never read
} OverlayCoordChainScratch;
STATIC_ASSERT_SIZEOF(OverlayCoordChainScratch, 0x20);

/// Carries `v` from the frame of `coord` up the parent chain into world
/// space, walking in an `OverlayCoordChainScratch` taken from the scratch pad.
static __inline__ void overlayToWorld(GfxCoord* coord, SVECTOR* v)
{
    OverlayCoordChainScratch* blk;

    SCRATCH_STACK_CURSOR(OverlayCoordChainScratch)[-1].coord = coord;
    SCRATCH_STACK_RESERVE_BLOCK(OverlayCoordChainScratch);
    blk         = SCRATCH_STACK_CURSOR(OverlayCoordChainScratch);
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
        gte_stlvnl(&blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = blk->out.vx;
        blk->vec.vy = blk->out.vy;
        blk->vec.vz = blk->out.vz;
        blk->coord  = blk->coord->parent;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    SCRATCH_STACK_RELEASE_BLOCK(OverlayCoordChainScratch);
}

/// Carries `v` into world space, reserving the scratch block after copying
/// its initial coordinate and vector.
static __inline__ void overlayToWorld2(GfxCoord* coord, SVECTOR* v)
{
    OverlayCoordChainScratch* blk;

    blk         = SCRATCH_STACK_CURSOR(OverlayCoordChainScratch) - 1;
    blk->coord  = coord;
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    SCRATCH_STACK_CURSOR(OverlayCoordChainScratch) = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
        gte_stlvnl(&blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = blk->out.vx;
        blk->vec.vy = blk->out.vy;
        blk->vec.vz = blk->out.vz;
        blk->coord  = blk->coord->parent;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    SCRATCH_STACK_RELEASE_BLOCK(OverlayCoordChainScratch);
}

/// Scratch-stack block of a radius test on the XZ plane.
///
/// A test stages a horizontal offset and the radius it is measured against,
/// squares all three members in place, and reports whether `dx + dz` has
/// reached `radius`, so the distance itself is never computed. The offset
/// comes from `SVECTOR` components, so each square fits the member it
/// replaces. The block is released before the comparison reads it back;
/// nothing else may reserve scratch between the two.
typedef struct {
    s32 dx;     // X offset, then its square
    s32 dz;     // Z offset, then its square
    s32 radius; // Radius the offset is tested against, then its square
} OverlayRangeScratch;
STATIC_ASSERT_SIZEOF(OverlayRangeScratch, 0xC);

/// Returns 1 when the horizontal offset reaches or exceeds the radius, else 0.
///
/// `offset` and `radius` use the same game-coordinate units; Y and `pad` are
/// ignored, and the offset is unchanged. The signed 16-bit radius is squared,
/// so its sign does not affect the result. The squared X/Z sum must fit s32;
/// the pair (-32768, -32768) is outside that contract.
///
/// Requires an initialized scratch-stack cursor and one free, word-aligned
/// `OverlayRangeScratch` below it, separate from `offset`. Restores the cursor
/// before returning; the temporary block's contents remain until reused.
static __inline__ s32 _actorRangeOutsideRadiusXZ(const SVECTOR* offset, s16 radius)
{
    OverlayRangeScratch* savedCursor;
    OverlayRangeScratch* scratch;

    savedCursor                               = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    scratch                                   = savedCursor - 1;
    scratch->dx                               = offset->vx;
    scratch->dz                               = offset->vz;
    scratch->radius                           = radius;
    scratch->dx                              *= scratch->dx;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = scratch;
    scratch->dz                              *= scratch->dz;
    scratch->radius                          *= scratch->radius;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = savedCursor;
    // Read the released block before any other reservation can reuse it.
    return scratch->dx + scratch->dz >= scratch->radius;
}

/// Values of `OverlayEncounterSlot::status`.
enum {
    OVERLAY_ENCOUNTER_SLOT_WAITING = 0, // not started, or its spawner could not spawn
    OVERLAY_ENCOUNTER_SLOT_LIVE    = 1, // its spawner holds at least one enemy
    OVERLAY_ENCOUNTER_SLOT_DONE    = 2  // its enemies are dead or were removed
};

/// The action an encounter slot's command asks of its enemy: come out of
/// hiding at one of the room's `OverlayEncounterSpot` rows.
#define OVERLAY_ENCOUNTER_COMMAND_APPEAR 1

/// The action that ends a scripted encounter before its table runs out. The
/// room sends it to every actor at once: the encounter's controller stops
/// starting rows, and each enemy reacts to the same action in its own way.
#define OVERLAY_ENCOUNTER_COMMAND_STOP 4

/// Composes an `OverlayEncounterSlot::command` that brings the enemy out at
/// row `spot` (0..15) of the room's `OverlayEncounterSpot` table.
///
/// `entryMove` (0..15) picks the move the enemy comes out with. Only the Mad
/// Chaser decodes it; the other enemies compare the whole low byte with the
/// action, so their slots need 0 here. A room whose controller picks the spot
/// itself adds it to the command afterwards and passes 0 for `spot`.
#define OVERLAY_ENCOUNTER_APPEAR_COMMAND(spot, entryMove) (((spot) << 8) | ((entryMove) << 4) | OVERLAY_ENCOUNTER_COMMAND_APPEAR)

/// One row of a scripted encounter's table: a spawner task its controller
/// starts. The first three rows start together and each later one, in table
/// order, once fewer than three rows are live.
///
/// The spawner started for row `i` gets `(i << 16) + command` as its spawn
/// argument. It spawns its enemies hidden, brings each out after a delay by
/// sending it the low half of that argument as an `ActorCommand`, and writes
/// `status` back through the row index in the high half. The encounter is over
/// once every row is done.
typedef struct {
    s16  kind;       // Spawner the row starts (0 one Mad Chaser, 1 one of the Sucklerceph package's other enemy, 2 a pair of Sucklercephs)
    s16  command;    // Command its enemies are brought out with: action in bits 0..3, entry move in 4..7, spot row in 8..11
    byte field_4[2]; // Never read or written, zero in every table; role unproven
    s16  status;     // Progress of the row (`OVERLAY_ENCOUNTER_SLOT_`); the controller resets it when it starts
} OverlayEncounterSlot;
STATIC_ASSERT_SIZEOF(OverlayEncounterSlot, 0x8);

/// A place where a scripted encounter's enemy enters its room: the position
/// its root coordinate is moved to and the yaw it comes out along.
///
/// A room keeps one table of these. The command an `OverlayEncounterSlot`
/// sends its enemy names a row by its zero-based position in bits 8..11, and
/// the enemy looks that row up in the table of the room it is in. Each enemy
/// derives its own facing from `heading`.
typedef struct {
    s16 x;       // World X the enemy's root coordinate is moved to
    s16 y;       // World Y the enemy's root coordinate is moved to
    s16 z;       // World Z the enemy's root coordinate is moved to
    u16 heading; // Yaw the enemy comes out along, 4096 units per turn, in 0..4095
} OverlayEncounterSpot;
STATIC_ASSERT_SIZEOF(OverlayEncounterSpot, 0x8);

/// Work block of a scripted encounter's controller: the task that walks the
/// encounter's `OverlayEncounterSlot` table, starting its first three rows
/// together and each later one once fewer than three rows are live.
///
/// The controller allocates the block zeroed and the task's teardown frees
/// it. An actor command whose action is `OVERLAY_ENCOUNTER_COMMAND_STOP` ends
/// the encounter early: the controller records it in `stop` and from then on
/// neither starts a row nor finishes the encounter.
typedef struct {
    s16 frames;   // Frames counted after the first three rows are started; the encounter is armed when they reach 15, and the count stops there
    s16 nextSlot; // Table row the controller starts next; rows before it have been started
    s16 stop;     // `OVERLAY_ENCOUNTER_COMMAND_STOP` once that command has arrived, 0 until then
} OverlayEncounterControllerWork;
STATIC_ASSERT_SIZEOF(OverlayEncounterControllerWork, 0x6);

/// Work block of a scripted encounter's one-enemy spawner: the task an
/// `OverlayEncounterSlot` row starts to spawn a single enemy hidden, bring it
/// out after a delay and watch it until the row is done.
///
/// The spawner allocates the block zeroed and the task's teardown frees it.
/// The enemy belongs to its own task and is only borrowed here.
typedef struct {
    Enemy* enemy;  // Enemy the spawner spawned hidden and then watches
    s16    frames; // Frames counted since the spawn; the enemy is brought out once they pass the spawner's delay, and the count stops there
} OverlayEncounterSingleWork;
STATIC_ASSERT_SIZEOF(OverlayEncounterSingleWork, 0x8);

/// Bits of `OverlayEncounterPairWork::goneMask`.
enum {
    OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY0 = 1 << 0, // the spawner no longer holds its first enemy
    OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY1 = 1 << 1, // the spawner no longer holds its second enemy
    OVERLAY_ENCOUNTER_PAIR_GONE_BOTH   = OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY0 | OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY1
};

/// Work block of a scripted encounter's two-enemy spawner: the task an
/// `OverlayEncounterSlot` row starts to spawn a pair of enemies hidden, bring
/// the first out at once and the second after a delay, and watch both until
/// the row is done.
///
/// The spawner allocates the block zeroed and the task's teardown frees it.
/// Either spawn may fail, leaving that pointer null from the start; the task
/// only gives up when both do. The enemies belong to their own tasks and are
/// only borrowed here: the spawner clears a pointer once that enemy is dead or
/// has been sent away, and one tick later records it in `goneMask`.
typedef struct {
    Enemy* enemy0;   // First enemy of the pair, brought out as soon as the pair is spawned; null once it is gone or if it never spawned
    Enemy* enemy1;   // Second enemy of the pair, brought out after the spawner's delay; null once it is gone or if it never spawned
    s16    frames;   // Frames counted since the first enemy was brought out; the second follows once they pass the spawner's delay, and the count stops there
    s16    goneMask; // Which of the two the spawner has seen gone (`OVERLAY_ENCOUNTER_PAIR_GONE_`); the row is done once both are
} OverlayEncounterPairWork;
STATIC_ASSERT_SIZEOF(OverlayEncounterPairWork, 0xC);

/// The enemy table the pair slots of a scripted encounter spawn their two
/// enemies from. It lies at a fixed address outside the images of the
/// overlays that reach it.
extern TaskDesc D_actor_207000_80151E60;

/// Scratch-stack block a water-refraction drawer reserves for one call.
///
/// The drawer stores the transpose of `gGfxViewCoord.workm` in
/// `transposedView` and loads that rotation into the GTE. Each screen row
/// goes into `screenRow` and comes back rotated in `rotatedRow`.
/// `viewTranslation` receives the view matrix's translation and is rotated
/// by the same matrix; `depth` is that rotated Y plus a caller offset, times
/// the projection distance. The ordering-table depth is `depth` divided by
/// `rotatedRow.vy` when that Y is positive, and 0x3FFF otherwise. The block
/// is released before the drawer returns. The last sixteen bytes are never
/// read or written; they keep the reservation at 0x4C and their role is
/// unproven.
typedef struct {
    MATRIX  transposedView;  // Transpose of gGfxViewCoord.workm; its rotation is the GTE rotation for every row
    SVECTOR screenRow;       // Screen-row vector: X 0, Y the row minus 0x78, Z the projection distance
    SVECTOR rotatedRow;      // screenRow rotated by transposedView; only Y is read, as the divisor of depth
    SVECTOR viewTranslation; // View-matrix translation, rotated in place by transposedView; only Y is read
    s32     depth;           // (viewTranslation.vy + caller offset) * projection distance, the ordering-table dividend
    byte    field_3C[0x10];  // Never read or written; role unproven
} WaterRefractionScratch;
STATIC_ASSERT_SIZEOF(WaterRefractionScratch, 0x4C);

/// Scratch-stack block of the contact steering walk, which nudges a coordinate
/// away from the obstacles among its contact records.
///
/// The walk collects the bearing of each obstacle record from `origin`, drops
/// every pair of bearings more than 0x400 apart, and steps the coordinate a
/// short way away from each bearing that survives.
typedef struct {
    MATRIX   rot;        // Yaw rotation built for the current push, whose Z axis gives its direction
    SVECTOR  dir;        // First the coordinate's normalised Y-axis column, then each push step
    SVECTOR3 origin;     // Coordinate's world translation, the point bearings are taken from
    s32      kind;       // High half of the current record's key (0x10000 also sets `blocked`)
    s16      bearing[8]; // Bearings of the collected obstacle records
    s8       kept[8];    // Per bearing: 1 until it falls more than 0x400 from another bearing
    s16      heading;    // Coordinate's heading in the plane the bearings are measured in
    s16      diff;       // Wrapped difference between two bearings, then the push's yaw
    u8       i;          // Outer cursor over records, then over bearings
    u8       j;          // Inner cursor over the bearings paired with `i`
    u8       count;      // Number of bearings collected
    u8       blocked;    // Set when a record of the blocking kind 0x10000 was seen
} ActorContactSteerScratch;
STATIC_ASSERT_SIZEOF(ActorContactSteerScratch, 0x54);

/// Scratch-stack block for drawing one camera-facing quad around a projected
/// point.
///
/// A sprite drawer stores the quad's centre in `worldPos`, projects it through
/// `GsWSMATRIX` with one perspective transform, and treats an `otz` above 0x10
/// as in front of the camera. A world size divided by `otz` gives the corner
/// offsets, so the sprite shrinks with distance. A spinning quad scales that
/// quotient by the sine and cosine of its angle to place one pair of opposite
/// corners, then recomputes both offsets a quarter turn on for the other pair;
/// an upright quad stores its half-width and half-height instead. Reserve the
/// complete block and release it in scratch-stack order after drawing.
typedef struct {
    s32     otz;       // Projected depth (SZ3 / 4); the ordering-table depth and the divisor for the corner offsets
    s32     cornerDx;  // Pixels from the centre to the current corner pair along X; an upright quad's half-width
    s32     cornerDy;  // Pixels from the centre to the current corner pair along Y; an upright quad's half-height
    SVECTOR worldPos;  // Quad centre in world coordinates, the input to the projection
    DVECTOR screenPos; // Projected centre in screen pixels, stored as one GTE word
} OverlaySpriteScratch;
STATIC_ASSERT_SIZEOF(OverlaySpriteScratch, 0x18);

/// Scratch-stack block for projecting one four-corner quad straight into its
/// packet, with room for the GTE flag word.
///
/// A drawer fills `corners` with world positions, either copied from
/// coordinate translations or staged as local offsets that it rotates and
/// translates in place, each component narrowed to 16 bits. Corners share
/// indices 0..3 with the packet's vertices, in GPU quad strip order. One
/// perspective transform projects corner 0 and a triple transform projects
/// corners 1..3, and the screen positions are stored directly in the
/// primitive, so the block keeps none of them.
///
/// `otz` receives the depth after the triple transform. A drawer either
/// increments it before linking the primitive or skips a quad whose depth is
/// below 0x11 (too near the camera); it then selects the ordering-table entry
/// and the blend packet's depth. A drawer that checks the transforms stores
/// the flag word in `flag` after each one it checks and drops the quad when
/// the word is negative; one that does not leaves `flag` unwritten.
///
/// Reserve the complete block and release it in scratch-stack order after
/// drawing; no pointer into it survives release.
typedef struct {
    s32     otz;        // Last projected corner's SZ3 / 4, with the drawer's ordering bias
    s32     flag;       // Latest stored GTE flag word; negative means that transform failed
    SVECTOR corners[4]; // Local corner workspace, then world positions supplied to the projection
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

/// Stages the full-width XYZ offset from `origin` to `point` for an angle query.
///
/// Borrows one word-aligned scratch `VECTOR` below the initialized cursor,
/// writes XYZ without touching `pad`, and restores the cursor. Consume the
/// returned block before anything reserves scratch again. Inputs use the
/// SDK and packed signed-halfword layouts respectively and are only read.
static __inline__ VECTOR* _actorAngleStagePointOffset(const SVECTOR* point, const SVECTOR3* origin)
{
    VECTOR* scratchHead;
    VECTOR* delta;

    scratchHead                  = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = scratchHead - 1;
    delta->vx                    = point->vx - origin->vx;
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    delta->vy                    = point->vy - origin->vy;
    delta->vz                    = point->vz - origin->vz;
    // Read the released block before anything can reserve scratch again.
    SCRATCH_STACK_CURSOR(VECTOR) = scratchHead;
    return delta;
}

/// Measures the XZ bearing from `origin` to `point` for actor steering.
///
/// Both inputs are signed 16-bit positions in the same coordinate frame and
/// units. Returns a signed angle in 4096 units per turn: zero along +Z,
/// positive toward +X, and zero when the XZ positions coincide. Subtraction
/// retains the full 32-bit difference; the inputs are neither changed nor
/// retained. `point` uses the SDK vector layout, while `origin` is packed.
///
/// Requires an initialized, word-aligned scratch cursor with room for one
/// `VECTOR` (16 bytes). All three offsets are staged, although only X and Z
/// determine the angle; the vector's `pad` is untouched.
static __inline__ s16 _actorAngleBearingXZ(const SVECTOR* point, const SVECTOR3* origin)
{
    VECTOR* delta;

    delta = _actorAngleStagePointOffset(point, origin);
    return ratan2(delta->vx, delta->vz);
}

/// Measures the XY bearing from `origin` to `point` for actor steering.
///
/// Both inputs are signed 16-bit positions in the same coordinate frame and
/// units. Returns a signed angle in 4096 units per turn: zero along +Y,
/// positive toward +X, and zero when the XY positions coincide. Subtraction
/// retains the full 32-bit difference; the inputs are neither changed nor
/// retained. `point` uses the SDK vector layout, while `origin` is packed.
///
/// Requires an initialized, word-aligned scratch cursor with room for one
/// `VECTOR` (16 bytes). All three offsets are staged, although only X and Y
/// determine the angle; the vector's `pad` is untouched.
static __inline__ s16 _actorAngleBearingXY(const SVECTOR* point, const SVECTOR3* origin)
{
    VECTOR* scratchHead;
    VECTOR* delta;

    scratchHead                  = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = scratchHead - 1;
    delta->vx                    = point->vx - origin->vx;
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    delta->vy                    = point->vy - origin->vy;
    delta->vz                    = point->vz - origin->vz;
    // Read the released block before anything can reserve scratch again.
    SCRATCH_STACK_CURSOR(VECTOR) = scratchHead;
    return ratan2(delta->vx, delta->vy);
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

/// Whether the XZ offset staged in `d` is at least `r` long, squaring both
/// sides in a scratch block of its own.
static __inline__ s32 overlayWalkerOutOfRange(SVECTOR* d, s16 r)
{
    OverlayRangeScratch* b;
    OverlayRangeScratch* head;

    head                                      = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = head - 1;
    b                                         = SCRATCH_STACK_CURSOR(OverlayRangeScratch);

    b->dx                                     = d->vx;
    b->dz                                     = d->vz;
    b->radius                                 = r;
    b->dx                                     = b->dx * b->dx;
    b->dz                                     = b->dz * b->dz;
    b->radius                                 = b->radius * b->radius;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = head;
    return b->dx + b->dz >= b->radius;
}

/// Bearing of `pos` from the full-width translation of `coord` on the XZ
/// plane. The offset is staged in a scratch-stack `VECTOR` as in
/// `_actorAngleBearingXZ` and released before `ratan2` runs.
static __inline__ s32 overlayCoordBearingXZ(SVECTOR3* pos, GfxCoord* coord)
{
    VECTOR* head;
    VECTOR* delta;

    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = head - 1;
    delta->vx                    = pos->vx - coord->coord.t[0];
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    delta->vy                    = pos->vy - coord->coord.t[1];
    delta->vz                    = pos->vz - coord->coord.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = head;
    return ratan2(delta->vx, delta->vz);
}

/// One morph of a TMD model: what the included `modelMorph` code needs to
/// snapshot the model's rest shape and to deform it by a 0..`ONE` ramp.
///
/// A vertex morphs additively - its rest position plus its delta scaled by
/// the ramp - while a normal is interpolated between its rest value and a
/// target. The snapshot covers the model's first `savedVertexCount` vertices
/// and `normalCount` normals; the deltas apply to the `deltaCount` vertices
/// from `firstVertex`, which need not be all of them. The package owning the
/// model's data defines the record and both snapshot buffers.
typedef struct {
    SVECTOR* vertexDeltas;     // displacement of each morphed vertex at a full ramp, `deltaCount` entries
    SVECTOR* targetNormals;    // normals at a full ramp, `normalCount` entries; NULL leaves the model's normals alone
    SVECTOR* savedVertices;    // snapshot buffer for the rest vertices, at least `savedVertexCount` entries
    SVECTOR* savedNormals;     // snapshot buffer for the rest normals, at least `normalCount` entries; unused without `targetNormals`
    s16      savedVertexCount; // vertices snapshotted, from the model's first
    s16      normalCount;      // normals snapshotted and interpolated, from the model's first
    s16      firstVertex;      // model vertex the first delta applies to
    s16      deltaCount;       // vertices restored from the snapshot and displaced on each blend
} ModelMorph;
STATIC_ASSERT_SIZEOF(ModelMorph, 0x18);

#endif /* OVERLAY_H */
