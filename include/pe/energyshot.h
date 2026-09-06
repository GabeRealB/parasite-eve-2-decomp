#ifndef PE_ENERGYSHOT_H
#define PE_ENERGYSHOT_H

#include "common.h"

/// Sixteen per-vertex texture-frame offsets, refilled once per cast by
/// `func_energyshot_8012EF34` and consumed by the GTE pass in
/// `func_energyshot_8012FA50`, where each is added to `Display_State.field_8`
/// and reduced mod 6 to pick one of the six 0x28-wide frames of the beam
/// texture.
extern s16 D_energyshot_80130108[];

#endif /* PE_ENERGYSHOT_H */
