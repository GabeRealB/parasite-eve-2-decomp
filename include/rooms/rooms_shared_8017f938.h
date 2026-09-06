#ifndef ROOMS_SHARED_8017F938_H
#define ROOMS_SHARED_8017F938_H

#include "common.h"

#include "main/task.h"

/// The vending machine's purchase panel: shows the selected item, the quantity
/// the player is buying and the running BP cost, clamping the quantity to what
/// the stock table, the inventory ceiling and the player's balance allow.
/// Up/down adjust it, confirm spends the BP and grants the items, cancel backs
/// out; both exits report code 6 to the parent panel. Seven rooms carry this
/// body, each with its own "x" and "BP" labels.
void RoomsShared8017f938(Task* task);

extern u8 RoomsShared8017f938Bp[];
extern u8 RoomsShared8017f938Times[];

#endif // ROOMS_SHARED_8017F938_H
