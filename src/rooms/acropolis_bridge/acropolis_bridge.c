#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc D_acropolis_bridge_80188E7C[];

void func_acropolis_bridge_8017E60C(s32 arg0, s32 arg1);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge", func_acropolis_bridge_8017D6F4);

s32 func_acropolis_bridge_8017D7F0(void)
{
    return 0;
}

/// Slot-7 handler for the "player used the bridge switch" message: with the
/// room's progress nibble already at 3 it just restarts cap slot 7, otherwise
/// it clears the script step and spawns entry 1 of the room task table.
s32 func_acropolis_bridge_8017D7F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        if (GameFlag_GetNibble(2) == 3) {
            Gp_StartCapSlot(7, 1, 2);
            return 0;
        }
        func_acropolis_bridge_8017E60C(0xFFF, 1);
        Task_SpawnFromTable(D_acropolis_bridge_80188E7C, 1, 0, 0);
    }
    return 0;
}

s32 func_acropolis_bridge_8017D868(void)
{
    return 0;
}

s32 func_acropolis_bridge_8017D870(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge", D_acropolis_bridge_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge", D_acropolis_bridge_8017D5C4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge", func_acropolis_bridge_8017D878);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge", func_acropolis_bridge_8017D8D0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge", D_acropolis_bridge_8017D614);
