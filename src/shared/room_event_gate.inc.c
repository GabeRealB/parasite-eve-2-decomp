/* Part of the room events library; see room_events.h. */

/// Return choices, flag values and start indications used only by this gate.
enum {
    ROOM_EVENT_GATE_MISSING_COLLECTION     = 0,
    ROOM_EVENT_GATE_BYPASSED               = 1,
    ROOM_EVENT_GATE_ELIGIBLE               = 2,
    ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED = 0,
    ROOM_EVENT_GATE_FLAG_CLEAR             = 0,
    ROOM_EVENT_GATE_FLAG_SET               = 1,
    ROOM_EVENT_GATE_REFUSAL_FLAG_VALUE     = 2,
    ROOM_EVENT_GATE_NOT_STARTED            = 0,
    ROOM_EVENT_GATE_STARTED                = 1,
};

/// Latches an eligible room transition and requests its deferred event task.
///
/// The caller must have checked the event's flag and collection requirement
/// and selected execution. Both pointers borrow complete, live records for
/// this call only; the twenty-byte request and eight-byte message are copied
/// into the room's shared snapshots. A later start replaces those snapshots,
/// including the inputs a pending event task will read. Keep the room loaded
/// until that task finishes its CAP command, sounds and room transition.
///
/// The magnitude of `request->flagId` must be in 0..GAME_FLAG_NIBBLE_COUNT-1.
/// Negative IDs clear the nibble; nonnegative IDs set it to 1, including ID 0.
/// The flag is committed before spawning, with no rollback on allocation
/// failure. `ROOM_EVENT_ACTIVE` records this start request even if spawning
/// fails; it does not track the task's lifetime or successful allocation.
static inline void _roomEventLatchAndSpawn(const RoomEventReq* request, const RoomEventMsg* message)
{
    s32 flagId;
    s32 flagValue;

    gRoomEventMsg  = *message;
    ROOM_EVENT_REQ = *request;
    flagId         = request->flagId;
    flagValue      = ROOM_EVENT_GATE_FLAG_SET;
    if (flagId < 0) {
        flagId    = -flagId;
        flagValue = ROOM_EVENT_GATE_FLAG_CLEAR;
    }
    gameFlagSetNibble(flagId, flagValue);
    taskSpawnFromTable(&gRoomEventTaskDesc, 0, 0, 0);
    ROOM_EVENT_ACTIVE = ROOM_EVENT_GATE_STARTED;
}

/// Checks whether a room transition needs its event and optionally starts it.
///
/// Returns 1 when the flag bypasses the event, 0 when a required
/// collection bit is missing, or 2 when the event is eligible. Nonnegative
/// request flag IDs run while clear and are set to 1; negative IDs run while
/// nonzero and are cleared. The magnitude must be a valid nibble index
/// (0..503); request flag zero is an actual index, not an absent flag.
/// Collection IDs select their low seven bits; zero requires no collection.
///
/// Both inputs are borrowed and unchanged. Eligible execution copies the
/// complete twenty-byte request and eight-byte message for the deferred task;
/// another start replaces those snapshots. Missing-collection execution runs
/// `missingCapCmd` and writes 2 to the message's optional flag (0 none, otherwise
/// 1..503). Nonzero `queryOnly` suppresses both execution paths.
///
/// Every call first clears `ROOM_EVENT_ACTIVE`, including queries returning 2.
/// Eligible execution sets it after requesting the task. It records that call's
/// start branch, not task lifetime or allocation success: the spawn result is
/// unchecked, and allocation failure still commits the inputs and flag.
static s32 _roomEventGate(const RoomEventReq* request, const RoomEventMsg* message)
{
    s32 flagId;
    s32 flagIndex;
    s32 flagSatisfied;

    flagId            = request->flagId;
    ROOM_EVENT_ACTIVE = ROOM_EVENT_GATE_NOT_STARTED;
    // This halfword conversion preserves the flag-test branch's delay slot.
    flagIndex = (s16)flagId;

    // Test whether the flag polarity already bypasses this event.
    if (flagId < 0) {
        flagSatisfied = gameFlagGetNibble(-flagId) == ROOM_EVENT_GATE_FLAG_CLEAR;
    } else {
        flagSatisfied = gameFlagGetNibble(flagIndex);
    }
    if (flagSatisfied != 0) {
        return ROOM_EVENT_GATE_BYPASSED;
    }

    if (inventoryHasCollectedBit(request->collectedBit) != 0 || request->collectedBit == ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            _roomEventLatchAndSpawn(request, message);
        }
        return ROOM_EVENT_GATE_ELIGIBLE;
    }

    // Queries report the refusal without starting its caption or writing its flag.
    if (message->queryOnly == ROOM_EVENT_EXECUTE) {
        capRunCommandWithTransition(request->missingCapCmd);
        gameFlagSetNibbleIfPresent(message->flagId, ROOM_EVENT_GATE_REFUSAL_FLAG_VALUE);
    }
    return ROOM_EVENT_GATE_MISSING_COLLECTION;
}
