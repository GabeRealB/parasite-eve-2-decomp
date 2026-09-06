#ifndef ROOMS_SHARED_8017FF9C_H
#define ROOMS_SHARED_8017FF9C_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

/// The descriptor the panel is built from, one per carrying room.
extern UiObjectDesc RoomsShared8017ff9cDesc;

/// Runs a modal panel over the room: takes the prim buffer and stops the frame
/// timer while the panel is up, waits for the panel to report closed, then
/// gives both back and ends the stage. Nine rooms carry this body.
void RoomsShared8017ff9c(Task* task);

#endif // ROOMS_SHARED_8017FF9C_H
