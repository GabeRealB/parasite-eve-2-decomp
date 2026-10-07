#ifndef MAIN_PRIVATE_SESSION_H
#define MAIN_PRIVATE_SESSION_H

/// Clears the live session and cancels its pending saved-player-position flag.
///
/// Requires writable resident `gGameSession` storage. Clear only after its
/// borrowed task/resource handles are no longer needed; no teardown runs here.
/// The storage and `gGameSession` pointer remain at their fixed addresses.
void gameClearSession(void);

/// Resets the play clock's ticks accumulated toward its next saved minute.
///
/// The resident accumulator counts nominal 60-Hz play ticks; 3600 ticks
/// advance `McSaveData.state.playTime` by one minute. Call after a successful
/// save load to discard the previous session's partial minute, since saves
/// restore only whole minutes of play time.
void playClockResetMinuteTicks(void);

#endif // MAIN_PRIVATE_SESSION_H
