#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b3_garbage_incinerator.h"

extern TaskDesc D_shelter_b3_garbage_incinerator_80187190;

void func_shelter_b3_garbage_incinerator_80180FE4(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_garbage_incinerator_8017FD64(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_shelter_b3_garbage_incinerator_80187190, 0, arg2, 0);
}
