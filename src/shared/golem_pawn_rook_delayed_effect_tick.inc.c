/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame state of the child task `_golemPawnRookSwordSpawn` sets up. It mirrors
/// the enemy's model flags onto its own model and drains the enemy's
/// `swordTrailDelay` countdown; on the frame it reaches zero it spawns a
/// `effectSpawn` effect at part 7 of the enemy's coordinate array and
/// reparents the effect's task to this child. `arg0` is the spawn context
/// every state handler takes and is unused here.
void golemPawnRookDelayedEffectTick(Enemy* arg0, Task* task)
{
    EffectWork*        effect;
    Task*              parent;
    GolemPawnRookWork* work;
    s16                count;

    parent                 = task->parent;
    work                   = parent->work;
    task->extra.tmd->flags = (u16)parent->extra.tmd->flags;
    if (work->swordTrailDelay > 0) {
        count                 = (u16)work->swordTrailDelay - 1;
        work->swordTrailDelay = count;
        if (count == 0) {
            effect = effectSpawn(gRoomEffectTwinTrailId | 0x80000000,
                                 &task->parent->extra.tmd->coords[7], 0, NULL);
            if (effect != NULL) {
                taskReparent(task, effect->task);
            }
        }
    }
}
