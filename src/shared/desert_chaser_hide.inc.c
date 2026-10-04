/* Part of the Desert Chaser library; see desert_chaser.h. */

/// State 0 of `gDesertChaserStates`: when the work block's `stateEntered` flag
/// is set, flags the enemy's link node and raises bit 0x80 of the model's
/// flags. `obj` gets its own local: the fused form ranks the `Task::extra`
/// load with the store and transposes it.
void desertChaserHideState(Enemy* arg0, Task* arg1)
{
    DesertChaserWork* work;
    TmdObject*        obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}
