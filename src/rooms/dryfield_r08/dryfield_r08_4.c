#include "common.h"

/// The room's active data bank pointer, which starts out holding bank 0, and
/// the two banks `func_dryfield_r08_8017F438` chooses between.
extern void* D_dryfield_r08_8017F708;
extern u32   D_dryfield_r08_801809C0[];
extern u32   D_dryfield_r08_80180B58[];

/// Publishes one of the room's two data banks as the active one: bank 0 for a
/// zero argument, bank 1 otherwise.
void func_dryfield_r08_8017F438(s16 arg0)
{
    if (arg0 == 0) {
        D_dryfield_r08_8017F708 = D_dryfield_r08_801809C0;
        return;
    }
    D_dryfield_r08_8017F708 = D_dryfield_r08_80180B58;
}
