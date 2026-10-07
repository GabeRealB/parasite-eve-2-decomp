/* Shadows for jointed bodies: a stretched blob quad laid flat on the floor
 * under each bone segment, so a multi-limbed enemy casts a shadow shaped like
 * its skeleton.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_LIMB_SHADOWS_H
#define SRC_SHARED_LIMB_SHADOWS_H

#include "types.h"

#include "main/task_types.h"

/// Identifier of the included segment drawer; defaults to `limbShadowDrawSegment`.
///
/// A carrier selecting a static instance declares its matching signature before
/// including this header, then keeps the binding through the fragment include.
#ifndef LIMB_SHADOW_DRAW_SEGMENT
#define LIMB_SHADOW_DRAW_SEGMENT limbShadowDrawSegment
#endif

void LIMB_SHADOW_DRAW_SEGMENT(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade);

#endif /* SRC_SHARED_LIMB_SHADOWS_H */
