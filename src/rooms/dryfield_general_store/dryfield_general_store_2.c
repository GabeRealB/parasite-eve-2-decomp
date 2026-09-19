#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"

extern TaskDesc D_dryfield_general_store_8017E164;

s32 func_dryfield_general_store_8017DD58(s32 arg0, s32 arg1, s32 arg2)
{
    s32   arg;
    void* slot;

    if (arg2 == 0x18) {
        slot = gameGetPtrSlot(0xA);
        arg  = 0x19;
        if (slot != 0) {
            arg = 0x18;
        }
        Gp_SpawnIfCapIdle(arg, 0);
    }
    if (arg2 == 9) {
        Task_SpawnFromTable(&D_dryfield_general_store_8017E164, 0, 0x53, 9);
    }
    return 0;
}
