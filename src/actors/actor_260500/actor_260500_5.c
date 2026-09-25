#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

/// Loads cap file 2 and starts it (`func_800E6D4C(0x340, 0)`) when `arg0` is
/// non-zero, otherwise resets the cap state.
void func_actor_260500_80149E38(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}
