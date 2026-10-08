/* Part of the factory lift library; see factory_lift.h. */

/// Restores player and companion presentation after an operator-panel session.
///
/// Requires live actor models, session/save state and the panel's menu hold.
/// Defers another manual interaction for ten eligible direction updates and
/// selects the factory's return view before the cursor is torn down.
static inline void _factoryPanelRestorePlay(void)
{
    enum { FACTORY_PANEL_REENTRY_DELAY_UPDATES = 10,
           FACTORY_PANEL_EVENT_IDLE            = 0 };

    D_80114D08 = FACTORY_PANEL_REENTRY_DELAY_UPDATES;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                                   = FACTORY_PANEL_EVENT_IDLE;
    gGameSession->hideHud                                      = false;
    gGameSession->cutsceneHold                                 = false;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_VIEW_RETURN;
}

/// Ends the operator-panel session and hands its teardown back to its owner.
///
/// Requires the menu hold and live cursor task retained in
/// `task->spawnArg2.pointer`. Restores ordinary play and saved view 3, kills
/// the cursor immediately, then requests panel teardown with result zero.
/// The polling owner remains responsible for releasing the panel's work.
static void _factoryPanelExit(Task* task)
{
    enum { FACTORY_PANEL_END_RESULT = 0 };

    _factoryPanelRestorePlay();

    // End the cursor before the owner polls and releases this panel.
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, FACTORY_PANEL_END_RESULT);
}
