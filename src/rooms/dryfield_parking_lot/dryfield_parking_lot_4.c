#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

/// Per-view values `func_dryfield_parking_lot_8017DBAC` publishes, indexed by
/// camera view index minus one.
extern u16 D_dryfield_parking_lot_8017DC34[];

/// Publishes the value the current camera view maps to: stores
/// `D_dryfield_parking_lot_8017DC34[view - 1]` into `Gp_State1C`'s
/// `roomEffectMode`. Nothing in the room calls it; gameplay's data holds its
/// address.
void func_dryfield_parking_lot_8017DBAC(void)
{
    Gp_State1C->roomEffectMode = D_dryfield_parking_lot_8017DC34[(Gp_GetViewIndex() & 0xFF) - 1];
}
