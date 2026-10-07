#include "rooms/shelter_b1_elevator_hall.h"

#include "types.h"

#include "shelter_b1_elevator_hall_private.h"

#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

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
    mapShelterRoomVariantResolve(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_B1_MAIN_CORRIDOR && gameFlagGetNibble(GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED) == 0) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SetNibbleIf(src->flagId, 2);
            Gp_RunCapCmd1(2);
        }
        return 0;
    }
    if (src->areaId == GAME_AREA_SHELTER_B2_ELEVATOR) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED) == 0) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(src->flagId, 2);
                Gp_RunCapCmd1(1);
            }
        } else {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_RunCapCmd(4, 0);
                taskSpawnFromTable(&D_shelter_b1_elevator_hall_80182CAC, 0, 0x54090008, 0);
            }
        }
        return 0;
    }
    if (src->areaId == GAME_AREA_MINE_SECRET_PASSAGE) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b1_elevator_hall_801849F8.warp              = (u8)dst->areaId;
            D_shelter_b1_elevator_hall_801849F8.field_4           = dst->warp;
            ((u8*)&D_shelter_b1_elevator_hall_801849F8.areaId)[1] = dst->room;
            Gp_MsgPlayerWeapon(0);
            taskSpawnFromTable(&D_shelter_b1_elevator_hall_80182CE8, 0, 0, 0);
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
            arg0->state++;
            break;
        case 1:
            if (capIsBusy() != 0) {
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            arg0->state++;
            break;
        case 2:
            if (capGetVariantKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            arg0->killCountdown            = 3;
            arg0->state++;
            break;
        case 3:
            temp_v0             = (u16)arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if ((temp_v0 << 0x10) != 0) {
                break;
            }
            Gp_TriggerPeIfArmed();
            arg0->state++;
            break;
        case 4:
            D_shelter_b1_elevator_hall_801849F0.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_shelter_b1_elevator_hall_801849F0.fade.phase      = SCREEN_FADE_RUNNING;
            D_shelter_b1_elevator_hall_801849F0.fade.rampFrames = 0x1E;
            taskSpawn(1, 0x31, 0, &D_shelter_b1_elevator_hall_801849F0.fade);
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_ELEV_HALL_MINE_TRANSIT, 0, 0);
            arg0->state++;
            break;
        case 5:
            if (SndVoice_HasActiveId(SOUND_SHELTER_B1_ELEV_HALL_MINE_TRANSIT) == 0) {
                arg0->state++;
            }
            break;
        case 6:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b1_elevator_hall_801849F8.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b1_elevator_hall_801849F8.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_shelter_b1_elevator_hall_801849F8.areaId)[1];
            taskSpawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b1_elevator_hall_8017DB54(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB5C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB64(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB6C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 6:
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            break;
        case 8:
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_ELEVATOR_RIDE, 0, 0);
            break;
    }
    return 0;
}

static void func_shelter_b1_elevator_hall_8017DBB8(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_elevator_hall_80182CB8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ELEVATOR_HALL_VISITED) == 0) {
        gameFlagSetNibble(GAME_FLAG_SHELTER_B1_ELEVATOR_HALL_VISITED, 1);
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
