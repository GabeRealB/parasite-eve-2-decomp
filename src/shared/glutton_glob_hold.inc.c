/* Part of the Glutton library; see glutton.h. */

/// Hold state of the enemy dispatched through `D_actor_444000_80131EA8`: once
/// `stateChanged` says the take-over is armed and `playerCaught` says the player
/// animation is already installed, rebuild the overlay's own animation-set
/// table from the player's current weapon block and (re)send it as message
/// 0x3FF, flagging the model object busy. Then count the step, and after nine
/// of them cancel the animation with message 0x3F1 and step the task on.
/// Bails to `enemyDestroy` when the overlay is shutting down, cancelling a
/// still-installed animation on the way out.
void gluttonGlobHold(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;
    Task*                  player;
    s32                    armed;

    work   = task->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gGluttonEnded == 1) {
        if (work->playerCaught == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
            work->playerCaught = 0;
        }
        enemyDestroy(enemy, task);
        return;
    }

    if (work->stateChanged != 0) {
        armed            = work->playerCaught;
        work->stateTicks = 0;
        if (armed != 1) {
            task->state++;
            return;
        }
        gGluttonCaughtAnimSets[2] =
            (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[9];
        work->playerAnim.source.sets = gGluttonCaughtAnimSets;
        work->playerAnim.animationId = 2;
        work->playerAnim.blend       = armed;
        work->playerAnim.blendFrames = 9;
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (work->stateTicks >= 9) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
        work->playerCaught = 0;
        task->state++;
    }
    work->stateTicks++;
}
