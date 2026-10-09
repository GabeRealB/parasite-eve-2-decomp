#ifndef ACTORS_ACTOR_146300_H
#define ACTORS_ACTOR_146300_H

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
