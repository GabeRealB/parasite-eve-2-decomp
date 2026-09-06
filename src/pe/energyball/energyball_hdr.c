#include "common.h"

/// This overlay's id. Every package opens with one: a u16 in a u32 slot,
/// distinct across all 448, with the families in contiguous blocks.
///
/// Its own unit because the rest of this overlay's leading rodata is a
/// compiler-generated jump table that must start at the `rodata_head` cut;
/// folding the id into that unit reorders both.
const u32 D_energyball_8012EF30 = 59;
