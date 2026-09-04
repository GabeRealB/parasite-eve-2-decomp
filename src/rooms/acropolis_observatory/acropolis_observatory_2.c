#include "common.h"
#include "main/fs.h"
#include "gameplay/gameplay.h"
#include "rooms/room_common.h"
extern s32      D_acropolis_observatory_8017E7D8;
extern TaskDesc D_acropolis_observatory_8017E7DC;
extern TaskDesc D_acropolis_observatory_8017FE6C;

/// Message gate for the observatory hotspot: sub-id 1 arms the room's task the
/// first time it fires during session phase 2, latching nibble 0xCA so a later
/// visit does nothing. The outgoing record is never written - this handler only
/// consumes the message.
s32 func_acropolis_observatory_8017D7C4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if ((in->field_2 == 1) && (Game_Session->field_5 == 2) && (GameFlag_GetNibble(0xCA) == 0)) {
        GameFlag_SetNibble(0xCA, 1);
        Task_SpawnFromTable(&D_acropolis_observatory_8017FE6C, 0, 0, 0);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017D834);

void func_acropolis_observatory_8017D8AC(void)
{
    if ((D_acropolis_observatory_8017E7D8 == 0) && (Game_Session->field_8 == 3)) {
        D_acropolis_observatory_8017E7D8 = 1;
        Task_SpawnFromTable(&D_acropolis_observatory_8017E7DC, 1, 0, 0);
    }
    if ((D_acropolis_observatory_8017E7D8 == 0) && (Game_Session->field_8 == 4)) {
        D_acropolis_observatory_8017E7D8 = 1;
        Task_SpawnFromTable(&D_acropolis_observatory_8017E7DC, 0, 0, 0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017D950);

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017D9A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017DD3C);
