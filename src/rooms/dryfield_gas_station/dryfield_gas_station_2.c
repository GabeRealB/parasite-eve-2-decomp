#include "dryfield_gas_station_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/hud_sprites.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"
#include "../../shared/screen_fade.h"

/// Work block of the task that runs the gas station's arrival sequence: the
/// movie, then the in-room cutscene.
///
/// The task owns the block through `Task::work` and never clears it, so
/// `cutscene` is indeterminate until the cutscene task has been spawned.
typedef struct {
    Task* cutscene; // The cutscene task, spawned once the movie loader is running; the sequence ends when it asks to stop
} _DryfieldGasStationArrivalWork;
STATIC_ASSERT_SIZEOF(_DryfieldGasStationArrivalWork, 0x4);

void dryfieldGasStationArrivalMovieTask(Task* task)
{
    enum {
        DRYFIELD_GAS_STATION_MOVIE_PREPARE          = 0,
        DRYFIELD_GAS_STATION_MOVIE_START            = 1,
        DRYFIELD_GAS_STATION_MOVIE_WAIT_READY       = 2,
        DRYFIELD_GAS_STATION_MOVIE_PLAY             = 3,
        DRYFIELD_GAS_STATION_MOVIE_RESET_RESTORE    = 4,
        DRYFIELD_GAS_STATION_MOVIE_RESTORE          = 5,
        DRYFIELD_GAS_STATION_MOVIE_RESOURCE_VIEW    = 100,
        DRYFIELD_GAS_STATION_MOVIE_MUSIC_FRAME      = 390,
        DRYFIELD_GAS_STATION_MOVIE_MUSIC_FADE_TICKS = 10,
        DRYFIELD_GAS_STATION_MOVIE_FADE_TASK        = 2,
        DRYFIELD_GAS_STATION_MOVIE_FADE_FRAMES      = 8,
        DRYFIELD_GAS_STATION_MOVIE_KEEP_VRAM_IMAGES = 1,
        DRYFIELD_GAS_STATION_MOVIE_RELOAD_SPRITES   = 1
    };

    u8          streamArgs[4];
    GameLoc     movieLocation;
    CdCmdQueue* queue;
    s16         movieSlot;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case DRYFIELD_GAS_STATION_MOVIE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(DRYFIELD_GAS_STATION_MOVIE_KEEP_VRAM_IMAGES);
            task->state = task->state + 1;
            break;
        // PLAY_STREAM consumes the slot byte; the remaining argument bytes are uninterpreted.
        case DRYFIELD_GAS_STATION_MOVIE_START:
            movieLocation          = gGameSession->location;
            movieLocation.loc.view = DRYFIELD_GAS_STATION_MOVIE_RESOURCE_VIEW;
            movieSlot              = streamFindMovieSlot(&movieLocation.loc, 0, 0);
            streamArgs[0]          = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state = task->state + 1;
            break;
        case DRYFIELD_GAS_STATION_MOVIE_WAIT_READY:
            if (queue->movieReady == 0) {
                break;
            }
            task->killCountdown   = 0;
            task->spawnArg1.value = 0;
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case DRYFIELD_GAS_STATION_MOVIE_PLAY:
            if (++task->killCountdown == DRYFIELD_GAS_STATION_MOVIE_MUSIC_FRAME) {
                task->spawnArg1.value = 1;
                stageMusicRequestAreaStart(DRYFIELD_GAS_STATION_MOVIE_MUSIC_FADE_TICKS);
            }
            if (cdCmdIsIdle() != 0) {
                SetDispMask(0);
                task->state = task->state + 1;
                break;
            }
            if (padIsStartPressed() == 0) {
                break;
            }
            if (task->spawnArg1.value == 0) {
                stageMusicRequestAreaStart(DRYFIELD_GAS_STATION_MOVIE_MUSIC_FADE_TICKS);
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            break;
        case DRYFIELD_GAS_STATION_MOVIE_RESET_RESTORE:
            if (cdCmdIsIdle() == 0) {
                break;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            break;
        // Decoder use has ended before the full image workspace is cleared.
        case DRYFIELD_GAS_STATION_MOVIE_RESTORE:
            if (streamPollGameRestore(0, DRYFIELD_GAS_STATION_MOVIE_RELOAD_SPRITES) == 0) {
                break;
            }
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            taskKill(task);
            taskSpawnFromTableOnDefaultList(D_dryfield_gas_station_80181E7C, DRYFIELD_GAS_STATION_MOVIE_FADE_TASK, DRYFIELD_GAS_STATION_MOVIE_FADE_FRAMES, 0);
            displayResumeGameLoop();
            break;
    }
}

#include "../../shared/screen_fade_in.inc.c"

/// Hands presentation to the arrival movie and keeps current-view packets queued.
///
/// Starts descriptor 1 on the display task list and selects task-only flipping.
/// Requires this room's descriptors and current camera/packets to remain live
/// until the movie task restores the game loop. Presentation switches even if
/// the display task cannot be allocated.
static inline void _dryfieldGasStationStartArrivalMovieDisplay(void)
{
    enum { ARRIVAL_MOVIE_DESCRIPTOR = 1 };
    displaySpawnTaskFromTable(D_dryfield_gas_station_80181E7C, ARRIVAL_MOVIE_DESCRIPTOR, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
}

void dryfieldGasStationArrivalTask(Task* task)
{
    enum {
        DRYFIELD_GAS_STATION_ARRIVAL_INIT                = 0,
        DRYFIELD_GAS_STATION_ARRIVAL_DELAY_1             = 1,
        DRYFIELD_GAS_STATION_ARRIVAL_DELAY_2             = 2,
        DRYFIELD_GAS_STATION_ARRIVAL_SPAWN_CUTSCENE      = 3,
        DRYFIELD_GAS_STATION_ARRIVAL_WAIT_CUTSCENE       = 4,
        DRYFIELD_GAS_STATION_ARRIVAL_CUTSCENE_DESCRIPTOR = 0
    };

    _DryfieldGasStationArrivalWork* work;
    _DryfieldGasStationArrivalWork* allocatedWork;
    Task*                           cutscene;
    s32                             cutsceneResult;

    work = task->work;
    switch (task->state) {
        case DRYFIELD_GAS_STATION_ARRIVAL_INIT:
            allocatedWork = memMalloc(sizeof(*allocatedWork), false);
            task->work    = allocatedWork;
            if (allocatedWork == NULL) {
                taskKill(task);
                break;
            }
            _dryfieldGasStationStartArrivalMovieDisplay();
            task->state = task->state + 1;
            break;
        case DRYFIELD_GAS_STATION_ARRIVAL_DELAY_1:
        case DRYFIELD_GAS_STATION_ARRIVAL_DELAY_2:
            task->state = task->state + 1;
            break;
        case DRYFIELD_GAS_STATION_ARRIVAL_SPAWN_CUTSCENE:
            cutscene       = taskSpawnFromTable(D_dryfield_gas_station_8018312C, DRYFIELD_GAS_STATION_ARRIVAL_CUTSCENE_DESCRIPTOR, 0, 0);
            work->cutscene = cutscene;
            if (cutscene == NULL) {
                taskRequestKill(task, 0);
                break;
            }
            task->state = task->state + 1;
            break;
        case DRYFIELD_GAS_STATION_ARRIVAL_WAIT_CUTSCENE:
            if (taskPollKill(work->cutscene, &cutsceneResult) == 0) {
                break;
            }
            taskRequestKill(task, 0);
            break;
    }
}
