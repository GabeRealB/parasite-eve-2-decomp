/* Code the Bog Diver (actor_00400, which builds actor_100400/200400) and the
 * Sea Diver (actor_206100) share. Both attack by
 * spawning a short-lived strike child. The child is a task holding a sphere
 * collision body that carries the attack, placed at the strike point. On spawn
 * and on hits it bursts impact sparks: a six-frame spark billboard,
 * plus room particles and flashes at random offsets depending on the burst
 * kind. The shared code also includes their world-space joint turn,
 * used to twist body parts by a third of a yaw each, and its step-along-
 * heading helper.
 *
 * `DiverWork` is the library's name for the carrier's task work block, the
 * block its spawn state keeps at `Task::work`. It is a configuration binding
 * rather than a type of this header: each package defines it as an alias of
 * its own block (`_Actor00400Work` for the Bog Diver, `_Actor206100Work` for
 * the Sea Diver) after this header and before the first fragment that reaches
 * it. The two blocks differ in size and in where the members sit, so the
 * fragments compile against each package's own layout; they require these
 * members, spelled and typed alike in both:
 *
 *   `rig`          `ActorAnimRig15`  playback storage; slots 1 to 14 play the clip
 *   `animRequest`  `s16`             a `DIVER_ANIM_REQUEST_*`
 *   `animClip`     `s16`             requested clip, an index into the package's bank
 *   `animStep`     `s16`             playback rate of the driven slots
 *   `animBlend`    `s16`             frames a blend request takes
 *   `animPlaying`  `s16`             clip last applied to the slots
 *   `animStatus`   `u16`             slot 1's `ANIMATION_SLOT_*` results of the last tick
 *   `state`        `s16`             index into the current task state's handler table
 *   `subState`     `s16`             step of the current state
 *
 * Include this header in the prologue and each fragment at its function's
 * position. `diver_inlines.inc.c` has to precede every function that uses one
 * of its helpers, since a helper defined later is called rather than inlined.
 */

#ifndef SRC_SHARED_DIVER_H
#define SRC_SHARED_DIVER_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

#include "gameplay/world_collision.h"

/// Values of `DiverWork::animRequest`, which each package's animation step
/// consumes once a frame.
enum {
    DIVER_ANIM_REQUEST_BLEND   = 1, // seek the slots to `animClip`, blending over `animBlend` frames
    DIVER_ANIM_REQUEST_RESET   = 2, // restart the slots on `animClip`
    DIVER_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// What the Diver library reaches of a strike's work block.
///
/// A strike is the short-lived task a Diver attacks with: a shot, for both the
/// Bog Diver and the Sea Diver. Each package keeps its own block at the strike's
/// `Task::work`, of its own size, and opens it with this head so the shared
/// teardown can retire either one. The package's spawn state links
/// `attackBody` and its flight state disables it on the frame the strike
/// bursts; the teardown unlinks it before the task ends and the block is
/// freed.
typedef struct {
    byte               field_0[0x8]; // never accessed by either package; role unproven
    WorldCollisionBody attackBody;   // sphere on the strike's root coordinate whose key carries the Diver's attack; linked for the strike's whole life
} DiverStrikeWork;
STATIC_ASSERT_SIZEOF(DiverStrikeWork, 0x28);

/// Strike-effect recipes passed to `_diverImpactBurst` by either carrier.
enum {
    DIVER_BURST_LAUNCH = 0, // one room particle at the strike coordinate
    DIVER_BURST_TRAIL  = 1, // billboard, periodic room particles and offset flashes
    DIVER_BURST_IMPACT = 2  // billboard, one upright particle and four particle/flash pairs
};

/// Cells in the spark strip; each cell occupies 40 by 40 texels.
enum { DIVER_SPARK_FRAME_COUNT = 6 };

/// Teardown ticks a burst strike lingers before its linked attack body is removed.
enum { DIVER_STRIKE_LINGER_FRAMES = 12 };

static void _diverImpactBurst(GfxCoord* coord, u16 phase, u16 kind, u32 sizeAndSprayBias);
static void _diverDrawSpark(const GfxCoord* coord, u16 frameIndex, u16 size, s32 angle);
static void _diverTurnJoint(GfxCoord* coord, s16 yaw);
static void _diverStepForward(Task* task, s16 distance, s16 yaw);
static void _diverStrikeTeardown(Task* task);
static void _diverRestartClip(Task* task);
static void _diverEnterRecoil(Task* task);

#endif /* SRC_SHARED_DIVER_H */
