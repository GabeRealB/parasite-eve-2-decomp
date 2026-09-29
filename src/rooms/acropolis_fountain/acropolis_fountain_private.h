#ifndef SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H

#include "types.h"

extern u8 D_acropolis_fountain_80183BB0;

extern u8 D_acropolis_fountain_80183BB1;

/// Swaps which of the room's two list-0 objects is drawn: unlinks
/// `D_acropolis_fountain_8017FB3C` and links `D_acropolis_fountain_8017E7A4`
/// under the view coordinate. Called by the message handler that sets game
/// flag nibble 0x12, and again at room set-up whenever that nibble is set.
void func_acropolis_fountain_8017DA1C(void);

#endif // SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H
