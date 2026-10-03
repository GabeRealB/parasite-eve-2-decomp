#ifndef GAMEPLAY_PRIVATE_DIRECTION_H
#define GAMEPLAY_PRIVATE_DIRECTION_H

#include "common.h"

#include "gameplay/collision.h"

/// One frame of the active direction action, taking no arguments and returning nothing.
///
/// While a trigger's action is latched, the direction task calls the handler
/// its action selector picks once per frame. Warp and facing actions run in
/// phases, and their handlers pick a phase handler of this same type by the
/// action's phase counter. Every handler works on the latched trigger control
/// word and parameters held in the direction state, which is why none is
/// passed. A handler advances the phase counter to hand over to the next
/// phase, and ends the action by clearing the latched request.
typedef void (*DirectionActionHandler)(void);

/// Number of direction actions a trigger's control word can select.
///
/// `WORLD_COLLISION_TRIGGER_ACTION_CANCEL` is not one of them: it drops the
/// latched request without running a handler.
enum { DIRECTION_ACTION_COUNT = WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON + 1 };

/// The handler of every direction action, indexed by its action selector.
///
/// The selector is the low byte of the latched trigger's control word, one of
/// `WORLD_COLLISION_TRIGGER_ACTION_WARP` to
/// `WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON`. The direction task filters out
/// `WORLD_COLLISION_TRIGGER_ACTION_CANCEL` and otherwise indexes without a
/// range check, so a trigger's control word must hold no other selector.
///
/// The array is wrapped in a struct so that the table can be copied by
/// assignment: the direction task dispatches through a copy of its own.
typedef struct {
    DirectionActionHandler handlers[DIRECTION_ACTION_COUNT];
} DirectionActionTable;
STATIC_ASSERT_SIZEOF(DirectionActionTable, 0x1C);

#endif // GAMEPLAY_PRIVATE_DIRECTION_H
