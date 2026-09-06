#ifndef ROOMS_SHARED_8017EF24_H
#define ROOMS_SHARED_8017EF24_H

#include "common.h"

#include "main/task.h"

/// The shop's balance panel: the player's BP under the room's own "BP" caption,
/// and below it the carried/capacity pair under its "TOTAL" caption. Seven
/// rooms carry this body.
void RoomsShared8017ef24(Task* task);

extern u8 RoomsShared8017ef24Total[];

#endif // ROOMS_SHARED_8017EF24_H
