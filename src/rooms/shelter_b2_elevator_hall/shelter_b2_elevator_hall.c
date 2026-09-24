#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

extern s32      func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc D_shelter_b2_elevator_hall_8018379C;

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_elevator_hall/shelter_b2_elevator_hall", func_shelter_b2_elevator_hall_8017D8E4);

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 0x21 and 0x1C build a request for the gate at
/// `RoomsShared8017d638` (nibbles 0xAB and 0xA9, the latter with collected bit
/// 0x121, which it sets when the gate reports the event fired). Message 0x1A
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
        return RoomsShared8017d638(&req, out);
    }
    if (in->msgId == 0x1C) {
        req.field_0 = 3;
        req.field_4 = 2;
        req.field_8 = 0x541B0009;
        req.field_C = 0x541B0003;
        req.flagId  = 0xA9;
        req.itemId  = 0x21;
        ret         = RoomsShared8017d638(&req, out);
        if (RoomsShared8017d638Flag != 0) {
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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_elevator_hall/shelter_b2_elevator_hall", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_elevator_hall/shelter_b2_elevator_hall", jtbl_shelter_b2_elevator_hall_8017D5FC);
