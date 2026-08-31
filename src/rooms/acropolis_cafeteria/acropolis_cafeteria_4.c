#include "common.h"
#include "main/fs.h"
extern RECT D_acropolis_cafeteria_80184168;
extern RECT D_acropolis_cafeteria_80184170;

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_4", func_acropolis_cafeteria_8017E0DC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_4", func_acropolis_cafeteria_8017E154);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_4", func_acropolis_cafeteria_8017E22C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_4", func_acropolis_cafeteria_8017E27C);

void func_acropolis_cafeteria_8017E2B0(void)
{
    Gp_PulseState1C80();
}

void func_acropolis_cafeteria_8017E2D0(void)
{
    MoveImage(&D_acropolis_cafeteria_80184168, 0x180, 0x100);
    MoveImage(&D_acropolis_cafeteria_80184170, 0, 0xF7);
}
