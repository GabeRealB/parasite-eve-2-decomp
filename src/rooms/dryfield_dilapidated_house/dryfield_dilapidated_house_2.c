#include "common.h"

#include "main/display.h"
#include "main/task.h"

extern Task*    D_dryfield_dilapidated_house_801857E8;
extern TaskDesc D_dryfield_dilapidated_house_80186854;
extern Task*    D_dryfield_dilapidated_house_80189B7C;
/// Declared as a one-element array rather than a bare `s16`: measured. With a
/// bare scalar, GCC 2.8.1's `fixed_scalar_and_varying_struct_p` concludes it
/// cannot alias the `Task*` store beside it in
/// `func_dryfield_dilapidated_house_8017E970` and swaps the two stores.
extern s16 D_dryfield_dilapidated_house_80189B82[1];

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_2", func_dryfield_dilapidated_house_8017E8E8);

void func_dryfield_dilapidated_house_8017E970(s32 arg0)
{
    if (arg0 == 0) {
        D_dryfield_dilapidated_house_80189B7C->state = 0;
        D_dryfield_dilapidated_house_80189B82[0]     = 1;
        return;
    }
    D_dryfield_dilapidated_house_80189B7C->state     = 2;
    D_dryfield_dilapidated_house_80189B7C->spawnArg1 = arg0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_2", func_dryfield_dilapidated_house_8017E9A4);

void func_dryfield_dilapidated_house_8017EA10(s32 arg0)
{
    if (arg0 != 0) {
        D_dryfield_dilapidated_house_801857E8 = Task_SpawnFromTable(&D_dryfield_dilapidated_house_80186854, 0, 3, Game_GetPtrSlot(3));
        return;
    }
    if (D_dryfield_dilapidated_house_801857E8 != NULL) {
        Task_Kill(D_dryfield_dilapidated_house_801857E8);
        D_dryfield_dilapidated_house_801857E8 = NULL;
    }
}
