#ifndef ROOMS_SHARED_8017EA68_H
#define ROOMS_SHARED_8017EA68_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

/// The panel title this room's save point shows - "Telephone" in every carrier,
/// but with two bytes of the room's own rodata behind the terminator, so the
/// literal cannot live in the shared object and each room names its own copy.
extern const char RoomsShared8017ea68Title[];

/// The save-point menu's list state, one per carrying room.
extern UiList RoomsShared8017ea68List;

/// The save-point telephone panel: opens the memory-card list when a card with
/// room on it is present and the "no card" prompt when it is not, runs the list
/// while it is up, and hands a child prompt's result back to the room. Nineteen
/// rooms carry this body.
void RoomsShared8017ea68(Task* task);

#endif // ROOMS_SHARED_8017EA68_H
