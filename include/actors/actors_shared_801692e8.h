#ifndef ACTORS_SHARED_801692E8_H
#define ACTORS_SHARED_801692E8_H

#include "common.h"

/// Queue CD command 0x21 once, guarded by `Gp_StateF0.field_25`. The first parameter
/// block selects 2 or 3 when session `field_7` is 4, `field_6` is 0x27 or
/// 0x28 and `field_9` is 1 or 2 respectively, and 1 otherwise.
/// Shared by `actor_341700` and `actor_342400`.
void ActorsShared801692e8(void);

#endif
