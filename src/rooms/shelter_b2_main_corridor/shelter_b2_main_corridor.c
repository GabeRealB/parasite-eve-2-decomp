#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/shelter_b2_main_corridor.h"

/// `Gp_StateF0.field_4` (0x801153F4). Declared as a one-element array so the
/// store keeps the in-struct memory attribute a struct-member store has, which
/// makes it alias the task's argument load and keeps the two in source order.
extern u8 D_801153F4[1];
extern u8 D_80115690;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017D6BC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017D82C);

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
