#ifndef ROOMS_SHARED_8017DAE0_H
#define ROOMS_SHARED_8017DAE0_H

#include "common.h"

/// Tells both elevator doors to run backwards, by setting each task's
/// `spawnArg1` to -1 - the mirror of RoomsShared8017f470. Two rooms carry this
/// body; the door pair itself is RoomsShared8017f470Cars, held at each room's
/// own address.
s32 RoomsShared8017dae0(void);

#endif // ROOMS_SHARED_8017DAE0_H
