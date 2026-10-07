/* Part of the action prompt library; see action_prompt.h. */

/// Exit state of a room's key-item event task, undoing its set-up: sends the
/// two player messages with 1, releases the menu hold, clears the
/// session's event, HUD and cutscene holds, puts the save's camera view back
/// from 6 to 4, kills the prompt task the set-up spawned (`Task::spawnArg2`)
/// and asks for its own removal.
void actionPromptEventEnd(Task* task)
{
    D_80114D08 = 0xA;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 4;
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}
