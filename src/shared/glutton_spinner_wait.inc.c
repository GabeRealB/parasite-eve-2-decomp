/* Part of the Glutton library; see glutton.h. */

/// Waiting state of the spinner enemy (`D_actor_444000_80131F30`): hand the
/// enemy back to `Gp_DestroyEnemy` once `gGluttonEnded` is set;
/// otherwise keep the model's flag word cleared, so it is not drawn, and step
/// the task on once `gGluttonSpinnersReleased` is 1.
void gluttonSpinnerWait(Enemy* arg0, Task* arg1)
{
    if (gGluttonEnded == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    if (gGluttonSpinnersReleased == 1) {
        arg1->state++;
    }

    arg1->extra.tmd->flags = 0;
}
