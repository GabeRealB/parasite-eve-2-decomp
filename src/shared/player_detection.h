/* Enemy tests of whether the player can be engaged. A line-of-sight test
 * checks for a wall between the actor's and the player's head-height points. A
 * reach test checks whether the player is outside the actor's facing arc or
 * too far from a point ahead of it. A segment-versus-wall test is carried as a
 * package-private copy of gameplay's func_800E0308. Packages include only the
 * tests they carry.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PLAYER_DETECTION_H
#define SRC_SHARED_PLAYER_DETECTION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"
#include "main/task_types.h"

/// Scratch-stack block of the line-of-sight test between an actor and the
/// player.
///
/// The test reserves one block, raises each root's translation to head height
/// in `localEye` and carries it through the view coordinate into the space
/// the room's occluders are tested in, then asks whether an occluder crosses
/// the segment between the two points. The block is released before the test
/// returns `blocked`, which it reads while the bytes are still intact.
typedef struct {
    SVECTOR playerEye; // Player's head-height point, in the occluders' space
    SVECTOR actorEye;  // Actor's head-height point, in the occluders' space
    SVECTOR localEye;  // Root translation raised 1000 units, before the view coordinate is applied: the player's, then the actor's; `pad` is never written
    s32     blocked;   // Answer of the occluder query: 1 when an occluder crosses the segment, 0 when none does
} PlayerDetectionSightScratch;
STATIC_ASSERT_SIZEOF(PlayerDetectionSightScratch, 0x1C);

s32 detectSightBlocked(Task* arg0);
s32 detectPlayerOutOfReach(GfxCoord* coord, s16 range, s16 offset);
s32 detectSegmentHitsWall(SVECTOR* arg0, SVECTOR* arg1);

#endif /* SRC_SHARED_PLAYER_DETECTION_H */
