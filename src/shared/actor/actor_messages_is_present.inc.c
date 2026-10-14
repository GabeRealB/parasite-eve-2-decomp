/* Part of the Actor messages library; see actor_messages.h. */

s32 actorMsgIsPresent(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
    Enemy* enemy;
    u16    flags;

    enemy = task->spawnArg2.pointer;
    if (enemy->hp <= 0) {
        flags = task->extra.tmd->flags;
        if (flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return 0;
        }
        if (flags & TMD_OBJECT_SEMI_TRANS) {
            return 0;
        }
    }
    return 1;
}
