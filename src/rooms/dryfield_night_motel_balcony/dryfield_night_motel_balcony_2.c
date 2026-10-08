#include "rooms/dryfield_night_motel_balcony.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "dryfield_night_motel_balcony_private.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"

static void _dryfieldNightMotelBalconyBlackoutTask(Task* task);

static void _dryfieldNightMotelBalconyMovieTask(Task* task);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_motel_balcony_80182804[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantMotelBalconyDoorsMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldNightMotelBalconyRefuseKeyItemMsg },
    { DIRECTION_MESSAGE_ROOM_ACTION, dryfieldNightMotelBalconyIgnoreRoomActionMsg },
    { ROOM_MESSAGE_COMMAND, dryfieldNightMotelBalconyIgnoreCommandMsg },
    { ROOM_MESSAGE_SOUND, motelBalconyCueSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_motel_balcony_80182834[2] = {
    { { { TASK_BODY_NONE, 192 } }, dryfieldNightMotelBalconyBeginMoviesTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _dryfieldNightMotelBalconyMovieTask, { .value = 0 } },
};

TaskDesc D_dryfield_night_motel_balcony_8018284C = { { { TASK_BODY_NONE, 192 } }, _dryfieldNightMotelBalconyBlackoutTask, { .value = 0 } };

/// Reveals a ready movie and advances the borrowed controller to playback.
///
/// Requires the live movie controller in either wait-ready state and the CD
/// queue for its current playback. An unset latch leaves state and display
/// unchanged; a set latch may mean playback has started or was skipped.
static inline void _dryfieldNightMotelBalconyRevealReadyMovie(Task* task, const CdCmdQueue* cdQueue)
{
    if (cdQueue->movieReady == 0) {
        return;
    }
    SetDispMask(true);
    task->state = task->state + 1;
}

/// Plays the balcony's two-movie sequence, allowing Start to skip either part.
///
/// Start this bodyless controller in state 0 with the movie slots and display
/// task loaded. Disc 2 selects streams 103/102, other discs 101/100. Each lookup
/// must succeed with slot 0..14; failure is unchecked. Skipping the first movie
/// bypasses the second. Playback needs exclusive movie/CD workspace use and
/// intact saved VRAM images. After CD idle, reloads sprite images and restores
/// memory before releasing this task and resuming game-loop presentation.
static void _dryfieldNightMotelBalconyMovieTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_PREPARE                  = 0,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_QUEUE_FIRST              = 1,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_WAIT_FIRST_READY         = 2,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_PLAY_FIRST               = 3,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_QUEUE_SECOND             = 4,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_WAIT_SECOND_READY        = 5,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_PLAY_SECOND              = 6,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_BEGIN_RESTORE            = 7,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_RESTORE                  = 8,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_DEFAULT_FIRST_STREAM_ID  = 101,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_DISC_2_FIRST_STREAM_ID   = 103,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_DEFAULT_SECOND_STREAM_ID = 100,
        DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_DISC_2_SECOND_STREAM_ID  = 102,
    };
    u8          streamArgs[4]; // Serialized command bytes; only the slot byte is consumed by playback.
    GameLoc     firstMovieKey;
    GameLoc     secondMovieKey;
    CdCmdQueue* cdQueue;
    s16         movieSlot;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_PREPARE:
            SetDispMask(false);
            streamPrepareMovieWorkspace(true);
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_QUEUE_FIRST:
            firstMovieKey = gGameSession->location;
            if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_2) {
                firstMovieKey.loc.view = DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_DISC_2_FIRST_STREAM_ID;
            } else {
                firstMovieKey.loc.view = DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_DEFAULT_FIRST_STREAM_ID;
            }
            movieSlot     = streamFindMovieSlot(&firstMovieKey.loc, 0, 0);
            streamArgs[0] = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_WAIT_FIRST_READY:
            _dryfieldNightMotelBalconyRevealReadyMovie(task, cdQueue);
            return;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_PLAY_FIRST:
            if (cdCmdIsIdle()) {
                SetDispMask(false);
                task->state = task->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(false);
            cdCmdRequestCancel();
            task->state = DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_BEGIN_RESTORE;
            return;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_QUEUE_SECOND:
            if (cdCmdIsIdle()) {
                secondMovieKey = gGameSession->location;
                if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_2) {
                    secondMovieKey.loc.view = DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_DISC_2_SECOND_STREAM_ID;
                } else {
                    secondMovieKey.loc.view = DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_DEFAULT_SECOND_STREAM_ID;
                }
                movieSlot     = streamFindMovieSlot(&secondMovieKey.loc, 0, 0);
                streamArgs[0] = movieSlot;
                cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            }
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_WAIT_SECOND_READY:
            _dryfieldNightMotelBalconyRevealReadyMovie(task, cdQueue);
            return;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_PLAY_SECOND:
            if (cdCmdIsIdle()) {
                SetDispMask(false);
                task->state = task->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(false);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            return;
        // Cancellation must drain before the decoder workspace can be restored.
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_BEGIN_RESTORE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_MOVIE_RESTORE:
            if (streamPollGameRestore(false, true) == 0) {
                return;
            }
            taskKill(task);
            displayResumeGameLoop();
            return;
    }
}

/// Holds presentation black until its callback counter reaches 256.
///
/// A bodyless display task with `killCountdown` initially zero lasts 64 ticks.
/// Each tick subtracts full white, increments the wrapping halfword by four,
/// and releases the task when the signed result is at least 256. Requires the
/// current frame's packet arena and ordering table; the counter does not fade RGB.
static void _dryfieldNightMotelBalconyBlackoutTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_BLACKOUT_COUNTER_STEP = 4,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BLACKOUT_COUNTER_END  = 256,
    };
    u16 nextCounter;

    fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
    nextCounter         = task->killCountdown + DRYFIELD_NIGHT_MOTEL_BALCONY_BLACKOUT_COUNTER_STEP;
    task->killCountdown = nextCounter;
    if ((s16)nextCounter >= DRYFIELD_NIGHT_MOTEL_BALCONY_BLACKOUT_COUNTER_END) {
        taskKill(task);
    }
}
