/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Task callback: on its first tick it hides the display and hands control to
/// the captioned cutscene; on every later tick it kills the task and clears the
/// collected bit. Either way it advances its own state.
void drivewayBlackoutTask(Task* arg0)
{
    if (arg0->state == 0) {
        gGameSession->hideHud = 1;
        D_80115768            = 1;
        SetDispMask(0);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x10);
        evsStartScriptWithSkip(gDrivewayBlackoutScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gDrivewayBlackoutTail);
    } else {
        taskKill(arg0);
        inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_WIRE_ROPE);
    }
    arg0->state = (s32)(arg0->state + 1);
}
