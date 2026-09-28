#include "common.h"

#include "gameplay/captions.h"
#include "gameplay/area_transitions.h"

#include "main/mc.h"
#include "main/text.h"

extern GpAreaApplyRec D_8018A638;

/// With a non-zero `arg0`, clears `Gp_CapFile`, loads capture file 2 and
/// passes (0x140, 0x100) to `func_800E6D4C`; with zero, resets the capture
/// state instead. Reached only through the function pointers in the actor's
/// data.
void func_actor_143400_80131E24(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x140, 0x100);
        return;
    }
    Gp_ResetCap();
}

/// Applies the 0xFF-terminated area record list at `D_8018A638` through
/// `Gp_ApplyAreaRecs`. Reached only through the function pointers in the
/// actor's data.
void func_actor_143400_80131E6C(void)
{
    Gp_ApplyAreaRecs(&D_8018A638);
}

/// Stores `arg0` in the gameplay byte `Mc_SaveData[0].state.sceneEvent`. Reached only through the
/// function pointers in the actor's data.
void func_actor_143400_80131E90(s8 arg0)
{
    Mc_SaveData[0].state.sceneEvent = arg0;
}
