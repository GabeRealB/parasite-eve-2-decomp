/* Glow drawing: helpers that project world points through the view and queue additive primitives around them -
 * discs, diamonds, capsules, cones, shafts, prisms, wedges, beams and flickering textured flares - for rooms' lamps,
 * lights and beams and the PE spells' glows.
 *
 * Include this header in the prologue and each <shape>.inc.c where its drawer sits; a package includes only the drawers
 * it carries. What a fragment lets its carrier configure, the carrier defines before including it.
 */

#ifndef EFFECTS_GLOW_GLOW_H
#define EFFECTS_GLOW_GLOW_H

#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include <math/gte.h>

#include <gameplay/effects.h>

#include <engine/coord.h>
#include <engine/scratch.h>

/// Scratch block for a sprite centered on one projected point; `halfExtent` is a world size divided by `otz`, so the
/// sprite shrinks with distance.
typedef struct {
    s32     otz;        // Projected depth, also the sort depth
    s32     halfExtent; // On-screen half size in pixels
    SVECTOR worldPos;   // Center in world space, the projection's input
    DVECTOR screenPos;  // Projected center in pixels
} RoomGlowSpriteScratch;

/// Scratch block for a radial glow around one projected point: `outerRadius` spans its fan or diamond and a star's ray
/// tips, `innerRadius` places the rays' shoulders. `SpriteQuadScratch` has the same layout but other meanings.
typedef struct {
    s32     otz;         // Projected depth, also the sort depth
    s32     outerRadius; // In pixels
    s32     innerRadius; // In pixels
    SVECTOR worldPos;    // Center in world space, the projection's input
    DVECTOR screenPos;   // Projected center in pixels
} RoomGlowRadiiScratch;

/// Scratch block for one projected center and the on-screen half-extent of the primitive around it.
typedef struct {
    s32 otz;    // Projected depth, also the sort depth
    s32 radius; // In pixels
    u16 sx;     // Projected center
    u16 sy;
} GlowCentreRadiusScratch;

/// Scratch block for one projected center, with the GTE flag word that rejects a failed projection.
typedef struct {
    s32 otz;    // Projected depth, also the sort depth
    s32 flag;   // Negative when the projection failed
    s32 radius; // In pixels
    u16 sx;     // Projected center
    u16 sy;
} GlowCentreScratch;

/// Scratch block for one projected center and two radii: the outer one spans the disc, the inner one places the blades'
/// shoulders.
typedef struct {
    s32 otz;         // Projected depth, also the sort depth
    s32 flag;        // Negative when the projection failed
    s32 outerRadius; // In pixels
    s32 innerRadius; // In pixels
    u16 sx;          // Projected center
    u16 sy;
} GlowCentreRadiiScratch;

/// `GlowCentreScratch` with `radius` ahead of `flag`, as some rooms' `glowDrawDisc` stores them.
typedef struct {
    s32 otz;    // Projected depth, also the sort depth
    s32 radius; // In pixels
    s32 flag;   // Negative when the projection failed
    u16 sx;     // Projected center
    u16 sy;
} GlowCentreRadiusFirstScratch;

/// Scratch block for a glow's two projected ends and the radius at each, a world size divided by that end's depth. It
/// has no flag word: the drawer rejects the pair by the second end's depth.
typedef struct {
    s32 otz0;    // Projected depths, also the sort depths
    s32 otz1;
    s32 radius0; // In pixels
    s32 radius1;
    u16 sx0;     // Projected first point
    u16 sy0;
    u16 sx1;     // Projected second point
    u16 sy1;
} GlowPointPairScratch;

/// `GlowPointPairScratch` for two points in a coordinate's local space, with the world points they transform to.
typedef struct {
    s32     otz0;        // Projected depths, also the sort depths
    s32     otz1;
    s32     radius0;     // In pixels
    s32     radius1;
    SVECTOR worldPoint0; // The points in world space, the projection's input
    SVECTOR worldPoint1;
    u16     sx0;         // Projected first point
    u16     sy0;
    u16     sx1;         // Projected second point
    u16     sy1;
} GlowWorldPointPairScratch;

/// Scratch block for two projected world points and the radius at each, with the GTE flag word that rejects either
/// end's failed projection.
typedef struct {
    s32 otz0;    // Projected depths, also the sort depths
    s32 otz1;
    s32 flag;    // Of the latest projection; negative when it failed
    s32 radius0; // In pixels
    s32 radius1;
    u16 sx0;     // Projected first point
    u16 sy0;
    u16 sx1;     // Projected second point
    u16 sy1;
} GlowFlaggedPointPairScratch;

/// Projection, angle, pulse and texture constants the glow drawers share.
enum {
    GLOW_MIN_DEPTH                 = 17, // Nearest depth a depth-clipped drawer draws
    GLOW_NEAR_DEPTH_CLAMP          = 16, // A fixed-angle beam's first end is clamped to this depth
    GLOW_TRIG_SHIFT                = 12, // rsin/rcos return Q12 values
    GLOW_FULL_TURN                 = 0x1000,
    GLOW_HALF_TURN                 = 0x800,
    GLOW_QUARTER_TURN              = 0x400,
    GLOW_EIGHTH_TURN               = 0x200,
    GLOW_SIXTEENTH_TURN            = 0x100,
    GLOW_RADIUS_SCALE              = 64, // Multiplies a radius scale before the depth divide
    GLOW_INNER_RADIUS_SCALE        = 8,
    GLOW_DIAMOND_RADIUS_SCALE      = 32,
    GLOW_PULSE_DIVISOR             = 34,
    GLOW_PULSE_BASE_INTENSITY      = 120,
    GLOW_FLICKER_BASE_INTENSITY    = 32,
    GLOW_FLICKER_INTENSITY_STEP    = 8,
    GLOW_BRIGHT_FLICKER_SHIFT      = 4, // Odd frames add 16 in shafts and textured flares
    GLOW_FLARE_TEXTURE_PAGE        = 0x2B,
    GLOW_FLARE_PALETTE_BASE        = 0x4380,
    GLOW_FLARE_PALETTE_OFFSET_MASK = 0x3F,
    GLOW_FLARE_CELL_STRIDE         = 40, // Texels between a flare sheet's cells
    GLOW_FLARE_CELL_LAST_TEXEL     = 39, // A cell's last texel; also the half-extent's multiplier
};

/// The scratch block `glowDrawDisc` uses: `GlowCentreScratch`, or `GlowCentreRadiusFirstScratch` where a room defines
/// it so before including this header.
#ifndef GLOW_DRAW_DISC_SCRATCH
#define GLOW_DRAW_DISC_SCRATCH GlowCentreScratch
#endif

/// Draws a flickering additive disc of four wedges around a world point, fading to black at the rim; `radiusScale`
/// sizes it in perspective, and `packedColor` holds red, green and blue nibbles in bits 8..11, 4..7 and 0..3. Odd
/// frames add 8, or with `GLOW_DRAW_DISC_SHIFTED_FLICKER` 1 shifted by bits 12..15; `GLOW_DRAW_DISC_DEPTH_PULL` draws a
/// point farther than 0x50 that much nearer.
void glowDrawDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

/// Draws a flickering red disc of four wedges around a world point, fading to black at the rim; `radiusScale` sizes it
/// in perspective, and a point too near draws nothing.
void glowDrawRedDisc(const SVECTOR* worldPoint, s16 radiusScale);

/// Draws a flickering grey capsule between two world points, half-disc caps joined by sides at the fixed screen angle
/// `startAngle` and sized by `radiusScale` in perspective; a second point too near draws nothing.
void glowDrawDimGreyCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle);

/// Draws a tinted star glow around a world point: eight half-bright wedges with full-bright copies at half radius, and
/// four half-bright blades whose tips alternate between the outer radius and twice it. `packedColor` holds RGB nibbles
/// in bits 8..11, 4..7 and 0..3 and, in bits 12..15, the shift of the 1 odd frames add.
void        glowDrawTintedDiscNoBias(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
static void _glowDrawFactorDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
static void _glowDrawBitDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
static void _glowDrawWideDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPrism(const GfxCoord* coord, s16 firstCorner);
static void _glowDrawTaperedBeam(const GfxCoord* coord, const SVECTOR* startPoint, const SVECTOR* endPoint, s32 radiusScale);

/// Draws one additive fan blade from a coordinate's origin toward `angle` (zero points down the screen), `rgb` at the
/// center fading to black at the rim; `radiusScale` sets its length in perspective.
void        glowDrawWedge(const GfxCoord* coord, s32 radiusScale, s32 angle, const u8 rgb[3]);
static void _glowDrawHalo(const GfxCoord* coord, s32 innerRadiusScale, s32 widthScale, const u8 rgb[3]);
static void _glowDrawCappedBeam(Task* task, const SVECTOR screenPoints[24], s32 sortingDepth);

static void _glowDrawRayStar(GfxCoord* coord, const SVECTOR* localPoint, s16 pulseRate, s16 radiusScale);

/// Draws a flickering textured flare around a world point; `textureIndex` picks the sheet's cell and palette (0..2),
/// `radiusScale` its size in perspective. A point too near draws nothing, but still takes a packet.
void glowDrawFlareClipped(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Draws an additive flame cone around a coordinate: a ring of `innerRadius` raised 0x100, lit (`intensity`, half,
/// quarter), fading down to a black ring 0x100 wider.
void glowDrawFlameCone(const GfxCoord* coord, s16 innerRadius, s16 intensity);

/// Draws an additive disc of eight wedges facing the screen at a coordinate's origin, sized by `radiusScale` in
/// perspective, (`intensity`, half, quarter) at its center and black at the rim.
void glowDrawFlameDisc(const GfxCoord* coord, s16 radiusScale, s16 intensity);

/// Draws an additive flat ring in a coordinate's XZ plane, lit (`intensity`, half, quarter) at `innerRadius` and fading
/// to black `width` further out.
void glowDrawFlameRing(const GfxCoord* coord, s16 innerRadius, s32 width, s16 intensity);

static void _glowDrawGreyPrism(const GfxCoord* coord, s16 firstCorner);

static void _glowDrawStarLocal(GfxCoord* coord, const SVECTOR* localPoint, s32 pulseRate, s32 radiusScale);

/// Draws a pulsing red diamond around a world point with two diagonal streaks through it, the second twice as long;
/// `pulseRate` sets how fast it pulses, `radiusScale` its size in perspective.
void glowDrawPulsingStar(const SVECTOR* worldPoint, s16 pulseRate, s32 radiusScale);

static void _glowDrawGreyCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle);

static void _glowDrawTwinShafts(const GfxCoord* coord);

#endif /* EFFECTS_GLOW_GLOW_H */
