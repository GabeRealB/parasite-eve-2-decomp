#ifndef SRC_ROOMS_MINE_SECRET_PASSAGE_MINE_SECRET_PASSAGE_PRIVATE_H
#define SRC_ROOMS_MINE_SECRET_PASSAGE_MINE_SECRET_PASSAGE_PRIVATE_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_mine_secret_passage_80180E8C[6];

extern TaskDesc D_mine_secret_passage_80180EBC;

extern RoomFadeStorage D_mine_secret_passage_80183440;

// Callbacks referenced by the overlay's shared data tables.
void func_mine_secret_passage_8017D60C(Task*);

s32 func_mine_secret_passage_8017D7C4(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_mine_secret_passage_8017D7CC(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_mine_secret_passage_8017D888(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_mine_secret_passage_8017D890(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_mine_secret_passage_8017D898(Task*, s32, s32, s32);

#endif // SRC_ROOMS_MINE_SECRET_PASSAGE_MINE_SECRET_PASSAGE_PRIVATE_H
