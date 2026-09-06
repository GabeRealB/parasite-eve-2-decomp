#ifndef ROOMS_SHARED_8017FE74_H
#define ROOMS_SHARED_8017FE74_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

/// The room's item-list root task: lays its `RoomsShared8017fe74List` out into
/// the panel on the first frame with scrolling enabled, then updates the list
/// every frame. Cancel closes the panel, the menu button reports -1 upwards,
/// and a child panel reporting 6 is torn down and the list re-armed. Seven
/// rooms carry this body.
void RoomsShared8017fe74(Task* task);

extern UiList RoomsShared8017fe74List;

#endif // ROOMS_SHARED_8017FE74_H
