#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b2_operating_room.h"

/// Parameters of an event the operating room's message handler starts, latched
/// into the room's pending copy when it fires. `field_0` is the CAP command the
/// room's event task runs and `field_4` the stage sound it then plays; `flagId`
/// is the game-flag nibble that records the event as done: a set nibble stops
/// it firing again, and starting it sets the nibble (0 means no flag). A
/// non-zero `field_A` makes the event task start helper task 0x31.
typedef struct _OperatingRoomEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _OperatingRoomEvent;

extern s16 D_80071076;
extern u8  D_801153F4;
extern u8  D_80115690;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc D_shelter_b2_operating_room_80180910;

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern GpStateBD8 D_shelter_b2_operating_room_80184214;

/// The message and event the message handler latched for the room's event
/// task, and the flag saying the handler spawned it.
extern RoomEventMsg        D_shelter_b2_operating_room_8018422C;
extern u8                  D_shelter_b2_operating_room_80184234;
extern _OperatingRoomEvent D_shelter_b2_operating_room_80184258;

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_shelter_b2_operating_room_8017D8FC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_operating_room_80184258.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_operating_room_80184258.field_A != 0) {
                    D_shelter_b2_operating_room_80184214.field_0 = 0;
                    D_shelter_b2_operating_room_80184214.field_1 = 0;
                    D_shelter_b2_operating_room_80184214.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_operating_room_80184214);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_operating_room_80184258.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_operating_room_80184258.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_operating_room_80184258.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_operating_room_8018422C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_operating_room_8018422C.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b2_operating_room_8018422C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

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
/// `func_80179A04`. Message 0x1E goes through the exit gate
/// `func_shelter_b2_operating_room_8017D628` on flag 0xA8. Message 0x1C, while nibble 0xAA is clear, answers 0 and - unless
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
        return func_shelter_b2_operating_room_8017D628(&req, out);
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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_operating_room/shelter_b2_operating_room", D_shelter_b2_operating_room_8017D5F0);
