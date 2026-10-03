#ifndef GAMEPLAY_PRIVATE_DIRECTION_H
#define GAMEPLAY_PRIVATE_DIRECTION_H

#include "common.h"

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

/// Direction-action handlers copied to the stack by func_800AD6BC.
typedef struct _GpDirActionTable {
    DirectionActionHandler funcs[7];
} GpDirActionTable;
STATIC_ASSERT_SIZEOF(GpDirActionTable, 0x1C);

#endif // GAMEPLAY_PRIVATE_DIRECTION_H
