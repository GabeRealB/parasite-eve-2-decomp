#ifndef ROOMS_ACROPOLIS_FOUNTAIN_H
#define ROOMS_ACROPOLIS_FOUNTAIN_H

#include "types.h"

#include "gameplay/area.h"

#include "main/task_types.h"

#include "common.h"

/// Swaps which of the room's two list-0 objects is drawn: unlinks
/// `D_acropolis_fountain_8017FB3C` and links `D_acropolis_fountain_8017E7A4`
/// under the view coordinate. Called by the message handler that sets game
/// flag nibble 0x12, and again at room set-up whenever that nibble is set.
void func_acropolis_fountain_8017DA1C(void);

void func_acropolis_fountain_8017DD44(Task* task);
extern GpAreaVariant D_acropolis_fountain_8017FC9C[11];

void func_acropolis_fountain_8017DA78(s32 unused0, s32 unused1);

void func_acropolis_fountain_8017E014(Task* task);

#endif // ROOMS_ACROPOLIS_FOUNTAIN_H
