/* Part of the incinerator boss library; see incinerator_boss.h. */

/// Waiting state of the spinner enemy (`D_actor_444000_80131F30`): hand the
/// enemy back to `Gp_DestroyEnemy` once `gIncinBossEnded` is set;
/// otherwise keep the model's flag word cleared, so it is not drawn, and step
/// the task on once `gIncinBossSpinnersReleased` is 1.
void incinBossSpinnerWait(GpEnemy* arg0, Task* arg1)
{
    if (gIncinBossEnded == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    if (gIncinBossSpinnersReleased == 1) {
        arg1->state++;
    }

    arg1->extra.tmd->flags = 0;
}
