#include "rooms/dryfield_night_motel_balcony.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "dryfield_night_motel_balcony_private.h"

#include "gameplay/message.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#define ROOM_EVENT_ACTIVE gRoomEventActive[0]
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"

void func_dryfield_night_motel_balcony_8017E068(Task*);

void func_dryfield_night_motel_balcony_8017DDD0(Task*);

TaskDesc gRoomEventTaskDesc = { 0, 32, roomEventTask, { .model = NULL } };

GpMsgEntry D_dryfield_night_motel_balcony_80182804[6] = {
    { 5102, roomVariantMotelBalconyDoorsMsg },
    { 5105, func_dryfield_night_motel_balcony_8017DC18 },
    { 5103, func_dryfield_night_motel_balcony_8017DC28 },
    { 5104, func_dryfield_night_motel_balcony_8017DC20 },
    { 5106, roomVariantMotelBalconySoundMsg },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_night_motel_balcony_80182834[2] = {
    { 0, 192, func_dryfield_night_motel_balcony_8017E0C8, { .model = NULL } },
    { 0, 192, func_dryfield_night_motel_balcony_8017DDD0, { .model = NULL } },
};

TaskDesc D_dryfield_night_motel_balcony_8018284C = { 0, 192, func_dryfield_night_motel_balcony_8017E068, { .model = NULL } }; /// The balcony movie task. It blanks the display, allocates the movie
/// buffers and plays two streams keyed on the current location - view 0x65
/// then 0x64, or 0x67 then 0x66 when `Wip_SysFlags.field_0` is 2 - either of
/// which the pad can skip, then restores the stream state, resets the display
/// heap and kills itself.
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
            Mem_AllocAuxWithImages(1);
            task->state = task->state + 1;
            return;
        case 1:
            introKey = gGameSession->location;
            if (Wip_SysFlags.field_0 == 2) {
                introKey.loc.view = 0x67;
            } else {
                introKey.loc.view = 0x65;
            }
            slot         = Stream_FindSlot((u8*)&introKey, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
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
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = 7;
            return;
        case 4:
            if (CdCmd_IsIdle() & 0xFFFF) {
                loopKey = gGameSession->location;
                if (Wip_SysFlags.field_0 == 2) {
                    loopKey.loc.view = 0x66;
                } else {
                    loopKey.loc.view = 0x64;
                }
                slot         = Stream_FindSlot((u8*)&loopKey, 0, 0);
                slotParam[0] = slot;
                CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
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
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = task->state + 1;
            return;
        case 7:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 8:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            taskKill(task);
            Display_ResetHeapWrapper();
            return;
    }
}

/// Draws a white fade overlay (mode 2) each tick while `killCountdown`
/// climbs by 4, and kills the task once it reaches 0x100.
void func_dryfield_night_motel_balcony_8017E068(Task* arg0)
{
    u16 temp_v0;

    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}
