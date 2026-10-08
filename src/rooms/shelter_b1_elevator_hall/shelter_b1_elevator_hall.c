#include "rooms/shelter_b1_elevator_hall.h"

#include "types.h"

#include "shelter_b1_elevator_hall_private.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
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

static void _shelterB1ElevatorHallInitRoomTask(Task* task);
static void _shelterB1ElevatorHallIdleRoomTask(Task* task);

RoomEventMsg D_shelter_b1_elevator_hall_801849F8;

#include "../../shared/shelter_elevator_task.inc.c"

s32 func_shelter_b1_elevator_hall_8017D810(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    mapShelterRoomVariantResolve(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_B1_MAIN_CORRIDOR && gameFlagGetNibble(GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED) == 0) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(src->flagId, 2);
            capRunCommandWithTransition(2);
        }
        return 0;
    }
    if (src->areaId == GAME_AREA_SHELTER_B2_ELEVATOR) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED) == 0) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(src->flagId, 2);
                capRunCommandWithTransition(1);
            }
        } else {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommand(4, CAP_PLAYBACK_IN_PLACE);
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
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&D_shelter_b1_elevator_hall_80182CE8, 0, 0, 0);
        }
        return 2;
    }
    return 1;
}

/// The room task's state table, dispatched by
/// `shelterB1ElevatorHallRoomTask` from a stack copy.
static const TaskFuncTable3 D_shelter_b1_elevator_hall_8017D5D8 = {
    {
        _shelterB1ElevatorHallInitRoomTask,
        _shelterB1ElevatorHallIdleRoomTask,
        taskKill,
    },
};

void func_shelter_b1_elevator_hall_8017D99C(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            capRunCommand(5, CAP_PLAYBACK_IN_PLACE);
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
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
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
            sceneQueueBattleEscapeResult();
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
            if (sndScriptHasActiveId(SOUND_SHELTER_B1_ELEV_HALL_MINE_TRANSIT) == 0) {
                arg0->state++;
            }
            break;
        case 6:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b1_elevator_hall_801849F8.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b1_elevator_hall_801849F8.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_shelter_b1_elevator_hall_801849F8.areaId)[1];
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(arg0);
            break;
    }
}

s32 shelterB1ElevatorHallRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 shelterB1ElevatorHallIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandMode)
{
    return 0;
}

s32 shelterB1ElevatorHallIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 secondArg)
{
    return 0;
}

s32 shelterB1ElevatorHallPlaySoundCueMessage(Task* task, s32 messageId, s32 cueId, s32 secondArg)
{
    enum { SOUND_CUE_CONFIRM       = 6,
           SOUND_CUE_ELEVATOR_RIDE = 8 };

    switch (cueId) {
        case SOUND_CUE_CONFIRM:
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            break;
        case SOUND_CUE_ELEVATOR_RIDE:
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_ELEVATOR_RIDE, 0, 0);
            break;
    }
    return 0;
}

/// Registers the hall's room-message receiver and records its first visit.
///
/// State 0 installs the borrowed message table and publishes the live task in
/// `GAME_TASK_SLOT_ROOM`. First entry sets the visited nibble and objective
/// byte; later entries preserve the objective. Advances to the idle state.
static void _shelterB1ElevatorHallInitRoomTask(Task* task)
{
    enum { ROOM_UNVISITED        = 0,
           ROOM_VISITED          = 1,
           FIRST_VISIT_OBJECTIVE = 0x1D };

    task->msgTable = D_shelter_b1_elevator_hall_80182CB8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ELEVATOR_HALL_VISITED) == ROOM_UNVISITED) {
        gameFlagSetNibble(GAME_FLAG_SHELTER_B1_ELEVATOR_HALL_VISITED, ROOM_VISITED);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, FIRST_VISIT_OBJECTIVE);
    }
    task->state++;
}

/// Keeps the initialized room task available for messages without frame work.
///
/// State 1 leaves the task and its state unchanged until external teardown.
static void _shelterB1ElevatorHallIdleRoomTask(Task* task)
{
}

void shelterB1ElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b1_elevator_hall_8017D5D8;
    stateHandlers.funcs[task->state](task);
}
