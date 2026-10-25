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
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
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

static void _mineGorgeInitializeRoomTask(Task* task);
static void _mineGorgeIdleRoomTask(Task* task);

static s32 _mineGorgeUseOakBoardMessage(Task* unusedTask, s32 messageId, s32 collectedItemId, s32 unusedArg);
static s32 _mineGorgeResolveRoomEvent(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _mineGorgeCommandMsg(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _mineGorgeHandleEncounterAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32 _mineGorgeSoundMsg(Task* task, s32 messageId, s32 cueId, s32 secondArg);

static void _mineGorgeSetPlayerStatePaused(u8 paused);

static AnimationSet _gMineGorgeAnimation00C98;

static void _mineGorgeSetEnemyWave(s32 wave);

/// CAP command slots and entrance-wave threshold used by the room's event scripts.
enum {
    MINE_GORGE_CAP_OAK_BOARD          = 2,
    MINE_GORGE_CAP_ENCOUNTER          = 4,
    MINE_GORGE_ENCOUNTER_RELEASE_WAVE = 20
};

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

/// Message dispatch for the registered gorge room task.
///
/// Five handlers followed by the unsupported-message sentinel. The task borrows
/// this table while registered; all handlers consume their payloads synchronously.
static TaskMessageEntry _gMineGorgeRoomMessageTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mineGorgeResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mineGorgeUseOakBoardMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mineGorgeHandleEncounterAction },
    { ROOM_MESSAGE_COMMAND, _mineGorgeCommandMsg },
    { ROOM_MESSAGE_SOUND, _mineGorgeSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless oak-board cutscene task at priority `TASK_PRIORITY_SCENE`, followed by the table end.
///
/// The default-list spawner reads entry zero synchronously. The task uses no
/// descriptor metadata or spawn payload and releases itself on its second tick.
static TaskDesc _gMineGorgeOakBoardCutsceneTaskDescriptors[2] = {
    { .header = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, .callback = _mineGorgeOakBoardCutsceneTask, .data = { .value = 0 } },
    { .header = { .fields = { TASK_DESC_END, 0 } }, .callback = NULL, .data = { .model = NULL } },
};

/// Weapon-bank clip 24 played after the oak-board scene restores the walking view.
///
/// Restarts without a blend and disables grid collision until scripted control
/// ends. EVS overwrites the bank selector in its copy; the clip's visual role
/// is unproven. The request is borrowed only through dispatch.
static AnimationPlayRequest _gMineGorgeOakBoardAnimation24 = {
    .source               = { .index = 1 },
    .animationId          = 24,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Equipped-weapon idle pose shared by the oak-board and encounter scenes.
///
/// Blends into clip 1 over ten normal-rate frames with grid collision disabled.
/// EVS resolves the equipped bank in a copy, ignoring the stored selector.
static AnimationPlayRequest _gMineGorgeCutsceneIdleAnimation = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Normal oak-board placement scene, paced by the room's CAP command 2.
///
/// Resumes the held player state tick, reveals the display, hides then shows the
/// player model and restores the walking view before ending scripted control.
/// END terminates the interpreter; keep the room's script and CAP resources loaded.
static EvsCommand _gMineGorgeOakBoardScript[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = MINE_GORGE_CAP_OAK_BOARD }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _mineGorgeSetPlayerStatePaused }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineGorgeCutsceneIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 9) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineGorgeOakBoardAnimation24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Oak-board skip path that fades around view restoration and scene cleanup.
///
/// Restores normal player control after the fade returns. The board's collection
/// bit and room-state change belong to the spawning task and remain committed.
/// END terminates the interpreter; the room owns this script through completion.
static EvsCommand _gMineGorgeOakBoardSkipScript[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Two words installed at the player's weapon-bank extension: encounter clip and NULL.
///
/// Slots `ANIMATION_BANK_BASE_SET_COUNT` and the following slot receive these words;
/// NULL is copied storage, not a terminator. The clip data stays borrowed while played.
static AnimationSet* _gMineGorgeEncounterAnimationSets[2] = {
    &_gMineGorgeAnimation00C98,
    NULL,
};

/// Installs both encounter-extension words in the equipped player animation bank.
///
/// The request and source table are read synchronously; the copied clip remains
/// borrowed from the loaded gorge. No adjacent request or script bytes are copied.
static AnimationBankCopyRequest _gMineGorgeEncounterAnimationBankCopy = {
    .sets  = _gMineGorgeEncounterAnimationSets,
    .count = ARRAY_SIZE(_gMineGorgeEncounterAnimationSets)
};

/// Player encounter clip installed in the first weapon-bank extension slot.
///
/// Blends for seven normal-rate frames with grid collision enabled. The encounter
/// script plays it at half rate before engaging battle; EVS replaces the selector.
static AnimationPlayRequest _gMineGorgeEncounterAnimation = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 7,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// Unreferenced playback request for bank 1, clip 48; its intended role is unproven.
///
/// Retained storage requests a seven-frame blend with grid collision enabled.
/// The encounter extension installs NULL at this clip index; no script plays it.
static AnimationPlayRequest _gMineGorgeUnusedAnimation48 = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 7,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// One-time bat-wave entrance scene, paced by CAP command 4.
///
/// Installs the room clip, holds equipped idle, releases wave 20, plays the room
/// clip at half rate and engages battle before restoring player control.
/// No skip script is supplied. END terminates playback; keep the gorge loaded.
static EvsCommand _gMineGorgeEncounterScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = MINE_GORGE_CAP_ENCOUNTER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMineGorgeEncounterAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineGorgeCutsceneIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 7) }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineGorgeSetEnemyWave }, { .value = MINE_GORGE_ENCOUNTER_RELEASE_WAVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_SET_RATE }, { .value = ANIMATION_RATE_ONE / 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineGorgeEncounterAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/room/room_event_trigger_hit.inc.c"

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
            if (_roomEventTriggerHit() != 0) {
                gameFlagSetNibble(GAME_FLAG_MINE_GORGE_TRIGGER_EVENT_DONE, 1);
                taskSpawnFromTableOnDefaultList(_gMineGorgeOakBoardCutsceneTaskDescriptors, 0, 0, 0);
                gGameSession->location.roomState = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MINE_GORGE_OAK_BOARD_EVENT_ROOM);
                gGameSession->hideHud            = (gGameSession->roomObjsDirty = 1);
                gGameSession->eventState         = 1;
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

    if (actionId == MINE_GORGE_ACTION_START_ENCOUNTER && gameFlagGetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN) == 0 && gGameSession->location.variant == actionId) {
        gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN, 1);
        evsStartScript(_gMineGorgeEncounterScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
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
        gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
        SetDispMask(0);
        gGameSession->hideHud = 1;
        evsStartScriptWithSkip(_gMineGorgeOakBoardScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gMineGorgeOakBoardSkipScript);
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
    gPlayerActorStateTickHeld = paused;
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

    task->msgTable = _gMineGorgeRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.variant == MINE_GORGE_ENCOUNTER_VARIANT) && (gameFlagGetNibble(GAME_FLAG_MINE_GORGE_CUTSCENE_SEEN) != 0)) {
        gSceneCombatState.actor03700Wave = MINE_GORGE_ENCOUNTER_RESTORED_WAVE;
    }
    // Latch the one-time story event before its idle-only playback request.
    if ((gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == MINE_GORGE_POWER_PANEL_READY) && (gameFlagGetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE) == MINE_GORGE_REFUGE_EVENT_UNSEEN)) {
        gameFlagSetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE, MINE_GORGE_REFUGE_EVENT_STARTED);
        gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, MINE_GORGE_CAVERN_DOOR_UNPOWERED);
        capSpawnEventIfIdle(MINE_GORGE_REFUGE_EVENT_COMMAND, CAP_EVENT_NO_FLAGS);
    }
    task->state               = task->state + 1;
    gStageCountdownMusicEntry = MINE_GORGE_COUNTDOWN_MUSIC_ENTRY;
}

/// Keeps the installed room task available for messages between entry and teardown.
static void _mineGorgeIdleRoomTask(Task* task)
{
}

/// Gorge room-task states: 0 initialize, 1 receive messages, 2 release.
///
/// Copied by value before dispatch; the live task state must be in 0..2.
static const TaskFuncTable3 _gMineGorgeRoomStateHandlers = {
    .funcs = { _mineGorgeInitializeRoomTask, _mineGorgeIdleRoomTask, taskKill }
};

void mineGorgeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gMineGorgeRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
