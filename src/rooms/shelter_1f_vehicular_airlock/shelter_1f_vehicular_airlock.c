#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_1f_vehicular_airlock.h"

extern s32 func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

extern s16 D_80071076;
extern u8  D_801153F4;
extern u8  D_80115690;

extern TaskDesc D_shelter_1f_vehicular_airlock_80182028;

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern GpStateBD8 D_shelter_1f_vehicular_airlock_80182AA0;

/// The message and event the message handler latched for the room's event
/// task.
extern RoomEventMsg                   D_shelter_1f_vehicular_airlock_80182AA8;
extern s8                             D_shelter_1f_vehicular_airlock_80182AB0;
extern Shelter1fVehicularAirlockEvent D_shelter_1f_vehicular_airlock_80182AB4;

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_shelter_1f_vehicular_airlock_8017D644(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_vehicular_airlock_80182AB4.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_vehicular_airlock_80182AB4.field_A != 0) {
                    D_shelter_1f_vehicular_airlock_80182AA0.field_0 = 0;
                    D_shelter_1f_vehicular_airlock_80182AA0.field_1 = 0;
                    D_shelter_1f_vehicular_airlock_80182AA0.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_1f_vehicular_airlock_80182AA0);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_vehicular_airlock_80182AB4.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_vehicular_airlock_80182AB4.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_vehicular_airlock_80182AB4.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_1f_vehicular_airlock_80182AA8.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_1f_vehicular_airlock_80182AA8.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_1f_vehicular_airlock_80182AA8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

static __inline__ s32 _shelter1fVehicularAirlockStartEvent(RoomEventMsg* dst, Shelter1fVehicularAirlockEvent* event)
{
    D_shelter_1f_vehicular_airlock_80182AB0 = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_shelter_1f_vehicular_airlock_80182AA8 = *dst;
            D_shelter_1f_vehicular_airlock_80182AB4 = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_vehicular_airlock_80182028, 0, 0, 0);
            D_shelter_1f_vehicular_airlock_80182AB0 = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D7DC(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    Shelter1fVehicularAirlockEvent event;

    *out = *in;
    func_80179B14(in, out);
    if (in->msgId == 3) {
        if (GameFlag_GetNibble(0xB2) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(2);
            }
            return 0;
        }
        event.field_0 = 4;
        event.field_4 = 0x55020003;
        event.field_8 = 0x15B;
        event.field_A = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    if (in->msgId == 5) {
        event.field_0 = 6;
        event.field_4 = 0x55020001;
        event.field_8 = 0x15A;
        event.field_A = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D988(void)
{
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D990(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        if (Gp_GetCurBit2Flag(6) == 2 && GameFlag_GetNibble(0x7A) >= 6) {
            arg2 = 5;
        }
        Gp_SpawnIfCapIdle(arg2, 0);
    }
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D9F4(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_vehicular_airlock/shelter_1f_vehicular_airlock", D_shelter_1f_vehicular_airlock_8017D5D8);
