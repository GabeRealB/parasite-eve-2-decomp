/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame state of the model child: mirrors the actor's model flags onto
/// its own, and when the actor raises `fireRequest` spawns entry 2 of the
/// actor's spawn table and hands it this child's texture page and CLUT row,
/// reprocessing its model stream for both half-buffers.
void golemPawnRookGunTick(Enemy* enemy, Task* task)
{
    GolemPawnRookWork* work;
    Enemy*             spawned;
    TmdObject*         src;
    TmdObject*         dst;

    work                   = task->parent->work;
    task->extra.tmd->flags = task->parent->extra.tmd->flags;
    if (work->fireRequest != 0) {
        work->fireRequest      = 0;
        spawned                = Gp_SpawnEnemyFromTable(work->taskTable, 2, 0, enemy);
        src                    = task->extra.tmd;
        dst                    = spawned->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
}
