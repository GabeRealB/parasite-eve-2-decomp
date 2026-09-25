#ifndef ACTORS_SHARED_8013852C_H
#define ACTORS_SHARED_8013852C_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler: switches the enemy's model and display nodes between
/// hidden and shown. `flags ^ 1` is the requested mode, latched in `field_BA0`
/// so only a change acts. Mode 1 hides the model (bit 0x80 of
/// `TmdObject::flags`), saves the enemy link node's `field_4` into
/// `field_BA1` and releases that node slot, and clears the 0xC000 pair off all
/// four display nodes' flags; mode 0 puts the saved `field_4` back, lifts the
/// hidden bit, and sets those two bits on the first and last display node.
///
/// Five actor overlays carry this body - `actor_101100`, `actor_104900`,
/// `actor_201100`, `actor_204900` and `actor_301100` - each in the `_2` unit
/// that holds it between the 0x…5E0 and 0x…774 handlers, and each with the same
/// `{ 0x7D5, handler }` entry in its message table at the same offset.
///
/// The middle argument is unread but not removable: a handler registered as
/// `GpMsgHandler` is called with `(task, msgId, arg2, arg3)`, so the mode has to
/// be declared third to arrive in `$a2`.
s32 ActorsShared8013852c(Task* task, s32 arg1, s32 flags);

#endif // ACTORS_SHARED_8013852C_H
