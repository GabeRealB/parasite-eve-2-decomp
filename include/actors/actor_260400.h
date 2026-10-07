#ifndef ACTORS_ACTOR_260400_H
#define ACTORS_ACTOR_260400_H

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
