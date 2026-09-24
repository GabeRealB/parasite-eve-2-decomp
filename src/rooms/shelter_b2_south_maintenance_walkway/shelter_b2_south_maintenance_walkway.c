#include "common.h"

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b2_south_maintenance_walkway.h"

/// Parameters of the event the walkway's message handler starts, latched into
/// the room's pending copy when it fires. `flagId` is the game-flag nibble that
/// records the event as done: a set nibble stops it firing again, and starting
/// it sets the nibble (0 means no flag).
typedef struct _WalkwayEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _WalkwayEvent;

extern s32           func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc      D_shelter_b2_south_maintenance_walkway_80182544;
extern RoomEventMsg  D_shelter_b2_south_maintenance_walkway_801838EC;
extern u8            D_shelter_b2_south_maintenance_walkway_801838F4;
extern _WalkwayEvent D_shelter_b2_south_maintenance_walkway_80183918;

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _walkwayStartEvent(RoomEventMsg* dst, _WalkwayEvent* event)
{
    D_shelter_b2_south_maintenance_walkway_801838F4 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_south_maintenance_walkway_801838EC = *dst;
            D_shelter_b2_south_maintenance_walkway_80183918 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_south_maintenance_walkway_80182544, 0, 0, 0);
            D_shelter_b2_south_maintenance_walkway_801838F4 = 1;
        }
        return 2;
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_south_maintenance_walkway/shelter_b2_south_maintenance_walkway", func_shelter_b2_south_maintenance_walkway_8017D8E4);

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Message 0x1D goes through the rooms' event gate on flag
/// 0xAA with no prerequisite; message 0x1B starts the room's own event on flag
/// 0x13C; any other message answers 1.
s32 func_shelter_b2_south_maintenance_walkway_8017DA7C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq  req;
    _WalkwayEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x1D) {
        req.field_0 = 1;
        req.field_4 = 1;
        req.field_8 = 0x541C0005;
        req.field_C = 0x541C0001;
        req.flagId  = 0xAA;
        req.itemId  = 0;
        return func_shelter_b2_south_maintenance_walkway_8017D610(&req, out);
    }
    if (in->msgId != 0x1B) {
        return 1;
    }
    event.field_0 = 2;
    event.field_4 = 0x541C0001;
    event.flagId  = 0x13C;
    event.field_A = 0;
    return _walkwayStartEvent(out, &event);
}

s32 func_shelter_b2_south_maintenance_walkway_8017DC08(void)
{
    return 0;
}

s32 func_shelter_b2_south_maintenance_walkway_8017DC10(void)
{
    return 0;
}

s32 func_shelter_b2_south_maintenance_walkway_8017DC18(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_south_maintenance_walkway/shelter_b2_south_maintenance_walkway", D_shelter_b2_south_maintenance_walkway_8017D5F0);
