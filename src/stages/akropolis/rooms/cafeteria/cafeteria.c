#include "stages/akropolis/rooms/cafeteria.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"
#include "cafeteria_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "actors/actor_202900.h"

#include "actors/actor_210600.h"

#include "actors/actor_310600.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/game_debug.h"
#include "gameplay/gameflag.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/event_flags.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

/// The twin-trail endpoint table is defined here and read by cafeteria_effects.c.
#define ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../../../shared/room/room_visual_effects.h"

#define ROOM_PART declarations_cafeteria
#include "assets/rooms/akropolis/cafeteria/declarations.inc"

static void       _akropolisCafeteriaUpdatePlayerDebugDisplay(Task* unusedTask);
static void       _akropolisCafeteriaInitializeRoomTask(Task* task);
static inline s32 _akropolisCafeteriaResolvePatioRoom(const RoomEventMsg* request, RoomEventMsg* reply);

/// The battle's progress in this visit.
enum {
    SCENE_NOT_STARTED = 0,
    SCENE_ACTIVE      = 1,
    SCENE_RELEASED    = 2, // The battle is over and its rewards given
};

/// The collision grid's cells.
enum {
    COLLISION_CELLS_X = 3,
    COLLISION_CELLS_Z = 3,
};

/// Room task states: initialize, the player debug display, release.
static const TaskFuncTable3 _gAkropolisCafeteriaRoomStateHandlers = {
    { _akropolisCafeteriaInitializeRoomTask, _akropolisCafeteriaUpdatePlayerDebugDisplay, taskKill },
};

/// The player's label in the debug display.
static const char _gAkropolisCafeteriaPlayerDebugLabel[12] = "Player";

static AnimationSet _gAkropolisCafeteriaFollowUpPlayerClip;
static s32          _akropolisCafeteriaResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32          _akropolisCafeteriaRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32          _akropolisCafeteriaHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32          _akropolisCafeteriaHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32          _akropolisCafeteriaHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg);
static void         _akropolisCafeteriaBattleSceneTask(Task* task);
static void         _akropolisCafeteriaFollowUpSceneTask(Task* task);
static void         _akropolisCafeteriaSettleStrangerTask(Task* task);
static void         _akropolisCafeteriaSetPlayerSurface(s32 surfaceClass);
static void         _akropolisCafeteriaCancelPeEffects(void);
static void         _akropolisCafeteriaCopyPostBattleVram(void);
static void         _akropolisCafeteriaPreparePostBattleScene(void);

/// Messages of the room task.
static TaskMessageEntry _gAkropolisCafeteriaRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisCafeteriaResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisCafeteriaHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _akropolisCafeteriaHandleRoomCommand },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisCafeteriaRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _akropolisCafeteriaHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Scene tasks: the battle (0), the follow-up (1) and the Stranger's settling (2).

TaskDesc gAkropolisCafeteriaSceneTasks[4] = {
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
      .callback = _akropolisCafeteriaBattleSceneTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
      .callback = _akropolisCafeteriaFollowUpSceneTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
      .callback = _akropolisCafeteriaSettleStrangerTask,
      .data     = { .value = 0 } },
    { .header = { .fields = { .flags = TASK_DESC_END, .priority = 0 } }, .callback = NULL, .data = { .model = NULL } },
};

#define ROOM_PART _gAkropolisCafeteriaPlayerAnimation1
#include "assets/rooms/akropolis/cafeteria/events.inc"

/// The player's clip in the opening, copied into the equipped weapon's bank at slot 47.
static AnimationSet* _gAkropolisCafeteriaOpeningPlayerAnimationSets[1] = {
    &gAkropolisCafeteriaPlayerOpeningClip,
};

#define ROOM_PART _gAkropolisCafeteriaOpeningPlayerBankCopy
#include "assets/rooms/akropolis/cafeteria/events.inc"

/// The player's clips in the follow-up, copied into the equipped weapon's bank from slot 47.
static AnimationSet* _gAkropolisCafeteriaFollowUpPlayerAnimationSets[4] = {
    &gAkropolisCafeteriaPlayerFollowUpStartClip,
    &_gAkropolisCafeteriaFollowUpPlayerClip,
    &gAkropolisCafeteriaPlayerRupertTalkClip,
    &gAkropolisCafeteriaPlayerFollowUpEndClip,
};

#define ROOM_PART _gAkropolisCafeteriaFollowUpPlayerBankCopy
#include "assets/rooms/akropolis/cafeteria/events.inc"

/// The battle's progress in this visit.
static s32 _gAkropolisCafeteriaSceneProgress = SCENE_NOT_STARTED;

/// The VRAM the post-battle scene copies: a page of the effect sheet, and a palette row.
static RECT _gAkropolisCafeteriaPostBattleImageSource = { .x = 704, .y = 0, .w = 64, .h = 256 };

static RECT _gAkropolisCafeteriaPostBattleRowSource = { .x = 0, .y = 271, .w = 256, .h = 1 };

/// Movie tasks: the launcher (0), the blackout (1, never spawned) and the playback (2).
TaskDesc gAkropolisCafeteriaMovieTasks[AKROPOLIS_CAFETERIA_MOVIE_TASK_COUNT] = {
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
      .callback = akropolisCafeteriaStartMovieTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
      .callback = akropolisCafeteriaBlackoutTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
      .callback = akropolisCafeteriaPlayMovieTask,
      .data     = { .value = 0 } },
};

#include "assets/rooms/akropolis/cafeteria/clips.inc"

TaskMessageEntry gAkropolisCafeteriaRoomEffectMessages[2] = {
    { AKROPOLIS_CAFETERIA_MESSAGE_SET_PUFF_ENABLED, akropolisCafeteriaSetPuffEnabled },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 gAkropolisCafeteriaPuffEnabled = 0;

#include "assets/rooms/akropolis/cafeteria/wandering_creature.inc"

#include "../../../../shared/room/room_visual_effects_trail_data.inc.c"
#undef ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS

#include "assets/rooms/akropolis/cafeteria/loose_prop_1.inc"

#include "assets/rooms/akropolis/cafeteria/loose_prop_2.inc"

#include "assets/rooms/akropolis/cafeteria/loose_prop_3.inc"

#include "assets/rooms/akropolis/cafeteria/loose_prop_4.inc"
#include "assets/rooms/akropolis/cafeteria/header.inc"
#include "assets/rooms/akropolis/cafeteria/geometry.inc"
#include "assets/rooms/akropolis/cafeteria/view_boundaries.inc"
#include "assets/rooms/akropolis/cafeteria/action_triggers.inc"
#include "assets/rooms/akropolis/cafeteria/occluders.inc"
#include "assets/rooms/akropolis/cafeteria/placements.inc"
#define ROOM_PART gAkropolisCafeteriaPointLights
#include "assets/rooms/akropolis/cafeteria/lights.inc"

STATIC_ASSERT(ARRAY_SIZE(_gAkropolisCafeteriaCollisionGridTable) == COLLISION_CELLS_X * COLLISION_CELLS_Z, cafeteriaCollisionCellTableSize);

/// Shows the player in the debug display.
static void _akropolisCafeteriaUpdatePlayerDebugDisplay(Task* unusedTask)
{
    if (gDisplayState.debugMode != 0) {
        func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), AKROPOLIS_CAFETERIA_PLAYER_DEBUG_X, AKROPOLIS_CAFETERIA_PLAYER_DEBUG_Y, _gAkropolisCafeteriaPlayerDebugLabel);
        func_807245E4(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
    }
}

/// The patio's room state the player arrives in: before or after its SWAT scene, or after the boss.
static inline s32 _akropolisCafeteriaResolvePatioRoom(const RoomEventMsg* request, RoomEventMsg* reply)
{
    s32 destinationArea = request->areaId;

    if (destinationArea == AKROPOLIS_ROOM_ID_PATIO && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) <= AKROPOLIS_PATIO_CUTSCENE_START) {
                reply->roomState = AKROPOLIS_ROOM_STATE_PATIO_PRE_SWAT_CUTSCENE;
            } else {
                reply->roomState = AKROPOLIS_ROOM_STATE_PATIO_POST_SWAT_CUTSCENE;
            }
        } else {
            // The patio's room id, 3, is also its state after the boss.
            reply->roomState = destinationArea;
        }
    }
    return ROOM_EVENT_DEPART;
}

/// The doors: the hallway's is held until MIST is contacted; the patio's is held during the battle, then plays CAP 6
/// until the boss is reported.
static s32 _akropolisCafeteriaResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    s32 destinationArea;

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_HALLWAY && request->warp == AKROPOLIS_CAFETERIA_HALLWAY_EXIT_WARP) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED) {
            return ROOM_EVENT_DEPART;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
            capStartSequenceSlot(AKROPOLIS_CAFETERIA_HALLWAY_EXIT_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        }
        return ROOM_EVENT_STAY;
    }
    destinationArea = request->areaId;
    if (destinationArea == AKROPOLIS_ROOM_ID_PATIO) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
            if (_gAkropolisCafeteriaSceneProgress == SCENE_NOT_STARTED) {
                return _akropolisCafeteriaResolvePatioRoom(request, reply);
            }
            if (_gAkropolisCafeteriaSceneProgress == SCENE_RELEASED) {
                if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                    capStartSequenceSlot(AKROPOLIS_CAFETERIA_PATIO_EXIT_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
                }
            }
            return ROOM_EVENT_STAY;
        }
        // Compared with the patio's room id, 3, which no writer of the flag stores.
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT) == destinationArea && request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT, 2);
        }
        return _akropolisCafeteriaResolvePatioRoom(request, reply);
    }
    return ROOM_EVENT_DEPART;
}

/// Covers the screen in black while the battle's layout is set up.
static inline void _akropolisCafeteriaDrawBattleBlackout(void)
{
    enum {
        OT_INDEX = 10,
        HEIGHT   = 256, // A little taller than the screen
    };
    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->x0 = -DISPLAY_SCREEN_WIDTH / 2;
    tile->y0 = -HEIGHT / 2;
    tile->w  = DISPLAY_SCREEN_WIDTH;
    tile->h  = HEIGHT;
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    addPrim(&gGpuCurrentOt[OT_INDEX], tile);
}

/// The battle: its opening, the room's battle state and layout behind a black screen, the fight, the scene after it
/// and its rewards.
static void _akropolisCafeteriaBattleSceneTask(Task* task)
{
    enum {
        START_INTRO              = 0,
        WAIT_INTRO               = 1,
        SELECT_BATTLE_ROOM       = 2,
        EXIT_PLACED_ACTORS       = 3,
        WAIT_ACTOR_EXIT          = 4,
        RESTORE_VIEW             = 5,
        LOAD_ACTOR_FILE          = 6,
        SPAWN_BATTLE_LAYOUT      = 7,
        START_BATTLE_CONTROLLER  = 8,
        WAIT_BATTLE_SETUP        = 14,
        START_BATTLE_SCRIPT      = 15,
        WAIT_BATTLE_SCRIPT       = 16,
        DELAY_BATTLE_CHECK       = 17,
        WAIT_ENEMY_DEFEAT        = 18,
        START_POST_BATTLE_SCRIPT = 19,
        WAIT_POST_BATTLE_SCRIPT  = 20,
        RELEASE_BATTLE           = 21,
        DELAY_1                  = 22,
        DELAY_2                  = 23,
        DELAY_3                  = 24,
        DELAY_4                  = 25,
        DELAY_5                  = 26,
        WAIT_PLAYER_ANIMATION    = 27,
    };

    u8 fileKeyBytes[4];
    u8 loadArgsBytes[4];
    u8 drawBlackout;

    drawBlackout = 0;
    switch (task->state) {
        case START_INTRO:
            gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            evsStartScriptWithSkip(_gAkropolisCafeteriaOpeningSceneScript, EVENT_SCRIPT_HUD_KEEP, _gAkropolisCafeteriaOpeningSceneSkipScript);
            task->state += 1;
            break;
        case WAIT_INTRO:
            if (gGameSession->eventState != 1) {
                task->state += 1;
            }
            break;
        case SELECT_BATTLE_ROOM:
            drawBlackout                     = 1;
            gGameSession->location.roomState = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_ROOM_STATE_CAFETERIA_BATTLE;
            gGameSession->roomObjsDirty                                                                    = 1;
            task->state                                                                                   += 1;
            break;
        case EXIT_PLACED_ACTORS:
            drawBlackout = 1;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_EXIT_PLACED_ACTORS, 0, 0);
            task->state += 1;
            break;
        case WAIT_ACTOR_EXIT:
            drawBlackout = 1;
            task->state += 1;
            break;
        case RESTORE_VIEW:
            drawBlackout = 1;
            loadingRequestViewGraphicsRestore();
            task->state += 1;
            break;
        case LOAD_ACTOR_FILE:
            drawBlackout = 1;
            // The battle's Stranger package, stage-zero file 210600; byte 1 of the key is ignored by the CD API.
            fileKeyBytes[2]  = 0x15;
            fileKeyBytes[3]  = 0;
            fileKeyBytes[0]  = 0;
            loadArgsBytes[0] = 6;
            loadArgsBytes[1] = CD_COMMAND_LOAD_DEFAULT;
            loadArgsBytes[2] = 4;
            loadArgsBytes[3] = 6;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgsBytes);
            task->state += 1;
            break;
        case SPAWN_BATTLE_LAYOUT:
            drawBlackout = 1;
            if (cdCmdIsIdle()) {
                areaSetPlacementVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, AKROPOLIS_CAFETERIA_BATTLE_VARIANT, AREA_VARIANT_RESET_ALWAYS);
                areaSyncLocationVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
                areaSpawnPlacements(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
                gCapControlFlags &= ~CAP_CONTROL_EVENT_SCRIPT_WAITING_FOR_CUE;
                task->state      += 1;
            }
            break;
        case START_BATTLE_CONTROLLER:
            drawBlackout = 1;
            if (gDisplayState.debugMode != -1) {
                taskSpawnFromTable(gAkropolisCafeteriaMovieTasks, AKROPOLIS_CAFETERIA_MOVIE_TASK_LAUNCH, 0, 0);
            }
            task->state = WAIT_BATTLE_SETUP;
            break;
        case WAIT_BATTLE_SETUP:
            drawBlackout = 1;
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case START_BATTLE_SCRIPT:
            evsStartScript(_gAkropolisCafeteriaBattleIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state += 1;
            break;
        case WAIT_BATTLE_SCRIPT:
        case WAIT_POST_BATTLE_SCRIPT:
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case WAIT_ENEMY_DEFEAT:
            if (taskMessageDispatch(sceneFindPlacedActor(AKROPOLIS_CAFETERIA_STRANGER_ACTOR), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0 && gPlayerStatus.hp > 0 && gAttachmentState.mode != ATTACHMENT_MODE_WHEEL &&
                gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                task->state += 1;
            }
            break;
        case START_POST_BATTLE_SCRIPT:
            displaySetShakeY(0);
            evsStartScriptWithSkip(_gAkropolisCafeteriaPostBattleSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisCafeteriaPostBattleSceneSkipScript);
            task->state += 1;
            break;
        case RELEASE_BATTLE:
            // Release combat before returning control and the next objective.
            sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(AKROPOLIS_CAFETERIA_STRANGER_ACTOR), 0xA);
            gGameSession->flowFlags                       |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            gSceneCombatState.signals.bytes.endDelayFrames = 3;
            _gAkropolisCafeteriaSceneProgress              = SCENE_RELEASED;
            task->state                                   += 1;
            break;
        case DELAY_BATTLE_CHECK:
        case DELAY_1:
        case DELAY_2:
        case DELAY_3:
        case DELAY_4:
        case DELAY_5:
            task->state += 1;
            break;
        case WAIT_PLAYER_ANIMATION:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_AKROPOLIS_CAFETERIA_BOSS_KILLED);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
    }
    if (drawBlackout != 0) {
        _akropolisCafeteriaDrawBattleBlackout();
    }
}

/// Records the follow-up's story progress and leaves for the patio, in its state after the boss.
static inline void _akropolisCafeteriaCommitFollowUpDeparture(Task* task)
{
    enum { FOLLOW_UP_SPRITE_VARIANT = 1 };

    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS, AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED);
    gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
    gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_NEW_SPECIES);
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT, AKROPOLIS_CAFETERIA_BOSS_REPORT_QUEUED);
    areaApplySavedUpdates(gAkropolisCafeteriaFollowUpAreaUpdates);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = AKROPOLIS_CAFETERIA_FOLLOW_UP_SCENE_EVENT;
    gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_AKROPOLIS_CAFETERIA_BOSS_EXAMINED);
    func_800ABFF8();
    func_800AC000();
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_PATIO;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = AKROPOLIS_CAFETERIA_FOLLOW_UP_PATIO_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_ROOM_STATE_PATIO_POST_CAFETERIA_BOSS;
    gDisplayState.spriteVariant                                 = FOLLOW_UP_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// The follow-up: the conversation by Rupert, then, when it is accepted, the scene and the departure for the patio.
static void _akropolisCafeteriaFollowUpSceneTask(Task* task)
{
    enum {
        FOLLOW_UP_CHECK             = 0,
        FOLLOW_UP_WAIT_PRESENTATION = 1,
        FOLLOW_UP_WAIT_CAP          = 2,
        FOLLOW_UP_WAIT_RESUME       = 3,
        FOLLOW_UP_START_SCENE       = 6,
        FOLLOW_UP_WAIT_SCENE        = 7,
        FOLLOW_UP_DEPART            = 8,
        PRESENTATION_ENTER          = 0,
        PRESENTATION_LEAVE          = 1,
        EVENT_RUNNING               = 1
    };
    // Removing this unused reservation changes the original call frame.
    char retainedStackSpace[8];

    switch (task->state) {
        case FOLLOW_UP_CHECK:
            if (areaGetCurrentObjectState(AKROPOLIS_CAFETERIA_FOLLOW_UP_OBJECT) == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, PRESENTATION_ENTER, 0);
                task->state = task->state + 1;
            } else {
                taskKill(task);
            }
            break;

        case FOLLOW_UP_WAIT_PRESENTATION:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                capRunCommandWithTransition(AKROPOLIS_CAFETERIA_FOLLOW_UP_CAP_COMMAND);
                task->state = task->state + 1;
            }
            break;

        case FOLLOW_UP_WAIT_CAP:
            if (capIsBusy() == 0) {
                if (areaGetCurrentObjectState(AKROPOLIS_CAFETERIA_FOLLOW_UP_OBJECT) == 1) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, PRESENTATION_LEAVE, 0);
                    task->state = task->state + 1;
                } else {
                    task->state = FOLLOW_UP_START_SCENE;
                }
            }
            break;

        case FOLLOW_UP_WAIT_RESUME:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;

        case FOLLOW_UP_START_SCENE:
            evsStartScriptWithSkip(_gAkropolisCafeteriaFollowUpSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisCafeteriaFollowUpSceneSkipScript);
            task->state = task->state + 1;
            break;

        case FOLLOW_UP_WAIT_SCENE:
            if (gGameSession->eventState != EVENT_RUNNING) {
                task->state = task->state + 1;
            }
            break;

        case FOLLOW_UP_DEPART:
            _akropolisCafeteriaCommitFollowUpDeparture(task);
            break;
    }
}

/// Moves the shot Stranger one tick: it slides to a stop and falls to the floor.
static inline void _akropolisCafeteriaStepStrangerRecoil(Task* task, GfxCoord* root)
{
    enum {
        FLOOR_Y        = -300,
        GRAVITY        = 5,
        MAX_FALL_SPEED = 20,
    };

    root->coord.t[0] += gAkropolisCafeteriaStrangerRecoilVelocityX;
    root->coord.t[1] += gAkropolisCafeteriaStrangerRecoilVelocityY;
    if (root->coord.t[1] > FLOOR_Y) {
        root->coord.t[1] = FLOOR_Y;
    }
    root->coord.t[2]                           += gAkropolisCafeteriaStrangerRecoilVelocityZ;
    root->composeStamp                          = GRAPHICS_COORD_DIRTY;
    gAkropolisCafeteriaStrangerRecoilVelocityX  = gAkropolisCafeteriaStrangerRecoilVelocityX / 2;
    gAkropolisCafeteriaStrangerRecoilVelocityY += GRAVITY;
    if (gAkropolisCafeteriaStrangerRecoilVelocityY > MAX_FALL_SPEED) {
        gAkropolisCafeteriaStrangerRecoilVelocityY = MAX_FALL_SPEED;
    }
    gAkropolisCafeteriaStrangerRecoilVelocityZ = gAkropolisCafeteriaStrangerRecoilVelocityZ / 2;
    if (gAkropolisCafeteriaStrangerRecoilVelocityX == 0 && gAkropolisCafeteriaStrangerRecoilVelocityZ == 0 &&
        root->coord.t[1] == FLOOR_Y) {
        taskKill(task);
    }
}

/// The Stranger's recoil after the last shot.
static void _akropolisCafeteriaSettleStrangerTask(Task* task)
{
    enum {
        SETTLE_INITIALIZE       = 0,
        SETTLE_MOVE             = 1,
        STRANGER_INITIAL_Y_STEP = -20,
        STRANGER_INITIAL_Z_STEP = -20,
    };
    GfxCoord* root;

    root = sceneFindPlacedActor(AKROPOLIS_CAFETERIA_STRANGER_ACTOR)->extra.tmd->coords;
    switch (task->state) {
        case SETTLE_INITIALIZE:
            // Start the recoil clip before taking over the root's translation.
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(AKROPOLIS_CAFETERIA_STRANGER_ACTOR), ACTOR_MESSAGE_PLACE, &_gAkropolisCafeteriaStrangerRecoilPlacement, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(AKROPOLIS_CAFETERIA_STRANGER_ACTOR), ACTOR_COMMAND_MESSAGE_APPLY, &_gAkropolisCafeteriaStrangerSettleCommand, 0);
            gAkropolisCafeteriaStrangerRecoilVelocityX = 0;
            gAkropolisCafeteriaStrangerRecoilVelocityY = STRANGER_INITIAL_Y_STEP;
            gAkropolisCafeteriaStrangerRecoilVelocityZ = STRANGER_INITIAL_Z_STEP;
            task->state                                = task->state + 1;
            break;

        case SETTLE_MOVE:
            _akropolisCafeteriaStepStrangerRecoil(task, root);
            break;
    }
}

/// No key item can be used here.
static s32 _akropolisCafeteriaRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room command 7, the scientific journal, plays its CAP after the battle, while the journal is in state 0 or 1.
static s32 _akropolisCafeteriaHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    if (command == AKROPOLIS_CAFETERIA_ROOM_COMMAND_JOURNAL) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED || _gAkropolisCafeteriaSceneProgress >= SCENE_RELEASED) {
            if (areaGetCurrentObjectState(AKROPOLIS_CAFETERIA_JOURNAL_OBJECT) == 1 || areaGetCurrentObjectState(AKROPOLIS_CAFETERIA_JOURNAL_OBJECT) == 0) {
                capStartSequenceSlot(AKROPOLIS_CAFETERIA_JOURNAL_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            }
        }
    }
    return 0;
}

/// Room actions: examining the creature after the battle, the dead SWAT member, and the battle itself, once.
static s32 _akropolisCafeteriaHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    if (request->actionId == AKROPOLIS_CAFETERIA_ACTION_FOLLOW_UP) {
        if (_gAkropolisCafeteriaSceneProgress >= SCENE_RELEASED || gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
            taskSpawnFromTable(gAkropolisCafeteriaSceneTasks, AKROPOLIS_CAFETERIA_FOLLOW_UP_TASK_ENTRY, 0, 0);
            return 0;
        }
    }
    if (request->actionId == AKROPOLIS_CAFETERIA_ACTION_DEAD_SWAT) {
        capRunCommandWithTransition(AKROPOLIS_CAFETERIA_DEAD_SWAT_CAP_COMMAND);
    } else if (request->actionId == AKROPOLIS_CAFETERIA_ACTION_START_BATTLE) {
        if (_gAkropolisCafeteriaSceneProgress == SCENE_NOT_STARTED && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_ENTERED_AKROPOLIS) {
            _gAkropolisCafeteriaSceneProgress = SCENE_ACTIVE;
            taskSpawnFromTable(gAkropolisCafeteriaSceneTasks, AKROPOLIS_CAFETERIA_BATTLE_TASK_ENTRY, 0, 0);
        }
    }
    return 0;
}

/// Plays room sound cues 10, the ampoule's container, and 11.
static s32 _akropolisCafeteriaHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg)
{
    switch (cueIndex) {
        case AKROPOLIS_CAFETERIA_SOUND_AMPOULE_CONTAINER_OPEN:
            sndEvtRequestScriptStart(AKROPOLIS_CAFETERIA_SOUND(AKROPOLIS_CAFETERIA_SOUND_AMPOULE_CONTAINER_OPEN), 0, 0);
            break;
        case AKROPOLIS_CAFETERIA_SOUND_CUE_11:
            sndEvtRequestScriptStart(AKROPOLIS_CAFETERIA_SOUND(AKROPOLIS_CAFETERIA_SOUND_CUE_11), 0, 0);
            break;
    }
    return 0;
}

/// Sets the surface class the player's footsteps play, for the event scripts.
static void _akropolisCafeteriaSetPlayerSurface(s32 surfaceClass)
{
    GameActor* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    player->surfaceClass = surfaceClass;
}

/// Ends the player's Parasite Energy effects, for the event scripts.
static void _akropolisCafeteriaCancelPeEffects(void)
{
    roomEffectRequestCancelPe();
}

/// Copies the effect page and palette row the post-battle scene's actor reads, for the event scripts.
static void _akropolisCafeteriaCopyPostBattleVram(void)
{
    enum {
        IMAGE_X = 384,
        IMAGE_Y = 256,
        ROW_X   = 0,
        ROW_Y   = 247,
    };

    MoveImage(&_gAkropolisCafeteriaPostBattleImageSource, IMAGE_X, IMAGE_Y);
    MoveImage(&_gAkropolisCafeteriaPostBattleRowSource, ROW_X, ROW_Y);
}

/// Ends the room's effects and locks the attachments, for the post-battle scene's script.
static void _akropolisCafeteriaPreparePostBattleScene(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Installs the room's messages and sets its actors up for the story so far: before the battle the scripted form;
/// after it, the boss's body and Rupert, with the trays' trigger disabled.
static void _akropolisCafeteriaInitializeRoomTask(Task* task)
{
    enum {
        DRAW_SUSPEND = 2,
    };
    WorldCollisionTrigger* disabledTrigger;

    task->msgTable = _gAkropolisCafeteriaRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_ENTERED_AKROPOLIS) {
        sceneSetPlacedActorDrawMode(AKROPOLIS_CAFETERIA_STRANGER_ACTOR, ACTOR_MESSAGE_DRAW_HIDE);
        sceneSetPlacedActorDrawMode(AKROPOLIS_CAFETERIA_SCRIPTED_FORM_ACTOR, ACTOR_MESSAGE_DRAW_SHOW);
    } else if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
        sceneSetPlacedActorDrawMode(AKROPOLIS_CAFETERIA_STRANGER_ACTOR, ACTOR_MESSAGE_DRAW_SHOW);
        sceneSetPlacedActorDrawMode(AKROPOLIS_CAFETERIA_SCRIPTED_FORM_ACTOR, DRAW_SUSPEND);
        sceneSetPlacedActorDrawMode(AKROPOLIS_CAFETERIA_RUPERT_ACTOR, ACTOR_MESSAGE_DRAW_SHOW);
        disabledTrigger         = &_gAkropolisCafeteriaRoom1ActionTriggers[AKROPOLIS_CAFETERIA_TRAYS_TRIGGER];
        disabledTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(AKROPOLIS_CAFETERIA_RUPERT_ACTOR), ACTOR_MESSAGE_PLACE, &_gAkropolisCafeteriaRupertConversationPlacement, 0);
    }
    task->state = task->state + 1;
}

void akropolisCafeteriaRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAkropolisCafeteriaRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
