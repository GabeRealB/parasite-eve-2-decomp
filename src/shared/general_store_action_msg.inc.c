/* Part of the general store library; see general_store.h. */

/// 0x13F0 handler. Action 0x18 shows caption 0x18, or 0x19 when pointer slot
/// 0xA is empty, if no caption is running. Action 9 spawns storeToggleTask on
/// nibble 0x53 with caption 9. Returns 0.
s32 storeActionMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32   arg;
    Task* companionTask;

    if (arg2 == 0x18) {
        companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
        arg           = 0x19;
        if (companionTask != 0) {
            arg = 0x18;
        }
        capSpawnEventIfIdle(arg, CAP_EVENT_NO_FLAGS);
    }
    if (arg2 == 9) {
        taskSpawnFromTable(gStoreTaskDescs, 0, 0x53, 9);
    }
    return 0;
}
