#include "common.h"

#include "main/game.h"
#include "main/unknown_syms.h"

void func_800422F8(Task* arg0)
{
    if (Display_State.field_112 != 0) {
        Task_SpawnFromTable((TaskDesc*)&D_80725C54, 0, 0, 0);
    }
    Task_Kill(arg0);
}
