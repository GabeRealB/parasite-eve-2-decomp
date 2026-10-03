/* The sprites of the M4A1's Pyke flamethrower attachment, also carried by
 * actor_800100's copy of the weapon. One is the flame at the nozzle: a six-
 * cell animated strip billboarded at a world point. The other is the flying
 * flame: a spinning, widening billboard that the flame's task draws each
 * frame as it flies, falls and ricochets, with a splash on the ground below.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The task is an entry point gameplay's effect table names by
 * address, so each package keeps its own name for it and calls the inline
 * body. The two builds differ in:
 *   PYKE_FLAME_KEY                    the flame body's collision key
 *   PYKE_FLAME_REDRAW_UPDATES_COORD   1: a paused flame recomposes its
 *                                     coordinate before redrawing (actor_800100)
 *   PYKE_FLAME_SPLASH_DEAD_BIAS       1: the splash's depth bias precedes the
 *                                     depth store and is lost (the M4A1)
 */

#ifndef SRC_SHARED_PYKE_FLAME_H
#define SRC_SHARED_PYKE_FLAME_H

#include "common.h"
#include "types.h"

#include "gameplay/world_collision_types.h"
#include "main/task_types.h"

void pykeFlameDrawNozzle(VECTOR3* pos, u16 frame, s32 brightness);
void pykeFlameDrawBlob(VECTOR3* pos, u16 frame, u16 width, s16 ang);

/// The flying flame's collision body: a sphere linked on list 1 with one
/// contact record.
typedef struct PykeFlameBody {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[1];
} PykeFlameBody;
STATIC_ASSERT_SIZEOF(PykeFlameBody, 0x38);

/// Scratch-stack workspace for the flying flame's ground splash.
///
/// `vertices` stages each corner of the flat quad and is reused for that
/// corner after the view rotation and the move onto the ground point, narrowed
/// to signed 16-bit coordinate units. Corners and screen positions share
/// indices 0..3 in GPU quad strip order.
///
/// One RTPS projects corner 0 and one RTPT projects corners 1..3, the same
/// corner projection as `EffectQuadScratch`. Ordering depth and the GTE FLAG
/// word stay on the call stack. A negative FLAG rejects the quad before the
/// screen positions are copied to the textured primitive.
///
/// Reserve the whole block and release it before the drawer returns. Pointers
/// into the block must not survive release.
typedef struct {
    SVECTOR vertices[4];      // Local corner workspace, then world positions supplied to the projection
    DVECTOR screenCorners[4]; // Signed screen X/Y pixels, written together as one GTE word per corner
} PykeFlameSplashScratch;
STATIC_ASSERT_SIZEOF(PykeFlameSplashScratch, 0x30);

static void pykeFlameDrawSplash(VECTOR3* pos, s32 width);
static void pykeFlameRelease(Task* task);

#endif /* SRC_SHARED_PYKE_FLAME_H */
