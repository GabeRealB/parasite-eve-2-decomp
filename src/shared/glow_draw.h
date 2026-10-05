/* Glow drawing: helpers that project world points through the view and queue
 * primitives around them - gouraud discs, diamonds, capsules, cones, shafts,
 * prisms, wedges and beams of light, and flickering textured flares - for
 * rooms' lamps, lights and beams and the PE spells' glows. Call with the
 * view matrices composed, the scratch stack initialized with room for the
 * drawer's block, and a current depth ordering table and packet arena with
 * enough space. Queued packets borrow that frame's arena until GPU completion.
 *
 * Include this header in the prologue and each glow_draw_<shape>.inc.c at the
 * position of that helper. A package includes only the helpers it carries. The
 * helpers have external linkage, since some packages call them from another of
 * their files. glowDrawPrism reads its corners from the package's
 * gGlowPrismCorners.
 */

#ifndef SRC_SHARED_GLOW_DRAW_H
#define SRC_SHARED_GLOW_DRAW_H

#include <psyq/libgte.h>

#include "overlay.h"

#include "gameplay/effects.h"

#include "main/coord.h"

/// Scratch-block type `glowDrawDisc` reserves.
///
/// The replacement is a type name, not a value. `glow_draw_disc.inc.c` uses
/// it as the block pointer's type and as the type argument of
/// `SCRATCH_STACK_RESERVE_BLOCK` and `SCRATCH_STACK_RELEASE_BLOCK`, so the
/// chosen type must be in scope at that include. Both types are declared in
/// `room_common.h`. The macro has no parameters and is not pasted or
/// stringified. Define it before including this header: the test below keeps
/// the first definition, and nothing undefines it.
///
/// The default, `GlowCentreScratch`, stores the GTE flag word and then the
/// on-screen half-extent. A room whose disc stores the half-extent ahead of
/// the flag defines `GlowCentreRadiusFirstScratch` instead. Both types are
/// 16 bytes, so the scratch cursor moves the same distance either way; only
/// those two words change places. `dryfield_r08`, `mist_shooting_gallery`,
/// `neo_ark_power_plant_2` and `shelter_b2_breeding_room` supply the
/// alternate. Every other carrier of the disc body leaves this undefined.
/// The other glow disc drawers do not use it.
#ifndef GLOW_DRAW_DISC_SCRATCH
#define GLOW_DRAW_DISC_SCRATCH GlowCentreScratch
#endif

/// Draws an additive flickering disc around a world point with four Gouraud wedges.
///
/// Projects `worldPoint` through the view matrix. The signed low halfword of
/// `radiusScale` gives a pixel radius of `radiusScale * 64 / depth`, where
/// depth is camera Z / 4. The rim is black; `packedColor` bits 8..11, 4..7 and
/// 0..3 supply red, green and blue nibbles scaled by 16. The default flicker
/// inserts bit 3 on odd animation frames. The shifted-flicker variant adds
/// `1 << shift` instead, with shift in bits 12..15 restricted to 0..7.
///
/// The pull variant subtracts `GLOW_DRAW_DISC_PULL` from depths above 0x50
/// before sizing and sorting. All variants reject negative GTE flags and
/// require a nonzero resulting depth. Borrows the point for this call and
/// queues four packets plus their additive blend commands in the current frame.
void glowDrawDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
void glowDrawRedDisc(SVECTOR* arg0, s16 arg1);
void glowDrawPulsingDisc(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawTintedDisc(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawDiamond(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawCapsule(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawShaft(SVECTOR* arg0, s32 arg1);
void glowDrawCone(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawBeam(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Draws layered tinted discs and four glow blades at an unbiased world-point depth.
///
/// The view projection supplies camera Z / 4 without a depth increment. The
/// signed low halfword of `radiusScale` gives outer and inner pixel radii of
/// `radiusScale * 64 / depth` and `radiusScale * 8 / depth`. Eight half-bright
/// outer wedges each receive a full-bright copy at half radius. Four half-bright
/// blades overlay them, with alternating tips at the outer radius and twice it.
///
/// `packedColor` bits 8..11, 4..7 and 0..3 are RGB nibbles scaled by 16;
/// bits 12..15 select an odd-frame addition of `1 << shift` to each byte.
/// The shift must be 0..7; colour bytes wrap. Rejects negative GTE flags and
/// requires nonzero depth. Borrows `worldPoint` for the call and queues twenty
/// additive quads plus blend commands in the current frame.
void glowDrawTintedDiscNoBias(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
void glowDrawFactorDisc(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
void glowDrawBitDisc(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
void glowDrawWideDiamond(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawPrism(GfxCoord* coord, s16 arg1);
void glowDrawTaperedBeam(GfxCoord* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

/// Draws one additive fan blade around a coordinate's world origin.
///
/// Uses the signed low halfwords of the composed origin, `radiusScale` and
/// `angle`. The origin is view-projected; screen radius is `radiusScale * 128`
/// divided by camera Z / 4 plus one. The rim endpoints are `angle +/- 0x20`
/// in 4096 units per turn, with zero pointing down the screen. Only the centre
/// carries the three RGB bytes; the rim fades to black. Negative GTE flags
/// reject the blade. Borrows `coord` and `rgb` during this call and queues one
/// Gouraud triangle plus its additive blend command in the current frame.
void glowDrawWedge(const GfxCoord* coord, s32 radiusScale, s32 angle, const u8 rgb[3]);
void glowDrawHalo(GfxCoord* coord, s32 inner, s32 width, u8* rgb);
void glowDrawRingBeam(Task* task, SVECTOR* points, s32 otz);
void glowDrawRayStar(GfxCoord* coord, SVECTOR* point, s32 rate, s32 arg3);

/* glowDrawRingBeam's tables, the package's data at its own positions */
extern s8 gGlowRingBeamQuads[16][4];
extern u8 gGlowRingBeamColors[24][4];
void      glowDrawFlare(SVECTOR* arg0, s32 arg1, s32 arg2);
void      glowDrawFlareClipped(SVECTOR* arg0, s32 arg1, s32 arg2);
void      glowDrawFlareLocal(GfxCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

/// Draws an additive flame cone between a lit raised ring and a wider dark rim.
///
/// The two 16-vertex rings lie in local XY: the lit ring has `innerRadius`
/// and local Z = 0x100; the dark ring has signed-16-bit radius
/// `innerRadius + 0x100` and local Z = 0. The composed coordinate rotates and
/// translates both rings into signed-16-bit world positions. `innerRadius`
/// is a local distance.
///
/// The lit edge uses the low byte of `intensity` for red and the unsigned
/// low halfword shifted by one and two for green and blue. Normal intensities
/// are 0..255; colour stores narrow to bytes. Borrows `coord` during the call;
/// each segment with nonnegative projection flags queues an additive Gouraud
/// quad and blend command at its last vertex's camera Z / 4 plus one.
void glowDrawFlameCone(const GfxCoord* coord, s16 innerRadius, s16 intensity);

/// Draws an additive flame-coloured disc facing the screen at a coordinate's origin.
///
/// Projects the signed low halfwords of the composed world origin, using no
/// coordinate rotation. Eight Gouraud wedges share a pixel radius of
/// `radiusScale * 64 / depth`, with depth equal to camera Z / 4 plus one.
/// Their centres use `(intensity, intensity >> 1, intensity >> 2)` and their
/// rims are black. Normal intensities are 0..255; signed shifts and byte
/// narrowing are retained. Negative GTE flags reject the whole disc. Borrows
/// `coord` during the call and queues eight additive quads plus blend commands.
void glowDrawFlameDisc(const GfxCoord* coord, s16 radiusScale, s16 intensity);

/// Draws an additive flat flame ring with a lit inner edge and a black outer edge.
///
/// Two 16-vertex rings lie in local XZ with radii `innerRadius` and
/// signed-16-bit `innerRadius + width`. Radii and width are local distances;
/// the composed coordinate rotates and translates both rings into world
/// positions narrowed to signed 16 bits. The lit edge uses
/// `(intensity, intensity >> 1, intensity >> 2)`, normally with intensity
/// 0..255; shifts remain signed and colour stores narrow to bytes.
///
/// Borrows `coord` during the call. Each segment with nonnegative projection
/// flags queues an additive Gouraud quad and blend command at its last
/// vertex's camera Z / 4 plus one.
void glowDrawFlameRing(const GfxCoord* coord, s16 innerRadius, s32 width, s16 intensity);

void glowDrawGreyPrism(GfxCoord* coord, s16 arg1);

void glowDrawStarLocal(GfxCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

void glowDrawAngledCapsule(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Draws a pulsing red diamond with two diagonals around a world point.
///
/// The view projection supplies a pixel half-extent of the signed low halfword
/// of `radiusScale` times 32 divided by camera Z / 4. Red intensity is
/// `rsin(animFrame * pulseRate) / 34 + 0x78`; `pulseRate` is in 4096 units per
/// turn per animation frame. Two Gouraud quads fill the diamond, then two
/// three-vertex lines cross its centre; the second extends twice as far.
/// Rejects negative GTE flags and requires nonzero depth. Borrows `worldPoint`
/// during the call and queues four additive packets plus blend commands.
void glowDrawPulsingStar(const SVECTOR* worldPoint, s16 pulseRate, s32 radiusScale);

void glowDrawGreyCapsule(SVECTOR* arg0, s32 arg1, s32 arg2);

void glowDrawTwinShafts(GfxCoord* coord);

#endif /* SRC_SHARED_GLOW_DRAW_H */
