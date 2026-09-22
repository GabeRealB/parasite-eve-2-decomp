#include "common.h"
#include "main/task.h"

extern TaskDesc ActorsShared80134898Desc;

void ActorsShared80134898(void)
{
    Task_SpawnFromTable(&ActorsShared80134898Desc, 4, 9, 0);
}
