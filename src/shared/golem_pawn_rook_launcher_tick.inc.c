/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Mirrors the body's draw flags and consumes one pending grenade launch request.
///
/// `task` is the attached launcher child and `enemy` its enemy record. A request
/// spawns task-table entry 2, then copies this launcher's texture page and CLUT
/// offsets to the grenade. A buffered grenade rebuilds both drawing halves.
/// The spawn path requires the grenade enemy and model to be available.
static void _golemPawnRookLauncherTick(Enemy* enemy, Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_GRENADE_TASK_ENTRY = 2,
    };

    GolemPawnRookWork* work;
    Enemy*             grenadeEnemy;
    TmdObject*         launcherModel;
    TmdObject*         grenadeModel;

    work                   = task->parent->work;
    task->extra.tmd->flags = task->parent->extra.tmd->flags;
    if (work->fireRequest != 0) {
        work->fireRequest = 0;
        grenadeEnemy      = enemySpawnFromTable(work->taskTable, GOLEM_PAWN_ROOK_GRENADE_TASK_ENTRY, 0, enemy);
        // Rebuild both GPU-buffer halves with the launcher texture placement.
        launcherModel                   = task->extra.tmd;
        grenadeModel                    = grenadeEnemy->task->extra.tmd;
        grenadeModel->texturePageOffset = launcherModel->texturePageOffset;
        grenadeModel->clutRowOffset     = launcherModel->clutRowOffset;
        if (grenadeModel->buffer != NULL) {
            tmdBuildBufferHalf(grenadeModel);
            tmdBuildBufferHalf(grenadeModel);
        }
    }
}
