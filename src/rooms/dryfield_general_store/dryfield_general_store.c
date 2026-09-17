#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

extern TaskDesc D_dryfield_general_store_8017E164;

/// The store's two script arguments, latched from the message that armed the
/// cutscene task `D_dryfield_general_store_8017E164`; the task itself reads them
/// back to place its actors.
extern u8 D_dryfield_general_store_80185709;
extern u8 D_dryfield_general_store_8018570A;

/// Handler for the store's two event ids. Both answer with a furniture-style
/// "which variant" byte in `out->field_3`, and a non-zero `field_5` asks what
/// would happen without the side effects.
///
/// Message 1 is the grandfather clock: with nibble 0x63 clear the reply is the
/// id itself, otherwise 4, or 2 + nibble 0x61 while nibble 0x7A is still below
/// 4. The final arm offers the gate a request that plays the two stage sounds
/// 0x5203000C / 0x52030003 under flag nibble 0x3B.
///
/// Message 0x26 is the shop till: with nibble 0xC9 set the reply is 2, or 1
/// while nibble 0x53 is clear, plus 2 more while nibble 0x51 is clear;
/// otherwise 5, or 6 while nibble 0x51 is clear. The arm that is not asking
/// latches `field_2` / `field_3` for the spawned task and answers 2, or runs
/// CAP command 0xE when nibble 0x62 is set. Anything else answers 1.
s32 func_dryfield_general_store_8017D8D4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;
    s32          v;

    *out  = *in;
    msgId = in->msgId;
    if (msgId == 1 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x63) == 0) {
            out->field_3 = msgId;
        } else {
            if (GameFlag_GetNibble(0x7A) < 4) {
                v = 4;
                TOUCH_REG(v);
                v = GameFlag_GetNibble(0x61) + 2;
            } else {
                v = 4;
            }
            out->field_3 = v;
        }
    }
    if (in->msgId == 0x26 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) == 0) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->field_3 = out->field_3 + 2;
            }
        } else if (GameFlag_GetNibble(0x51) == 0) {
            out->field_3 = 6;
        } else {
            out->field_3 = 5;
        }
    }
    if (in->msgId == 1) {
        req.field_0 = 0xD;
        req.field_4 = 0xD;
        req.field_8 = Gp_PackStageSndId(0x5203000C);
        req.field_C = Gp_PackStageSndId(0x52030003);
        req.flagId  = 0x3B;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, in);
    }
    if (in->msgId != 0x26) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 2;
    }
    if (GameFlag_GetNibble(0x62) == 0) {
        Task_SpawnFromTable(&D_dryfield_general_store_8017E164, 1, 0, 0);
        D_dryfield_general_store_80185709 = in->field_2;
        D_dryfield_general_store_8018570A = in->field_3;
    } else {
        Gp_RunCapCmd1(0xE);
    }
    return 2;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_general_store/dryfield_general_store", func_dryfield_general_store_8017DAC0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_general_store/dryfield_general_store", RoomsShared8017d878Table);
