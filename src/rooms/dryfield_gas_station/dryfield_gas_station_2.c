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

void func_dryfield_gas_station_8017FFE4(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state = task->state + 1;
            break;
        case 1:
            key          = gGameSession->location;
            key.loc.view = 0x64;
            slot         = streamFindMovieSlot(&key.loc, 0, 0);
            slotParam[0] = slot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state = task->state + 1;
            break;
        case 2:
            if (queue->movieReady == 0) {
                break;
            }
            task->killCountdown   = 0;
            task->spawnArg1.value = 0;
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case 3:
            if (++task->killCountdown == 0x186) {
                task->spawnArg1.value = 1;
                Stage_RequestFromAreaTable(0xA);
            }
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                break;
            }
            if (Pad_CheckFlag800() == 0) {
                break;
            }
            if (task->spawnArg1.value == 0) {
                Stage_RequestFromAreaTable(0xA);
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            break;
        case 4:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            break;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                break;
            }
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            taskKill(task);
            taskSpawnFromTableOnDefaultList(D_dryfield_gas_station_80181E7C, 2, 8, 0);
            displayResumeGameLoop();
            break;
    }
}

#include "../../shared/screen_fade_in.inc.c"

/// Runs the gas station's shaft sequence. State 0 allocates the task's
/// `_DryfieldGasStationArrivalWork` into `Task::work`, spawns the
/// `D_dryfield_gas_station_80181E7C` entry 1 loader through
/// `Display_SpawnWithOt` and turns the view tasks on; states 1 and 2 only
/// step, so state 3 spawns the cutscene task from
/// `D_dryfield_gas_station_8018312C` entry 0 into that block, and state 4 kills
/// this task once the cutscene has died. The block is read at function entry,
/// before state 0 writes the freshly allocated one, so only a later run of
/// the state machine sees it.
void func_dryfield_gas_station_801802C0(Task* task)
{
    _DryfieldGasStationArrivalWork* work;
    _DryfieldGasStationArrivalWork* allocated;
    Task*                           cutscene;
    s32                             killed;

    work = task->work;
    switch (task->state) {
        case 0:
            allocated  = memMalloc(sizeof(*allocated), false);
            task->work = allocated;
            if (allocated == NULL) {
                taskKill(task);
                break;
            }
            Display_SpawnWithOt(D_dryfield_gas_station_80181E7C, 1, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            Gp_SpawnViewTasks();
            task->state = task->state + 1;
            break;
        case 1:
        case 2:
            task->state = task->state + 1;
            break;
        case 3:
            cutscene       = taskSpawnFromTable(D_dryfield_gas_station_8018312C, 0, 0, 0);
            work->cutscene = cutscene;
            if (cutscene == NULL) {
                Task_RequestKill(task, 0);
                break;
            }
            task->state = task->state + 1;
            break;
        case 4:
            if (Task_PollKill(work->cutscene, &killed) == 0) {
                break;
            }
            Task_RequestKill(task, 0);
            break;
    }
}
