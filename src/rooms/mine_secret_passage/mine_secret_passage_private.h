#ifndef MINE_SECRET_PASSAGE_PRIVATE_H
#define MINE_SECRET_PASSAGE_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_mine_secret_passage_8017D60C(Task *);
s32 func_mine_secret_passage_8017D7C4(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_mine_secret_passage_8017D7CC(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_mine_secret_passage_8017D888(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_mine_secret_passage_8017D890(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_mine_secret_passage_8017D898(Task *, s32, s32, s32);

#endif // MINE_SECRET_PASSAGE_PRIVATE_H
