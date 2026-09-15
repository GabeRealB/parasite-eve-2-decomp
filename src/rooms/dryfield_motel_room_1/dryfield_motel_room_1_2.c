#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

#include "rooms/dryfield_motel_room_1.h"

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017D7AC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017DC2C);
INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017DD3C);

void func_dryfield_motel_room_1_8017DF08(void)
{
    Dmr1Work*  work = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->idMap;
    Dmr1Msg7DA msg;

    Gp_ArmStateF0(1);
    msg.field_0 = Game_Session->field_7;
    msg.field_1 = Game_Session->field_6;
    msg.field_2 = 3;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    Gp_DispatchMsg(work->field_C, 0x7D4, (s32)&D_dryfield_motel_room_1_8017E130[0], 0);
    Gp_DispatchMsg(work->field_10, 0x7D4, (s32)&D_dryfield_motel_room_1_8017E130[1], 0);
}

void func_dryfield_motel_room_1_8017DFB0(s16 arg0)
{
    Dmr1Work* work = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->idMap;

    work->field_2C = arg0;
    work->field_2E = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017DFD0);
void func_dryfield_motel_room_1_8017E0A0(void)
{
}
