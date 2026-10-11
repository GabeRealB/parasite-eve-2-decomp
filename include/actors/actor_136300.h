#ifndef ACTORS_ACTOR_136300_H
#define ACTORS_ACTOR_136300_H

#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Indices in the night-garage scene package's fixed task table.
enum {
    ACTOR_136300_TASK_RETAINED             = 0,
    ACTOR_136300_TASK_GARAGE_DEPARTURE     = 1,
    ACTOR_136300_TASK_STAGE_EXIT_COUNTDOWN = 2,
    ACTOR_136300_TASK_COUNT                = 3,
};

/// Task recipes for the night garage's departure scene and stage-exit countdown.
///
/// The garage's companion variant loads this package and spawns entry 1 for
/// departure dialogue; the scene queues entry 2 with a signed countdown in
/// whole task updates. This fixed table has no terminator. Entry 0 is retained
/// but must not be spawned: its stored enemy destructor has
/// a two-argument signature rather than a task callback's one-argument signature.
/// Keep the actor package loaded while either task can execute its callbacks.
extern TaskDesc gActor136300TaskTable[ACTOR_136300_TASK_COUNT];

/// Companion placement restored on entry to the night garage and after its scene.
///
/// Position is (2230, 0, 5240) in room-coordinate units, with yaw -1024 in
/// 4096 units per turn. The garage's companion variant loads the owning actor
/// package; borrow this record only while that package is loaded. Placement
/// messages consume the position and all three Euler angles synchronously.
extern ActorTransform gActor136300CompanionGaragePlacement;

/// First-visit companion scene in the night garage, including its movie interlude.
///
/// The companion room variant starts this with HUD hide/restore and
/// `gActor136300GarageCompanionSceneSkipScript` as its input-skip target.
/// Requires live player, companion, CAP and garage-movie resources. Installs
/// borrowed animation-bank extensions, stages the actors and synchronizes
/// dialogue with CAP cues. Completion restores player weapon effects, companion
/// placement, player control and default captions, selecting saved scene event
/// 7 and view 2.
/// The 149-command stream ends with `EVENT_SCRIPT_OPCODE_END`; keep this actor
/// package and the garage overlay loaded until the interpreter finishes.
extern EvsCommand gActor136300GarageCompanionSceneScript[149];

/// Input-skip cleanup for the night garage's first companion scene.
///
/// The 16-command stream fades out, releases scene effects and restores the
/// same saved scene event, view, player weapon effects, companion placement,
/// player control and default captions as normal completion. It also shows the
/// companion in clip 50. Run only as the main scene's skip target, with both
/// overlays and their borrowed resources loaded through `EVENT_SCRIPT_OPCODE_END`.
extern EvsCommand gActor136300GarageCompanionSceneSkipScript[16];

#endif // ACTORS_ACTOR_136300_H
