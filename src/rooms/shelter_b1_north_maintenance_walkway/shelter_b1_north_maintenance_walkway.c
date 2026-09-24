#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_shelter_b1_north_maintenance_walkway_80184A84[];
extern TaskDesc   D_shelter_b1_north_maintenance_walkway_80184AAC[];
extern u8         D_80071075;

void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0);

/// Parameters of the event this room's message handler starts, latched into
/// the room's pending copy when it fires. `flagId` is the game-flag nibble that
/// records the event as done: a set nibble stops it firing again, and starting
/// it sets the nibble (0 means no flag). What reads the other fields back is
/// still undecompiled.
typedef struct _ShelterB1NorthMaintenanceWalkwayEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _ShelterB1NorthMaintenanceWalkwayEvent;

extern s32                                    func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc                               D_shelter_b1_north_maintenance_walkway_80184A78;
extern RoomEventMsg                           D_shelter_b1_north_maintenance_walkway_80185B74;
extern s8                                     D_shelter_b1_north_maintenance_walkway_80185B7C;
extern _ShelterB1NorthMaintenanceWalkwayEvent D_shelter_b1_north_maintenance_walkway_80185B80;

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB1NorthMaintenanceWalkwayStartEvent(
    RoomEventMsg* dst, _ShelterB1NorthMaintenanceWalkwayEvent* event)
{
    D_shelter_b1_north_maintenance_walkway_80185B7C = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_north_maintenance_walkway_80185B74 = *dst;
            D_shelter_b1_north_maintenance_walkway_80185B80 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_north_maintenance_walkway_80184A78, 0, 0, 0);
            D_shelter_b1_north_maintenance_walkway_80185B7C = 1;
        }
        return 2;
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", func_shelter_b1_north_maintenance_walkway_8017D60C);

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 0xB and 0xE start the room's event - command 3 /
/// 2 on flag 0x14D / 0x14E; any other message answers 1.
s32 func_shelter_b1_north_maintenance_walkway_8017D7A4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    _ShelterB1NorthMaintenanceWalkwayEvent event;
    s32                                    cmd;
    s32                                    snd;
    s16                                    flag;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 0xB) {
        goto message0E;
    }
    snd           = 0x540C0001;
    cmd           = 3;
    event.field_4 = snd;
    flag          = 0x14D;
start_event:
    event.field_0 = cmd;
    event.flagId  = flag;
    event.field_A = 0;
    return _shelterB1NorthMaintenanceWalkwayStartEvent(out, &event);
message0E:
    if (in->msgId == 0xE) {
        snd           = 0x540C0003;
        cmd           = 2;
        event.field_4 = snd;
        flag          = 0x14E;
        goto start_event;
    }
    return 1;
}

void func_shelter_b1_north_maintenance_walkway_8017D918(Task* arg0)
{
    SVECTOR unused;

    switch (arg0->state) {
        case 0:
            if (Gp_StateF0.field_0 == 1) {
                gGameSession->flowFlags |= 0x80;
                gGameSession->flowFlags |= 0x40;
                arg0->state++;
            }
            break;
        case 1:
            if (Gp_StateF0.field_6 == 0) {
                Gp_StateF0.field_1  = 0x3C;
                arg0->killCountdown = 0x3E;
                arg0->state++;
            }
            break;
        case 2:
            if (arg0->killCountdown == 0) {
                if (D_80071075 == 0) {
                    Gp_SpawnIfCapIdle(1, 0);
                    taskKill(arg0);
                }
            } else {
                arg0->killCountdown--;
            }
            break;
    }
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA34(void)
{
    return 0;
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA3C(void)
{
    return 0;
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA44(void)
{
    return 0;
}

void func_shelter_b1_north_maintenance_walkway_8017DA4C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_north_maintenance_walkway_80184A84;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 2) {
        Task_SpawnFromTable(D_shelter_b1_north_maintenance_walkway_80184AAC, 0, 0, 0);
        if (GameFlag_GetNibble(0x157) == 0) {
            GameFlag_SetNibble(0x157, 1);
            Gp_SpawnIfCapIdle(4, 0);
        }
    }
    func_shelter_b1_north_maintenance_walkway_8017DB54(GameFlag_GetNibble(0x84));
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b1_north_maintenance_walkway_8017DAF4(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", RoomsShared8017d878Table);
