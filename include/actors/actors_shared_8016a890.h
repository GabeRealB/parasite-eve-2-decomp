#ifndef ACTORS_SHARED_8016A890_H
#define ACTORS_SHARED_8016A890_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// 0x454-byte work block `actor_104400` / `actor_341700` / `actor_342400`
/// allocate with `Mem_Calloc` and park in `Task::idMap` (that slot is not a
/// `TaskIdMap` here). The three `GpObj` nodes are the display objects
/// `ActorsShared8016a890` hands back to `Gp_UnlinkObj`; `field_412` is the
/// per-state frame counter and `field_420` is the state index the handler
/// table walks. The size below is the allocation, not a guess.
typedef struct ActorsShared8016a890Work {
    /* 0x000 */ byte  pad_0[0x2AC];
    /* 0x2AC */ GpObj obj_2AC;
    /* 0x2CC */ GpObj obj_2CC;
    /* 0x2EC */ byte  pad_2EC[0xC0];
    /* 0x3AC */ GpObj obj_3AC;
    /* 0x3CC */ byte  pad_3CC[0x46];
    /* 0x412 */ u16   field_412; // per-state frame counter
    /* 0x414 */ byte  pad_414[0xC];
    /* 0x420 */ u16   field_420; // state index
    /* 0x422 */ byte  pad_422[0x32];
} ActorsShared8016a890Work;
STATIC_ASSERT_SIZEOF(ActorsShared8016a890Work, 0x454);

/// Clear the enemy's `field_54` slot, unlink the three display objects, reset
/// the frame counter and advance `field_420`. `Actor04400_Fn08A40` is the
/// same body inside the slot-1/slot-3 `actor_104400_text` object, which cannot
/// join this unit: that overlay's whole `.text` is already one shared span.
void ActorsShared8016a890(Task* arg0);

#endif
