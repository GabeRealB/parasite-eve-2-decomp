#include "common.h"

#include "main/display.h"
#include "rooms/acropolis_cafeteria.h"

INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria", D_acropolis_cafeteria_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria", RoomsShared8017d878Table);

static const char CafeteriaPlayerLabel[12] = "Player";

void func_acropolis_cafeteria_8017D6AC(void)
{
    if (Display_State.field_112 != 0) {
        func_80724608(Game_GetPtrSlot(3), -0x8C, -0x32, (void*)CafeteriaPlayerLabel);
        func_807245E4(Game_GetPtrSlot(3));
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria", func_acropolis_cafeteria_8017D700);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria", func_acropolis_cafeteria_8017D8F8);
