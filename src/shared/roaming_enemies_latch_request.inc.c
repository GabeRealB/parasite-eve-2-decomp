/* Part of the roaming enemies library; see roaming_enemies.h. */

// Bind ROAMER_LATCH_SPAWN_REQUEST to a function identifier declared as
// s32 (Task*, s32, const DirectionActionRequest*, s32) in the carrier's
// prologue. Each forest room selects roamerLatchRequest for pool A and its
// static _roamerLatchSpawnRequestPoolB for pool B. This object-like binding
// captures no locals, takes no arguments and uses no # or ## operations.
// Undefine it after each inclusion; both copies share the room's request state.
#ifndef ROAMER_LATCH_SPAWN_REQUEST
#error Define ROAMER_LATCH_SPAWN_REQUEST before including this fragment.
#endif

/// Latches a changed room-action spawn point when the roaming pool is ready.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed, non-null request.
/// Its `actionId` is a one-based spawn-point selector: forest-zone triggers send
/// 1..5 and woodland-path triggers send 1..6. Pool A indexes its six live rows;
/// pool B uses rows 1..4 directly and its fifth row for every larger selector.
/// Zero clears the pending request. The request's control and argument are unused.
///
/// A changed selector is accepted only when `gRoamerCooldown` is exactly zero;
/// every other call clears `gRoamerSpawnRequest`. The selector is remembered even
/// during cooldown, so repeating a suppressed request after cooldown still
/// clears it. Both pool callbacks share this state. Only the selector is copied;
/// the request is neither modified nor retained. Other arguments are unused.
/// Returns 1 even when the pending request is cleared.
s32 ROAMER_LATCH_SPAWN_REQUEST(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ROAMER_SPAWN_REQUEST_NONE    = 0,
        ROAMER_SPAWN_REQUEST_HANDLED = 1,
    };

    if (request->actionId != gRoamerLastRequest && gRoamerCooldown == 0) {
        gRoamerSpawnRequest = request->actionId;
    } else {
        gRoamerSpawnRequest = ROAMER_SPAWN_REQUEST_NONE;
    }
    gRoamerLastRequest = request->actionId;
    return ROAMER_SPAWN_REQUEST_HANDLED;
}
