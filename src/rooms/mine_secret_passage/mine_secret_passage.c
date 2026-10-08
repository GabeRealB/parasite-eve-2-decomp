#include "rooms/mine_secret_passage.h"

#include "types.h"

#include "mine_secret_passage_private.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
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
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"

/// Staging save location this room's warp handler latches: area /
/// `field_4` / `field_1` take the three bytes the outgoing location carries.
extern RoomEventMsg D_mine_secret_passage_80183448;

static void _mineSecretPassageInitRoomTask(Task* task);
static void _mineSecretPassageTryIntroEvent(Task* task);
static void _mineSecretPassageIdleRoomTask(Task* unusedTask);

/// State handlers of the room task `mineSecretPassageRoomTask` drives:
/// set-up, the one-shot state, the idle state and `taskKill`.
static const TaskFuncTable4 D_mine_secret_passage_8017D5C4 = {
    _mineSecretPassageInitRoomTask,
    _mineSecretPassageTryIntroEvent,
    _mineSecretPassageIdleRoomTask,
    taskKill,
};

RoomEventMsg D_mine_secret_passage_80183448;

/// Runs the room's save sequence. State 0 asks for the caption, state 1 waits
/// for it and drops the periscope overlay, state 2 takes the confirm key or
/// backs out, state 3 counts the armed-shot window down before raising the PE
/// prompt, state 4 raises the helper task 0x31 and queues the sound event,
/// state 5 waits for that voice, and state 6 - the commit - copies the staged
/// location into `gMcSaveData` and reloads. Every state but the commit advances
/// through the shared `advance` tail; a confirmed cancel stops without it.
void func_mine_secret_passage_8017D60C(Task* arg0)
{
    s16 temp_v0;

    switch (arg0->state) {
        case 0:
            capRunCommand(2, CAP_PLAYBACK_IN_PLACE);
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
            D_mine_secret_passage_80183440.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_mine_secret_passage_80183440.fade.phase      = SCREEN_FADE_RUNNING;
            D_mine_secret_passage_80183440.fade.rampFrames = 0x1E;
            taskSpawn(1, 0x31, 0, &D_mine_secret_passage_80183440.fade);
            sndEvtRequestScriptStart(SOUND_MINE_SECRET_PASSAGE_EXIT_TRANSIT, 0, 0);
            arg0->state++;
            break;
        case 5:
            if (sndScriptHasActiveId(SOUND_MINE_SECRET_PASSAGE_EXIT_TRANSIT) != 0) {
                break;
            }
            arg0->state++;
            break;
        case 6:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_mine_secret_passage_80183448.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_mine_secret_passage_80183448.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_mine_secret_passage_80183448.areaId)[1];
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(arg0);
            break;
    }
}

s32 mineSecretPassageRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Handler id 0x13EE of the room's `TaskMessageEntry` table
/// `D_mine_secret_passage_80180E8C`: copies the
/// requested `RoomEventMsg` to `dst` and forwards both to `mapShelterRoomVariantResolve`. A
/// area-9 request latches the outgoing location's three bytes into the room's
/// staging save location and starts the cutscene task; `queryOnly` set only
/// suppresses that side effect. Returns 2 for a area-9 request and 1 for
/// every other one.
s32 func_mine_secret_passage_8017D7CC(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    mapShelterRoomVariantResolve(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_B1_ELEVATOR_HALL) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_mine_secret_passage_80183448.warp              = (u8)dst->areaId;
            D_mine_secret_passage_80183448.field_4           = dst->warp;
            ((u8*)&D_mine_secret_passage_80183448.areaId)[1] = dst->room;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&D_mine_secret_passage_80180EBC, 0, 0, 0);
        }
        return 2;
    }
    return 1;
}

s32 mineSecretPassageIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

s32 mineSecretPassageIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

s32 mineSecretPassageHandleSoundMessage(Task* task, s32 messageId, s32 cueId, s32 unusedArg)
{
    enum { MINE_SECRET_PASSAGE_SOUND_CUE_CONFIRM = 3 };

    if (cueId == MINE_SECRET_PASSAGE_SOUND_CUE_CONFIRM) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
    }
    return 0;
}

/// Registers the passage's room task and selects its countdown-music entry.
///
/// State 0 borrows the loaded room message table and publishes the live task
/// in `GAME_TASK_SLOT_ROOM`, then advances to the one-shot event state.
/// The stage music task later uses entry 1 of the Shelter countdown table.
static void _mineSecretPassageInitRoomTask(Task* task)
{
    enum { MINE_SECRET_PASSAGE_COUNTDOWN_MUSIC_ENTRY = 1 };

    task->msgTable = D_mine_secret_passage_80180E8C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
    gStageSceneMusicEntry = MINE_SECRET_PASSAGE_COUNTDOWN_MUSIC_ENTRY;
}

/// Attempts the passage's one-time CAP event and advances to idle.
///
/// State 1 marks an unseen event before requesting command 3 with actors paused.
/// Busy CAP playback or allocation failure still consumes the attempt; this
/// state does not wait or retry. The loaded command's resources must stay live
/// through any playback it starts.
static void _mineSecretPassageTryIntroEvent(Task* task)
{
    enum {
        MINE_SECRET_PASSAGE_INTRO_UNSEEN  = 0,
        MINE_SECRET_PASSAGE_INTRO_SEEN    = 1,
        MINE_SECRET_PASSAGE_INTRO_COMMAND = 3,
    };

    // Consume the one-shot gate before the idle-only spawn attempt.
    if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_INTRO_SEEN) == MINE_SECRET_PASSAGE_INTRO_UNSEEN) {
        gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_INTRO_SEEN, MINE_SECRET_PASSAGE_INTRO_SEEN);
        capSpawnEventIfIdle(MINE_SECRET_PASSAGE_INTRO_COMMAND, CAP_EVENT_PAUSE_ACTORS);
    }
    task->state++;
}

/// Keeps the registered room task alive in state 2 without changing it.
static void _mineSecretPassageIdleRoomTask(Task* unusedTask)
{
}

void mineSecretPassageRoomTask(Task* task)
{
    TaskFuncTable4 stateHandlers = D_mine_secret_passage_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
