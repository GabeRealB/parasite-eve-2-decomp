#include "common.h"
#include "main/task.h"

extern TaskDesc ActorsShared80136280Desc;

void ActorsShared801346b0(s32 arg0)
{
    Task_SpawnFromTable(&ActorsShared80136280Desc, 1, arg0, 0);
}
