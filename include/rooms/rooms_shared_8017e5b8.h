#ifndef ROOMS_SHARED_8017E5B8_H
#define ROOMS_SHARED_8017E5B8_H

#include "common.h"

#include "main/task.h"

/// The room's own cutscene-task descriptor, the table this body spawns from.
/// Every carrying room has one at its own address, named there by the family's
/// symbol maps, so the shared object owns no data.
extern TaskDesc RoomsShared8017e5b8Desc;

/// Spawns entry 3 of the room's cutscene task table. Three rooms carry this
/// body; each of them also spawns other entries of the same table from
/// overlay-local bodies.
void RoomsShared8017e5b8(void);

#endif // ROOMS_SHARED_8017E5B8_H
