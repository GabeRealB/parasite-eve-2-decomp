/* The NPC walker with footsteps: a 19-slot animated character that cutscene
 * scripts walk around. Its spawn state publishes a singleton work block,
 * lights and binds the model and installs the message table; the per-frame
 * update reseeds the rig in states 1 and 2, then in state 3 walks forward at
 * the mode's speed while the walk clip has travel left, queues the idle clip
 * with a 10-frame blend when it runs out, turns while the turn clip has frames
 * left and plays a panned step sound on each foot cue. The 'walk to' message
 * (0x7DD) turns the model to face its target and divides the planar distance
 * by the mode's distance per update to obtain a whole-frame travel count.
 * Each sound-enabled carrier provides a private task exit callback that
 * releases its enemy and begins task teardown.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. Each carrier declares its private function instances there and
 * defines FOOTSTEP_WALK_WORK_T when including the walk-target fragment, as
 * its allocated work type (FootstepWalkWork or FootstepWalkQuietWork). That
 * fragment uses the selected type at
 * Task::work. This binding is a type name, with no argument evaluation.
 * The walker's state belongs to the package, which defines it at
 * its own positions under these names:
 *
 *   FootstepWalkWork* _gFootstepWalkWork        the borrowed task-owned block; a
 *                                               package whose walker plays no
 *                                               step sounds carries the quiet
 *                                               update and declares this a
 *                                               FootstepWalkQuietWork*
 *   Task*             gFootstepWalkTask         the walker's task
 *   s16               _gFootstepWalkMode       last walk's signed-halfword mode
 *   s16               _gFootstepWalkBlendFrames whole frames for the next blend
 *   the native animation-set pointer table and message table the spawn state
 *   installs, as gFootstepWalkAnims and gFootstepWalkMsgTable
 *
 * These three state objects are static per-carrier instances. Early work and
 * mode declarations retain the BSS first-declaration order; the initialized
 * duration stays at its data position. Document each at its definition.
 * Spawn and task dispatch publish the work borrowed by singleton message
 * handlers. Teardown does not clear that pointer, so the handlers must stop
 * using it when the task releases its allocation.
 */

#ifndef SRC_SHARED_FOOTSTEP_WALK_H
#define SRC_SHARED_FOOTSTEP_WALK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/message.h"

/// Travel modes accepted by the walk-target handler; these do not select a clip.
enum {
    FOOTSTEP_WALK_MODE_FORWARD      = 0,
    FOOTSTEP_WALK_MODE_BACKWARD     = 1,
    FOOTSTEP_WALK_MODE_SLOW_FORWARD = 2
};

/// Positive distances per moving update, in the model root's parent space.
/// Backward mode negates its distance when translating the model.
enum {
    FOOTSTEP_WALK_FORWARD_DISTANCE      = 60,
    FOOTSTEP_WALK_BACKWARD_DISTANCE     = 15,
    FOOTSTEP_WALK_SLOW_FORWARD_DISTANCE = 25
};

/// Clip keys whose playback permits travel or turning, and the travel-end pose.
/// The three walk keys all use the independently selected travel mode.
enum {
    FOOTSTEP_WALK_ANIM_WALK_2  = 2,
    FOOTSTEP_WALK_ANIM_TURN    = 3,
    FOOTSTEP_WALK_ANIM_IDLE    = 13,
    FOOTSTEP_WALK_ANIM_WALK_14 = 14,
    FOOTSTEP_WALK_ANIM_WALK_15 = 15
};

/// Turn increment in 1/4096 turns per update, and travel-end blend in whole frames.
enum {
    FOOTSTEP_WALK_TURN_ANGLE_PER_UPDATE = 51,
    FOOTSTEP_WALK_IDLE_BLEND_FRAMES     = 10
};

/// Initial whole-frame blend duration of each carrier's duration latch.
enum { FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES = 8 };

/// Preliminary rate in sixteenths of a frame, overwritten by slot reset's normal rate.
enum { FOOTSTEP_WALK_PRE_RESET_RATE = 1 };

/// Published work pointer used by the reusable yaw-placement fragment.
///
/// Bind to a stable pointer expression whose live work has writable `st.yaw`.
/// The expression is evaluated once; the heading store does not retain it.
/// Other walker operations retain their own work bindings. A carrier borrowing
/// only placement binds this around that fragment, then restores the default.
#ifndef FOOTSTEP_WALK_PLACE_WORK
#define FOOTSTEP_WALK_PLACE_WORK _gFootstepWalkWork
#endif

/// Work block of a footstep walker that plays no step sounds, allocated
/// zeroed at its full size by the walker's spawn state and kept both at
/// `Task::work` and in the walker's `_gFootstepWalkWork`.
///
/// It is also what `FootstepWalkWork` opens with. The model object borrows `light`
/// and `color` for as long as the block lives.
typedef struct {
    MATRIX          light;      // Light-direction matrix lent to the model object
    MATRIX          color;      // Light-colour matrix lent to the model object
    ActorAnimRig19  rig;        // Playback storage of the nineteen-part model; slots 1 to 18 are driven
    ActorEnemyState st;         // Animation request, heading last given the root and frames of walk left
    s16             turnFrames; // Frames the update still turns the model for while the turn clip plays
} FootstepWalkQuietWork;
STATIC_ASSERT_SIZEOF(FootstepWalkQuietWork, 0x4B8);

/// Work block of a footstep walker, allocated zeroed at its full size by the
/// walker's spawn state and kept both at `Task::work` and in the walker's
/// `_gFootstepWalkWork`.
///
/// It opens as `FootstepWalkQuietWork` does and adds the step sounds' state.
/// The model object borrows `light` and `color` for as long as the block
/// lives.
typedef struct {
    MATRIX                 light;         // Light-direction matrix lent to the model object
    MATRIX                 color;         // Light-colour matrix lent to the model object
    ActorAnimRig19         rig;           // Playback storage of the nineteen-part model; slots 1 to 18 are driven
    ActorEnemyState        st;            // Animation request, heading last given the root and frames of walk left
    s16                    turnFrames;    // Frames the update still turns the model for while the turn clip plays
    const AnimationRecord* stepRecord;    // Animation record of slot 1 the step check last saw, so a record held for several frames sounds once; NULL after a reseed
    u8                     playFootsteps; // Nonzero once a script has turned the step sounds on: the update then runs the step check each frame. Never cleared
} FootstepWalkWork;
STATIC_ASSERT_SIZEOF(FootstepWalkWork, 0x4C0);

#endif /* SRC_SHARED_FOOTSTEP_WALK_H */
