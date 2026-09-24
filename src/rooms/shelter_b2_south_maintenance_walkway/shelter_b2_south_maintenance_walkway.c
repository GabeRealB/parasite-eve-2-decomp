#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b2_south_maintenance_walkway.h"

/// Parameters of the event the walkway's message handler starts, latched into
/// the room's pending copy when it fires. `field_0` is the CAP command the
/// event task runs and `field_4` the stage sound it plays (0 for none).
/// `flagId` is the game-flag nibble that records the event as done: a set
/// nibble stops it firing again, and starting it sets the nibble (0 means no
/// flag). A non-zero `field_A` has the event task spawn task 0x31.
typedef struct _WalkwayEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _WalkwayEvent;

extern s16 D_80071076;
extern u8  D_801153F4;
extern u8  D_80115690;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Descriptor of the event task the walkway's handler spawns for its own event.
extern TaskDesc D_shelter_b2_south_maintenance_walkway_80182544;

/// Spawn payload of the task 0x31 the event task may start.
extern GpStateBD8 D_shelter_b2_south_maintenance_walkway_801838D4;

/// The message and the event the walkway's handler latched for its event task,
/// and the flag saying its last call did so.
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

/// The event task the walkway's message handler spawns for its own event. It
/// runs the latched event's CAP command and waits for it to finish, starting
/// task 0x31 when the event asks for it; then plays the event's stage sound
/// (if any) and waits for the voice to end. Finally it commits the latched
/// message's area, warp and room as the save location, respawns the player
/// task as type 0x11 and ends.
void func_shelter_b2_south_maintenance_walkway_8017D8E4(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_south_maintenance_walkway_80183918.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_south_maintenance_walkway_80183918.field_A != 0) {
                    D_shelter_b2_south_maintenance_walkway_801838D4.field_0 = 0;
                    D_shelter_b2_south_maintenance_walkway_801838D4.field_1 = 0;
                    D_shelter_b2_south_maintenance_walkway_801838D4.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_south_maintenance_walkway_801838D4);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_south_maintenance_walkway_80183918.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_south_maintenance_walkway_80183918.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_south_maintenance_walkway_80183918.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_south_maintenance_walkway_801838EC.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_south_maintenance_walkway_801838EC.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_south_maintenance_walkway_801838EC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Message 0x1D goes through the room's event gate on flag
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
