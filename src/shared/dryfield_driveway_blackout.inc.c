/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Starts the driveway blackout script and clears wire-rope collection on its next tick.
///
/// Entry hides HUD/display output, holds player updates, stores objective byte
/// 0x10 and starts the live blackout script with its skip tail. The next tick
/// kills this controller and clears the wire-rope collection bit; the script owns
/// its own continuation. The state increment is retained after taskKill.
static void _drivewayBlackoutTask(Task* task)
{
    enum {
        DRIVEWAY_BLACKOUT_START          = 0,
        DRIVEWAY_BLACKOUT_OBJECTIVE_BYTE = 0x10,
    };
    if (task->state == DRIVEWAY_BLACKOUT_START) {
        gGameSession->hideHud = 1;
        D_80115768            = 1;
        SetDispMask(0);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, DRIVEWAY_BLACKOUT_OBJECTIVE_BYTE);
        evsStartScriptWithSkip(gDrivewayBlackoutScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gDrivewayBlackoutTail);
    } else {
        taskKill(task);
        inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_WIRE_ROPE);
    }
    task->state = (s32)(task->state + 1);
}
