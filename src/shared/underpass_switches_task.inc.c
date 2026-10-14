/* Included underpass switch prompt; each carrier declares its static instance. */

/// Plays a switch prompt, toggles its flag and updates the live underpass variant.
///
/// Start in state 0 with a valid nibble index (0..503) in `spawnArg1.value`
/// and a loaded CAP command index in `spawnArg2.value`; keep both fixed.
/// Once CAP is idle, keys 10 and above toggle zero to one or nonzero to zero.
/// Toggling switch 1 selects underpass room 1..6 from the event latch, flag 053
/// and switch 1, committing the room to session and live save. The final tick
/// marks the view dirty for rooms 5 and above even when no switch was toggled,
/// then releases the task. Keep the room and CAP resources loaded throughout.
static void _underpassSwitchTask(Task* task)
{
    enum {
        UNDERPASS_SWITCH_START          = 0,
        UNDERPASS_SWITCH_WAIT           = 1,
        UNDERPASS_SWITCH_APPLY          = 2,
        UNDERPASS_SWITCH_FINISH         = 3,
        UNDERPASS_SWITCH_KEY_MINIMUM    = 10,
        UNDERPASS_SWITCH_VIEW_DIRTY_MIN = 5,
    };
    enum {
        UNDERPASS_SWITCH_ROOM_AFTER_EVENT      = 1,
        UNDERPASS_SWITCH_ROOM_AFTER_EVENT_053  = 2,
        UNDERPASS_SWITCH_OFF_ROOM_OFFSET       = 2,
        UNDERPASS_SWITCH_ROOM_BEFORE_EVENT_ON  = 5,
        UNDERPASS_SWITCH_ROOM_BEFORE_EVENT_OFF = 6,
    };
    /// Selects underpass room 1..6 after a switch-1 toggle according to story progress.
    ///
    /// Captures `requestPtr`, `replyPtr`, `reply` and the function's
    /// `UNDERPASS_SWITCH_*` room constants; `replyPtr` must address writable
    /// RoomEventMsg `reply`. Only request.queryOnly needs initialization.
    /// Execution chooses 1/2 after the event (flag 053 selects 2), plus two
    /// when switch 1 is off; before the event it chooses 5 on or 6 off.
    /// Query requests perform no flag reads or writes. Only reply.roomState changes;
    /// committing session/save state remains the caller's responsibility.
    /// Expands to one statement and is undefined after the function.
#define UNDERPASS_SELECT_SWITCH_ROOM()                                                    \
    (void)({                                                                              \
        if (requestPtr->queryOnly == ROOM_EVENT_EXECUTE) {                                \
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {                 \
                if (gameFlagGetNibble(GAME_FLAG_053) != 0) {                              \
                    replyPtr->roomState = UNDERPASS_SWITCH_ROOM_AFTER_EVENT_053;          \
                } else {                                                                  \
                    replyPtr->roomState = UNDERPASS_SWITCH_ROOM_AFTER_EVENT;              \
                }                                                                         \
                if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {               \
                    reply.roomState = reply.roomState + UNDERPASS_SWITCH_OFF_ROOM_OFFSET; \
                }                                                                         \
            } else {                                                                      \
                if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) != 0) {               \
                    replyPtr->roomState = UNDERPASS_SWITCH_ROOM_BEFORE_EVENT_ON;          \
                } else {                                                                  \
                    replyPtr->roomState = UNDERPASS_SWITCH_ROOM_BEFORE_EVENT_OFF;         \
                }                                                                         \
            }                                                                             \
        }                                                                                 \
    })

    RoomEventMsg  request;
    RoomEventMsg  reply;
    RoomEventMsg* requestPtr;
    RoomEventMsg* replyPtr;
    GameSession*  session;
    s32           flagId;
    s32           state;
    s32           capCommand;
    u8            destinationRoom;

    flagId     = task->spawnArg1.value;
    state      = task->state;
    capCommand = task->spawnArg2.value;
    switch (state) {
        case UNDERPASS_SWITCH_START:
            capRunCommandWithTransition(capCommand);
            task->state = task->state + 1;
            return;
        case UNDERPASS_SWITCH_WAIT:
            if (capIsBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case UNDERPASS_SWITCH_APPLY:
            if (capGetVariantKey() >= UNDERPASS_SWITCH_KEY_MINIMUM) {
                gameFlagSetNibble(flagId, gameFlagGetNibble(flagId) == 0);
                if (flagId == GAME_FLAG_UNDERPASS_SWITCH_1) {
                    // Keep the request/reply records: scalar selection changes the compiled frame.
                    replyPtr          = &reply;
                    requestPtr        = &request;
                    request.areaId    = GAME_AREA_DRYFIELD_UNDERPASS;
                    request.queryOnly = ROOM_EVENT_EXECUTE;
                    UNDERPASS_SELECT_SWITCH_ROOM();
                    session                                                     = gGameSession;
                    destinationRoom                                             = reply.roomState;
                    session->location.roomState                                 = destinationRoom;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = destinationRoom;
                }
            }
            task->state = task->state + 1;
            return;
        case UNDERPASS_SWITCH_FINISH:
            if (gGameSession->location.roomState >= UNDERPASS_SWITCH_VIEW_DIRTY_MIN) {
                gGameSession->viewDirty = 1;
            }
            taskKill(task);
            return;
    }
#undef UNDERPASS_SELECT_SWITCH_ROOM
}
