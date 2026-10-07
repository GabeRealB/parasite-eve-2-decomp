/* Part of the room cutscene library; see room_cutscene.h. */

/// The scene's sound task, entry 1 of `gRoomCutsceneTaskDescs`: plays the sound
/// event passed in `spawnArg2` on frames 0 and 0x50 of its life and kills
/// itself at frame 0x78.
void roomCutsceneSoundTask(Task* task)
{
    switch (task->state) {
        case 0x50:
        case 0x0:
            sndEvtRequestScriptStart(task->spawnArg2.value, 0, 0);
            task->state += 1;
            break;
        case 0x78:
            taskRequestKill(task, 0);
            break;
        default:
            task->state += 1;
            break;
    }
}
