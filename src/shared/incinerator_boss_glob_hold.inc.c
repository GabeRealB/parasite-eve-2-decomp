/* Part of the incinerator boss library; see incinerator_boss.h. */

/// Hold state of the enemy dispatched through `D_actor_444000_80131EA8`: once
/// `field_1A8` says the take-over is armed and `field_1B2` says the player
/// animation is already installed, rebuild the overlay's own animation-set
/// table from the player's current weapon block and (re)send it as message
/// 0x3FF, flagging the model object busy. Then count the step, and after nine
/// of them cancel the animation with message 0x3F1 and step the task on.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down, cancelling a
/// still-installed animation on the way out.
void incinBossGlobHold(Enemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Task*                player;
    s32                  armed;

    work   = task->work;
    player = gameGetPtrSlot(3);
    if (gIncinBossEnded == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        armed           = work->field_1B2;
        work->field_1AC = 0;
        if (armed != 1) {
            task->state++;
            return;
        }
        gIncinBossCaughtAnimSets[2] =
            (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + Player_Status.weapon])->table.sets[9];
        work->anim.source.sets = gIncinBossCaughtAnimSets;
        work->anim.animationId = 2;
        work->anim.blend       = armed;
        work->anim.blendFrames = 9;
        Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->anim, 0);
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (work->field_1AC >= 9) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
        work->field_1B2 = 0;
        task->state++;
    }
    work->field_1AC++;
}
