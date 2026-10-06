/* Part of the streamed scene library; see streamed_scene.h. */

/// Streamed-scene task. It blanks the display and queues CD command 0x61 on
/// the stream slot of the current location with its view replaced by 0x64,
/// then shows the display once the command queue signals it. The scene runs
/// until the CD is idle or the pad check aborts it, which is recorded in
/// `spawnArg1`. After the stream state is restored an aborted scene kills the
/// task at once, and a finished one after 0x3D more ticks; either way the
/// session image memory is restored and presentation returns to the game loop.
void streamedScenePlayThenHold(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state++;
            break;
        case 1:
            key          = gGameSession->location;
            key.loc.view = 0x64;
            slotParam[0] = streamFindMovieSlot(&key.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state++;
            break;
        case 2:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->spawnArg1.value = 0;
                task->state++;
            } else if (Pad_CheckFlag800() != 0) {
                SetDispMask(0);
                CdCmd_ActivatePhase1();
                task->spawnArg1.value = 1;
                task->state++;
            }
            break;
        case 4:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Stream_ResetRestoreState();
                task->state++;
            }
            break;
        case 5:
            if (Stream_RestoreAfterLoad(0, 1) & 0xFFFF) {
                if (task->spawnArg1.value != 0) {
                    taskKill(task);
                    displayResumeGameLoop();
                } else {
                    task->state++;
                }
            }
            break;
        case 6:
            task->killCountdown++;
            if (task->killCountdown >= 0x3D) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}
