/* Part of the action prompt library; see action_prompt.h. */

/// Resumes play after a room's action-prompt event.
///
/// Requires the current player, its model and present attachments to be live,
/// and one outstanding menu hold acquired by the event. Resumes ordinary player
/// control, enables automatic model drawing, releases that one menu hold and
/// clears the event, HUD and cutscene gates. Sets the live save's view to 4.
/// The session and live save must be available. The caller owns cursor and
/// event teardown.
static inline void _actionPromptRestoreEventPlay(void)
{
    enum {
        ACTION_PROMPT_EVENT_IDLE        = 0,
        ACTION_PROMPT_EVENT_RETURN_VIEW = 4
    };

    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                                   = ACTION_PROMPT_EVENT_IDLE;
    gGameSession->hideHud                                      = false;
    gGameSession->cutsceneHold                                 = false;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTION_PROMPT_EVENT_RETURN_VIEW;
}

/// Ends a room's action-prompt event and restores normal play.
///
/// The event must hold the menu and retain its live cursor task in
/// `eventTask->spawnArg2.pointer`. Restores player control and automatic model
/// visibility, clears the event/HUD/cutscene holds and restores saved view 4.
/// Kills the cursor task, then suspends the event with result 0 for its owner
/// to finish teardown through `taskPollKill`; the event's work and body remain
/// live until that poll. Both room owners discard the result.
static void _actionPromptEventEnd(Task* eventTask)
{
    enum {
        ACTION_PROMPT_EVENT_REENTRY_DELAY_UPDATES = 10,
        ACTION_PROMPT_EVENT_END_RESULT            = 0
    };

    // Restore play while delaying another manual interaction for ten eligible direction updates.
    D_80114D08 = ACTION_PROMPT_EVENT_REENTRY_DELAY_UPDATES;
    _actionPromptRestoreEventPlay();

    // The cursor ends now; the polling owner releases the event's own resources.
    taskKill(eventTask->spawnArg2.pointer);
    taskRequestKill(eventTask, ACTION_PROMPT_EVENT_END_RESULT);
}
