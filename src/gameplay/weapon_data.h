#ifndef GAMEPLAY_PRIVATE_WEAPON_DATA_H
#define GAMEPLAY_PRIVATE_WEAPON_DATA_H

#include "common.h"

/// One damage-scale row of `Gp_DmgRows`, selected by `gSceneCombatState.difficulty`.
/// Each half holds five columns picked through `D_80113F54` by HP / 10:
/// `field_A` scales against the player's HP, `field_0` against the companion's.
typedef struct _GpDmgRow {
    /* 0x00 */ u16 field_0[5];
    /* 0x0A */ u16 field_A[5];
} GpDmgRow;
STATIC_ASSERT_SIZEOF(GpDmgRow, 0x14);

#endif // GAMEPLAY_PRIVATE_WEAPON_DATA_H
