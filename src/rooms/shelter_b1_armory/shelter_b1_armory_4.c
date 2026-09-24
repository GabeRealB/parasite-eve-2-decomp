#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_armory.h"

extern SVECTOR D_shelter_b1_armory_80182528[];
extern SVECTOR D_shelter_b1_armory_80182538[];
extern SVECTOR D_shelter_b1_armory_80182558[];
extern SVECTOR D_shelter_b1_armory_80182568[];
extern SVECTOR D_shelter_b1_armory_80182570[];
extern SVECTOR D_shelter_b1_armory_80182578[];

void func_shelter_b1_armory_801807E4(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_shelter_b1_armory_80180934(&D_shelter_b1_armory_80182528[0], 0x200, 0, 0x10);
            func_shelter_b1_armory_80180934(&D_shelter_b1_armory_80182528[2], 0x200, 0x800, 0x111);
            func_shelter_b1_armory_80180934(&D_shelter_b1_armory_80182528[4], 0x200, 0x800, 0x111);
            break;
        case 3:
            func_shelter_b1_armory_80180934(&D_shelter_b1_armory_80182558[0], 0x200, 0, 0x111);
            func_shelter_b1_armory_8018111C(&D_shelter_b1_armory_80182558[2], 0x300, 0x11);
            if (GameFlag_GetNibble(0xF0) != 0) {
                func_shelter_b1_armory_801814C0(&D_shelter_b1_armory_80182558[3], 0x100, 0x50C0);
            } else {
                func_shelter_b1_armory_801814C0(&D_shelter_b1_armory_80182558[4], 0x100, 0x5C00);
            }
            break;
        case 8:
            func_shelter_b1_armory_80180934(D_shelter_b1_armory_80182538, 0x200, 0x800, 0x111);
            break;
        case 9:
            if (GameFlag_GetNibble(0xF0) != 0) {
                func_shelter_b1_armory_801814C0(D_shelter_b1_armory_80182570, 0x60, 0x50C0);
            } else {
                func_shelter_b1_armory_801814C0(D_shelter_b1_armory_80182578, 0x60, 0x5C00);
            }
            break;
        case 13:
            func_shelter_b1_armory_8018111C(D_shelter_b1_armory_80182568, 0x300, 0x11);
            break;
    }
}
