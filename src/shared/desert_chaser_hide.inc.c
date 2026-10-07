/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Hides the cutscene chaser and disables targeting when its hidden state is entered.
///
/// Later ticks leave the model and enemy flags alone; work must be initialized.
static void _desertChaserHideState(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}
