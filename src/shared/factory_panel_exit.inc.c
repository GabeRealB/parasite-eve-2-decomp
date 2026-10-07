/* Part of the factory lift library; see factory_lift.h. */

/// Script state that ends the scene: gives the player back their weapon and
/// the HUD, releases the menu hold, kills the prompt task and asks for this one
/// to be killed.
void factoryPanelExit(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgAlly3F3(1);
    displayReleaseMenuHold();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill(arg0->spawnArg2.pointer);
    taskRequestKill(arg0, 0);
}
