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

/* glowDrawDisc's scratch block. The disc comes in two builds whose blocks
   order the radius and the GTE flag differently; a room carrying the second
   defines this as RoomDraw31Scratch before including this header. */
#ifndef GLOW_DRAW_DISC_SCRATCH
#define GLOW_DRAW_DISC_SCRATCH RoomDraw13Scratch
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
void glowDrawFlare(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawFlareClipped(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawFlareLocal(GfxCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

void glowDrawFlameBand(GfxCoord* arg0, s16 arg1, s16 arg2);
void glowDrawFlameStar(GfxCoord* arg0, s16 arg1, s16 arg2);
void glowDrawFlameRing(GfxCoord* arg0, s16 arg1, s32 arg2, s16 arg3);

#endif /* SRC_SHARED_GLOW_DRAW_H */
