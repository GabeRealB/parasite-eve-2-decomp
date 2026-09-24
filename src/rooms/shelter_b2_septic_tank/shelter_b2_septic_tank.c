#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_shelter_b2_septic_tank_80182F4C[];
extern TaskDesc   D_shelter_b2_septic_tank_801832C0[];
extern s16        D_shelter_b2_septic_tank_80182FFE;
extern u8         D_shelter_b2_septic_tank_80183004;
extern u8         D_shelter_b2_septic_tank_8018310C;
extern u8         D_shelter_b2_septic_tank_80187045;
extern u8         D_80071075;
extern s16        D_80071076;
extern s8         D_80114C12;
extern u8         D_801153F4;
extern u8         D_80115690;

/// Parameters of the event this room's message handler starts, latched into
/// the room's pending copy when it fires. `flagId` is the game-flag nibble that
/// records the event as done: a set nibble stops it firing again, and starting
/// it sets the nibble (0 means no flag). The event task runs `field_0` as a CAP
/// command and then plays `field_4` as a stage sound (0: none); a non-zero
/// `field_A` makes it start helper task 0x31.
typedef struct _ShelterB2SepticTankEvent {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _ShelterB2SepticTankEvent;

extern s32                       func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc                  D_shelter_b2_septic_tank_80182F40;
extern GpStateBD8                D_shelter_b2_septic_tank_80187034;
extern RoomEventMsg              D_shelter_b2_septic_tank_8018703C;
extern u8                        D_shelter_b2_septic_tank_80187044;
extern _ShelterB2SepticTankEvent D_shelter_b2_septic_tank_80187048;

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB2SepticTankStartEvent(RoomEventMsg* dst, _ShelterB2SepticTankEvent* event)
{
    D_shelter_b2_septic_tank_80187044 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_septic_tank_8018703C = *dst;
            D_shelter_b2_septic_tank_80187048 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_septic_tank_80182F40, 0, 0, 0);
            D_shelter_b2_septic_tank_80187044 = 1;
        }
        return 2;
    }
    return 1;
}

/// The room's event task, spawned by `_shelterB2SepticTankStartEvent` for the
/// event it latched in `D_shelter_b2_septic_tank_80187048`. State 0 runs the
/// event's CAP command; state 1 waits for it to finish and, when the event
/// asks for it, starts helper task 0x31; states 2 and 3 play the event's stage
/// sound, if any, and wait for it to end. State 4 copies the latched message's
/// destination into the save data and spawns the room-load task 0x11.
void func_shelter_b2_septic_tank_8017D614(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_septic_tank_80187048.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_septic_tank_80187048.field_A != 0) {
                    D_shelter_b2_septic_tank_80187034.field_0 = 0;
                    D_shelter_b2_septic_tank_80187034.field_1 = 0;
                    D_shelter_b2_septic_tank_80187034.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_septic_tank_80187034);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_septic_tank_80187048.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_septic_tank_80187048.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_septic_tank_80187048.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_septic_tank_8018703C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_septic_tank_8018703C.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b2_septic_tank_8018703C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b2_septic_tank_8017D7AC(void)
{
    return 0;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Message 0x21 starts the room's event on flag 0x131; any
/// other message answers 1.
s32 func_shelter_b2_septic_tank_8017D7B4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    _ShelterB2SepticTankEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 0x21) {
        return 1;
    }
    event.field_0 = 3;
    event.field_4 = 0x54220001;
    event.flagId  = 0x131;
    event.field_A = 0;
    return _shelterB2SepticTankStartEvent(out, &event);
}

s32 func_shelter_b2_septic_tank_8017D904(void)
{
    return 0;
}

s32 func_shelter_b2_septic_tank_8017D90C(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    u8  kind;
    s32 flag;

    kind = arg2->field_2;
    if (kind == 2) {
        flag = GameFlag_GetNibble(0xEB);
        if (flag == 1 && D_shelter_b2_septic_tank_80187045 == flag) {
            func_800E8614((s32)&D_shelter_b2_septic_tank_8018310C, 0);
            D_shelter_b2_septic_tank_80187045 = kind;
        }
    }
    return 0;
}

void func_shelter_b2_septic_tank_8017D97C(s32 arg0)
{
    GameFlag_SetNibble(0xEB, arg0);
}

void func_shelter_b2_septic_tank_8017D9A0(void)
{
    Task*          target;
    GsCOORDINATE2* player;
    GsCOORDINATE2* coords;

    target = (Task*)Gp_LookupSlot4(0);
    player = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords;
    if (target != NULL) {
        coords = ((TmdObject*)target->extra)->coords;
        D_shelter_b2_septic_tank_80182FFE =
            (ratan2(coords->coord.t[0] - player->coord.t[0], coords->coord.t[2] - player->coord.t[2]) + 0x1000) & 0xFFF;
    }
}

void func_shelter_b2_septic_tank_8017DA18(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_septic_tank_80182F4C;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b2_septic_tank_801832C0, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b2_septic_tank_8017DA74(void)
{
    s32 place;

    if (gGameSession->at4.loc.view == 4) {
        place = gGameSession->at4.loc.place;
        if (place == 1 && D_80114C12 != place && D_80071075 == 0 && D_shelter_b2_septic_tank_80187045 == 0) {
            if (GameFlag_GetNibble(0xEB) == 0) {
                func_800E8614((s32)&D_shelter_b2_septic_tank_80183004, 0);
            }
            D_shelter_b2_septic_tank_80187045 = place;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", D_shelter_b2_septic_tank_8017D5D8);
