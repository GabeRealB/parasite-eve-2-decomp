/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame state of the child task `golemPawnRookDelayedEffectSpawn` sets up. It mirrors
/// the enemy's model flags onto its own model and drains the enemy's
/// `field_6D8` countdown; on the frame it reaches zero it spawns a
/// `Gp_SpawnEff` effect at part 7 of the enemy's coordinate array and
/// reparents the effect's task to this child. `arg0` is the spawn context
/// every state handler takes and is unused here.
void golemPawnRookDelayedEffectTick(Enemy* arg0, Task* task)
{
    EffectWork*      effect;
    Task*            parent;
    Actor105600Work* work;
    s16              count;

    parent                 = task->parent;
    work                   = (Actor105600Work*)parent->work;
    task->extra.tmd->flags = (u16)parent->extra.tmd->flags;
    if (work->field_6D8 > 0) {
        count           = (u16)work->field_6D8 - 1;
        work->field_6D8 = count;
        if (count == 0) {
            effect = Gp_SpawnEff(D_8011572C | 0x80000000,
                                 &task->parent->extra.tmd->coords[7], 0, NULL);
            if (effect != NULL) {
                Task_Reparent(task, effect->task);
            }
        }
    }
}
