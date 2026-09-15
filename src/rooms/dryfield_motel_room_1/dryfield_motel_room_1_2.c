#include "common.h"

#include "rooms/dryfield_motel_room_1.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017D7AC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017DC2C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017DD3C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017DF08);

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
