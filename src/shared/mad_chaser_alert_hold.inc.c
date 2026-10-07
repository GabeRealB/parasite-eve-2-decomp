/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Claims or releases the shared Mad Chaser alert for this enemy's placement index.
///
/// A nonzero low halfword of claim acquires only an unclaimed alert. Zero releases
/// an alert whose low nibble equals this enemy's index, even if the claim bit is
/// clear. The task's Enemy spawn argument must be live; its placement index is 0..15.
static void _madChaserSetAlertHold(Task* task, s32 claim)
{
    if ((s16)claim != 0) {
        if (!((s8)gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED)) {
            Enemy* enemy = task->spawnArg2.pointer;

            gSceneCombatState.madChaserAlertOwner = (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) | SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED;
        }
    } else {
        Enemy* enemy = task->spawnArg2.pointer;

        if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
            gSceneCombatState.madChaserAlertOwner = 0;
        }
    }
}
