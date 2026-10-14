/* Part of the room cutscene library; see room_cutscene.h. */

/// Queues the scene's sound script twice and signals completion of its timed wait.
///
/// `task` must be live with `state` initially zero. `spawnArg2.value` holds a
/// packed request id for `sndEvtRequestScriptStart`; both starts use the base
/// pan and attenuation. The counter advances once per callback invocation,
/// regardless of sound admission or playback. At tick 120 the task suspends
/// with result zero until the cutscene runner polls and tears it down.
/// Keep the room's callback code and the required sound bank loaded while used.
static void _roomCutsceneSoundTask(Task* task)
{
    enum {
        ROOM_CUTSCENE_SOUND_START_TICK    = 0,
        ROOM_CUTSCENE_SOUND_REPEAT_TICK   = 80,
        ROOM_CUTSCENE_SOUND_COMPLETE_TICK = 120
    };

    switch (task->state) {
        case ROOM_CUTSCENE_SOUND_REPEAT_TICK:
        case ROOM_CUTSCENE_SOUND_START_TICK:
            sndEvtRequestScriptStart(task->spawnArg2.value, 0, 0);
            task->state += 1;
            break;
        case ROOM_CUTSCENE_SOUND_COMPLETE_TICK:
            taskRequestKill(task, 0);
            break;
        default:
            task->state += 1;
            break;
    }
}
