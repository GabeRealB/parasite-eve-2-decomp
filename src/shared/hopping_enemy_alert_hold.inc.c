/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Claims or releases `Gp_StateF0`'s hold for this enemy. With `arg1` set it
/// claims the hold (bit 7 plus the enemy's slot) unless one is already held;
/// with `arg1` clear it releases the hold if it is this enemy's.
void hopperSetAlertHold(Task* arg0, s32 arg1)
{
    if ((arg1 << 0x10) != 0) {
        if (!((s8)Gp_StateF0.hopperAlertOwner & SCENE_COMBAT_HOPPER_ALERT_CLAIMED)) {
            Gp_StateF0.hopperAlertOwner = (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) | SCENE_COMBAT_HOPPER_ALERT_CLAIMED;
        }
    } else if ((Gp_StateF0.hopperAlertOwner & SCENE_COMBAT_HOPPER_OWNER_MASK) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        Gp_StateF0.hopperAlertOwner = 0;
    }
}
