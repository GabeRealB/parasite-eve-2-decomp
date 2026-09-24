#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
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

/// Destination and effects of the room's exit task
/// `func_shelter_1f_parking_garage_8017D7E8`: `field_0`..`field_3` are the
/// stage, area, warp and room it commits to the save data, `field_4` the
/// message 0x3EE argument it sends first (-1: none) and `field_8` the sound
/// event it plays (0: none). `field_1`..`field_3` travel through the answer
/// function `func_shelter_1f_parking_garage_8017D6AC` as a message's `msgId`,
/// `field_2` and `field_3` before the record is published.
typedef struct {
    u8  field_0;
    u8  field_1;
    u8  field_2;
    u8  field_3;
    s16 field_4;
    s32 field_8;
} _Shelter1fParkingGarageKeyRecord;

extern s16                              D_80071076;
extern u8                               D_801153F4;
extern u8                               D_80115690;
extern TaskDesc                         D_shelter_1f_parking_garage_80180BA0;
extern GpStateBD8                       D_shelter_1f_parking_garage_80181974;
extern GpStateBD8                       D_shelter_1f_parking_garage_80181978;
extern _Shelter1fParkingGarageKeyRecord D_shelter_1f_parking_garage_80181988;

s32 func_shelter_1f_parking_garage_8017D6AC(RoomEventMsg* in, RoomEventMsg* out);

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

/// Answers the progress query `in->msgId` in `out->field_3`, unless
/// `in->field_5` is set. Six queries have an answer, each read from a
/// game-flag nibble: 2 answers 2 once nibble 0x10F is set and 3 once nibble
/// 0x11A reaches 2; 5, 41 and 45 answer nibbles 0xA4, 0xB6 and 0xB7 plus one;
/// 16 answers 3 once nibble 0x7A reaches 6; and 20 maps nibble 0xF4's values
/// 0-3 to 1, 6, 7 and 8 (1 otherwise). Any other query leaves `out`
/// untouched. Always returns 1.
s32 func_shelter_1f_parking_garage_8017D6AC(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_5 == 0) {
        switch (in->msgId) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    out->field_3 = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    out->field_3 = 3;
                }
                break;
            case 5:
                out->field_3 = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    out->field_3 = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        out->field_3 = 1;
                        break;
                    case 1:
                        out->field_3 = 6;
                        break;
                    case 2:
                        out->field_3 = 7;
                        break;
                    case 3:
                        out->field_3 = 8;
                        break;
                    default:
                        out->field_3 = 1;
                        break;
                }
                break;
            case 45:
                out->field_3 = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                out->field_3 = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

/// The room's exit task, run on the record published in
/// `D_shelter_1f_parking_garage_80181988`. State 0 sends the record's
/// `field_4` to the slot-3 game pointer as message 0x3EE, going straight to
/// state 2 when it is -1; state 1 waits until that pointer answers 0x3F0
/// with 0. States 2 and 3 play the record's sound event, if any, and wait for
/// it to go quiet. State 4 queues type-7 sound event 0x80000000, commits the
/// record's stage, area, warp and room to the save data, spawns task type
/// 0x11 and kills itself.
void func_shelter_1f_parking_garage_8017D7E8(Task* arg0)
{
    GpMsg3EE msg;
    void*    slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.field_12 = D_shelter_1f_parking_garage_80181988.field_4;
            if (msg.field_12 == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_shelter_1f_parking_garage_80181988.field_8 == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_shelter_1f_parking_garage_80181988.field_8, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_shelter_1f_parking_garage_80181988.field_8) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            D_80071076                = 1;
            Mc_SaveData.at4.loc.stage = D_shelter_1f_parking_garage_80181988.field_0;
            Mc_SaveData.at4.loc.area  = D_shelter_1f_parking_garage_80181988.field_1;
            Mc_SaveData.at4.loc.warp  = D_shelter_1f_parking_garage_80181988.field_2;
            Mc_SaveData.at4.loc.room  = D_shelter_1f_parking_garage_80181988.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// The room's event task, spawned when the message handler starts an event.
/// State 0 runs the latched event's CAP command; state 1 waits for it and,
/// when the event's `field_A` asks for it, spawns helper task 0x31; states 2
/// and 3 play the event's stage sound, if any, and wait for it; state 4
/// commits the latched message's area, warp and room to the save data, spawns
/// task type 0x11 and kills itself.
void func_shelter_1f_parking_garage_8017D958(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_parking_garage_80181998.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_parking_garage_80181998.field_A != 0) {
                    D_shelter_1f_parking_garage_80181974.field_0 = 0;
                    D_shelter_1f_parking_garage_80181974.field_1 = 0;
                    D_shelter_1f_parking_garage_80181974.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_1f_parking_garage_80181974);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_parking_garage_80181998.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_parking_garage_80181998.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_parking_garage_80181998.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_1f_parking_garage_8018197C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_1f_parking_garage_8018197C.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_1f_parking_garage_8018197C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

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
    s32                               (*handler)(RoomEventMsg*, RoomEventMsg*);

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

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_parking_garage/shelter_1f_parking_garage", D_shelter_1f_parking_garage_8017D6A0);
