#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

/// Cutscene script blob handed to `func_800E8614`; unnamed in the gameplay
/// map, which keeps the raw address.
extern s32 D_mine_tunnel_8017E024;

s32 func_mine_tunnel_8017D630(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0x11A) >= 2 ? 3 : 2);
    }
    return 0;
}

s32 func_mine_tunnel_8017D670(s32 arg0, s32 arg1, RoomEventMsg* msg, s32 arg3)
{
    u8 temp_v1;

    temp_v1 = msg->field_2;
    if ((temp_v1 == 1) && (gGameSession->at4.loc.place == temp_v1) && (GameFlag_GetNibble(0xA1) == 0)) {
        GameFlag_SetNibble(0xA1, 1);
        Gp_MsgPlayerWeapon(0);
        func_800E8614((s32)&D_mine_tunnel_8017E024, 1);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/mine_tunnel/mine_tunnel", D_mine_tunnel_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/mine_tunnel/mine_tunnel", RoomsShared8017d878Table);
