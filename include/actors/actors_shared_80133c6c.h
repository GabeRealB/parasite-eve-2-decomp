#ifndef ACTORS_SHARED_80133C6C_H
#define ACTORS_SHARED_80133C6C_H

#include "common.h"

#include "gameplay/message.h"
#include "main/task.h"

/// The placement payload under the name room code still declares it with.
typedef GpPlaceArg ActorShared80133c6cPlacement;

void ActorsShared80133c6c(Task* task, s32 arg1, GpPlaceArg* placement);

#endif
