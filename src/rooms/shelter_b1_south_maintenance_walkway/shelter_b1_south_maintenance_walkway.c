#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"

/// Parameters of the event this room's message handler starts, latched into
/// the room's pending copy when it fires. `flagId` is the game-flag nibble that
/// records the event as done: a set nibble stops it firing again, and starting
/// it sets the nibble (0 means no flag). What reads the other fields back is
/// still undecompiled.
typedef struct _ShelterB1SouthMaintenanceWalkwayEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _ShelterB1SouthMaintenanceWalkwayEvent;

extern s32                                    func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc                               D_shelter_b1_south_maintenance_walkway_801822FC;
extern RoomEventMsg                           D_shelter_b1_south_maintenance_walkway_8018363C;
extern s8                                     D_shelter_b1_south_maintenance_walkway_80183644;
extern _ShelterB1SouthMaintenanceWalkwayEvent D_shelter_b1_south_maintenance_walkway_80183648;

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB1SouthMaintenanceWalkwayStartEvent(
    RoomEventMsg* dst, _ShelterB1SouthMaintenanceWalkwayEvent* event)
{
    D_shelter_b1_south_maintenance_walkway_80183644 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_south_maintenance_walkway_8018363C = *dst;
            D_shelter_b1_south_maintenance_walkway_80183648 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_south_maintenance_walkway_801822FC, 0, 0, 0);
            D_shelter_b1_south_maintenance_walkway_80183644 = 1;
        }
        return 2;
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_south_maintenance_walkway/shelter_b1_south_maintenance_walkway", func_shelter_b1_south_maintenance_walkway_8017D5F8);

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 9 and 0xB start the room's event, each with its
/// own parameters and flag; any other message answers 1.
s32 func_shelter_b1_south_maintenance_walkway_8017D790(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    _ShelterB1SouthMaintenanceWalkwayEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 9) {
        event.field_0 = 1;
        event.field_4 = 0x540A0001;
        event.flagId  = 0x14B;
        event.field_A = 0;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(out, &event);
    }
    if (in->msgId == 0xB) {
        event.field_0 = 2;
        event.field_4 = 0x540A0003;
        event.flagId  = 0x14C;
        event.field_A = 0;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9D0(void)
{
    return 0;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9D8(void)
{
    return 0;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9E0(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_south_maintenance_walkway/shelter_b1_south_maintenance_walkway", D_shelter_b1_south_maintenance_walkway_8017D5D8);
