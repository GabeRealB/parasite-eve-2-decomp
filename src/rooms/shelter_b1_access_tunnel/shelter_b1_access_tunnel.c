#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b1_access_tunnel.h"

/// Parameters of an event the access tunnel's message handler starts, latched
/// into the room's pending copy when it fires and played back by the room's
/// event task. `field_0` is the CAP command the task runs, `field_4` the stage
/// sound it plays and waits for (0 for none), and a non-zero `field_A` makes it
/// start helper task 0x31. `flagId` is the game-flag nibble that records the
/// event as done: a set nibble stops it firing again, and starting it sets the
/// nibble (0 means no flag).
typedef struct _AccessTunnelEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _AccessTunnelEvent;

extern s16 D_80071076;
extern u8  D_801153F4;
extern u8  D_80115690;

extern s32                func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc           D_shelter_b1_access_tunnel_8017E710;
extern GpAreaApplyRec     D_shelter_b1_access_tunnel_8017FF44;
extern GpStateBD8         D_shelter_b1_access_tunnel_8017FF4C;
extern RoomEventMsg       D_shelter_b1_access_tunnel_8017FF64;
extern u8                 D_shelter_b1_access_tunnel_8017FF6C;
extern _AccessTunnelEvent D_shelter_b1_access_tunnel_8017FF90;

/// The room's event task, spawned by the message handler when it starts the
/// latched event. It runs the event's CAP command, waits for it to finish and
/// optionally starts helper task 0x31, then plays the event's stage sound and
/// waits for it; finally it writes the latched message's destination into the
/// save location and hands over to task 0x11.
void func_shelter_b1_access_tunnel_8017D8D0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b1_access_tunnel_8017FF90.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b1_access_tunnel_8017FF90.field_A != 0) {
                    D_shelter_b1_access_tunnel_8017FF4C.field_0 = 0;
                    D_shelter_b1_access_tunnel_8017FF4C.field_1 = 0;
                    D_shelter_b1_access_tunnel_8017FF4C.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b1_access_tunnel_8017FF4C);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b1_access_tunnel_8017FF90.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b1_access_tunnel_8017FF90.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b1_access_tunnel_8017FF90.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b1_access_tunnel_8017FF64.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b1_access_tunnel_8017FF64.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b1_access_tunnel_8017FF64.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

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
        return func_shelter_b1_access_tunnel_8017D5FC(&req, out);
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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_access_tunnel/shelter_b1_access_tunnel", D_shelter_b1_access_tunnel_8017D5F0);
