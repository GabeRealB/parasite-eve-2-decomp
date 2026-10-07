/* Part of the roaming enemies library; see roaming_enemies.h. */

// The carrier includes roaming_enemies.h and gameplay/direction.h and defines
// its signed-halfword cooldown, pending selector and last action ID beforehand.
// Bind ROAMER_LATCH_SPAWN_REQUEST to a function identifier declared as
// s32 (Task*, s32, const DirectionActionRequest*, s32) in the carrier's
// prologue. Each forest room selects its static _roamerLatchSpawnRequestPoolA
// or _roamerLatchSpawnRequestPoolB instance. This object-like binding captures
// no locals, takes no arguments and uses no # or ## operations. Undefine it
// after each inclusion; both copies share the room's request state.
#ifndef ROAMER_LATCH_SPAWN_REQUEST
#error Define ROAMER_LATCH_SPAWN_REQUEST before including this fragment.
#endif

/// Handles a room action by latching a changed spawn point while cooldown is zero.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` supplies a borrowed, non-null request. Its
/// `actionId` must be zero or a valid one-based spawn-point selector: forest-zone
/// triggers send 1..5 and woodland-path triggers send 1..6. Pool A indexes that
/// row; pool B uses rows 1..4 directly and its fifth row for larger selectors.
/// Zero clears the pending selector and becomes the last observed action ID.
///
/// Either pool's latch replaces `_gRoamerPendingSpawnPoint` on every call and
/// updates `_gRoamerLastActionId` even when cooldown suppresses the request.
/// A repeated ID clears the pending selector, including a suppressed ID repeated
/// after cooldown. The request is neither modified nor retained; its control
/// and argument bytes and the other callback arguments are unused. Returns 1
/// even when no selector is pending.
s32 ROAMER_LATCH_SPAWN_REQUEST(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ROAMER_SPAWN_REQUEST_HANDLED = 1,
    };

    if (request->actionId != _gRoamerLastActionId && _gRoamerCooldownFrames == ROAMER_COOLDOWN_READY) {
        _gRoamerPendingSpawnPoint = request->actionId;
    } else {
        _gRoamerPendingSpawnPoint = ROAMER_SPAWN_POINT_NONE;
    }
    _gRoamerLastActionId = request->actionId;
    return ROAMER_SPAWN_REQUEST_HANDLED;
}
