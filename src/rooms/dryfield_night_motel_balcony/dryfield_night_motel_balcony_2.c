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
#include "../../shared/room_variants.h"

void func_dryfield_night_motel_balcony_8017E068(Task*);

void func_dryfield_night_motel_balcony_8017DDD0(Task*);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_motel_balcony_80182804[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantMotelBalconyDoorsMsg },
    { 5105, func_dryfield_night_motel_balcony_8017DC18 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_motel_balcony_8017DC28 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_motel_balcony_8017DC20 },
    { ROOM_MESSAGE_SOUND, roomVariantMotelBalconySoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_motel_balcony_80182834[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_motel_balcony_8017E0C8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_motel_balcony_8017DDD0, { .value = 0 } },
};

TaskDesc D_dryfield_night_motel_balcony_8018284C = { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_motel_balcony_8017E068, { .value = 0 } }; /// The balcony movie task. It blanks the display, allocates the movie
/// buffers and plays two streams keyed on the current location - view 0x65
/// then 0x64, or 0x67 then 0x66 when `Wip_SysFlags.discNumber` is disc 2 - either of
/// which the pad can skip, then restores the stream state, kills itself and
/// restores session image memory and game-loop presentation.
void func_dryfield_night_motel_balcony_8017DDD0(Task* task)
{
    u8          slotParam[4];
    GameLoc     introKey;
    GameLoc     loopKey;
    CdCmdQueue* queue;
    s16         slot;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state = task->state + 1;
            return;
        case 1:
            introKey = gGameSession->location;
            if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_2) {
                introKey.loc.view = 0x67;
            } else {
                introKey.loc.view = 0x65;
            }
            slot         = streamFindMovieSlot(&introKey.loc, 0, 0);
            slotParam[0] = slot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 3:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = 7;
            return;
        case 4:
            if (cdCmdIsIdle() & 0xFFFF) {
                loopKey = gGameSession->location;
                if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_2) {
                    loopKey.loc.view = 0x66;
                } else {
                    loopKey.loc.view = 0x64;
                }
                slot         = streamFindMovieSlot(&loopKey.loc, 0, 0);
                slotParam[0] = slot;
                cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            }
            task->state = task->state + 1;
            return;
        case 5:
            if (queue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 6:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            return;
        case 7:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            return;
        case 8:
            if ((streamPollGameRestore(0, 1) & 0xFFFF) == 0) {
                return;
            }
            taskKill(task);
            displayResumeGameLoop();
            return;
    }
}

/// Draws a white fade overlay (mode 2) each tick while `killCountdown`
/// climbs by 4, and kills the task once it reaches 0x100.
void func_dryfield_night_motel_balcony_8017E068(Task* arg0)
{
    u16 temp_v0;

    fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}
