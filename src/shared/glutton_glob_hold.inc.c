/* Part of the Glutton library; see glutton.h. */

/// Plays the glob's release clip on its caught player, then returns control.
///
/// Requires live projectile work and the player task. On entry, an uncaught glob
/// skips this state; a caught one installs clip 2, using set 9 of the equipped
/// weapon bank, and hides its own model. After nine ticks it ends scripted control
/// without adding the player's clip translation to the root. Fight teardown also
/// releases a player caught by this glob before destroying it. The animation
/// request and set table remain live while the player borrows them.
static void _gluttonGlobHold(Enemy* enemy, Task* task)
{
    enum { GLUTTON_GLOB_RELEASE_CLIP       = 2,
           GLUTTON_GLOB_WEAPON_RELEASE_SET = 9,
           GLUTTON_GLOB_HOLD_TICKS         = 9,
           GLUTTON_GLOB_RELEASE_IN_PLACE   = 2 };
    GluttonProjectileWork* work;
    Task*                  player;
    s32                    playerCaught;

    work   = task->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gGluttonEnded == 1) {
        if (work->playerCaught == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, GLUTTON_GLOB_RELEASE_IN_PLACE, 0);
            work->playerCaught = 0;
        }
        enemyDestroy(enemy, task);
        return;
    }

    if (work->stateChanged != 0) {
        playerCaught     = work->playerCaught;
        work->stateTicks = 0;
        if (playerCaught != 1) {
            task->state++;
            return;
        }
        // Restore a weapon-bank clip before handing scripted control back.
        gGluttonCaughtAnimSets[GLUTTON_GLOB_RELEASE_CLIP] =
            (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[GLUTTON_GLOB_WEAPON_RELEASE_SET];
        work->playerAnim.source.sets = gGluttonCaughtAnimSets;
        work->playerAnim.animationId = GLUTTON_GLOB_RELEASE_CLIP;
        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
        work->playerAnim.blendFrames = GLUTTON_GLOB_HOLD_TICKS;
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (work->stateTicks >= GLUTTON_GLOB_HOLD_TICKS) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, GLUTTON_GLOB_RELEASE_IN_PLACE, 0);
        work->playerCaught = 0;
        task->state++;
    }
    work->stateTicks++;
}
