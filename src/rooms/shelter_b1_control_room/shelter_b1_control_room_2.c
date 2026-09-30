#include "rooms/shelter_b1_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b1_control_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

#define D_shelter_b1_control_room_80181C3C (D_shelter_b1_control_room_80181BD4 + 13)

// Indexed views below share one contiguous table.
void func_shelter_b1_control_room_8017EF24(Task*);
void func_shelter_b1_control_room_8017F100(Task*);

TaskDesc D_shelter_b1_control_room_80181BBC[2] = {
    { 0, 192, func_shelter_b1_control_room_8017F100, { .model = NULL } },
    { 0, 192, func_shelter_b1_control_room_8017EF24, { .model = NULL } },
};

SVECTOR D_shelter_b1_control_room_80181BD4[18] = {
    { 8480, -3330, -2490, 0 },
    { 9080, -3330, -2490, 0 },
    { 9280, -3330, -2490, 0 },
    { 9880, -3330, -2490, 0 },
    { 10090, -3330, -2490, 0 },
    { 10680, -3330, -2490, 0 },
    { 8480, -3330, -6090, 0 },
    { 9080, -3330, -6090, 0 },
    { 9280, -3330, -6090, 0 },
    { 9880, -3330, -6090, 0 },
    { 10090, -3330, -6090, 0 },
    { 10680, -3330, -6090, 0 },
    { 10970, -3000, -3040, 0 },
    { 1180, -850, -2530, 0 },
    { 2180, -850, -2530, 0 },
    { 3180, -850, -2530, 0 },
    { 4180, -850, -2530, 0 },
    { 5180, -850, -2530, 0 },
};

/// Streamed-scene task. It blanks the display and queues CD command 0x61 on
/// the stream slot of the current location with its view replaced by 0x64,
/// then shows the display once the command queue signals it. The scene runs
/// until the CD is idle or the pad check aborts it, which is recorded in
/// `spawnArg1`. After the stream state is restored an aborted scene kills the
/// task at once, and a finished one after 0x3D more ticks; either way the
/// display heap is reset.
void func_shelter_b1_control_room_8017EF24(Task* arg0)
{
    u8          slotParam[4];
    s32         state;
    GameLoc     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
        case 6:
            goto L_case6;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->location;
    key.loc.view = 0x64;
    slotParam[0] = Stream_FindSlot((u8*)&key, 0, 0);
    CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case2:
    if (queue->movieReady == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        state                 = task->state;
        task->spawnArg1.value = 0;
        task->state           = state + 1;
        return;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    task->spawnArg1.value = 1;
    task->state           = task->state + 1;
    return;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
    goto advance;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    if (task->spawnArg1.value != 0) {
        goto kill;
    }
advance:
    task->state = task->state + 1;
    return;

L_case6:
    task->killCountdown = task->killCountdown + 1;
    if (task->killCountdown < 0x3D) {
        return;
    }
kill:
    taskKill(task);
    Display_ResetHeapWrapper();
}

void func_shelter_b1_control_room_8017F100(Task* arg0)
{
    Display_SpawnWithOt(D_shelter_b1_control_room_80181BBC, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_shelter_b1_control_room_8017F150(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_80115734  = 0x60276;
        D_80115730  = 0x60277;
        D_80115754  = 0x60278;
        task->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[0], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[2], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[4], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[6], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[8], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[10], 0x100, 0x243);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[12], 0x180, 0x421);
            break;
        case 3:
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[0], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[2], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[4], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[6], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[8], 0x100, 0x243);
            glowDrawCapsule(&D_shelter_b1_control_room_80181BD4[10], 0x100, 0x243);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[12], 0x180, 0x421);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[14], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[15], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[16], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181BD4[17], 0x200, 0x23);
            break;
        case 4:
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[0], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[1], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[2], 0x200, 0x23);
            break;
        case 6:
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[0], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[1], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[2], 0x200, 0x23);
            glowDrawDisc(&D_shelter_b1_control_room_80181C3C[3], 0x200, 0x23);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"
