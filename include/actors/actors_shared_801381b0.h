#ifndef ACTORS_SHARED_801381B0_H
#define ACTORS_SHARED_801381B0_H

#include "main/task.h"

/// Applies the reaction rotation to model coordinates 3 and 5, then decreases
/// its X angle by 0x20 until it clears the reaction at field_38C. Shared by
/// actor_107000 and actor_207000, which use the same second-form work layout.
void ActorsShared801381b0(Task* arg0);

#endif
