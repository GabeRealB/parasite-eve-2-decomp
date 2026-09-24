#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_shelter_1f_parking_garage_80180BB8[];
extern TaskDesc   D_shelter_1f_parking_garage_80180BE0;

/// Parameters of the event this room's message handler starts, latched into
/// the room's pending copy when it fires. `flagId` is the game-flag nibble that
/// records the event as done: a set nibble stops it firing again, and starting
/// it sets the nibble (0 means no flag). What reads the other fields back is
/// still undecompiled.
typedef struct _Shelter1fParkingGarageEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _Shelter1fParkingGarageEvent;

extern void                         func_80179B14(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc                     D_shelter_1f_parking_garage_80180BAC;
extern RoomEventMsg                 D_shelter_1f_parking_garage_8018197C;
extern u8                           D_shelter_1f_parking_garage_80181984;
extern _Shelter1fParkingGarageEvent D_shelter_1f_parking_garage_80181998;

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelter1fParkingGarageStartEvent(RoomEventMsg* dst, _Shelter1fParkingGarageEvent* event)
{
    D_shelter_1f_parking_garage_80181984 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_1f_parking_garage_8018197C = *dst;
            D_shelter_1f_parking_garage_80181998 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_parking_garage_80180BAC, 0, 0, 0);
            D_shelter_1f_parking_garage_80181984 = 1;
        }
        return 2;
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017D6AC);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017D7E8);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017D958);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", func_shelter_1f_parking_garage_8017DAF0);

s32 func_shelter_1f_parking_garage_8017DCEC(void)
{
    return 0;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179B14`. Message 5 starts the room's event on flag 0x159; any
/// other message answers 1.
s32 func_shelter_1f_parking_garage_8017DCF4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    _Shelter1fParkingGarageEvent event;

    *out = *in;
    func_80179B14(in, out);
    if (in->msgId != 5) {
        return 1;
    }
    event.field_0 = 3;
    event.field_4 = 0x55010001;
    event.flagId  = 0x159;
    event.field_A = 0;
    return _shelter1fParkingGarageStartEvent(out, &event);
}

s32 func_shelter_1f_parking_garage_8017DE44(void)
{
    return 0;
}

s32 func_shelter_1f_parking_garage_8017DE4C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0xA) {
        Gp_MsgPlayerWeapon(0);
        Gp_RunCapCmd1(2);
        Task_SpawnFromTable(&D_shelter_1f_parking_garage_80180BE0, 0, 0, 0);
    }
    return 0;
}

void func_shelter_1f_parking_garage_8017DE9C(Task* task)
{
    task->msgTable = D_shelter_1f_parking_garage_80180BB8;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.warp == 1) {
        Gp_RunCapCmd1(5);
    }
    task->state = task->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", RoomsShared8017d878Table);
