#include "common.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/wipsys.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "rooms/acropolis_observatory.h"
#include "rooms/room_common.h"

/// One byte of gameplay state shared with the field actors, latched here when
/// the observatory task first runs during session phase 2 with nibble 0xCA
/// still clear. See `Room_Util31`, which writes the same byte.
extern s8 D_8011540A;

extern GpMsgEntry D_acropolis_observatory_8017E7B8[];
extern s32        D_acropolis_observatory_8017E7D8;
extern TaskDesc   D_acropolis_observatory_8017E7DC;
extern TaskDesc   D_acropolis_observatory_8017FE6C;

/// Per-frame paths the two streamed scenes walk the player's matrix along,
/// indexed by `CdCmd_Queue.field_1EA + 0xA8`, each with the script pair its
/// scene runs.
extern SVECTOR D_acropolis_observatory_8017E80C[];
extern s32     D_acropolis_observatory_80183480;
extern s32     D_acropolis_observatory_80183498;

extern SVECTOR D_acropolis_observatory_8017F16C[];
extern s32     D_acropolis_observatory_801834A0;
extern s32     D_acropolis_observatory_801834B8;

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

/// Observatory task entry: parks the overlay's message table in the task and
/// registers it as the room's slot-7 pointer. On the phase-2 visit that has not
/// yet latched nibble 0xCA it also arms the shared field-actor byte, then steps
/// the task on to its next state.
void func_acropolis_observatory_8017D834(Task* task)
{
    task->field_24 = D_acropolis_observatory_8017E7B8;
    Game_SetPtrSlot(task, 7);
    if ((Game_Session->field_5 == 2) && (GameFlag_GetNibble(0xCA) == 0)) {
        D_8011540A = 1;
    }
    task->state = (s32)(task->state + 1);
}

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
