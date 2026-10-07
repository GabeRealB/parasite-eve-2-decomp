/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Task callback: a four-step script. State 0 queues the weapon message and the
/// captioned command, state 1 waits one tick, state 2 starts the cutscene at
/// `gDrivewayCutsceneScript`, and state 3 - reached by falling out of
/// state 2 - clears area flag 4 for the current location and kills the task once
/// `eventState` is zero.
void drivewayCutsceneTask(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(1);
            arg0->state += 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            evsStartScript(gDrivewayCutsceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            arg0->state += 1;
            /* fallthrough */
        case 3:
            if (gGameSession->eventState == 0) {
                Gp_ClearAreaFlag4(&gGameSession->location.loc);
                taskKill(arg0);
            }
            return;
    }
}
