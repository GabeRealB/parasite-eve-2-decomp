#include "rooms/mine_gorge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

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

static void _mineGorgeOakBoardCutsceneTask(Task* task);

/// The room's message table, installed by the room task's first state.
extern TaskMessageEntry D_mine_gorge_8017E280[];

/// The cutscene task `_mineGorgeUseOakBoardMessage` spawns: one descriptor and a
/// terminator.
extern TaskDesc D_mine_gorge_8017E2B0[];

extern EvsCommand D_mine_gorge_8017E2F0[];
extern EvsCommand D_mine_gorge_8017E500[];
extern EvsCommand D_mine_gorge_8017E610[];

static void _mineGorgeInitializeRoomTask(Task* task);
static void _mineGorgeIdleRoomTask(Task* task);

static s32 _mineGorgeUseOakBoardMessage(Task* unusedTask, s32 messageId, s32 collectedItemId, s32 unusedArg);
static s32 _mineGorgeResolveRoomEvent(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _mineGorgeCommandMsg(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _mineGorgeHandleEncounterAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32 _mineGorgeSoundMsg(Task* task, s32 messageId, s32 cueId, s32 secondArg);

static void _mineGorgeSetPlayerStatePaused(u8 paused);

static AnimationSet _gMineGorgeAnimation00C98;

extern AnimationPlayRequest     D_mine_gorge_8017E2DC;
extern AnimationPlayRequest     D_mine_gorge_8017E5E8;
extern AnimationBankCopyRequest D_mine_gorge_8017E5E0;
static void                     _mineGorgeSetEnemyWave(s32 wave);

static AnimationPackedPose _gMineGorgeAnimation00C98Bank1[10] = {
#include "assets/mine_gorge_animation_00C98_bank1.inc"
};

static AnimationPackedRotation _gMineGorgeAnimation00C98Bank4[95] = {
#include "assets/mine_gorge_animation_00C98_bank4.inc"
};

static AnimationRecord _gMineGorgeAnimation00C98Records[139] = {
#include "assets/mine_gorge_animation_00C98_records.inc"
};

static u16 _gMineGorgeAnimation00C98Indices[20] = {
#include "assets/mine_gorge_animation_00C98_indices.inc"

};

static AnimationSet _gMineGorgeAnimation00C98 = {
    _gMineGorgeAnimation00C98Records,
    _gMineGorgeAnimation00C98Indices,
    { NULL, _gMineGorgeAnimation00C98Bank1, NULL, NULL, _gMineGorgeAnimation00C98Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_mine_gorge_8017E280[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mineGorgeResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mineGorgeUseOakBoardMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mineGorgeHandleEncounterAction },
    { ROOM_MESSAGE_COMMAND, _mineGorgeCommandMsg },
    { ROOM_MESSAGE_SOUND, _mineGorgeSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_mine_gorge_8017E2B0[2] = {
    { { { TASK_BODY_NONE, 32 } }, _mineGorgeOakBoardCutsceneTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationPlayRequest D_mine_gorge_8017E2C8 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mine_gorge_8017E2DC = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_mine_gorge_8017E2F0[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _mineGorgeSetPlayerStatePaused }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54050009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_gorge_8017E500[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationSet* D_mine_gorge_8017E5D8[2] = {
    &_gMineGorgeAnimation00C98,
    NULL,
};

AnimationBankCopyRequest D_mine_gorge_8017E5E0 = { { .sets = D_mine_gorge_8017E5D8 }, ARRAY_SIZE(D_mine_gorge_8017E5D8) };

AnimationPlayRequest D_mine_gorge_8017E5E8 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_mine_gorge_8017E5FC = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_ENABLE };

EvsCommand D_mine_gorge_8017E610[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_gorge_8017E5E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54050007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineGorgeSetEnemyWave }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E5E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Tests whether the live action-trigger list contains a latched room event.
///
/// Returns 1 for a room action with event ID 255 and nonzero hit, or 0 when
/// none exists, including an empty list. Borrows NULL-terminated collision
/// records without changing the links or consuming the hit.
static inline s32 _mineGorgeRoomTriggerHit(void)
{
    const WorldCollisionTrigger* trigger;

    for (trigger = Gp_PendingObj4C; trigger != NULL; trigger = trigger->next) {
        if (trigger->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && trigger->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && trigger->hit != 0) {
            return 1;
        }
    }
    return 0;
}

/// Handles oak-board use at the latched gorge action trigger.
///
/// For `ROOM_MESSAGE_USE_KEY_ITEM`, accepts only the oak-board collection ID
/// while its event flag is clear and a room-event trigger is hit. Starts the
/// board cutscene, selects room 2 in live and saved state and hides the HUD.
/// Returns the used-notice reply on acceptance, otherwise the refused reply.
/// The task, message ID and second payload are unused; allocation failure does
/// not undo the event flag or location changes.
static s32 _mineGorgeUseOakBoardMessage(Task* unusedTask, s32 messageId, s32 collectedItemId, s32 unusedArg)
{
    enum {
        MINE_GORGE_OAK_BOARD_EVENT_ROOM = 2,
    };
    if (collectedItemId == INVENTORY_COLLECTION_ID_OAK_BOARD) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_GORGE_TRIGGER_EVENT_DONE) == 0) {
            if (_mineGorgeRoomTriggerHit() != 0) {
                gameFlagSetNibble(GAME_FLAG_MINE_GORGE_TRIGGER_EVENT_DONE, 1);
                taskSpawnFromTableOnDefaultList(D_mine_gorge_8017E2B0, 0, 0, 0);
                gGameSession->location.loc.room = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = MINE_GORGE_OAK_BOARD_EVENT_ROOM);
                gGameSession->hideHud           = (gGameSession->roomObjsDirty = 1);
                gGameSession->eventState        = 1;
                return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
            }
        }
    }
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves Mine/Shelter room variants and blocks the unpowered cavern entrance.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with borrowed eight-byte request and
/// writable reply records, which may alias. Copies the request and resolves
/// the destination room first. An unpowered cavern request returns 0; executing
/// it also writes the optional flag nibble to 2 and queues the blocked-door CAP
/// command. Other destinations and the powered cavern return 1. Retains neither
/// pointer; requires the map overlay and room CAP resources to stay loaded.
static s32 _mineGorgeResolveRoomEvent(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        MINE_GORGE_TRANSITION_BLOCKED       = 0,
        MINE_GORGE_TRANSITION_ALLOWED       = 1,
        MINE_GORGE_BLOCKED_EVENT_FLAG_VALUE = 2,
        MINE_GORGE_UNPOWERED_CAVERN_COMMAND = 3,
    };
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId != GAME_AREA_MINE_CAVERN) {
        return MINE_GORGE_TRANSITION_ALLOWED;
    }
    if (gameFlagGetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED) != 0) {
        return MINE_GORGE_TRANSITION_ALLOWED;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return MINE_GORGE_TRANSITION_BLOCKED;
    }
    gameFlagSetNibbleIfPresent(request->flagId, MINE_GORGE_BLOCKED_EVENT_FLAG_VALUE);
    capRunCommandWithTransition(MINE_GORGE_UNPOWERED_CAVERN_COMMAND);
    return MINE_GORGE_TRANSITION_BLOCKED;
}

/// Ignores every `ROOM_MESSAGE_COMMAND` request and returns zero.
static s32 _mineGorgeCommandMsg(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Starts the gorge encounter script once from its direction-action point.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`; borrows the request only during
/// dispatch. Action 1 in room variant 1 raises the encounter-seen flag and starts
/// the event script when that flag is clear. Other requests do nothing. Ignores
/// the remaining arguments and returns zero; requires loaded script resources.
static s32 _mineGorgeHandleEncounterAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        MINE_GORGE_ACTION_START_ENCOUNTER = 1,
    };
    u8 actionId = request->actionId;

    if (actionId == MINE_GORGE_ACTION_START_ENCOUNTER && gameFlagGetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN) == 0 && gGameSession->location.loc.variant == actionId) {
        gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN, 1);
        evsStartScript(D_mine_gorge_8017E610, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Maps room sound cue 10 to Mine Gorge sound script 10 and returns zero.
///
/// Handles `ROOM_MESSAGE_SOUND`; all other cues and the second payload are
/// ignored. Queues the script with no pan offset or attenuation and retains no arguments.
static s32 _mineGorgeSoundMsg(Task* task, s32 messageId, s32 cueId, s32 secondArg)
{
    enum { MINE_GORGE_SOUND_CUE_SCRIPT_0A = 0xA };

    if (cueId == MINE_GORGE_SOUND_CUE_SCRIPT_0A) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 0) | cueId, 0, 0);
    }
    return 0;
}

/// Starts the oak-board event script and consumes its collection bit on the next tick.
///
/// State 0 pauses the player state tick, hides the display and queues the normal
/// and skip scripts. Every later state releases this task and clears the oak-board
/// collection bit. No body or spawn payload is used. Requires the room scripts
/// loaded; the scripts resume player state and own the display transition.
/// The final state increment remains after task release as in the original.
static void _mineGorgeOakBoardCutsceneTask(Task* task)
{
    enum {
        MINE_GORGE_OAK_BOARD_CUTSCENE_START = 0,
    };
    if (task->state == MINE_GORGE_OAK_BOARD_CUTSCENE_START) {
        D_80115768 = 1;
        SetDispMask(0);
        gGameSession->hideHud = 1;
        evsStartScriptWithSkip(D_mine_gorge_8017E2F0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mine_gorge_8017E500);
    } else {
        taskKill(task);
        inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_OAK_BOARD);
    }
    task->state = task->state + 1;
}

/// Sets the player control-mode/timer hold used during the room's scripted scene.
///
/// Event-script byte callback: zero resumes the state tick and every nonzero
/// value pauses it. Movement, collision and rendering outside that tick still
/// run. Stores the byte unchanged; the room's opening script supplies zero.
static void _mineGorgeSetPlayerStatePaused(u8 paused)
{
    D_80115768 = paused;
}

/// Sets the entrance-wave threshold for the room's actor-03700 enemy group.
///
/// Event-script word callback: keeps only the low signed byte in the scene's
/// wave counter. Waiting enemies enter when this counter meets their placement
/// threshold. The encounter script supplies 20; this does not acquire battle
/// references or engage the battle itself.
static void _mineGorgeSetEnemyWave(s32 wave)
{
    gSceneCombatState.actor03700Wave = wave;
}

/// Registers the gorge room and restores its encounter and refuge-story progress.
///
/// State 0 publishes the room message receiver and advances to idle. In variant
/// 1 a previously seen encounter restores the actor-03700 wave. Once the power
/// panel reaches stage 2, an unseen refuge event is marked before attempting
/// CAP command 8 and the cavern door's powered flag is cleared. Busy CAP or
/// allocation failure consumes that attempt. Requires loaded room/CAP resources.
static void _mineGorgeInitializeRoomTask(Task* task)
{
    enum {
        MINE_GORGE_ENCOUNTER_VARIANT       = 1,
        MINE_GORGE_ENCOUNTER_RESTORED_WAVE = 21,
        MINE_GORGE_POWER_PANEL_READY       = 2,
        MINE_GORGE_REFUGE_EVENT_UNSEEN     = 0,
        MINE_GORGE_REFUGE_EVENT_STARTED    = 1,
        MINE_GORGE_CAVERN_DOOR_UNPOWERED   = 0,
        MINE_GORGE_REFUGE_EVENT_COMMAND    = 8,
        MINE_GORGE_COUNTDOWN_MUSIC_ENTRY   = 1
    };

    task->msgTable = D_mine_gorge_8017E280;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == MINE_GORGE_ENCOUNTER_VARIANT) && (gameFlagGetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN) != 0)) {
        gSceneCombatState.actor03700Wave = MINE_GORGE_ENCOUNTER_RESTORED_WAVE;
    }
    // Latch the one-time story event before its idle-only playback request.
    if ((gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == MINE_GORGE_POWER_PANEL_READY) && (gameFlagGetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE) == MINE_GORGE_REFUGE_EVENT_UNSEEN)) {
        gameFlagSetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE, MINE_GORGE_REFUGE_EVENT_STARTED);
        gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, MINE_GORGE_CAVERN_DOOR_UNPOWERED);
        capSpawnEventIfIdle(MINE_GORGE_REFUGE_EVENT_COMMAND, CAP_EVENT_NO_FLAGS);
    }
    task->state           = task->state + 1;
    gStageSceneMusicEntry = MINE_GORGE_COUNTDOWN_MUSIC_ENTRY;
}

/// Keeps the installed room task available for messages between entry and teardown.
static void _mineGorgeIdleRoomTask(Task* task)
{
}

/// State handlers of the room task `mineGorgeRoomTask` runs: the room's
/// setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_mine_gorge_8017D5C4 = {
    { _mineGorgeInitializeRoomTask, _mineGorgeIdleRoomTask, taskKill }
};

void mineGorgeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_mine_gorge_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
