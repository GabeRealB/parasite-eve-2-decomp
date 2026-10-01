/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once bit 7 of `gSceneCombatState.madChaserAlertOwner` is set, puts the task in state 3 with
/// the state machine at state 5 and returns 1; otherwise returns 0.
s16 madChaserJoinAlert(Task* arg0)
{
    if ((s8)gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED) {
        madChaserEnterState(arg0, 3);
        madChaserSetStateS16(arg0, 5);
        return 1;
    }
    return 0;
}
