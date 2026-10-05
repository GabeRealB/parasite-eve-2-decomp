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

/// Sizing numerator for the nozzle billboard; projected half-side is 31 * scale / depth.
enum { PYKE_FLAME_NOZZLE_SIZE_SCALE = 0x80 };

static void _pykeFlameDrawNozzle(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale);
static void _pykeFlameDrawBlob(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale, s16 spinAngle);

/// Collision block of one flying flame, allocated on its first tick and kept
/// at `Task::work`.
///
/// The sphere is linked on collision list 1. Its radius is half the width
/// seeded at spawn; later growth of the drawn flame leaves that radius
/// unchanged. Its packed key is the carrier's `PYKE_FLAME_KEY` (contact
/// category 2), and its centre stays the origin of the coordinate the flame
/// flies on. The sphere takes pair tests. Room geometry is a separate segment
/// test along the flight. An occupied contact whose key has category 3 in the
/// high halfword (0x30000) ends the flame. The exit callback unlinks `body`
/// before default task teardown frees this block.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked on list 1; pair tests are enabled after the link
    WorldCollisionContact contacts[1]; // One-entry table `body` borrows. The entry is marked LAST; occupied contacts are cleared each flight frame
} PykeFlameBody;
STATIC_ASSERT_SIZEOF(PykeFlameBody, 0x38);

static void _pykeFlameDrawSplash(const VECTOR3* worldPosition, s32 halfSize);
static void _pykeFlameRelease(Task* task);

#endif /* SRC_SHARED_PYKE_FLAME_H */
