/* Part of the factory lift library; see factory_lift.h. */

/// The room's second cutscene: states 0..2 silence both weapons, run the cap in
/// `Task::spawnArg1` and wait for it to report event key 1; states 3 and 6 count
/// `Task::killCountdown` up to and back down from 0x1E and tint the screen with
/// the count scaled to 0xFF over 30 steps; states 4 and 5 publish the progress
/// flags and tint it white, and anything past 6 restores the weapons and kills
/// the task.
///
/// `fade` does two jobs on purpose: state 4 reads the session variant through
/// it before testing it. That cross-block use is what makes the state-3/6 tint
/// value a *global* pseudo, and `local-alloc` only folds the `(u8)fade`
/// conversion into the division's quantity when that pseudo is local to one
/// block -- global, the conversion keeps its own quantity and takes `$a0` from
/// the argument move, while the division chain keeps `$v1`.
void factoryWhiteoutScene(Task* task)
{
    u8 fade;

    switch (task->state) {
        case 0:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) != 0) {
                taskKill(task);
                return;
            }
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd1(task->spawnArg1.value);
            task->state = task->state + 1;
            return;
        case 2:
            if (capGetVariantKey() == 1) {
                task->killCountdown = 0;
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x0C), 0, 0);
                }
                task->state = task->state + 1;
                return;
            }
            task->state = -1;
            return;
        case 3:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= 0x1E) {
                task->state = task->state + 1;
            }
            fade = (task->killCountdown * 255) / 30;
            fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
            return;
        case 4:
            gGameSession->viewDirty = 1;
            gameFlagSetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED, 1);
            fade = gGameSession->location.loc.stage;
            if (fade == 2) {
                Gp_EnqueueStageSnd6(SOUND_FACTORY_WHITEOUT, 0, 0);
            }
            fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
            task->state = task->state + 1;
            return;
        case 5:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
            gGameSession->location.loc.room                            = 2;
            gGameSession->roomObjsDirty                                = 1;
            fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
            task->state = task->state + 1;
            return;
        case 1:
            task->state = task->state + 1;
            return;
        case 6:
            task->killCountdown = task->killCountdown - 1;
            if (task->killCountdown <= 0) {
                task->state = task->state + 1;
            }
            fade = (task->killCountdown * 255) / 30;
            fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
            return;
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            taskKill(task);
            return;
    }
}
