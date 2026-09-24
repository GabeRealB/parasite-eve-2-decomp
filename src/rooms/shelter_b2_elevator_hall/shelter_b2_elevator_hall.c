#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b2_elevator_hall.h"

extern s16 D_80071076;
extern u8  D_801153F4;

extern s32      func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc D_shelter_b2_elevator_hall_8018379C;

/// Elevator task: sends player-weapon message 0 and waits for the cap to go
/// idle, then picks the destination from the cap event key (0xB: area 9 warp 3,
/// 0xC: area 0x1B warp 2, 0xD: area 0x2A warp 3; any other key sends message 1
/// and ends the task). Once the voice line in `spawnArg1` has finished it
/// resolves the destination through `func_80179A04`, stores the resolved warp
/// and room in the save location and spawns task 0x11.
void func_shelter_b2_elevator_hall_8017D8E4(Task* task)
{
    RoomEventMsg msg;
    RoomEventMsg msg2;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            D_801153F4 = 1;
            goto next;
        case 1:
            if (Gp_CapBusy() == 0) {
                D_801153F4 = 0;
                goto next;
            }
            break;
        case 2:
            D_801153F4 = 1;
            switch (Gp_GetCapEventKey()) {
                case 0xB:
                    Mc_SaveData.at4.loc.area = 9;
                    Mc_SaveData.at4.loc.warp = 3;
                    break;
                case 0xC:
                    Mc_SaveData.at4.loc.area = 0x1B;
                    Mc_SaveData.at4.loc.warp = 2;
                    break;
                case 0xD:
                    Mc_SaveData.at4.loc.area = 0x2A;
                    Mc_SaveData.at4.loc.warp = 3;
                    break;
                default:
                    Gp_MsgPlayerWeapon(1);
                    D_801153F4 = 0;
                    taskKill(task);
                    break;
            }
            goto next;
        case 3:
            if (SndVoice_HasActiveId(task->spawnArg1) != 0) {
                break;
            }
        next:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            msg.field_3 = 1;
            msg.field_5 = 0;
            msg.msgId   = Mc_SaveData.at4.loc.area;
            msg.field_2 = Mc_SaveData.at4.loc.warp;
            msg2        = msg;
            func_80179A04(&msg, &msg2);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.warp = msg2.field_2;
            Mc_SaveData.at4.loc.room = msg2.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 0x21 and 0x1C build a request for the gate
/// `func_shelter_b2_elevator_hall_8017D610` (nibble 0xAB with no item, and
/// nibble 0xA9 with item 0x21, which also sets item-seen bit 0x121 when the
/// gate reports the event fired). Message 0x1A
/// answers 0 and, unless `in->field_5` asks for a dry run, either sets the
/// message's nibble and runs CAP command 4 while nibble 0xBA is clear, or runs
/// CAP command 5 and spawns the room's task once it is set. Anything else
/// answers 1.
s32 func_shelter_b2_elevator_hall_8017DAD4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x21) {
        req.field_0 = 1;
        req.field_4 = 1;
        req.field_8 = 0x541B0007;
        req.field_C = 0x541B0005;
        req.flagId  = 0xAB;
        req.itemId  = 0;
        return func_shelter_b2_elevator_hall_8017D610(&req, out);
    }
    if (in->msgId == 0x1C) {
        req.field_0 = 3;
        req.field_4 = 2;
        req.field_8 = 0x541B0009;
        req.field_C = 0x541B0003;
        req.flagId  = 0xA9;
        req.itemId  = 0x21;
        ret         = func_shelter_b2_elevator_hall_8017D610(&req, out);
        if (D_shelter_b2_elevator_hall_80184D84 != 0) {
            Gp_SetItemSeenBit(0x121, 1);
        }
        return ret;
    }
    if (in->msgId == 0x1A) {
        if (GameFlag_GetNibble(0xBA) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(4);
            }
            return 0;
        }
        if (in->field_5 == 0) {
            Gp_RunCapCmd(5, 0);
            Task_SpawnFromTable(&D_shelter_b2_elevator_hall_8018379C, 0, 0x541B0001, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b2_elevator_hall_8017DC70(void)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC78(void)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC80(void)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC88(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        SndEvt_EnqueueType6(0x541B0000 | 1, 0, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_elevator_hall/shelter_b2_elevator_hall", D_shelter_b2_elevator_hall_8017D5F0);
