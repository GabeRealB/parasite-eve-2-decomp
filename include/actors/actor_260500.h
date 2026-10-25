#ifndef ACTORS_ACTOR_260500_H
#define ACTORS_ACTOR_260500_H

#include "main/task_types.h"

/// Spawn recipe for Jodie, the heliport's nineteen-part quiet conversation walker.
///
/// Used at index 0 by the heliport's variant-1 actor resource. Creates a TMD
/// body at execution priority `TASK_PRIORITY_CONTROL` with automatic model buffers. The spawned task
/// owns its model and work; keep this package loaded for its callback, messages
/// and borrowed model and animation data until the task is destroyed.
extern TaskDesc gActor260500JodieTask;

/// Restores actor 260500's standing placement on heliport entry.
///
/// Requires the initialized scene manager in heliport variant 1 and this
/// actor package to remain loaded. Sends a synchronous placement to actor
/// index 0, if present, at (860, 0, 6730) world units with yaw -2161/4096
/// turns. The placement is borrowed through dispatch; no task is retained.
void actor260500RestoreHeliportPlacement(void);

/// Starts actor 260500's next heliport conversation.
///
/// Requires this package and its event-script resources to remain loaded.
/// The heliport invokes this entry in variant 1. Progress 0 starts the opening
/// conversation with a skip script; progress 1..3 starts the next conversation.
/// Progress advances immediately on starting, including when the opening is
/// skipped. Progress 4 repeats the final conversation; other values do nothing.
void actor260500StartHeliportConversation(void);

#endif // ACTORS_ACTOR_260500_H
