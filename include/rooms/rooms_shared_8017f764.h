#ifndef ROOMS_SHARED_8017F764_H
#define ROOMS_SHARED_8017F764_H

#include "common.h"

#include "main/task.h"

/// Shop quantity row: the room's own "Amount" label at the preview slot plus
/// how many of the previewed item the player already holds. Stackable items
/// (0xA0..0xBF) ask the scan for their stack quantity; everything else is
/// counted by walking the item table. Seven rooms carry this body.
void RoomsShared8017f764(Task* task);

extern u8 RoomsShared8017f764Amount[];

#endif // ROOMS_SHARED_8017F764_H
