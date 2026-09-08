#ifndef ACTORS_SHARED_8013587C_H
#define ACTORS_SHARED_8013587C_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block of the enemy actors that share `ActorsShared8013587c`, reached
/// through `Task::idMap`. Only the animation selector, the frame counter, the
/// dwell code and the state field are modelled here; the same offsets sit in
/// `Actor02000Work` (`include/actors/actor_102000.h`), which describes the
/// rest of the block.
typedef struct ActorsShared8013587cWork {
    /* 0x000 */ byte pad_0[0x694];
    /* 0x694 */ s16  field_694; ///< animation to select
    /* 0x696 */ byte pad_696[2];
    /* 0x698 */ s16  field_698; ///< frames spent in the current animation
    /* 0x69A */ byte pad_69A[0xC];
    /* 0x6A6 */ s16  field_6A6; ///< dwell code handed back to the state machine
    /* 0x6A8 */ s16  field_6A8; ///< state
    /* 0x6AA */ byte pad_6AA[0x36];
    /* 0x6E0 */ s16  field_6E0;
} ActorsShared8013587cWork;

void ActorsShared8013587c(Task* task);

#endif
