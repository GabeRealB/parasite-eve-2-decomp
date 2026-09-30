/* Glow drawing: helpers that project world points through the view and queue
 * gouraud primitives around them - discs, diamonds, capsules, cones, shafts
 * and beams of light - for rooms' lamps, lights and beams.
 *
 * Include this header in the prologue and each glow_draw_<shape>.inc.c at the
 * position of that helper. A package includes only the helpers it carries. The
 * helpers have external linkage, since some packages call them from another of
 * their files.
 */

#ifndef SRC_SHARED_GLOW_DRAW_H
#define SRC_SHARED_GLOW_DRAW_H

#include <psyq/libgte.h>

#include "overlay.h"

void glowDrawDisc(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawRedDisc(SVECTOR* arg0, s16 arg1);
void glowDrawPulsingDisc(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawTintedDisc(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawDiamond(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawCapsule(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawShaft(SVECTOR* arg0, s32 arg1);
void glowDrawCone(SVECTOR* arg0, s32 arg1, s32 arg2);
void glowDrawBeam(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

#endif /* SRC_SHARED_GLOW_DRAW_H */
