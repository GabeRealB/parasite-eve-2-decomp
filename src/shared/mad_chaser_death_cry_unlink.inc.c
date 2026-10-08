/* Part of the Mad Chaser library; see mad_chaser.h. */

#ifndef MAD_CHASER_SHRINK_DEATH_START_HANDLER
/// Releases the alert for this enemy even when the claim bit is already clear.
///
/// Reloads the live payload after the sound request, compares only the owner
/// nibble with the enemy's placement index and clears the complete latch.
static __inline__ void _madChaserDropDeathReleaseAlert(Task* task)
{
    Enemy* alertEnemy = task->spawnArg2.pointer;

    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (alertEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
}
#endif

#ifdef MAD_CHASER_SHRINK_DEATH_START_HANDLER
/// Starts scripted shrink death by fading the alert cry and detaching targeting.
///
/// Requires a live `Enemy` spawn argument and task-owned `MadChaserWork` at behavior
/// state 0. The sound selector tags the alert cry with the placement index
/// (0..15); fade control 15 is a nominal duration in audio updates. Clears
/// the shared alert when its owner nibble matches, even if the claim bit is clear.
/// Releases target locks and tracking, then advances the 16-bit behavior state
/// to 1. The task, model, collision bodies and their storage remain live.
static void MAD_CHASER_SHRINK_DEATH_START_HANDLER(Task* task)
#else
/// Starts scripted drop death by fading the alert cry and detaching targeting.
///
/// Requires a live enemy and task-owned work in drop-death behavior 0. Requests
/// a nominal 15-audio-update fade of character-bank entry 2 tagged with the
/// placement index (0..15). Releases the shared alert when its owner nibble
/// matches, even with the claim bit clear, then unlinks target locks/tracking
/// and enters behavior 1. Enemy, model, collision and animation storage remain
/// live for the settle and collision-detach steps.
static void _madChaserDropDeathStart(Task* task)
#endif
{
    enum {
        MAD_CHASER_COMMAND_DEATH_FADE_UPDATES         = 15,
        MAD_CHASER_COMMAND_DEATH_SOUND_INSTANCE_SHIFT = 8
    };
    MadChaserWork* work;
    Enemy*         enemy;
#ifdef MAD_CHASER_SHRINK_DEATH_START_HANDLER
    Enemy* alertEnemy;
#endif

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_COMMAND_DEATH_SOUND_INSTANCE_SHIFT) | SOUND_MAD_CHASER_ALERT_CRY,
                            MAD_CHASER_COMMAND_DEATH_FADE_UPDATES);
#ifdef MAD_CHASER_SHRINK_DEATH_START_HANDLER
    alertEnemy = task->spawnArg2.pointer;
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (alertEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
#else
    _madChaserDropDeathReleaseAlert(task);
#endif
    worldTargetUnlinkNode(&enemy->node);
    work->state++;
}
