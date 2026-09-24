#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern u8       D_8007216D;
extern TaskDesc D_mine_gorge_8017E2B0;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

s32 func_mine_gorge_8017D5F8(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x11F) {
        if (GameFlag_GetNibble(0xA4) == 0) {
            node = Gp_PendingObj4C;
            while (node != NULL) {
                if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                    found = 1;
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                GameFlag_SetNibble(0xA4, 1);
                Task_SpawnOnDefaultList(&D_mine_gorge_8017E2B0, 0, 0, 0);
                gGameSession->at4.loc.room = (D_8007216D = 2);
                gGameSession->hideHud      = (gGameSession->roomObjsDirty = 1);
                gGameSession->eventState   = 1;
                return 1;
            }
        }
    }
    return 0;
}

s32 func_mine_gorge_8017D6E8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 2) {
        return 1;
    }
    if (GameFlag_GetNibble(0xB5) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(3);
    return 0;
}
INCLUDE_RODATA("rooms/nonmatchings/mine_gorge/mine_gorge", D_mine_gorge_8017D5C4);
