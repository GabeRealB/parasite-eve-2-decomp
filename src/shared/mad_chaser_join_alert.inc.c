/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Joins a claimed Mad Chaser alert and returns 1 when the task was switched to combat.
///
/// Any alert owner suffices, including this enemy. Combat starts at its alert
/// behavior with sub-state zero; an unclaimed alert returns 0 and retains the
/// current state. Requires a live Mad Chaser work block.
static s16 _madChaserJoinAlert(Task* task)
{
    if ((s8)gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED) {
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorStateS16(task, MAD_CHASER_COMBAT_STATE_ALERT);
        return 1;
    }
    return 0;
}
