#ifndef ACTORS_SHARED_80137F1C_H
#define ACTORS_SHARED_80137F1C_H

#include "common.h"
#include "main/task.h"

/// View of the work field used by both specimen actor overlays.
typedef struct ActorsShared80137f1cWork {
    /* 0x000 */ byte pad_0[0x384];
    /* 0x384 */ s16  field_384; // scale delta; 4096 is one unit
} ActorsShared80137f1cWork;

/// Scales coordinate 5 by 1 + delta on X and 1 + (delta >> 2) on Y/Z,
/// then clears its transform flag. Shared by actor_107000 and actor_207000.
void ActorsShared80137f1c(Task* arg0);

#endif
