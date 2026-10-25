#ifndef ACTORS_ACTOR_260400_H
#define ACTORS_ACTOR_260400_H

#include "main/task_types.h"

/// Wounded Rupert Broderick spawn recipes: 0 body walker, 1 attached Mongoose.
///
/// Both create TMD bodies at priority `TASK_PRIORITY_CONTROL` with automatic model buffers enabled.
/// Entry 0 borrows a live Enemy placement owner in spawn argument 2; entry 1
/// borrows the published walker and takes its attachment part in spawn argument 1
/// (the body spawns it at part 8). Indices are 0..1 with no terminator.
/// The callbacks and borrowed model sources must remain loaded until teardown.
extern TaskDesc gActor260400RupertBroderickTasks[2];

/// Starts wounded Rupert Broderick's next heliport conversation.
///
/// Requires this actor package and its event-script resources to remain loaded.
/// The heliport uses this entry on variant 2. Progress advances when a script
/// starts, including when its opening conversation is later skipped. At progress
/// 1, the gift reminder repeats while either the Mongoose or 44 Maeda SP pickup
/// has object state 1; otherwise the conversation advances. Progress 4 repeats
/// the final conversation, and other values outside 0..4 do nothing.
void actor260400StartHeliportConversation(void);

/// Restores wounded Rupert Broderick's standing placement on heliport entry.
///
/// Requires the initialized scene manager in the heliport's variant 2 and this
/// actor package to be loaded. Sends a synchronous placement to actor index 0,
/// if present, at (860, 0, 6730) world units with yaw -2048/4096 turns. The
/// placement is borrowed only through dispatch; no task pointer is retained.
void actor260400RestoreHeliportPlacement(void);

#endif // ACTORS_ACTOR_260400_H
