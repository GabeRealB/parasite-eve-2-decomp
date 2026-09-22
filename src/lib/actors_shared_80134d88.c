#include "common.h"
#include "actors/actors_shared_80134d88.h"
#include "rooms/room_common.h"
#include "actors/actors_shared_80134dbc.h"
#include "gameplay/3688.h"

void ActorsShared80134d88(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

void ActorsShared80134dbc(Task* task)
{
    RoomActionPrompt*         prompt = &D_80114D28;
    ActorsShared80134dbcWork* work   = (ActorsShared80134dbcWork*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}
