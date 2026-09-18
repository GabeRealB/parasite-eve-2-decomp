#ifndef ACTORS_SHARED_801384AC_H
#define ACTORS_SHARED_801384AC_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// 0xBCC-byte work block this enemy's spawn function allocates with
/// `Mem_Calloc` and parks in the task's `Task::work` slot (that slot is not a
/// `TaskIdMap` here). The run of four `GpObj` display nodes at 0x9A8 is what
/// `ActorsShared801384ac` hands back to `Gp_UnlinkObj`; the setup body at
/// 0x80132B10 fills all four and gives each one its own three-entry `GpRec18`
/// collision table out of the `field_A28` run, node `n` taking table `n`
/// (`Gp_InitRec18Table(&field_A28[n][0], 3, 0)`).
typedef struct ActorShared801384acWork {
    /* 0x000 */ byte    pad_0[0x9A8];
    /* 0x9A8 */ GpObj   field_9A8[4];
    /* 0xA28 */ GpRec18 field_A28[4][3];
    /* 0xB48 */ byte    pad_B48[0x84];
} ActorShared801384acWork;
STATIC_ASSERT_SIZEOF(ActorShared801384acWork, 0xbcc);

void ActorsShared801384ac(Task* task);

#endif
