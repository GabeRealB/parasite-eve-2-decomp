#include "rooms/shelter_b1_elevator_hall.h"

#include "types.h"

#include "shelter_b1_elevator_hall_private.h"

#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"

extern RoomEventMsg D_shelter_b1_elevator_hall_801849F8;

static void func_shelter_b1_elevator_hall_8017DBB8(Task* arg0);
static void func_shelter_b1_elevator_hall_8017DC20(Task* task);

RoomEventMsg D_shelter_b1_elevator_hall_801849F8;

#include "../../shared/shelter_elevator_task.inc.c"

s32 func_shelter_b1_elevator_hall_8017D810(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (src->areaId == 0xF && GameFlag_GetNibble(0xA5) == 0) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SetNibbleIf(src->flagId, 2);
            Gp_RunCapCmd1(2);
        }
        return 0;
    }
    if (src->areaId == 0x1A) {
        if (GameFlag_GetNibble(0xBA) == 0) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(src->flagId, 2);
                Gp_RunCapCmd1(1);
            }
        } else {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_RunCapCmd(4, 0);
                Task_SpawnFromTable(&D_shelter_b1_elevator_hall_80182CAC, 0, 0x54090008, 0);
            }
        }
        return 0;
    }
    if (src->areaId == 8) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b1_elevator_hall_801849F8.warp              = (u8)dst->areaId;
            D_shelter_b1_elevator_hall_801849F8.field_4           = dst->warp;
            ((u8*)&D_shelter_b1_elevator_hall_801849F8.areaId)[1] = dst->room;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_elevator_hall_80182CE8, 0, 0, 0);
        }
        return 2;
    }
    return 1;
}

/// The room task's state table, dispatched by
/// `func_shelter_b1_elevator_hall_8017DC28` from a stack copy.
static const TaskFuncTable3 D_shelter_b1_elevator_hall_8017D5D8 = {
    {
        func_shelter_b1_elevator_hall_8017DBB8,
        func_shelter_b1_elevator_hall_8017DC20,
        taskKill,
    },
};

void func_shelter_b1_elevator_hall_8017D99C(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            Gp_RunCapCmd(5, 0);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_StateF0.field_4 = 0;
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                Gp_StateF0.field_4 = 0;
                break;
            }
            Gp_StateF0.field_4  = 1;
            arg0->killCountdown = 3;
            arg0->state++;
            break;
        case 3:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if ((temp_v0 << 0x10) != 0) {
                break;
            }
            Gp_TriggerPeIfArmed();
            goto advance;
        case 4:
            D_shelter_b1_elevator_hall_801849F0.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_shelter_b1_elevator_hall_801849F0.fade.phase      = SCREEN_FADE_RUNNING;
            D_shelter_b1_elevator_hall_801849F0.fade.rampFrames = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_shelter_b1_elevator_hall_801849F0.fade);
            SndEvt_EnqueueType6(0x54090007, 0, 0);
            goto advance;
        case 5:
            if (SndVoice_HasActiveId(0x54090007) != 0) {
                break;
            }
        advance:
            arg0->state++;
            break;
        case 6:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant            = 1;
            Mc_SaveData[0].state.location.loc.area = D_shelter_b1_elevator_hall_801849F8.warp;
            Mc_SaveData[0].state.location.loc.warp = D_shelter_b1_elevator_hall_801849F8.field_4;
            Mc_SaveData[0].state.location.loc.room = ((u8*)&D_shelter_b1_elevator_hall_801849F8.areaId)[1];
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b1_elevator_hall_8017DB54(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB5C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB64(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB6C(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 6:
            SndEvt_EnqueueType6(0x16, 0, 0);
            break;
        case 8:
            SndEvt_EnqueueType6(0x54090008, 0, 0);
            break;
    }
    return 0;
}

static void func_shelter_b1_elevator_hall_8017DBB8(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_elevator_hall_80182CB8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x122) == 0) {
        GameFlag_SetNibble(0x122, 1);
        func_800E3FAC(0xA2, 0x1D);
    }
    arg0->state++;
}

/// Empty middle state of the room task's state table.
static void func_shelter_b1_elevator_hall_8017DC20(Task* task)
{
}

/// Runs the room task through its state table, copied onto the stack first and
/// indexed by the task's state.
void func_shelter_b1_elevator_hall_8017DC28(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_elevator_hall_8017D5D8;
    sp.funcs[task->state](task);
}
