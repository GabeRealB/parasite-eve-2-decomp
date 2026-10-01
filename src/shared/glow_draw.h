/* Glow drawing: helpers that project world points through the view and queue
 * primitives around them - gouraud discs, diamonds, capsules, cones, shafts,
 * prisms, wedges and beams of light, and flickering textured flares - for
 * rooms' lamps, lights and beams and the PE spells' glows.
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

void glowDrawDisc(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawRedDisc(SVECTOR* arg0, s16 arg1);
void glowDrawPulsingDisc(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawTintedDisc(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawDiamond(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawCapsule(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawShaft(SVECTOR* arg0, s32 arg1);
void glowDrawCone(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawBeam(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void glowDrawTintedDiscNoBias(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawFactorDisc(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
void glowDrawBitDisc(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
void glowDrawWideDiamond(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawPrism(GfxCoord* coord, s16 arg1);
void glowDrawTaperedBeam(GfxCoord* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);
void glowDrawWedge(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
void glowDrawHalo(GfxCoord* coord, s32 inner, s32 width, u8* rgb);
void glowDrawFlare(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawFlareClipped(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawFlareLocal(GfxCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

void glowDrawFlameBand(GfxCoord* arg0, s16 arg1, s16 arg2);
void glowDrawFlameStar(GfxCoord* arg0, s16 arg1, s16 arg2);
void glowDrawFlameRing(GfxCoord* arg0, s16 arg1, s32 arg2, s16 arg3);

void glowDrawGreyPrism(GfxCoord* coord, s16 arg1);

void glowDrawStarLocal(GfxCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

void glowDrawAngledCapsule(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

void glowDrawPulsingStar(SVECTOR* arg0, s16 arg1, s32 arg2);

void glowDrawGreyCapsule(SVECTOR* arg0, s32 arg1, s32 arg2);

void glowDrawTwinShafts(GfxCoord* coord);

#endif /* SRC_SHARED_GLOW_DRAW_H */
