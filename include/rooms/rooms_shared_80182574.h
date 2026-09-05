#ifndef ROOMS_SHARED_80182574_H
#define ROOMS_SHARED_80182574_H

#include "common.h"

#include "main/task.h"

/// Per-frame visibility hook for a pick-up prop: the model is drawn
/// (`field_C = 8`) until the item's 2-bit flag reaches 2, after which it is
/// hidden (`field_C = 0x80`). Two rooms carry this body.
void RoomsShared80182574(Task* task);

#endif // ROOMS_SHARED_80182574_H
