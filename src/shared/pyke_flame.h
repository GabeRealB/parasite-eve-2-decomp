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

/// 0x30-byte scratch `pykeFlameDrawSplash` takes from the scratch stack: `vec`
/// the splash quad's four corners in world space, `sxy` where they project.
/// Same shape as the gameplay `GpQuadScratch`, but with `otz` and `flag` kept
/// on the stack instead of in the block.
typedef struct PykeFlameSplashScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ DVECTOR sxy[4];
} PykeFlameSplashScratch;
STATIC_ASSERT_SIZEOF(PykeFlameSplashScratch, 0x30);

static void pykeFlameDrawSplash(VECTOR3* pos, s32 width);
static void pykeFlameRelease(Task* task);

#endif /* SRC_SHARED_PYKE_FLAME_H */
