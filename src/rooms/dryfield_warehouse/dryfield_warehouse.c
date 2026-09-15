#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_warehouse.h"
#include "rooms/room_common.h"

extern GpMsgEntry D_dryfield_warehouse_8017F554[];
extern TaskDesc   D_dryfield_warehouse_8017F56C[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", func_dryfield_warehouse_8017D5E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", func_dryfield_warehouse_8017D764);

/// Copies the room message, then answers msg 9 by running CAP command 3 and
/// setting the event's nibble. field_5 suppresses the side effects (the
/// handler only reports what *would* happen); any other message plays the
/// "refused" sound instead.
s32 func_dryfield_warehouse_8017D824(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 9) {
        if (GameFlag_GetNibble(0x3C) != 0) {
            return 1;
        }
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(3);
            Gp_SetNibbleIf(in->field_6, 2);
        }
        return 0;
    }
    if (in->field_5 == 0) {
        SndEvt_EnqueueType7(0x52070005, 0xF);
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", func_dryfield_warehouse_8017D8D4);

void func_dryfield_warehouse_8017D99C(Task* arg0)
{
    arg0->field_24 = D_dryfield_warehouse_8017F554;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_warehouse_8017F56C, 1, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_warehouse_8017D9F8(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", D_dryfield_warehouse_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", jtbl_dryfield_warehouse_8017D5D0);
