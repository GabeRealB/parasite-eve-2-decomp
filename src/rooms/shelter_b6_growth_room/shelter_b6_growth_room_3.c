#include "common.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "rooms/shelter_b6_growth_room.h"

void func_shelter_b6_growth_room_8017E448(s16 arg0, s16 arg1)
{
    POLY_G4* prim;

    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, arg1, arg1, arg1);
    setRGB3(prim, arg1, arg1, arg1);
    setXY4(prim, -160, 120 - arg0, 160, 120 - arg0, -160, 120, 160, 120);
    addPrim((u_long*)((((u32)(0x40 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, 0x40);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_3", func_shelter_b6_growth_room_8017E564);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_3", func_shelter_b6_growth_room_8017E7F0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_3", func_shelter_b6_growth_room_8017EAC8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room_3", func_shelter_b6_growth_room_8017ED28);
