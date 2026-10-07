/* Part of the streamed scene library; see streamed_scene.h. */

/// Plays the location's streamed scene (see streamed_scene.h) and kills the
/// task as soon as the stream state is restored.
void streamedScenePlay(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
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
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state++;
            } else if (padIsStartPressed() != 0) {
                SetDispMask(0);
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        case 4:
            if (cdCmdIsIdle() & 0xFFFF) {
                streamResetGameRestore();
                task->state++;
            }
            break;
        case 5:
            if (streamPollGameRestore(0, 1) & 0xFFFF) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}
