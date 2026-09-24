#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_underground_parking.h"

extern s32                    D_shelter_b1_underground_parking_8018D758;
extern ShelterParkingSceneRec D_shelter_b1_underground_parking_8018D75C;
extern TaskDesc               D_shelter_b1_underground_parking_80187260[];
extern TaskDesc               D_shelter_b1_underground_parking_8018726C[];
extern u8                     D_80071075;
extern s8                     D_80114C12;

extern void func_80131E38(void);

/// Spawn payload handed to `Task_Spawn(1, 0x31, ...)` as arg3 when caption key
/// 0xB is answered. Only the three stores the caller makes are known.
typedef struct {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} _ShelterParkingKeySpawnArg;

extern s8                         D_8007272D;
extern TaskDesc                   D_shelter_b1_underground_parking_80187200;
extern _ShelterParkingKeySpawnArg D_shelter_b1_underground_parking_8018D750;

/// The room's ambience table, one entry per area.
extern RoomAmbienceEntry D_shelter_b1_underground_parking_8018761C[];

/// Area id published to the sound system; compared against the session's view.
extern u8 D_8007216C;

s32 func_shelter_b1_underground_parking_80183124(RoomEventMsg* in, RoomEventMsg* out);

/// Starts caption slot 0xA and spawns entry 4 of
/// `D_shelter_b1_underground_parking_8018726C` when the player asks for it.
///
/// The player must not be aiming (`field_954 != 2`), captions must be idle,
/// the session room must be 7 or later, and the model root must stand with X
/// below -0x1266 and Z inside [-0x7CF, 0x7D0), with `D_80114C12 != 1` and
/// `D_80071075` clear. Then the 0x1000 pad mask with the yaw in the 0x3FF-wide
/// window opening at 0xA01, or the 0x4000 mask with it in the window at 0x201,
/// takes the weapon away and runs the handoff.
void func_shelter_b1_underground_parking_801826C0(void)
{
    Task*          task;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            z;
    s32            facing;

    task  = gameGetPtrSlot(3);
    actor = (GameActor*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    if ((actor->field_954 != 2) && (Gp_CapBusy() == 0) && (gGameSession->at4.loc.room >= 7) &&
        (coord->coord.t[0] < -0x1266)) {
        z = coord->coord.t[2];
        if (z < 0x7D0) {
            if ((z >= -0x7CF) && (D_80114C12 != 1) && (D_80071075 == 0)) {
                facing = (u16)actor->field_52 & 0xFFF;
                if (Pad_CheckButtons(0, 0, 0x1000) != 0) {
                    if ((u32)(facing - 0xA01) < 0x3FFU) {
                        Gp_MsgPlayerWeapon(0);
                        Gp_StartCapSlot(0xA, 0, 1);
                        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                    }
                }
                if ((Pad_CheckButtons(0, 0, 0x4000) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                    Gp_MsgPlayerWeapon(0);
                    Gp_StartCapSlot(0xA, 0, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                }
            }
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_4", D_shelter_b1_underground_parking_8017D7F4);

/// Room event handler keyed on `msg->field_2`: 1 calls `func_80131E38` in
/// place 0x15, 0xA starts caption slot 0xA and sets nibble 0x1B4 to 2 while
/// the room is below 7, and 0xB / 0xC pick a caption or spawn per room.
s32 func_shelter_b1_underground_parking_80182830(Task* task, s32 msgId, RoomEventMsg* msg)
{
    if (msg->field_2 == 1 && gGameSession->at4.loc.place == 0x15) {
        func_80131E38();
    }
    if (msg->field_2 == 0xA) {
        if ((u8)msg->field_3 == 1 && gGameSession->at4.loc.room < 7) {
            Gp_StartCapSlot(0xA, 1, 0);
            GameFlag_SetNibble(0x1B4, 2);
        }
    }
    if (msg->field_2 == 0xB) {
        switch (gGameSession->at4.loc.room) {
            case 2:
                Gp_RunCapCmd1(8);
                break;
            case 3:
                Gp_RunCapCmd1(0xF);
                break;
            case 4:
                Gp_RunCapCmd1(9);
                break;
            case 5:
                Gp_MsgPlayerWeapon(0);
                Gp_RunCapCmd1(0xC);
                Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 1, 0, 0);
                break;
            case 6:
            case 7:
            case 8:
                Gp_RunCapCmd1(0xE);
                break;
        }
    }
    if (msg->field_2 == 0xC) {
        switch (gGameSession->at4.loc.room) {
            case 6:
                Gp_StartCapSlot(0xB, 1, 0);
                break;
            case 7:
                if (D_shelter_b1_underground_parking_8018D758 != 0) {
                    Gp_RunCapCmd1(0x1E);
                } else if (GameFlag_GetNibble(0x7A) < 6) {
                    Gp_MsgPlayerWeapon(0);
                    Gp_StartCapSlot(0xB, 1, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 3, 0, 0);
                } else {
                    Gp_StartCapSlot(0xB, 1, 2);
                }
                break;
            case 8:
                Gp_StartCapSlot(0xB, 1, 2);
                break;
        }
    }
    return 0;
}

s32 func_shelter_b1_underground_parking_80182A60(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    ShelterParkingSceneRec* st;

    switch (arg2) {
        case 1:
            if (gGameSession->at4.loc.room < 6) {
                if (GameFlag_GetNibble(0xC7) == 0) {
                    Gp_RunCapCmd1(1);
                } else if (GameFlag_GetNibble(0xE7) == 0) {
                    if (GameFlag_GetNibble(0xE8) == 0) {
                        Gp_RunCapCmd1(2);
                    } else {
                        Gp_MsgPlayerWeapon(0);
                        Gp_MsgPlayer3F3(0);
                        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                    }
                } else {
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgPlayer3F3(0);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(0x10);
            }
            break;
        case 7:
            switch (gGameSession->at4.loc.room) {
                case 1:
                    Gp_RunCapCmd1(7);
                    break;
                case 2:
                    Gp_RunCapCmd1(8);
                    break;
                case 3:
                    Gp_RunCapCmd1(0xF);
                    break;
                case 4:
                    Gp_RunCapCmd1(9);
                    break;
                case 6:
                case 7:
                case 8:
                    Gp_RunCapCmd1(0xE);
                    break;
            }
            break;
        case 4:
            switch (gGameSession->at4.loc.room) {
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    Gp_RunCapCmd1(4);
                    break;
                case 6:
                    Gp_RunCapCmd1(5);
                    break;
                case 7:
                    Gp_RunCapCmd1(6);
                    break;
                case 8:
                    Gp_RunCapCmd1(0x10);
                    break;
            }
            break;
        case 2:
        case 3:
        case 5:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, arg2, 0);
            break;
        case 13:
            st           = &D_shelter_b1_underground_parking_8018D75C;
            st->field_4  = 0x5414000B;
            st->field_8  = 0x5414000E;
            st->field_10 = 0x5414000C;
            st->field_C  = 0x5414000D;
            st->field_0  = 0x14;
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                if (GameFlag_GetNibble(0x158) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    GameFlag_SetNibble(0x158, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 1, 0);
                } else {
                    st->field_1 = 1;
                    st->field_3 = 1;
                    st->field_2 = 0;
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 0, 9, (s32)st);
                }
            } else {
                D_shelter_b1_underground_parking_8018D758 = 0;
                st->field_1                               = 0x1F;
                st->field_3                               = 0;
                st->field_2                               = 1;
                Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 0, arg2, (s32)st);
            }
            break;
        case 22:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_80187260, 0, arg2, 0);
            break;
        case 48:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 4, 0);
            break;
    }
    return 0;
}

/// Task body that waits for the caption to finish, then on caption key 0xB
/// spawns the 0x31 task and after 30 frames publishes
/// `D_shelter_b1_underground_parking_8018D77C` and spawns entry 0 of
/// `D_shelter_b1_underground_parking_80187200`. Any other key restores the
/// weapon and ends the task.
void func_shelter_b1_underground_parking_80182DB4(Task* task)
{
    ShelterParkingDeparture  rec;
    RoomEventMsg             msg;
    ShelterParkingDeparture* p;
    s32                      (*handler)(RoomEventMsg*, RoomEventMsg*);

    switch (task->state) {
        case 0:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                D_shelter_b1_underground_parking_8018D750.field_0 = 0;
                D_shelter_b1_underground_parking_8018D750.field_1 = 0;
                D_shelter_b1_underground_parking_8018D750.field_2 = 0x1E;
                Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b1_underground_parking_8018D750);
                task->killCountdown = 0x1E;
                task->state++;
            } else {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
        case 2:
            if (task->killCountdown == 0) {
                if (GameFlag_GetNibble(0x4B) == 0xA) {
                    GameFlag_SetNibble(0x4B, 9);
                }
                if (GameFlag_GetNibble(0x11F) == 1) {
                    GameFlag_SetNibble(0x11F, 2);
                    D_8007272D = 0x1B;
                }
                handler     = func_shelter_b1_underground_parking_80183124;
                rec.field_0 = 5;
                rec.field_1 = 1;
                rec.field_3 = 1;
                rec.field_2 = 1;
                rec.field_8 = 0x54140008;
                rec.field_4 = -1;
                Gp_MsgPlayerWeapon(0);
                p           = &rec;
                msg.msgId   = p->field_1;
                msg.field_2 = p->field_2;
                msg.field_3 = p->field_3;
                msg.field_5 = 0;
                handler(&msg, &msg);
                p->field_1                                = msg.msgId;
                p->field_2                                = msg.field_2;
                p->field_3                                = msg.field_3;
                D_shelter_b1_underground_parking_8018D77C = rec;
                Task_SpawnFromTable(&D_shelter_b1_underground_parking_80187200, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

/// Keeps the room's looping ambience in step with the area the session is in:
/// `gGameSession->at4.loc.view` selects an entry of the ambience table, and
/// state 0 starts the loop with `SndEvt_EnqueueType6`. Once
/// `D_shelter_b1_underground_parking_8018D758` is clear, state 1 queues a
/// `SndEvt_EnqueueType7` event for the loop and ends the task; otherwise it waits for the session's view to stop matching `D_8007216C`,
/// states 2 to 4 walk the task along, and state 5 retunes the loop to the new
/// entry with `SndEvt_EnqueueTypeA` and returns to state 1.
void func_shelter_b1_underground_parking_80182FC8(Task* task)
{
    s32 pan;
    s32 vol;
    u8  idx;

    idx = gGameSession->at4.loc.view;
    if (idx < 9) {
        pan = D_shelter_b1_underground_parking_8018761C[idx].pan;
        vol = D_shelter_b1_underground_parking_8018761C[idx].vol / 2;
    } else {
        pan = 0;
        vol = 0;
    }

    switch (task->state) {
        case 0:
            SndEvt_EnqueueType6(0x5414000F, (s8)pan, (s8)vol);
            task->state = task->state + 1;
            break;
        case 1:
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                func_shelter_b1_underground_parking_80186890(0);
                SndEvt_EnqueueType7(0x5414000F, 1);
                taskKill(task);
                break;
            }
            if (D_8007216C != gGameSession->at4.loc.view) {
                task->state = task->state + 1;
            }
            break;
        case 2:
        case 3:
        case 4:
            task->state = task->state + 1;
            break;
        case 5:
            SndEvt_EnqueueTypeA(0x5414000F, (s8)pan, (s8)vol);
            task->state = 1;
            break;
    }
}

/// Answers the marker query for one Neo Ark map room; the Neo Ark area map
/// carries the same body. Most rooms have no
/// marker; the five that do read a GameFlag nibble, either straight (plus one,
/// rooms 7 / 13 / 32) or folded into a fixed set of states (rooms 20 and 21).
s32 func_shelter_b1_underground_parking_80183124(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_5 == 0) {
        switch (in->msgId) {
            case 7:
                out->field_3 = GameFlag_GetNibble(0xE1) + 1;
                break;
            case 13:
                out->field_3 = GameFlag_GetNibble(0xD9) + 1;
                break;
            case 20:
                out->field_3 = 1;
                if (GameFlag_GetNibble(0xDD) != 0) {
                    if (GameFlag_GetNibble(0xDC) != 0) {
                        out->field_3 = 3;
                    } else {
                        out->field_3 = 2;
                    }
                }
                break;
            case 21:
                if (GameFlag_GetNibble(0xE9) != 0) {
                    out->field_3 = 4;
                } else {
                    out->field_3 = 1;
                }
                break;
            case 32:
                out->field_3 = GameFlag_GetNibble(0xDD) + 1;
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
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
            default:
                break;
        }
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_4", D_shelter_b1_underground_parking_8017D9A4);
