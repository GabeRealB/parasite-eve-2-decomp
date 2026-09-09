#ifndef ACTOR_104600_H
#define ACTOR_104600_H

#include "common.h"

#include "main/task.h"

/// The actor's per-instance work block, reached through `Task::idMap`. Only
/// the fields the decompiled state machine touches are modelled so far: the
/// state selector at 0x286, the frame counter it cycles at 0x28A, and the
/// flags it clears alongside them.
typedef struct Actor104600Work {
    /* 0x000 */ byte pad_0[0x286];
    /* 0x286 */ s16  field_286; ///< state
    /* 0x288 */ byte pad_288[2];
    /* 0x28A */ s16  field_28A; ///< frames spent in the current step
    /* 0x28C */ byte pad_28C[2];
    /* 0x28E */ s16  field_28E;
    /* 0x290 */ s16  field_290;
    /* 0x292 */ s16  field_292;
    /* 0x294 */ byte pad_294[0x12];
    /* 0x2A6 */ s16  field_2A6;
} Actor104600Work;

void func_actor_104600_80134EC8(Task* task);
void func_actor_104600_80135B74(Task* task);

#endif
