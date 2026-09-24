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

/// Spawn payload handed to `Task_Spawn(1, 0x31, ...)` when caption key 0xB is
/// answered. Only the three fields the caller stores are known.
typedef struct {
    u8  field_0;
    u8  field_1;
    s16 field_2;
} _Shelter1fParkingGarageKeySpawnArg;

/// 12-byte record published before the room's follow-up task is spawned.
/// `field_1`..`field_3` travel through a message handler as the message's
/// `msgId`, `field_2` and `field_3`, and are read back from its answer.
typedef struct {
    u8  field_0;
    u8  field_1;
    u8  field_2;
    u8  field_3;
    s16 field_4;
    s32 field_8;
} _Shelter1fParkingGarageKeyRecord;

extern u8                                 D_801153F4;
extern TaskDesc                           D_shelter_1f_parking_garage_80180BA0;
extern _Shelter1fParkingGarageKeySpawnArg D_shelter_1f_parking_garage_80181978;
extern _Shelter1fParkingGarageKeyRecord   D_shelter_1f_parking_garage_80181988;

void func_shelter_1f_parking_garage_8017D6AC(RoomEventMsg* in, RoomEventMsg* out);

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

/// Task body that holds `D_801153F4` set while the caption plays. On caption
/// key 0xB it spawns the 0x31 task and, 30 frames later, advances flag nibble
/// 0x4B from 9 to 0xA, publishes `D_shelter_1f_parking_garage_80181988` and
/// spawns entry 0 of `D_shelter_1f_parking_garage_80180BA0`. Any other key
/// clears `D_801153F4`, restores the weapon and ends the task.
void func_shelter_1f_parking_garage_8017DAF0(Task* task)
{
    _Shelter1fParkingGarageKeyRecord  rec;
    RoomEventMsg                      msg;
    _Shelter1fParkingGarageKeyRecord* p;
    void                              (*handler)(RoomEventMsg*, RoomEventMsg*);

    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                D_shelter_1f_parking_garage_80181978.field_0 = 0;
                D_shelter_1f_parking_garage_80181978.field_1 = 0;
                D_shelter_1f_parking_garage_80181978.field_2 = 0x1E;
                Task_Spawn(1, 0x31, 0, (s32)&D_shelter_1f_parking_garage_80181978);
                task->killCountdown = 0x1E;
                task->state++;
            } else {
                D_801153F4 = 0;
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
        case 2:
            if (task->killCountdown == 0) {
                if (GameFlag_GetNibble(0x4B) == 9) {
                    GameFlag_SetNibble(0x4B, 0xA);
                }
                handler     = func_shelter_1f_parking_garage_8017D6AC;
                rec.field_0 = 4;
                rec.field_1 = 0x14;
                rec.field_3 = 1;
                rec.field_2 = 2;
                rec.field_8 = 0x55010004;
                rec.field_4 = -1;
                Gp_MsgPlayerWeapon(0);
                p           = &rec;
                msg.msgId   = p->field_1;
                msg.field_2 = p->field_2;
                msg.field_3 = p->field_3;
                msg.field_5 = 0;
                handler(&msg, &msg);
                p->field_1                           = msg.msgId;
                p->field_2                           = msg.field_2;
                p->field_3                           = msg.field_3;
                D_shelter_1f_parking_garage_80181988 = rec;
                Task_SpawnFromTable(&D_shelter_1f_parking_garage_80180BA0, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

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
