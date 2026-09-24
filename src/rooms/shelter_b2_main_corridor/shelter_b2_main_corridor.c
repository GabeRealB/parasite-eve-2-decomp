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
#include "rooms/shelter_b2_main_corridor.h"

/// `Gp_StateF0.field_4` (0x801153F4). Declared as a one-element array so the
/// store keeps the in-struct memory attribute a struct-member store has, which
/// makes it alias the task's argument load and keeps the two in source order.
extern u8  D_801153F4[1];
extern u8  D_80115690;
extern s16 D_80071076;

/// Spawn argument of the helper task 0x31 the room's exit task starts.
extern GpStateBD8 D_shelter_b2_main_corridor_8018964C;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Carries out a staged event once the message handler has passed it through
/// `func_shelter_b2_main_corridor_8017E0FC`, from the copy in
/// `D_shelter_b2_main_corridor_80189664`. State 0 sends the event's `field_4`
/// to the slot-3 game pointer as message 0x3EE, unless it is -1, in which case
/// it skips to state 2; state 1 polls that pointer with message 0x3F0 until it
/// answers 0. States 2 and 3 queue the event's sound `field_8`, if any, and
/// wait for its voice to end. State 4 copies the event's stage, area, warp and
/// room into the save location and spawns the room-load task 0x11.
void func_shelter_b2_main_corridor_8017D6BC(Task* arg0)
{
    GpMsg3EE msg;
    void*    slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.field_12 = D_shelter_b2_main_corridor_80189664.field_4;
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
            if (D_shelter_b2_main_corridor_80189664.field_8 == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_shelter_b2_main_corridor_80189664.field_8, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_shelter_b2_main_corridor_80189664.field_8) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            D_80071076                = 1;
            Mc_SaveData.at4.loc.stage = D_shelter_b2_main_corridor_80189664.field_0;
            Mc_SaveData.at4.loc.area  = D_shelter_b2_main_corridor_80189664.field_1;
            Mc_SaveData.at4.loc.warp  = D_shelter_b2_main_corridor_80189664.field_2;
            Mc_SaveData.at4.loc.room  = D_shelter_b2_main_corridor_80189664.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// Carries out a room exit staged by the message handler in
/// `D_shelter_b2_main_corridor_80189674`. State 0 runs the exit's capture
/// command; state 1 waits for it to finish and, when the exit's `field_A` asks
/// for it, spawns helper task 0x31; states 2 and 3 queue the exit's stage
/// sound, if any, and wait for its voice to end. State 4 copies the destination
/// of the outgoing message `D_shelter_b2_main_corridor_80189654` into the save
/// location and spawns the room-load task 0x11.
void func_shelter_b2_main_corridor_8017D82C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4[0] = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_main_corridor_80189674.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_main_corridor_80189674.field_A != 0) {
                    D_shelter_b2_main_corridor_8018964C.field_0 = 0;
                    D_shelter_b2_main_corridor_8018964C.field_1 = 0;
                    D_shelter_b2_main_corridor_8018964C.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_main_corridor_8018964C);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_main_corridor_80189674.sndId != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_main_corridor_80189674.sndId, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_main_corridor_80189674.sndId)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_main_corridor_80189654.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_main_corridor_80189654.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b2_main_corridor_80189654.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b2_main_corridor_8017D9C4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    ShelterB2MainCorridorExit  staged;
    ShelterB2MainCorridorExit* p;
    s32                        capCmd;
    s16                        flag;
    s32                        sndId;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x1B && GameFlag_GetNibble(0xAB) == 0) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(1);
        return 0;
    }
    if (in->msgId == 0x1F && GameFlag_GetNibble(0xB1) == 0) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(2);
        return 0;
    }
    if ((in->msgId == 0x1F || in->msgId == 0x20 || in->msgId == 0x1B) && GameFlag_GetNibble(0xD1) == 2) {
        if (in->field_5 != 0) {
            return 2;
        }
        Gp_RunCapCmd1(4);
        return 2;
    }
    if (in->msgId == 0x22) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId        = 0x54210001;
        capCmd       = 0xA;
        staged.sndId = sndId;
        flag         = 0x133;
    } else if (in->msgId == 0x20) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId        = 0x54210001;
        capCmd       = 9;
        staged.sndId = sndId;
        flag         = 0x134;
    } else if (in->msgId == 0x1B) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId        = 0x54210001;
        capCmd       = 0xB;
        staged.sndId = sndId;
        flag         = 0x135;
    } else if (in->msgId == 0x1F) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId        = 0x54210001;
        capCmd       = 0xC;
        staged.sndId = sndId;
        flag         = 0x136;
    } else {
        return 1;
    }
    p                                   = &staged;
    staged.capCmd                       = capCmd;
    staged.flag                         = flag;
    staged.field_A                      = 0;
    D_shelter_b2_main_corridor_8018965C = 0;
    if (GameFlag_GetNibble(p->flag) == 0 || p->flag == 0) {
        if (out->field_5 != 0) {
            return 2;
        }
        D_shelter_b2_main_corridor_80189654 = *out;
        D_shelter_b2_main_corridor_80189674 = staged;
        if (p->flag != 0) {
            GameFlag_SetNibble(p->flag, 1);
        }
        Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C08, 0, 0, 0);
        D_shelter_b2_main_corridor_8018965C = 1;
        return 2;
    }
    return 1;
}

s32 func_shelter_b2_main_corridor_8017DC88(Task* arg0, s32 arg1, GpMsg13EF* arg2, s32 arg3)
{
    s32 id;

    if (arg2->field_2 == 0xA) {
        if (arg2->field_3 == 7) {
            if (GameFlag_GetNibble(0xAE) != 0) {
                if (GameFlag_GetNibble(0xDA) != 0) {
                    D_shelter_b2_main_corridor_80189684.field_0 = 5;
                    D_shelter_b2_main_corridor_80189684.field_1 = arg2->field_3;
                } else {
                    D_shelter_b2_main_corridor_80189684.field_0 = 4;
                    D_shelter_b2_main_corridor_80189684.field_1 = 0x31;
                }
                D_shelter_b2_main_corridor_80189684.field_3 = 1;
                D_shelter_b2_main_corridor_80189684.field_2 = 1;
                D_shelter_b2_main_corridor_80189684.field_8 = 0;
                D_shelter_b2_main_corridor_80189684.field_4 = -1;
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 0, 6, 0);
            } else {
                Gp_RunCapCmd1(3);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 1, 0x1C4, 0);
            }
        }
        if (arg2->field_3 == 8) {
            if (GameFlag_GetNibble(0xD1) == 2) {
                Gp_RunCapCmd1(4);
                return 0;
            }
            if (GameFlag_GetNibble(0xF8) != 0) {
                Gp_RunCapCmd1(8);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 1, 0x1AF, 0);
                return 0;
            }
            if (GameFlag_GetNibble(0xDF) != 0) {
                id = 0xD;
            } else {
                id = 7;
            }
            D_shelter_b2_main_corridor_80189684.field_0 = 5;
            D_shelter_b2_main_corridor_80189684.field_1 = arg2->field_3;
            D_shelter_b2_main_corridor_80189684.field_3 = 1;
            D_shelter_b2_main_corridor_80189684.field_2 = 1;
            D_shelter_b2_main_corridor_80189684.field_8 = 0;
            D_shelter_b2_main_corridor_80189684.field_4 = -1;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 0, id, 0);
        }
    }
    if (arg2->field_2 == 1 && GameFlag_GetNibble(0x82) >= 2 && GameFlag_GetNibble(0xD3) == 0) {
        GameFlag_SetNibble(0xD3, 1);
        GameFlag_SetNibble(0xAE, 1);
        GameFlag_SetNibble(0x1C4, 0);
        func_800E8614((s32)&D_shelter_b2_main_corridor_80182CA8, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", D_shelter_b2_main_corridor_8017D5F0);

void func_shelter_b2_main_corridor_8017DEB0(Task* arg0)
{
    RoomEventMsg param;
    s32          (*resolve)(RoomEventMsg*, RoomEventMsg*);

    switch (arg0->state) {
        case 0:
            D_801153F4[0] = 1;
            Gp_RunCapCmd1(arg0->spawnArg1);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            arg0->state++;
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xC) {
                D_801153F4[0] = 0;
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                break;
            }
            arg0->state++;
            break;
        case 3:
            arg0->state++;
            break;
        case 4:
            if (D_shelter_b2_main_corridor_80189684.field_1 == 7 && GameFlag_GetNibble(0xD1) == 2) {
                GameFlag_SetNibble(0x4C, 9);
            }
            if (D_shelter_b2_main_corridor_80189684.field_1 == 0x31) {
                if (GameFlag_GetNibble(0xD1) == 2) {
                    GameFlag_SetNibble(0x4C, 9);
                }
                if (GameFlag_GetNibble(0xDA) == 0) {
                    GameFlag_SetNibble(0xDA, 1);
                    GameFlag_SetNibble(0x7A, 5);
                }
            }
            if (D_shelter_b2_main_corridor_80189684.field_1 == 8) {
                if (GameFlag_GetNibble(0xDF) == 1) {
                    GameFlag_SetNibble(0xF8, 1);
                }
            }
            resolve = func_shelter_b2_main_corridor_8017E0FC;
            Gp_MsgPlayerWeapon(0);
            param.msgId   = D_shelter_b2_main_corridor_80189684.field_1;
            param.field_2 = D_shelter_b2_main_corridor_80189684.field_2;
            param.field_3 = D_shelter_b2_main_corridor_80189684.field_3;
            param.field_5 = 0;
            resolve(&param, &param);
            D_shelter_b2_main_corridor_80189684.field_1 = param.msgId;
            D_shelter_b2_main_corridor_80189684.field_2 = param.field_2;
            D_shelter_b2_main_corridor_80189684.field_3 = param.field_3;
            D_shelter_b2_main_corridor_80189664         = D_shelter_b2_main_corridor_80189684;
            Task_SpawnFromTable(&D_shelter_b2_main_corridor_801828E4, 0, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Answers the marker query for one Neo Ark map room; the Neo Ark area map
/// carries the same body. Most rooms have no
/// marker; the five that do read a GameFlag nibble, either straight (plus one,
/// rooms 7 / 13 / 32) or folded into a fixed set of states (rooms 20 and 21).
s32 func_shelter_b2_main_corridor_8017E0FC(RoomEventMsg* in, RoomEventMsg* out)
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

s32 func_shelter_b2_main_corridor_8017E1CC(void)
{
    return 0;
}

s32 func_shelter_b2_main_corridor_8017E1D4(void)
{
    return 0;
}

s32 func_shelter_b2_main_corridor_8017E1DC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 9) {
        SndEvt_EnqueueType6(0x54210000 | 9, 0, 0);
    }
    return 0;
}
