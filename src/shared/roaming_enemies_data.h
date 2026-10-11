#ifndef SRC_SHARED_ROAMING_ENEMIES_DATA_H
#define SRC_SHARED_ROAMING_ENEMIES_DATA_H

#include "types.h"

#include "gameplay/message.h"

// Defined separately by each forest-zone and woodland-path overlay.

/// Reusable command payload for enemies revived by the two reserve pools.
///
/// Pool A fills stage 5/area 29 and command 11; pool B fills stage 5/area 11
/// and command 11, or command 12 for its ambush. Dispatch borrows the four
/// bytes synchronously. Later requests overwrite this room-visit storage.
extern ActorCommand gRoamerCommand;

/// Five banked signed-halfword HP slots shared by the room's two enemy pools.
///
/// Zero is free; positive values are available for revival. Arming seeds up
/// to five slots at 420 HP. Retreat banking scales reported HP to 110 percent,
/// narrows to s16 before clamping to 420, and uses the first zero slot. A
/// narrowed negative value remains stored but is not counted as available.
/// Revival copies the halfword to Enemy.hp and clears the consumed slot.
/// Valid indices are 0..4; the bank is owned by the current room overlay.
extern s16 gRoamerReserveHp[5];

#endif // SRC_SHARED_ROAMING_ENEMIES_DATA_H
