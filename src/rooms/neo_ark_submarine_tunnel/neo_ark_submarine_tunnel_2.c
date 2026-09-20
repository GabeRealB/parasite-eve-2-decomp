#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"

#include "rooms/room_common.h"

/// The area-record id the handler publishes, and the cutscene script blobs
/// `func_800E8634` / `func_800E8614` are handed as `(s32)&blob`.
extern s8  D_8007272D;
extern s32 D_80135220;
extern s32 D_80135FD0;
extern s32 D_80136108;

/// The tunnel's own script blob and the byte recording which of its scenes has
/// already been staged.
extern s32 D_neo_ark_submarine_tunnel_80181AF0;
extern u8  D_neo_ark_submarine_tunnel_80181DF0;

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_2", jtbl_neo_ark_submarine_tunnel_8017D620);

s32 func_neo_ark_submarine_tunnel_8017F064(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    u8 temp_s0;
    u8 temp_s0_2;
    u8 temp_s0_3;
    u8 temp_s0_4;

    temp_s0 = arg2->field_2;
    if ((temp_s0 == 1) && (GameFlag_GetNibble(0xFF) == temp_s0) && (gGameSession->at4.loc.place == 3)) {
        func_800E3FAC(0xA2, 0x35);
        GameFlag_SetNibble(0xFF, 2);
        GameFlag_SetNibble(0x11F, 1);
        D_8007272D = 0x1A;
        func_800E8634((s32)&D_80135220, 0, (s32)&D_80135FD0);
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0xBC) == 0)) {
        temp_s0_2 = gGameSession->at4.loc.place;
        if (temp_s0_2 == 1) {
            func_800E8614((s32)&D_neo_ark_submarine_tunnel_80181AF0, 0);
            D_neo_ark_submarine_tunnel_80181DF0 = temp_s0_2;
        }
    }
    temp_s0_3 = arg2->field_2;
    if ((temp_s0_3 == 3) && (D_neo_ark_submarine_tunnel_80181DF0 == 0) && (gGameSession->at4.loc.warp == 2) && (GameFlag_GetNibble(0xFF) == 0) && (gGameSession->at4.loc.place == temp_s0_3)) {
        GameFlag_SetNibble(0xFF, 1);
        func_800E8614((s32)&D_80136108, 0);
        D_neo_ark_submarine_tunnel_80181DF0 = 1;
    }
    if ((arg2->field_2 == 2) && (D_neo_ark_submarine_tunnel_80181DF0 == 0)) {
        temp_s0_4 = gGameSession->at4.loc.warp;
        if (temp_s0_4 == 1) {
            Gp_MsgPlayerWeapon(1);
            D_neo_ark_submarine_tunnel_80181DF0 = temp_s0_4;
        }
    }
    if ((arg2->field_2 == 3) && (D_neo_ark_submarine_tunnel_80181DF0 == 0) && (gGameSession->at4.loc.warp == 2)) {
        Gp_MsgPlayerWeapon(1);
        D_neo_ark_submarine_tunnel_80181DF0 = 1;
    }
    return 0;
}
