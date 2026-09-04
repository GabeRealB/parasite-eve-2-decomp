#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern TaskDesc D_acropolis_bridge_80188E7C[];
extern s32      D_acropolis_bridge_80188EBC;
extern s32      D_acropolis_bridge_8018912C;

void func_acropolis_bridge_8017E60C(s32 arg0, s32 arg1);

/// Room message handler: answers msg 0xF (first use of the bridge) by running
/// the cutscene once and marking the area object, and msg 0xB by asking for
/// response 2 in the outgoing copy.
s32 func_acropolis_bridge_8017D6F4(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    GpAreaKey key;

    *out = *in;
    if (in->msgId == 0xF) {
        if (GameFlag_GetNibble(0x10) == 0) {
            if (in->field_5 == 0) {
                GameFlag_SetNibble(0x10, 1);
                func_800E8634((s32)&D_acropolis_bridge_80188EBC, 0, (s32)&D_acropolis_bridge_8018912C);
                GameFlag_SetNibble(6, 1);
                key.field_3 = 1;
                key.field_2 = 0xC;
                Gp_SetAreaObjId(&key, 3, 1);
            }
            return 2;
        }
    }
    if ((in->msgId == 0xB) && (in->field_5 == 0)) {
        out->field_3 = 2;
    }
    return 1;
}

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
