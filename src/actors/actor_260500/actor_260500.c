#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"

extern s32 D_actor_260500_8014CB38;

/// Sends message 0x7D4 (placement) with the record at
/// `D_actor_260500_8014CB38` to the task in lookup slot 4, when there is one.
void func_actor_260500_80149E80(void)
{
    s32 slot;

    slot = Gp_LookupSlot4(0);
    if (slot != 0) {
        Gp_DispatchMsg((Task*)slot, 0x7D4, (s32)&D_actor_260500_8014CB38, 0);
    }
}
