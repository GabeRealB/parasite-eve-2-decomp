/* Enemy facing, distance and sight tests. Each actor translation unit carries
 * its own static copies of the tests it uses. The segment test scans sight
 * occluders, as does gameplay's worldCollisionSegmentOccluded.
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

static s32 _playerDetectionSightBlocked(const Task* actor);
static s32 _playerDetectionOutOfReach(const GfxCoord* actorCoord, s16 stopDistance, s16 forwardStep);
static s32 _playerDetectionSegmentOccluded(const SVECTOR* segmentStart, const SVECTOR* segmentEnd);

#endif /* SRC_SHARED_PLAYER_DETECTION_H */
