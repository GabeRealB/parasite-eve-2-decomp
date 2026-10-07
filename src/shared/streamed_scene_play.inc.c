/* Part of the streamed scene library; see streamed_scene.h. */

void streamedScenePlay(Task* movieTask)
{
    u8          commandArgs[sizeof(gCdCmdQueue.entries[0].args)];
    GameLoc     movieLocation;
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (movieTask->state) {
        case STREAMED_SCENE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            movieTask->state++;
            break;
        case STREAMED_SCENE_QUEUE_MOVIE:
            movieLocation          = gGameSession->location;
            movieLocation.loc.view = STREAMED_SCENE_MOVIE_ID;
            // This opcode consumes only the slot byte of the four-byte argument block.
            commandArgs[0] = streamFindMovieSlot(&movieLocation.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
            movieTask->state++;
            break;
        case STREAMED_SCENE_WAIT_READY:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                movieTask->state++;
            }
            break;
        case STREAMED_SCENE_PLAYING:
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                movieTask->state++;
            } else if (padIsStartPressed() != 0) {
                SetDispMask(0);
                cdCmdRequestCancel();
                movieTask->state++;
            }
            break;
        // Restore game resources only after playback or cancellation has drained the CD queue.
        case STREAMED_SCENE_WAIT_IDLE:
            if (cdCmdIsIdle()) {
                streamResetGameRestore();
                movieTask->state++;
            }
            break;
        case STREAMED_SCENE_RESTORE_GAME:
            if (streamPollGameRestore(0, 1)) {
                taskKill(movieTask);
                displayResumeGameLoop();
            }
            break;
    }
}
