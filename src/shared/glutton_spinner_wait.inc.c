/* Part of the Glutton library; see glutton.h. */

/// Keeps a spinner visible on the floor until the boss releases the set.
///
/// Requires the spinner's live TMD body. Release advances its task to the chase
/// state; fight end destroys the enemy. Clearing the model flags enables normal
/// drawing, including on the tick the state advances.
static void _gluttonSpinnerWait(Enemy* enemy, Task* task)
{
    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
        enemyDestroy(enemy, task);
        return;
    }

    if (_gGluttonSpinnersReleased == GLUTTON_SPINNER_RELEASE_ON) {
        task->state++;
    }

    task->extra.tmd->flags = 0;
}
