#ifndef ROOMS_SHARED_8017ED60_H
#define ROOMS_SHARED_8017ED60_H

#include "common.h"

#include "main/task.h"

/// The save point's root task: hands the UI holder and the save-panel exit hook
/// over on its first frame, then draws the prompt lines every frame. Nineteen
/// rooms carry this body.
void RoomsShared8017ed60(Task* task);

#endif // ROOMS_SHARED_8017ED60_H
