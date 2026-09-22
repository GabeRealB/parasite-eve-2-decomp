#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

#include "actors/actors_shared_8016945c.h"

s16 ActorsShared8016945c(Task* arg0)
{
    if ((s8)Gp_StateF0.field_1F & 0x80) {
        ActorsShared_SetTaskState(arg0, 3);
        ActorsShared_SetWorkState(arg0, 5);
        return 1;
    }
    return 0;
}
