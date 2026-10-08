#include "acropolis_observatory_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"

#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room.h"

/// Per-frame paths the two streamed scenes walk the player's matrix along,
/// indexed by `gCdCmdQueue.movieFrame + 0xA8`, each with the script pair its
/// scene runs.
extern SVECTOR D_acropolis_observatory_8017E80C[];

extern SVECTOR D_acropolis_observatory_8017F16C[];

static void _acropolisObservatoryForkedRoadArrivalMoviePathTask(Task* task);
static void _acropolisObservatoryPromenadeArrivalMoviePathTask(Task* task);
static void _acropolisObservatorySkipFadeOutTask(Task* task);
static void _acropolisObservatorySkipFadeInTask(Task* task);

TaskDesc D_acropolis_observatory_8017E7DC[4] = {
    { { { TASK_BODY_NONE, 192 } }, _acropolisObservatoryForkedRoadArrivalMoviePathTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisObservatoryPromenadeArrivalMoviePathTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisObservatorySkipFadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisObservatorySkipFadeInTask, { .value = 0 } },
};

SVECTOR D_acropolis_observatory_8017E80C[300] = {
#include "assets/acropolis_observatory_path_0124C.inc"
};

SVECTOR D_acropolis_observatory_8017F16C[300] = {
#include "assets/acropolis_observatory_path_01BAC.inc"
};

static AnimationPackedPose _gAcropolisObservatoryAnimation02878Bank1[7] = {
#include "assets/acropolis_observatory_animation_02878_bank1.inc"
};

static AnimationPackedRotation _gAcropolisObservatoryAnimation02878Bank4[65] = {
#include "assets/acropolis_observatory_animation_02878_bank4.inc"
};

static AnimationRecord _gAcropolisObservatoryAnimation02878Records[123] = {
#include "assets/acropolis_observatory_animation_02878_records.inc"
};

static u16 _gAcropolisObservatoryAnimation02878Indices[20] = {
#include "assets/acropolis_observatory_animation_02878_indices.inc"
};

AnimationSet gAcropolisObservatoryAnimation02878 = {
    _gAcropolisObservatoryAnimation02878Records,
    _gAcropolisObservatoryAnimation02878Indices,
    { NULL, _gAcropolisObservatoryAnimation02878Bank1, NULL, NULL, _gAcropolisObservatoryAnimation02878Bank4, NULL, NULL, NULL },
};

AnimationSet* gAcropolisObservatoryPlayerAnimationSets[2] = { NULL, &gAcropolisObservatoryAnimation02878 };

/// Phases and sampling units shared by the two observatory arrival controllers.
enum {
    ACROPOLIS_OBSERVATORY_MOVIE_INITIALIZE                 = 0,
    ACROPOLIS_OBSERVATORY_MOVIE_WAIT_READY                 = 1,
    ACROPOLIS_OBSERVATORY_MOVIE_FOLLOW_PATH                = 2,
    ACROPOLIS_OBSERVATORY_MOVIE_WAIT_PLAYER                = 3,
    ACROPOLIS_OBSERVATORY_MOVIE_RESTORE                    = 4,
    ACROPOLIS_OBSERVATORY_MOVIE_FRAME_ORIGIN               = 168,
    ACROPOLIS_OBSERVATORY_MOVIE_WALK_HANDOFF_INDEX         = 230,
    ACROPOLIS_OBSERVATORY_MOVIE_PROMENADE_Z_OFFSET         = 200,
    ACROPOLIS_OBSERVATORY_MOVIE_PRIMARY_CHARACTER          = 1,
    ACROPOLIS_OBSERVATORY_MOVIE_ALTERNATE_WEAPON_BANK_BASE = 34,
    ACROPOLIS_OBSERVATORY_MOVIE_SKIP_FADE_OUT_TASK         = 2,
    ACROPOLIS_OBSERVATORY_MOVIE_SKIP_FADE_IN_TASK          = 3,
    ACROPOLIS_OBSERVATORY_MOVIE_EXIT_YAW                   = 1024
};

/// Copies one movie-path XYZ sample into the borrowed player root, in room units.
///
/// Requires a valid index in the supplied path. Work, path and sample index are
/// evaluated once per axis and must be stable and free of side effects. The Z
/// offset is evaluated once. Rotation and coordinate cache stamps are untouched.
/// Expands to a braced statement block; use inside a braced scope.
#define ACROPOLIS_OBSERVATORY_APPLY_MOVIE_PATH_POSITION(movieWork, path, sampleIndex, zOffset) \
    {                                                                                          \
        (movieWork)->playerMtx->t[0] = (path)[(sampleIndex)].vx;                               \
        (movieWork)->playerMtx->t[1] = (path)[(sampleIndex)].vy;                               \
        (movieWork)->playerMtx->t[2] = (path)[(sampleIndex)].vz + (zOffset);                   \
    }

/// Completes the forked-road movie arrival by moving the player along its sampled path.
///
/// Starts bodyless in state 0, owns zeroed `RoomMoviePathWork` and borrows the
/// live player/root matrix. Allocation failure kills the controller. Waits for
/// movie readiness, starts a child vibration script, and indexes the 300-point
/// path by `movieFrame + 168`; the caller must keep that index in 0..299.
/// A Start skip fades out before placing the exit pose; normal playback hands
/// off to a scripted walk at index 230. After player motion ends, selects mapped view 2
/// and restores HUD, input and actor updates before task teardown.
/// Child script/fade allocation is assumed to succeed; spawn results are retained.
static void _acropolisObservatoryForkedRoadArrivalMoviePathTask(Task* task)
{
    AnimationPlayRequest animationRequest;
    ActorTransform       exitPlacement;
    s32                  fadeResult;
    RoomMoviePathWork*   work;
    CdCmdQueue*          cdQueue;
    s32                  weaponId;

    cdQueue = &gCdCmdQueue;
    work    = task->work;
    switch (task->state) {
        case ACROPOLIS_OBSERVATORY_MOVIE_INITIALIZE:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            weaponId                                     = gPlayerStatus.weapon;
            animationRequest.source.index                = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACROPOLIS_OBSERVATORY_MOVIE_PRIMARY_CHARACTER) ? weaponId + 1 : weaponId + ACROPOLIS_OBSERVATORY_MOVIE_ALTERNATE_WEAPON_BANK_BASE;
            animationRequest.animationId                 = 1;
            animationRequest.blend                       = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames                 = 0;
            animationRequest.enableWorldCollision        = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case ACROPOLIS_OBSERVATORY_MOVIE_WAIT_READY:
            if (cdQueue->movieReady != 0) {
                work->padScriptTask           = padScriptSpawn(D_acropolis_observatory_80183480,
                                                               D_acropolis_observatory_80183498);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_OBSERVATORY_MOVIE_FOLLOW_PATH:
            // Movie samples change only the player root translation, in room units.
            ACROPOLIS_OBSERVATORY_APPLY_MOVIE_PATH_POSITION(work, D_acropolis_observatory_8017E80C, cdQueue->movieFrame + ACROPOLIS_OBSERVATORY_MOVIE_FRAME_ORIGIN, 0);
            if (work->skipFadeStarted != 0) {
                if (taskPollKill(work->skipFadeTask, &fadeResult) != 0) {
                    exitPlacement.pos.vx = -0x968;
                    exitPlacement.pos.vy = -0xBAD;
                    exitPlacement.pos.vz = -0x6D4;
                    exitPlacement.rot.vz = 0;
                    exitPlacement.rot.vx = 0;
                    exitPlacement.rot.vy = ACROPOLIS_OBSERVATORY_MOVIE_EXIT_YAW;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &exitPlacement, 0);
                    taskSpawnFromTable(D_acropolis_observatory_8017E7DC, ACROPOLIS_OBSERVATORY_MOVIE_SKIP_FADE_IN_TASK, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(D_acropolis_observatory_8017E7DC, ACROPOLIS_OBSERVATORY_MOVIE_SKIP_FADE_OUT_TASK, 0, 0);
                work->skipFadeStarted = 1;
            }
            // The normal handoff reads XYZ only; skip placement also reads angles.
            if ((cdQueue->movieFrame + ACROPOLIS_OBSERVATORY_MOVIE_FRAME_ORIGIN) >= ACROPOLIS_OBSERVATORY_MOVIE_WALK_HANDOFF_INDEX) {
                exitPlacement.pos.vx = -0x968;
                exitPlacement.pos.vy = -0xBAD;
                exitPlacement.pos.vz = -0x6D4;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &exitPlacement, 0);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_OBSERVATORY_MOVIE_WAIT_PLAYER:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewFindLogicalIndex(2);
                task->state                                                = task->state + 1;
            }
            break;

        case ACROPOLIS_OBSERVATORY_MOVIE_RESTORE:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// Completes the promenade movie arrival by moving the player along its sampled path.
///
/// Starts bodyless in state 0, owns zeroed `RoomMoviePathWork` and borrows the
/// live player/root matrix. Allocation failure kills the controller. Waits for
/// movie readiness, starts a child vibration script, and indexes the 300-point
/// path by `movieFrame + 168`; the caller must keep that index in 0..299.
/// A Start skip fades out before placing the exit pose; normal playback hands
/// off to a scripted walk at index 230. After player motion ends, selects mapped view 4
/// and restores HUD, input and actor updates before task teardown.
/// Child script/fade allocation is assumed to succeed; spawn results are retained.
static void _acropolisObservatoryPromenadeArrivalMoviePathTask(Task* task)
{
    AnimationPlayRequest animationRequest;
    ActorTransform       exitPlacement;
    s32                  fadeResult;
    RoomMoviePathWork*   work;
    CdCmdQueue*          cdQueue;
    s32                  weaponId;

    cdQueue = &gCdCmdQueue;
    work    = task->work;
    switch (task->state) {
        case ACROPOLIS_OBSERVATORY_MOVIE_INITIALIZE:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            weaponId                                     = gPlayerStatus.weapon;
            animationRequest.source.index                = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACROPOLIS_OBSERVATORY_MOVIE_PRIMARY_CHARACTER) ? weaponId + 1 : weaponId + ACROPOLIS_OBSERVATORY_MOVIE_ALTERNATE_WEAPON_BANK_BASE;
            animationRequest.animationId                 = 1;
            animationRequest.blend                       = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames                 = 0;
            animationRequest.enableWorldCollision        = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case ACROPOLIS_OBSERVATORY_MOVIE_WAIT_READY:
            if (cdQueue->movieReady != 0) {
                work->padScriptTask           = padScriptSpawn(D_acropolis_observatory_801834A0,
                                                               D_acropolis_observatory_801834B8);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_OBSERVATORY_MOVIE_FOLLOW_PATH:
            // Movie samples change only the player root translation, in room units.
            ACROPOLIS_OBSERVATORY_APPLY_MOVIE_PATH_POSITION(work, D_acropolis_observatory_8017F16C, cdQueue->movieFrame + ACROPOLIS_OBSERVATORY_MOVIE_FRAME_ORIGIN, ACROPOLIS_OBSERVATORY_MOVIE_PROMENADE_Z_OFFSET);
            if (work->skipFadeStarted != 0) {
                if (taskPollKill(work->skipFadeTask, &fadeResult) != 0) {
                    exitPlacement.pos.vx = -0x8F8;
                    exitPlacement.pos.vy = -0xBAD;
                    exitPlacement.pos.vz = -0x2936;
                    exitPlacement.rot.vz = 0;
                    exitPlacement.rot.vx = 0;
                    exitPlacement.rot.vy = ACROPOLIS_OBSERVATORY_MOVIE_EXIT_YAW;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &exitPlacement, 0);
                    taskSpawnFromTable(D_acropolis_observatory_8017E7DC, ACROPOLIS_OBSERVATORY_MOVIE_SKIP_FADE_IN_TASK, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(D_acropolis_observatory_8017E7DC, ACROPOLIS_OBSERVATORY_MOVIE_SKIP_FADE_OUT_TASK, 0, 0);
                work->skipFadeStarted = 1;
            }
            // The normal handoff reads XYZ only; skip placement also reads angles.
            if ((cdQueue->movieFrame + ACROPOLIS_OBSERVATORY_MOVIE_FRAME_ORIGIN) >= ACROPOLIS_OBSERVATORY_MOVIE_WALK_HANDOFF_INDEX) {
                exitPlacement.pos.vx = -0x8F8;
                exitPlacement.pos.vy = -0xBAD;
                exitPlacement.pos.vz = -0x2936;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &exitPlacement, 0);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_OBSERVATORY_MOVIE_WAIT_PLAYER:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewFindLogicalIndex(4);
                task->state                                                = task->state + 1;
            }
            break;

        case ACROPOLIS_OBSERVATORY_MOVIE_RESTORE:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

#undef ACROPOLIS_OBSERVATORY_APPLY_MOVIE_PATH_POSITION

/// Darkens either observatory ride before its skip caller replaces the player's pose.
///
/// Requires a bodyless task with `killCountdown` initially zero. Draws eight
/// equal-channel subtractive overlays, intensities 0..224 in steps of 32,
/// then suspends with result zero at progress 256. The ride must collect the
/// retained task with `taskPollKill` before starting the reverse fade.
static void _acropolisObservatorySkipFadeOutTask(Task* task)
{
    enum { ACROPOLIS_OBSERVATORY_SKIP_FADE_STEP = 32,
           ACROPOLIS_OBSERVATORY_SKIP_FADE_SPAN = 256 };
    u8  fadeLevel;
    s16 nextProgress;

    fadeLevel = (u8)task->killCountdown;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    nextProgress        = (u16)task->killCountdown + ACROPOLIS_OBSERVATORY_SKIP_FADE_STEP;
    task->killCountdown = nextProgress;
    if (nextProgress >= ACROPOLIS_OBSERVATORY_SKIP_FADE_SPAN) {
        taskRequestKill(task, 0);
    }
}

/// Reveals either observatory ride's final pose after the skip transition.
///
/// Requires a bodyless task with `killCountdown` initially zero. Draws eight
/// equal-channel subtractive overlays, intensities 255..31 in steps of 32,
/// then tears down the task at progress 256. The signed 16-bit counter is
/// callback-owned progress; no work or spawn arguments are used.
static void _acropolisObservatorySkipFadeInTask(Task* task)
{
    enum { ACROPOLIS_OBSERVATORY_SKIP_FADE_STEP = 32,
           ACROPOLIS_OBSERVATORY_SKIP_FADE_SPAN = 256 };
    u8  fadeLevel;
    s16 nextProgress;

    fadeLevel = ~(u8)task->killCountdown;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    nextProgress        = (u16)task->killCountdown + ACROPOLIS_OBSERVATORY_SKIP_FADE_STEP;
    task->killCountdown = nextProgress;
    if (nextProgress >= ACROPOLIS_OBSERVATORY_SKIP_FADE_SPAN) {
        taskKill(task);
    }
}
