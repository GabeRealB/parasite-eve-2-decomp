#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 D_mine_cavern_8018EB50;

#include "rooms/mine_cavern.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "mine_cavern_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

// Message-table callbacks use the argument views required by this TU.

extern TaskDesc D_mine_cavern_8018E3F4;

u16 D_mine_cavern_8018E360 = 6016;

u16 D_mine_cavern_8018E362 = 2048;

u16 D_mine_cavern_8018E364 = 2048;

u16 D_mine_cavern_8018E366 = 950;

u16 D_mine_cavern_8018E368 = 2710;

SVECTOR D_mine_cavern_8018E36C[6] = {
    { 200, -2060, 5160, 0 },
    { 9000, -2040, 8710, 0 },
    { 0x4560, -2060, 5000, 0 },
    { 0x36B0, -2060, 290, 0 },
    { 4550, -2090, 280, 0 },
    { 6910, -2780, 3700, 0 },
};

SVECTOR D_mine_cavern_8018E39C[4] = {
    { 4550, -700, 340, 0 },
    { 0x3566, -800, 330, 0 },
    { 4340, -800, 8710, 0 },
    { 0x3552, -800, 8710, 0 },
};

u8 D_mine_cavern_8018E3BC[4][8] = {
    { 4, 22, 5, 23, 24, 25, 18, 19 },
    { 3, 2, 0, 0, 0, 0, 0, 0 },
    { 6, 21, 16, 0, 0, 0, 0, 0 },
    { 7, 8, 20, 17, 0, 0, 0, 0 },
};

s16 D_mine_cavern_8018E3DC = 0;

CVECTOR D_mine_cavern_8018E3E0[5] = {
    { 30, 30, 30, 0 },
    { 25, 25, 25, 0 },
    { 17, 21, 22, 0 },
    { 7, 15, 16, 0 },
    { 0, 9, 11, 0 },
};

TaskDesc D_mine_cavern_8018E3F4 = { { { TASK_BODY_NONE, 96 } }, mineCavernTargetEffectsTask, { .value = 0 } };

static TmdBone _gMineCavernModel10F60Skeleton[1] = {
#include "assets/mine_cavern_model_10F60_skeleton.inc"
};

static u32 _gMineCavernModel10F60PartVerts[1] = {
#include "assets/mine_cavern_model_10F60_partVerts.inc"
};

static SVECTOR _gMineCavernModel10F60Verts[20] = {
#include "assets/mine_cavern_model_10F60_verts.inc"
};

static SVECTOR _gMineCavernModel10F60Normals[11] = {
#include "assets/mine_cavern_model_10F60_normals.inc"
};

static u32 _gMineCavernModel10F60Stream[104] = {
#include "assets/mine_cavern_model_10F60_stream.inc"
};

static TmdSource _gMineCavernModel10F60 = {
    0,
    728,
    0,
    1,
    _gMineCavernModel10F60PartVerts,
    _gMineCavernModel10F60Verts,
    _gMineCavernModel10F60Normals,
    _gMineCavernModel10F60Skeleton,
    _gMineCavernModel10F60Stream,
};

static TmdBone _gMineCavernModel11244Skeleton[1] = {
#include "assets/mine_cavern_model_11244_skeleton.inc"
};

static u32 _gMineCavernModel11244PartVerts[1] = {
#include "assets/mine_cavern_model_11244_partVerts.inc"
};

static SVECTOR _gMineCavernModel11244Verts[20] = {
#include "assets/mine_cavern_model_11244_verts.inc"
};

static SVECTOR _gMineCavernModel11244Normals[11] = {
#include "assets/mine_cavern_model_11244_normals.inc"
};

static u32 _gMineCavernModel11244Stream[174] = {
#include "assets/mine_cavern_model_11244_stream.inc"
};

static TmdSource _gMineCavernModel11244 = {
    0,
    1248,
    0,
    1,
    _gMineCavernModel11244PartVerts,
    _gMineCavernModel11244Verts,
    _gMineCavernModel11244Normals,
    _gMineCavernModel11244Skeleton,
    _gMineCavernModel11244Stream,
};

DamageAttack D_mine_cavern_8018EAE0[1] = {
    { 18, 0 },
};

EnemyParams D_mine_cavern_8018EAE4 = { D_mine_cavern_8018EAE0, 30, 0, 0, 0, 0, 0, 0, 0 };

u8 D_mine_cavern_8018EAF4[36] = {
    0,
    2,
    2,
    2,
    0,
    0,
    0,
    10,
    10,
    10,
    30,
    30,
    30,
    10,
    30,
    15,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    30,
    30,
    30,
    30,
    10,
    1,
    30,
    30,
    30,
    0,
    0,
    0,
};

SVECTOR D_mine_cavern_8018EB18[4] = {
    { 4550, 0, 310, 0 },
    { 0x3566, 0, 330, 0 },
    { 4340, 0, 8710, 0 },
    { 0x3552, 0, 8710, 0 },
};

TaskDesc D_mine_cavern_8018EB38[2] = {
    { { { TASK_BODY_TMD, 96 } }, mineCavernTargetTask, { .model = &_gMineCavernModel10F60 } },
    { { { TASK_BODY_TMD, 96 } }, mineCavernTargetRemainsTask, { .model = &_gMineCavernModel11244 } },
};

s32 D_mine_cavern_8018EB54;

s32 D_mine_cavern_8018EB58;

u16 D_mine_cavern_8018EB5C;

static void _mineCavernInitRoom(Task* task);
static void _mineCavernStartPendingEvent(Task* unusedTask);

// Persistent stages shared by the event message and its battle-release callback.
enum {
    MINE_CAVERN_EVENT_NOT_STARTED  = 0,
    MINE_CAVERN_EVENT_FIRST_STAGE  = 1,
    MINE_CAVERN_EVENT_SECOND_STAGE = 2
};

// Layouts used by the introductory and final scripted battles.
enum {
    MINE_CAVERN_VARIANT_INITIAL     = 1,
    MINE_CAVERN_VARIANT_FINAL_EVENT = 4
};

// Passage states shared by departure resolution and progress commits.
enum {
    MINE_CAVERN_PASSAGE_STATE_OPEN       = 1,
    MINE_CAVERN_PASSAGE_PROGRESS_ENTERED = 2
};

/// Records the first open-passage departure and resets its subsequent dialogue.
///
/// Requires live saved flags. The transition caller invokes this only for an
/// executing open-passage exit not yet at progress 2; queries do not commit it.
static inline void _mineCavernCommitPassageEntry(void)
{
    gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS, MINE_CAVERN_PASSAGE_PROGRESS_ENTERED);
    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
    gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0);
}

s32 mineCavernResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        MINE_CAVERN_TRANSITION_BLOCKED         = 0,
        MINE_CAVERN_TRANSITION_ALLOWED         = 1,
        MINE_CAVERN_PASSAGE_PROGRESS_UNTRIED   = 0,
        MINE_CAVERN_PASSAGE_PROGRESS_ATTEMPTED = 1,
        MINE_CAVERN_BLOCKED_DEPARTURE_FLAG     = 2,
        MINE_CAVERN_CAP_PASSAGE_BATTLE_BLOCKED = 9,
        MINE_CAVERN_CAP_GORGE_BATTLE_BLOCKED   = 11,
        MINE_CAVERN_CAP_PASSAGE_CLOSED         = 13
    };

    // Prepare the complete reply before resolving progress-dependent destination rooms.
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);

    if (request->areaId == GAME_AREA_MINE_SECRET_PASSAGE) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE) != MINE_CAVERN_PASSAGE_STATE_OPEN) {
            if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                return MINE_CAVERN_TRANSITION_BLOCKED;
            }
            gameFlagSetNibbleIfPresent(request->flagId, MINE_CAVERN_BLOCKED_DEPARTURE_FLAG);
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && gGameSession->location.loc.variant == gSceneCombatState.signals.bytes.battlePhase) {
                capRunCommandWithTransition(MINE_CAVERN_CAP_PASSAGE_BATTLE_BLOCKED);
                return MINE_CAVERN_TRANSITION_BLOCKED;
            }
            capRunCommandWithTransition(MINE_CAVERN_CAP_PASSAGE_CLOSED);
            if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) != MINE_CAVERN_PASSAGE_PROGRESS_UNTRIED) {
                return MINE_CAVERN_TRANSITION_BLOCKED;
            }
            gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS, MINE_CAVERN_PASSAGE_PROGRESS_ATTEMPTED);
            return MINE_CAVERN_TRANSITION_BLOCKED;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) != MINE_CAVERN_PASSAGE_PROGRESS_ENTERED) {
            _mineCavernCommitPassageEntry();
        }
    }

    if (request->areaId == GAME_AREA_MINE_GORGE) {
        if (gGameSession->location.loc.variant == MINE_CAVERN_VARIANT_INITIAL || gGameSession->location.loc.variant == MINE_CAVERN_VARIANT_FINAL_EVENT) {
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                    capRunCommandWithTransition(MINE_CAVERN_CAP_GORGE_BATTLE_BLOCKED);
                }
                return MINE_CAVERN_TRANSITION_BLOCKED;
            }
        }
    }
    return MINE_CAVERN_TRANSITION_ALLOWED;
}

/// Runs a passage caption and spawns the task that commits its final choice.
///
/// `commandIndex` selects a loaded room CAP command (callers use 5 or 18).
/// Playback uses a queued display transition and this function does not wait.
/// The independent watcher reads CAP's retained final choice after playback;
/// loaded caption and task resources must survive until both complete.
/// Watcher allocation failure is not reported and does not cancel playback.
static inline void _mineCavernRunCaptionAndWatchProgress(s32 commandIndex)
{
    capRunCommandWithTransition(commandIndex);
    taskSpawnFromTable(D_mine_cavern_80183CA4, 0, 0, 0);
}

s32 mineCavernHandleCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedArg)
{
    enum {
        MINE_CAVERN_COMMAND_PASSAGE_STATUS             = 1,
        MINE_CAVERN_CAP_POWER_PANEL                    = 5,
        MINE_CAVERN_CAP_TARGET_0                       = 8,
        MINE_CAVERN_CAP_PASSAGE_STATUS_BATTLE          = 10,
        MINE_CAVERN_CAP_TARGET_1                       = 14,
        MINE_CAVERN_CAP_TARGET_2                       = 15,
        MINE_CAVERN_CAP_TARGET_3                       = 16,
        MINE_CAVERN_CAP_PASSAGE_OPEN                   = 17,
        MINE_CAVERN_CAP_PASSAGE_POWERED                = 18,
        MINE_CAVERN_PASSAGE_STATE_POWERED_SECOND_STAGE = 3,
        MINE_CAVERN_POWER_PANEL_COMPLETE               = 2
    };
    u8 variant;

    // Passage-status captions may queue a watcher that commits the final CAP choice.
    if (commandIndex == MINE_CAVERN_COMMAND_PASSAGE_STATUS) {
        if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) != 0) {
            return 0;
        }
        if (gSceneCombatState.signals.bytes.battlePhase == commandIndex && (gGameSession->location.loc.variant == commandIndex || gGameSession->location.loc.variant == MINE_CAVERN_VARIANT_FINAL_EVENT)) {
            capRunCommandWithTransition(MINE_CAVERN_CAP_PASSAGE_STATUS_BATTLE);
        } else if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE) == MINE_CAVERN_PASSAGE_STATE_OPEN) {
            capRunCommandWithTransition(MINE_CAVERN_CAP_PASSAGE_OPEN);
        } else if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE) == MINE_CAVERN_PASSAGE_STATE_POWERED_SECOND_STAGE) {
            _mineCavernRunCaptionAndWatchProgress(MINE_CAVERN_CAP_PASSAGE_POWERED);
        } else if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) != MINE_CAVERN_POWER_PANEL_COMPLETE) {
            _mineCavernRunCaptionAndWatchProgress(MINE_CAVERN_CAP_POWER_PANEL);
        } else {
            capRunCommandWithTransition(MINE_CAVERN_CAP_POWER_PANEL);
        }
    }
    // Each target's saved destruction bit selects its intact/destroyed caption variant.
    variant = gGameSession->location.loc.variant;
    if (variant == MINE_CAVERN_VARIANT_INITIAL || variant == MINE_CAVERN_VARIANT_FINAL_EVENT) {
        switch (commandIndex) {
            case MINE_CAVERN_CAP_TARGET_0:
                capStartSequenceSlot(MINE_CAVERN_CAP_TARGET_0, CAP_PLAYBACK_DISPLAY_TRANSITION, gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) & 1);
                break;
            case MINE_CAVERN_CAP_TARGET_1:
                capStartSequenceSlot(MINE_CAVERN_CAP_TARGET_1, CAP_PLAYBACK_DISPLAY_TRANSITION, ((u32)gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) >> 1) & 1);
                break;
            case MINE_CAVERN_CAP_TARGET_2:
                capStartSequenceSlot(MINE_CAVERN_CAP_TARGET_2, CAP_PLAYBACK_DISPLAY_TRANSITION, ((u32)gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) >> 2) & 1);
                break;
            case MINE_CAVERN_CAP_TARGET_3:
                capStartSequenceSlot(MINE_CAVERN_CAP_TARGET_3, CAP_PLAYBACK_DISPLAY_TRANSITION, ((u32)gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) >> 3) & 1);
                break;
        }
    }
    return 0;
}

s32 mineCavernRefuseKeyItem(Task* task, s32 msgId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 mineCavernHandleActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum { MINE_CAVERN_ACTION_PROGRESS_CAPTION  = 6,
           MINE_CAVERN_PROGRESS_CAPTION_ENABLED = 1 };

    if ((request->actionId == MINE_CAVERN_ACTION_PROGRESS_CAPTION) && (gameFlagGetNibble(GAME_FLAG_0C4) == MINE_CAVERN_PROGRESS_CAPTION_ENABLED)) {
        capRunCommandWithTransition(MINE_CAVERN_ACTION_PROGRESS_CAPTION);
    }
    return 0;
}

s32 mineCavernAdvanceEvent(Task* task, s32 msgId, s32 unusedEventId, s32 unusedArg)
{
    enum { MINE_CAVERN_POST_EVENT_OBJECTIVE = 0x3D };

    if (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS) == MINE_CAVERN_EVENT_NOT_STARTED) {
        // Let the room tick start the first scene after the attachment wheel closes.
        Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
        roomEffectRequestCancelAll();
        gameFlagSetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS, MINE_CAVERN_EVENT_FIRST_STAGE);
        D_mine_cavern_8018EB50 = MINE_CAVERN_EVENT_FIRST_STAGE;
    } else if (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS) == MINE_CAVERN_EVENT_FIRST_STAGE) {
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, MINE_CAVERN_POST_EVENT_OBJECTIVE);
        evsStartScriptWithSkip(D_mine_cavern_80188A3C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mine_cavern_80188D24);
        gameFlagSetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS, MINE_CAVERN_EVENT_SECOND_STAGE);
    }
    return 0;
}

s32 mineCavernHandleSoundMessage(Task* task, s32 msgId, s32 soundCommand, s32 unusedArg)
{
    enum { MINE_CAVERN_SOUND_COMMAND_EVENT = 0xD };

    if (soundCommand == MINE_CAVERN_SOUND_COMMAND_EVENT) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, MINE_CAVERN_SOUND_COMMAND_EVENT), 0, 0);
    }
    return 0;
}

void mineCavernCommitCaptionProgressTask(Task* task)
{
    enum {
        MINE_CAVERN_CAP_KEY_POWER_PANEL  = 0xB,
        MINE_CAVERN_CAP_KEY_OPEN_PASSAGE = 0x15,
        MINE_CAVERN_POWER_PANEL_COMPLETE = 2,
        MINE_CAVERN_PASSAGE_OPEN         = 1
    };

    if (capIsBusy() == 0) {
        // CAP retains its final choice after playback releases its selection.
        if (capGetVariantKey() == MINE_CAVERN_CAP_KEY_POWER_PANEL) {
            gameFlagSetNibble(GAME_FLAG_0C4, 1);
            gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE, MINE_CAVERN_POWER_PANEL_COMPLETE);
            gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON, 0);
        }
        if (capGetVariantKey() == MINE_CAVERN_CAP_KEY_OPEN_PASSAGE) {
            gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, MINE_CAVERN_PASSAGE_OPEN);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_MINE_CAVERN, 0);
        }
        taskKill(task);
    }
}

/// Requests the one-time intro and resets its scripted battle-release credit.
///
/// Requires live flags and loaded normal/skip scripts through event completion.
/// The variant-1 room initializer gates this on the unseen flag. Script start
/// precedes the release-count reset and seen flag, including on spawn failure.
static inline void _mineCavernStartIntroEvent(void)
{
    enum { MINE_CAVERN_INTRO_SEEN = 1 };

    evsStartScriptWithSkip(D_mine_cavern_80187C74, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mine_cavern_8018804C);
    mineCavernResetEventBattleReleaseCount();
    gameFlagSetNibble(GAME_FLAG_MINE_CAVERN_INTRO_SEEN, MINE_CAVERN_INTRO_SEEN);
}

/// Initializes the room message interface, intro scene and target-effects controller.
///
/// Requires a live state-0 task and loaded cavern/map resources. The first
/// variant-1 entry requests the intro with its skip path and resets battle-release
/// credit; other entries select scene-music entry 1. Nursery progress hides
/// the progress sprites. Advances the task and clears the pending event latch.
static void _mineCavernInitRoom(Task* task)
{
    enum { MINE_CAVERN_SCENE_MUSIC_DEFAULT_ENTRY = 1,
           MINE_CAVERN_PROGRESS_SPRITES_SHOWN    = 0,
           MINE_CAVERN_PROGRESS_SPRITES_HIDDEN   = 1 };

    // Publish the controller before any intro script can send room messages.
    task->msgTable = D_mine_cavern_80183C6C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == MINE_CAVERN_VARIANT_INITIAL) && (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_INTRO_SEEN) == 0)) {
        _mineCavernStartIntroEvent();
    } else {
        gStageSceneMusicEntry = MINE_CAVERN_SCENE_MUSIC_DEFAULT_ENTRY;
    }
    taskSpawnFromTable(&D_mine_cavern_8018E3F4, 0, 0, 0);
    if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) != 0) {
        mineCavernSetProgressSpritesHidden(MINE_CAVERN_PROGRESS_SPRITES_HIDDEN);
    } else {
        mineCavernSetProgressSpritesHidden(MINE_CAVERN_PROGRESS_SPRITES_SHOWN);
    }
    task->state            = task->state + 1;
    D_mine_cavern_8018EB50 = MINE_CAVERN_EVENT_NOT_STARTED;
}

/// Starts the armed first event scene once the attachment wheel has closed.
///
/// Starts only when persistent progress and the pending latch are both at stage 1.
/// Issuing the start request consumes the latch by moving it to stage 2;
/// it is not retried if event-task allocation fails. Requires loaded event
/// scripts and live session/attachment state. The task is unused.
static void _mineCavernStartPendingEvent(Task* unusedTask)
{
    s32 eventProgress;

    eventProgress = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS);
    // Compare against the armed latch: its stage-1 value is also wheel-open mode.
    if ((eventProgress == MINE_CAVERN_EVENT_FIRST_STAGE) && (D_mine_cavern_8018EB50 == eventProgress) && (Gp_StateC08.mode != D_mine_cavern_8018EB50)) {
        evsStartScriptWithSkip(D_mine_cavern_80188214, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mine_cavern_801887B4);
        D_mine_cavern_8018EB50 = MINE_CAVERN_EVENT_SECOND_STAGE;
    }
}

/// The room task's state handlers, run by `mineCavernRoomTask`.
static const TaskFuncTable3 D_mine_cavern_8017D5C4 = {
    { _mineCavernInitRoom, _mineCavernStartPendingEvent, taskKill },
};

void mineCavernRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_mine_cavern_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

/// Records one scripted battle-hold release and schedules weapon restoration.
///
/// A remaining battle hold credits rewards from the live room's placement 0,
/// whose task must have an Enemy record; no task ownership changes. The signed
/// frame delay is stored modulo 256 and counts unpaused actor-update frames.
/// Weapon restoration is requested and the event-release count advances even
/// when there was no battle hold. The caller gates each event stage once.
static inline void _mineCavernCreditEventBattleRelease(s32 endDelayFrames)
{
    enum { MINE_CAVERN_EVENT_REWARD_PLACEMENT = 0 };
    // Rewards come from the placed enemy; the retained second argument is unused.
    sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(MINE_CAVERN_EVENT_REWARD_PLACEMENT), 0x1E);
    gSceneCombatState.signals.bytes.endDelayFrames = endDelayFrames;
    gGameSession->flowFlags                       |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    D_mine_cavern_8018EB54                        += 1;
}

void mineCavernReleaseEventBattleHold(s32 endDelayFrames)
{
    // Each persistent event stage consumes at most one scripted battle hold.
    if ((gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS) == MINE_CAVERN_EVENT_FIRST_STAGE && D_mine_cavern_8018EB54 == 0) ||
        (gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_EVENT_PROGRESS) == MINE_CAVERN_EVENT_SECOND_STAGE && D_mine_cavern_8018EB54 == 1)) {
        _mineCavernCreditEventBattleRelease(endDelayFrames);
        return;
    }
    // Compare the full signed argument before the byte store truncates it.
    if (endDelayFrames < gSceneCombatState.signals.bytes.endDelayFrames) {
        gSceneCombatState.signals.bytes.endDelayFrames = endDelayFrames;
    }
}

void mineCavernStartCaptionVariantOne(s16 commandIndex)
{
    enum { MINE_CAVERN_SCRIPT_CAP_VARIANT = 1 };

    capStartSequenceSlot(commandIndex, CAP_PLAYBACK_DISPLAY_TRANSITION, MINE_CAVERN_SCRIPT_CAP_VARIANT);
}

void mineCavernEngageScriptedBattle(void)
{
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    if (gSceneCombatState.battleRefs == 0) {
        sceneAcquireBattleRef(0);
    }
    sceneEngageBattle(1);
}

void mineCavernSetAreaMusicEnabled(s32 enabled)
{
    if (enabled != 0) {
        gGameSession->flowFlags &= ~GAME_SESSION_FLOW_SKIP_AREA_MUSIC;
        return;
    }
    gGameSession->flowFlags |= GAME_SESSION_FLOW_SKIP_AREA_MUSIC;
    gGameSession->flowFlags |= GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY;
}

void mineCavernSetSceneMusicEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

void mineCavernApplyPostEventAreaUpdates(void)
{
    areaApplySavedUpdates(D_mine_cavern_8018E32C);
}

void mineCavernSelectCountdownMusicEntry(u8 countdownEntry)
{
    gStageSceneMusicEntry = countdownEntry;
}

void mineCavernTimedSoundTask(Task* task)
{
    enum {
        MINE_CAVERN_TIMED_SOUND_A              = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3A,
        MINE_CAVERN_TIMED_SOUND_B              = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x39,
        MINE_CAVERN_TIMED_SOUND_ATTENUATION    = 0x30,
        MINE_CAVERN_TIMED_SOUND_DURATION_TICKS = 537
    };

    // The signed callback counter is elapsed ticks, rather than a teardown countdown.
    task->killCountdown++;
    switch (task->killCountdown) {
        case 33:
        case 6:
        case 64:
        case 124:
        case 96:
        case 140:
            sndEvtRequestScriptStart(MINE_CAVERN_TIMED_SOUND_A, 0, MINE_CAVERN_TIMED_SOUND_ATTENUATION);
            break;
        case 80:
        case 18:
        case 48:
        case 112:
        case 135:
        case 536:
            sndEvtRequestScriptStart(MINE_CAVERN_TIMED_SOUND_B, 0, MINE_CAVERN_TIMED_SOUND_ATTENUATION);
            break;
    }
    if ((gGameSession->evtSkipped != 0) || (task->killCountdown >= MINE_CAVERN_TIMED_SOUND_DURATION_TICKS)) {
        taskKill(task);
    }
}

void mineCavernFadeOutMusic(void)
{
    enum { MINE_CAVERN_MUSIC_ALL_SEQUENCES = 0,
           MINE_CAVERN_MUSIC_FADE_TICKS    = 100 };

    sndEvtRequestMidiStop(MINE_CAVERN_MUSIC_ALL_SEQUENCES, MINE_CAVERN_MUSIC_FADE_TICKS);
}

void mineCavernLockAttachmentsAndCancelEffects(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    roomEffectRequestCancelAll();
}
