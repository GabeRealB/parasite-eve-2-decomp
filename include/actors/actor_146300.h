#ifndef ACTORS_ACTOR_146300_H
#define ACTORS_ACTOR_146300_H

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Indices into the water-tank figure's two-entry spawn table.
enum {
    ACTOR146300_TASK_FIGURE     = 0,
    ACTOR146300_TASK_ATTACHMENT = 1,
    ACTOR146300_TASK_COUNT      = 2,
};

/// Spawn recipes for the water-tank ice-bag figure and its attached model.
///
/// The room spawns `ACTOR146300_TASK_FIGURE` as placed actor 0, with a live
/// `Enemy*` in the second spawn argument. Initialization creates
/// `ACTOR146300_TASK_ATTACHMENT`, whose model attaches to figure part 4 and
/// borrows its lighting. Both entries create TMD bodies at priority 192.
/// There is no terminator. This package's callbacks and geometry must remain
/// loaded while either task lives; the model bodies belong to their tasks.
extern TaskDesc gActor146300FigureTasks[ACTOR146300_TASK_COUNT];

/// Starts one ice-bag handover or follow-up conversation at task priority 192.
///
/// A single bodyless spawn recipe, addressed at index 0; both spawn arguments
/// are unused. The room must hold player control before spawning. The task
/// consumes a collected bag when advancing progress, runs the corresponding
/// scene and releases control when it ends. The live placement-0 figure, CAP
/// resources and this actor package must remain available through completion.
/// Run only one handover at a time: its CAP-slot latch and event runner are shared.
extern TaskDesc gActor146300IceBagHandoverTaskDesc;

/// Post-battle water-tank dialogue and animation sequence introducing the handovers.
///
/// Start with `evsStartScriptWithSkip` and `gActor146300WaterTankEventSkipScript`.
/// Requires the live player, initialized placement-0 figure and loaded room CAP
/// slots 16/17. Installs player clips 47..64, synchronizes animation with CAP
/// cues, then restores view 3, placement, weapons, lighting and player control.
/// The 99-command stream and its borrowed resources must remain loaded until
/// completion, including its skip target and the installed animation sets.
extern EvsCommand gActor146300WaterTankEventScript[99];

/// Skip cleanup for the post-battle water-tank sequence.
///
/// Restores view 3, player placement, weapons and ambient lighting, requests
/// the figure's waiting pose and CAP slot 17, then releases scripted player
/// control. The 14-command stream shares `gActor146300WaterTankEventScript`'s
/// live actor and loaded-resource requirements.
extern EvsCommand gActor146300WaterTankEventSkipScript[14];

/// Restores the placed actor's animation and placement for the ice-bag handover progress.
///
/// Requires actor_146300 to be loaded and placement 0 in the current room to
/// have its live, initialized actor task. Uses the current stage's object
/// states and the live-save ice-bag collection bit. The final progress value
/// also restores the final placement; progress 4 with a bag follows that same
/// path. Does not consume a bag or advance progress. Requests borrow this
/// overlay's animation and placement records through synchronous dispatch.
void actor146300RestoreHandoverPose(void);

#endif // ACTORS_ACTOR_146300_H
