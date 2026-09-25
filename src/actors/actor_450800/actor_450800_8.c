#include "common.h"

#include "gameplay/3CD8.h"

#include "actors/actor_450800.h"

/// Callback the overlay's event scripts name: a non-zero `arg0` clears
/// `Gp_CapFile`, loads capture file 2 and hands 0x340 to `func_800E6D4C`; zero
/// resets the capture state instead.
void func_actor_450800_80131F28(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}
