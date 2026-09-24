#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

#include <psyq/libgte.h>

/// Parameters of an event the access tunnel's message handler starts, latched
/// into the room's pending copy when it fires. `flagId` is the game-flag nibble
/// that records the event as done: a set nibble stops it firing again, and
/// starting it sets the nibble (0 means no flag). What reads the other fields
/// back is still undecompiled.
typedef struct _AccessTunnelEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _AccessTunnelEvent;

extern s32                func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc           D_shelter_b1_access_tunnel_8017E710;
extern GpAreaApplyRec     D_shelter_b1_access_tunnel_8017FF44;
extern RoomEventMsg       D_shelter_b1_access_tunnel_8017FF64;
extern u8                 D_shelter_b1_access_tunnel_8017FF6C;
extern _AccessTunnelEvent D_shelter_b1_access_tunnel_8017FF90;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", func_shelter_b1_access_tunnel_8017D8D0);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _accessTunnelStartEvent(RoomEventMsg* dst, _AccessTunnelEvent* event)
{
    D_shelter_b1_access_tunnel_8017FF6C = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_access_tunnel_8017FF64 = *dst;
            D_shelter_b1_access_tunnel_8017FF90 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_access_tunnel_8017E710, 0, 0, 0);
            D_shelter_b1_access_tunnel_8017FF6C = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 0x12 and 0x18, while nibble 0x113 is between 1
/// and 3 and this is not a dry run, apply the room's area records and set the
/// nibble to 4. Message 0x15, while nibble 0xE5 is clear, answers 0 and -
/// unless `in->field_5` asks for a dry run - passes `in->field_6` to
/// `Gp_SetNibbleIf` and runs cap command 1. Otherwise message 0x12 goes through
/// the rooms' event gate on flag 0xAD, message 0x14 starts the room event on
/// flag 0x13F, and any other message answers 1.
s32 func_shelter_b1_access_tunnel_8017DA68(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq       req;
    _AccessTunnelEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x12 || in->msgId == 0x18) {
        if (in->field_5 == 0 && GameFlag_GetNibble(0x113) > 0 && GameFlag_GetNibble(0x113) < 4) {
            Gp_ApplyAreaRecs(&D_shelter_b1_access_tunnel_8017FF44);
            GameFlag_SetNibble(0x113, 4);
        }
    }
    if (in->msgId == 0x15 && GameFlag_GetNibble(0xE5) == 0) {
        if (in->field_5 == 0) {
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (in->msgId == 0x12) {
        req.field_0 = 3;
        req.field_4 = 1;
        req.field_8 = 0x54130009;
        req.field_C = 0x54130001;
        req.flagId  = 0xAD;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, out);
    }
    if (in->msgId == 0x14) {
        event.field_0 = 4;
        event.field_4 = 0x54130005;
        event.flagId  = 0x13F;
        event.field_A = 0;
        return _accessTunnelStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_access_tunnel_8017DCA4(void)
{
    return 0;
}

s32 func_shelter_b1_access_tunnel_8017DCAC(void)
{
    return 0;
}

s32 func_shelter_b1_access_tunnel_8017DCB4(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", RoomsShared8017d878Table);
