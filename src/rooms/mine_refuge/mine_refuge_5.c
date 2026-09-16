#include "common.h"

#include "main/gameflag.h"

#include "gameplay/D4.h"

#include "rooms/room_common.h"

INCLUDE_ASM("rooms/nonmatchings/mine_refuge/mine_refuge_5", func_mine_refuge_8018029C);

INCLUDE_ASM("rooms/nonmatchings/mine_refuge/mine_refuge_5", func_mine_refuge_80180710);

INCLUDE_ASM("rooms/nonmatchings/mine_refuge/mine_refuge_5", func_mine_refuge_80181094);

void func_mine_refuge_8018029C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_mine_refuge_80180710(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_mine_refuge_80181094(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draw anchors of the refuge, in the shared data blob at the end of the
/// overlay: `D8` and `E0` are the two wedges of the first view, `E8` heads the
/// run the later views sweep. `E0` is reached both as `D8[1]` (the draw pair
/// below) and by its own name (view 6), and the two forms are different code -
/// indexing emits `D8+8`, naming emits its own `lui` - so it keeps its own
/// declaration.
extern SVECTOR D_mine_refuge_801818D8[];
extern SVECTOR D_mine_refuge_801818E0;
extern SVECTOR D_mine_refuge_801818E8;

/// Draws the refuge for whichever view is current. View 2 draws two wedges in
/// sequence; views 3 and 4/5 sweep one anchor each, but only while progress
/// flag 0xC3 is 1, and view 6 draws on its own. Every case shares the one
/// epilogue, so 6 falls into it and the rest jump.
void func_mine_refuge_80181454(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw17(&D_mine_refuge_801818D8[0], 1, 0x300);
            func_mine_refuge_8018029C(&D_mine_refuge_801818D8[1], 0x60, 0x40);
            break;
        case 3:
            if (GameFlag_GetNibble(0xC3) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x30, 0xF0);
            }
            break;
        case 4:
        case 5:
            if (GameFlag_GetNibble(0xC3) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x60, 0xD0);
            }
            break;
        case 6:
            func_mine_refuge_80180710(&D_mine_refuge_801818E0, 0x60, 0x80);
            break;
    }
}
