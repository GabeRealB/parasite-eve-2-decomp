#include "types.h"

#include "main/task_types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
Task* D_shelter_b3_garbage_incinerator_801855D8;

u16 D_shelter_b3_garbage_incinerator_801855DC;

#include "rooms/shelter_b3_garbage_incinerator.h"

#include "shelter_b3_garbage_incinerator_private.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
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

#include "mapui/map_shelter.h"

extern TaskDesc         D_actor_342400_8016BFE0[];
extern TaskDesc         D_actor_444000_801449F4;
extern TaskMessageEntry D_shelter_b3_garbage_incinerator_80185594[];

extern TaskDesc D_shelter_b3_garbage_incinerator_801855CC;

static void func_shelter_b3_garbage_incinerator_8017DB7C(Task* task);
static void _shelterB3GarbageIncineratorAdvanceSwitchTimer(Task* task);

/// State handlers of the room's controller task, run by
/// `func_shelter_b3_garbage_incinerator_8017DC7C`: set-up, a per-frame tick,
/// and the kill.
static const TaskFuncTable3 D_shelter_b3_garbage_incinerator_8017D5C4 = { {
    func_shelter_b3_garbage_incinerator_8017DB7C,
    _shelterB3GarbageIncineratorAdvanceSwitchTimer,
    taskKill,
} };

void       func_shelter_b3_garbage_incinerator_8017D6EC(Task*);
static s32 _shelterB3GarbageIncineratorRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
s32        func_shelter_b3_garbage_incinerator_8017D840(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _shelterB3GarbageIncineratorIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg);
s32        func_shelter_b3_garbage_incinerator_8017D9BC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_shelter_b3_garbage_incinerator_8017DA74(Task*, s32, s32, s32);
static s32 _shelterB3GarbageIncineratorHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unused);

TaskMessageEntry D_shelter_b3_garbage_incinerator_80185594[7] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b3_garbage_incinerator_8017D840 },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB3GarbageIncineratorRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b3_garbage_incinerator_8017D9BC },
    { ROOM_MESSAGE_COMMAND, _shelterB3GarbageIncineratorIgnoreCommand },
    { ROOM_MESSAGE_ACTOR_EVENT, func_shelter_b3_garbage_incinerator_8017DA74 },
    { ROOM_MESSAGE_SOUND, _shelterB3GarbageIncineratorHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b3_garbage_incinerator_801855CC = { { { TASK_BODY_NONE, 32 } }, func_shelter_b3_garbage_incinerator_8017D6EC, { .value = 0 } };

u16 D_shelter_b3_garbage_incinerator_801855DE;

void func_shelter_b3_garbage_incinerator_8017D6EC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(0x12, CAP_PLAYBACK_IN_PLACE);
            arg0->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (capGetVariantKey() == 0) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(arg0);
            } else {
                sndEvtRequestStageScriptStart(SOUND_SHELTER_B3_INCINERATOR_EXIT_TRANSIT, 0, 0);
                arg0->state++;
            }
            break;
        case 3:
            if (sndScriptHasActiveId(SOUND_SHELTER_B3_INCINERATOR_EXIT_TRANSIT) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b3_garbage_incinerator_8018FC2C.areaId;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b3_garbage_incinerator_8018FC2C.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_shelter_b3_garbage_incinerator_8018FC2C.room;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(arg0);
            break;
    }
}

/// Refuses key-item use with the inventory menu's refused reply.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored and the
/// room state remains unchanged.
static s32 _shelterB3GarbageIncineratorRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 func_shelter_b3_garbage_incinerator_8017D840(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM) {
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 0;
        }
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        if (gGameSession->location.loc.room < 4) {
            capSpawnEventIfIdle(3, CAP_EVENT_PAUSE_ACTORS);
            return 0;
        }
        if (gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) != 0) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 10);
        } else {
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 5);
        }
        out->warp                                 = 4;
        D_shelter_b3_garbage_incinerator_8018FC2C = *out;
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 1);
        taskSpawnFromTable(&D_shelter_b3_garbage_incinerator_801855CC, 0, 0, 0);
        return 2;
    }
    if (in->areaId == GAME_AREA_SHELTER_B3_DUMPING_HOLE && in->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = gGameSession->incineratorRoomGroup + 1;
    }
    return 1;
}

/// Ignores room commands and returns zero without changing room state.
///
/// Handles `ROOM_MESSAGE_COMMAND`; neither the command nor its argument is read.
static s32 _shelterB3GarbageIncineratorIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg)
{
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017D9BC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->warp == 2 && gGameSession->incineratorExitPhase == GAME_SESSION_INCINERATOR_EXIT_NONE) {
        if (gGameSession->incineratorDescentPhase == GAME_SESSION_INCINERATOR_DESCENT_COMPLETE) {
            taskSpawnFromTable(&D_shelter_b3_garbage_incinerator_801855E0, 0, 0, 0);
            gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_WARP;
        } else if (D_shelter_b3_garbage_incinerator_801855DC >= 0x3D) {
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_SWITCH_PRESS, 0, 0);
            func_shelter_b3_garbage_incinerator_80180FE4(0x16, 0, 0x3C);
            D_shelter_b3_garbage_incinerator_801855DC = 0;
        }
    }
    return 0;
}

s32 func_shelter_b3_garbage_incinerator_8017DA74(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0:
            taskMessageDispatch(D_shelter_b3_garbage_incinerator_801855D8, ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            break;
        case 1:
            gGameSession->skipEventIntro = 1;
            taskSpawnFromTable(&D_actor_444000_801449F4, 0, 0, 0);
            break;
        case 2:
            gGameSession->skipEventIntro              = 1;
            D_shelter_b3_garbage_incinerator_801855DE = 1;
            taskSpawnFromTable(&D_actor_444000_801449F4, 0, 1, 0);
            break;
    }
    return 0;
}

/// Maps room sound cues 9 and 10 to the incinerator bank's corresponding scripts.
///
/// Handles `ROOM_MESSAGE_SOUND` with an integer cue and unused second word.
/// Requires the room sound bank loaded. Other cues do nothing; returns zero.
static s32 _shelterB3GarbageIncineratorHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unused)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_9  = 9,
        SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_10 = 10
    };
    switch (cueId) {
        case SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_9:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_9), 0, 0);
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_10:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_10), 0, 0);
            break;
    }
    return 0;
}

static void func_shelter_b3_garbage_incinerator_8017DB7C(Task* task)
{
    task->msgTable = D_shelter_b3_garbage_incinerator_80185594;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    func_shelter_b3_garbage_incinerator_8018108C(0x180, 0, 0);
    D_shelter_b3_garbage_incinerator_801855D8 = taskSpawnFromTable(&D_shelter_b3_garbage_incinerator_80185BA0, 0, 0, 0);
    if (gGameSession->location.loc.room >= 4) {
        taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 0, 0, 0);
    }
    if (gGameSession->location.loc.variant == 2) {
        taskSpawnFromTable(D_actor_342400_8016BFE0, 0, 0, 0);
    }
    task->state = task->state + 1;
}

/// Advances the switch's elapsed-frame counter up to its ready threshold of 61.
///
/// The room controller calls this in state 1. A switch press resets the
/// unsigned halfword counter to zero; calls below 61 increment it once, and
/// values at least 61 are preserved. `task` is unused.
static void _shelterB3GarbageIncineratorAdvanceSwitchTimer(Task* task)
{
    enum { SHELTER_B3_GARBAGE_INCINERATOR_SWITCH_READY_TICKS = 61 };
    // Retained unused storage preserves the original 16-byte stack frame.
    char unusedStackBytes[0x10];

    if (D_shelter_b3_garbage_incinerator_801855DC < SHELTER_B3_GARBAGE_INCINERATOR_SWITCH_READY_TICKS) {
        D_shelter_b3_garbage_incinerator_801855DC++;
    }
}

/// Runs the room controller's current state through its three-entry state
/// table, copied onto the stack before the call.
void func_shelter_b3_garbage_incinerator_8017DC7C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_garbage_incinerator_8017D5C4;
    sp.funcs[task->state](task);
}
