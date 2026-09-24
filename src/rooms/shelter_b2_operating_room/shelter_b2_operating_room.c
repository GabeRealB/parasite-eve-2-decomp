#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

/// Parameters of an event the operating room's message handler starts, latched
/// into the room's pending copy when it fires. `flagId` is the game-flag nibble
/// that records the event as done: a set nibble stops it firing again, and
/// starting it sets the nibble (0 means no flag). What reads the other fields
/// back is still undecompiled.
typedef struct _OperatingRoomEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _OperatingRoomEvent;

extern s32                 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc            D_shelter_b2_operating_room_80180910;
extern RoomEventMsg        D_shelter_b2_operating_room_8018422C;
extern u8                  D_shelter_b2_operating_room_80184234;
extern _OperatingRoomEvent D_shelter_b2_operating_room_80184258;

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", func_shelter_b2_operating_room_8017D8FC);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _operatingRoomStartEvent(RoomEventMsg* dst, _OperatingRoomEvent* event)
{
    D_shelter_b2_operating_room_80184234 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_operating_room_8018422C = *dst;
            D_shelter_b2_operating_room_80184258 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_operating_room_80180910, 0, 0, 0);
            D_shelter_b2_operating_room_80184234 = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Message 0x1E goes through the rooms' event gate on flag
/// 0xA8. Message 0x1C, while nibble 0xAA is clear, answers 0 and - unless
/// `in->field_5` asks for a dry run - passes `in->field_6` to `Gp_SetNibbleIf`
/// and runs cap command 3; once the nibble is set it starts the room event on
/// flag 0x13A instead. Message 0x1F starts the event on flag 0x13B; any other
/// message answers 1.
s32 func_shelter_b2_operating_room_8017DA94(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq        req;
    _OperatingRoomEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x1E) {
        req.field_0 = 2;
        req.field_4 = 1;
        req.field_8 = 0x541D0007;
        req.field_C = 0x541D0003;
        req.flagId  = 0xA8;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, out);
    }
    if (in->msgId == 0x1C && GameFlag_GetNibble(0xAA) == 0) {
        if (in->field_5 == 0) {
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_RunCapCmd1(3);
        }
        return 0;
    }
    if (in->msgId == 0x1C) {
        event.field_0 = 0xE;
        event.field_4 = 0x541D0001;
        event.flagId  = 0x13A;
        event.field_A = 0;
        return _operatingRoomStartEvent(out, &event);
    }
    if (in->msgId == 0x1F) {
        event.field_0 = 0xD;
        event.field_4 = 0x541D0005;
        event.flagId  = 0x13B;
        event.field_A = 0;
        return _operatingRoomStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b2_operating_room_8017DC9C(void)
{
    return 0;
}

s32 func_shelter_b2_operating_room_8017DCA4(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 4:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) == 0 ? 4 : 0x10, 0);
            break;
        case 5:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 0xF : 5, 0);
            break;
    }
    return 0;
}

s32 func_shelter_b2_operating_room_8017DD0C(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", RoomsShared8017d878Table);
