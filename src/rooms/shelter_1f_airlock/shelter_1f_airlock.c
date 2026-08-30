#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"

/// Ambient effect emitter positions for the airlock, selected by view index.
/// `D_shelter_1f_airlock_8017E4BC` / `_8017E4C4` / `_8017E4D4` are successive
/// labels into one contiguous run of `SVECTOR`s, so the per-view lists overlap.
extern SVECTOR D_shelter_1f_airlock_8017E4BC[];
extern SVECTOR D_shelter_1f_airlock_8017E4C4[];
extern SVECTOR D_shelter_1f_airlock_8017E4D4[];

void func_shelter_1f_airlock_8017D8A8(SVECTOR* pos, s32 arg1, s32 arg2);
void func_shelter_1f_airlock_8017E0F0(SVECTOR* pos, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_airlock/shelter_1f_airlock", func_shelter_1f_airlock_8017D5D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_airlock/shelter_1f_airlock", func_shelter_1f_airlock_8017D61C);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_airlock/shelter_1f_airlock", func_shelter_1f_airlock_8017D624);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_airlock/shelter_1f_airlock", func_shelter_1f_airlock_8017D62C);

void func_shelter_1f_airlock_8017D670(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_airlock/shelter_1f_airlock", D_shelter_1f_airlock_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_airlock/shelter_1f_airlock", func_shelter_1f_airlock_8017D678);

void func_shelter_1f_airlock_8017D6D0(void)
{
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4C4[0], 0x200, 0x111);
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4C4[1], 0x200, 0x111);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[3], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[5], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[7], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[9], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[11], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[17], 0x180, 0x1011);
            break;
        case 4:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4BC[0], 0x200, 0x111);
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4BC[1], 0x200, 0x111);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[4], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[6], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[8], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[10], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[12], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[14], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[16], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[18], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[20], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[22], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[24], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[26], 0x180, 0x1011);
            break;
        case 5:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4D4[0], 0x200, 0x200);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_airlock/shelter_1f_airlock", func_shelter_1f_airlock_8017D8A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_airlock/shelter_1f_airlock", func_shelter_1f_airlock_8017E0F0);
