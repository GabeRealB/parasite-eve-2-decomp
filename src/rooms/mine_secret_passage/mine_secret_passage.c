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

#include "../../shared/room_variants.h"

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

/// Starts the persistent subtractive departure fade and exit-transit sound.
///
/// The room's fade storage must stay live while the resident fade task uses it.
static inline void _mineSecretPassageStartDepartureFade(void)
{
    enum {
        MINE_SECRET_PASSAGE_DEPARTURE_FADE_FRAMES = 30,
        MINE_SECRET_PASSAGE_FADE_TASK_BANK        = 1,
        MINE_SECRET_PASSAGE_FADE_TASK_SLOT        = 0x31
    };

    D_mine_secret_passage_80183440.fade.blend      = SCREEN_FADE_SUBTRACT;
    D_mine_secret_passage_80183440.fade.phase      = SCREEN_FADE_RUNNING;
    D_mine_secret_passage_80183440.fade.rampFrames = MINE_SECRET_PASSAGE_DEPARTURE_FADE_FRAMES;
    taskSpawn(MINE_SECRET_PASSAGE_FADE_TASK_BANK, MINE_SECRET_PASSAGE_FADE_TASK_SLOT, 0, &D_mine_secret_passage_80183440.fade);
    sndEvtRequestScriptStart(SOUND_MINE_SECRET_PASSAGE_EXIT_TRANSIT, 0, 0);
}

void mineSecretPassageDepartureTask(Task* task)
{
    enum {
        MINE_SECRET_PASSAGE_DEPARTURE_PROMPT       = 0,
        MINE_SECRET_PASSAGE_DEPARTURE_WAIT_PROMPT  = 1,
        MINE_SECRET_PASSAGE_DEPARTURE_CONFIRM      = 2,
        MINE_SECRET_PASSAGE_DEPARTURE_WAIT_ESCAPE  = 3,
        MINE_SECRET_PASSAGE_DEPARTURE_FADE         = 4,
        MINE_SECRET_PASSAGE_DEPARTURE_WAIT_SOUND   = 5,
        MINE_SECRET_PASSAGE_DEPARTURE_RELOAD       = 6,
        MINE_SECRET_PASSAGE_DEPARTURE_COMMAND      = 2,
        MINE_SECRET_PASSAGE_DEPARTURE_CONFIRM_KEY  = 10,
        MINE_SECRET_PASSAGE_DEPARTURE_ESCAPE_DELAY = 3,
        MINE_SECRET_PASSAGE_RELOAD_SPRITE_VARIANT  = 1,
        MINE_SECRET_PASSAGE_STAGED_ROOM_BYTE       = 1
    };
    s16 ticksLeft;

    switch (task->state) {
        case MINE_SECRET_PASSAGE_DEPARTURE_PROMPT:
            capRunCommand(MINE_SECRET_PASSAGE_DEPARTURE_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MINE_SECRET_PASSAGE_DEPARTURE_WAIT_PROMPT:
            if (capIsBusy() != 0) {
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            task->state++;
            break;
        case MINE_SECRET_PASSAGE_DEPARTURE_CONFIRM:
            if (capGetVariantKey() != MINE_SECRET_PASSAGE_DEPARTURE_CONFIRM_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->killCountdown            = MINE_SECRET_PASSAGE_DEPARTURE_ESCAPE_DELAY;
            task->state++;
            break;
        case MINE_SECRET_PASSAGE_DEPARTURE_WAIT_ESCAPE:
            ticksLeft           = (u16)task->killCountdown - 1;
            task->killCountdown = ticksLeft;
            if (ticksLeft != 0) {
                break;
            }
            sceneQueueBattleEscapeResult();
            task->state++;
            break;
        case MINE_SECRET_PASSAGE_DEPARTURE_FADE:
            _mineSecretPassageStartDepartureFade();
            task->state++;
            break;
        case MINE_SECRET_PASSAGE_DEPARTURE_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_MINE_SECRET_PASSAGE_EXIT_TRANSIT) != 0) {
                break;
            }
            task->state++;
            break;
        case MINE_SECRET_PASSAGE_DEPARTURE_RELOAD:
            // Commit the staged destination only after the exit-transit sound ends.
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = MINE_SECRET_PASSAGE_RELOAD_SPRITE_VARIANT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_mine_secret_passage_80183448.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_mine_secret_passage_80183448.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_mine_secret_passage_80183448.areaId)[MINE_SECRET_PASSAGE_STAGED_ROOM_BYTE];
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(task);
            break;
    }
}

s32 mineSecretPassageRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 mineSecretPassageResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { MINE_SECRET_PASSAGE_STAGED_ROOM_BYTE = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B1_ELEVATOR_HALL) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            // Latch the resolved destination for the asynchronous confirmation task.
            D_mine_secret_passage_80183448.warp                                                 = (u8)reply->areaId;
            D_mine_secret_passage_80183448.field_4                                              = reply->warp;
            ((u8*)&D_mine_secret_passage_80183448.areaId)[MINE_SECRET_PASSAGE_STAGED_ROOM_BYTE] = reply->room;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&D_mine_secret_passage_80180EBC, 0, 0, 0);
        }
        return ROOM_VARIANT_TRANSITION_HANDLED;
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
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
