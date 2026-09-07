#ifndef ACTORS_SHARED_801695A0_H
#define ACTORS_SHARED_801695A0_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// Work block of the enemy actors that share `ActorsShared801695a0`, reached
/// through `Task::idMap`. Only the fields this body touches are modelled; the
/// carriers describe the rest of the same 0x454-byte block in their own
/// headers (`Actor341700Work`, `Actor342400Work`).
///
/// The layout is the one `ActorsShared8016a98c` already documents - the two
/// bodies are the same teardown step reached from two different states, and
/// differ only in the order the source declares `enemy` and the model coord,
/// which swaps one pair of loads.
typedef struct ActorsShared801695a0Work {
    /* 0x000 */ MATRIX matrix_0;
    /* 0x020 */ byte   pad_20[0x28C];
    /* 0x2AC */ GpObj  obj_2AC;
    /* 0x2CC */ GpObj  obj_2CC;
    /* 0x2EC */ byte   pad_2EC[0xC0];
    /* 0x3AC */ GpObj  obj_3AC;
    /* 0x3CC */ byte   pad_3CC[0x46];
    /* 0x412 */ u16    field_412;
    /* 0x414 */ byte   pad_414[0xC];
    /* 0x420 */ u16    field_420;
    /* 0x422 */ byte   pad_422[0xE];
    /* 0x430 */ s16    field_430;
} ActorsShared801695a0Work;

void ActorsShared801695a0(Task* task);

#endif
