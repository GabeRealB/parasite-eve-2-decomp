#include "mist_parking_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/actor_messages.h"

extern ActorTransform D_mist_parking_8018FC3C;

static void _mistParkingPlayDepartureMovieTask(Task* task);
static void _mistParkingStartDepartureMovieTask(Task* task);

TaskDesc D_mist_parking_8018FC24[2] = {
    { { { TASK_BODY_NONE, 192 } }, _mistParkingStartDepartureMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistParkingPlayDepartureMovieTask, { .value = 0 } },
};

ActorTransform D_mist_parking_8018FC3C = { { 2105, -910, -3460, 0 }, { 20, 1081, 0, 0 } };

static SVECTOR _gMistParkingCollision126F8Normals[2] = {
#include "assets/mist_parking_collision_126F8_normals.inc"
};

static SVECTOR _gMistParkingCollision126F8Verts[6] = {
#include "assets/mist_parking_collision_126F8_verts.inc"
};

static WorldCollisionGridFace _gMistParkingCollision126F8Faces[2] = {
#include "assets/mist_parking_collision_126F8_faces.inc"
};

static s16 _gMistParkingCollision126F8Cells[4] = {
#include "assets/mist_parking_collision_126F8_cells.inc"
};

#define GRID_CELL(i) (&_gMistParkingCollision126F8Cells[i])
static s16* _gMistParkingCollision126F8Table[1] = {
#include "assets/mist_parking_collision_126F8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mist_parking_8018FCB8 = { NULL, _gMistParkingCollision126F8Normals, _gMistParkingCollision126F8Verts, _gMistParkingCollision126F8Faces, _gMistParkingCollision126F8Table, -4800, 6558, 1, 1, 4000, 2 };

static AnimationPackedPose _gMistParkingAnimation129F8Bank1[6] = {
#include "assets/mist_parking_animation_129F8_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation129F8Bank4[46] = {
#include "assets/mist_parking_animation_129F8_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation129F8Records[109] = {
#include "assets/mist_parking_animation_129F8_records.inc"
};

static u16 _gMistParkingAnimation129F8Indices[20] = {
#include "assets/mist_parking_animation_129F8_indices.inc"
};

AnimationSet gMistParkingAnimation129F8 = {
    _gMistParkingAnimation129F8Records,
    _gMistParkingAnimation129F8Indices,
    { NULL, _gMistParkingAnimation129F8Bank1, NULL, NULL, _gMistParkingAnimation129F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation12DCCBank1[8] = {
#include "assets/mist_parking_animation_12DCC_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation12DCCBank4[84] = {
#include "assets/mist_parking_animation_12DCC_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation12DCCRecords[117] = {
#include "assets/mist_parking_animation_12DCC_records.inc"
};

static u16 _gMistParkingAnimation12DCCIndices[20] = {
#include "assets/mist_parking_animation_12DCC_indices.inc"
};

AnimationSet gMistParkingAnimation12DCC = {
    _gMistParkingAnimation12DCCRecords,
    _gMistParkingAnimation12DCCIndices,
    { NULL, _gMistParkingAnimation12DCCBank1, NULL, NULL, _gMistParkingAnimation12DCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistParkingAnimation1323CBank1[5] = {
#include "assets/mist_parking_animation_1323C_bank1.inc"
};

static AnimationPackedRotation _gMistParkingAnimation1323CBank4[99] = {
#include "assets/mist_parking_animation_1323C_bank4.inc"
};

static AnimationRecord _gMistParkingAnimation1323CRecords[150] = {
#include "assets/mist_parking_animation_1323C_records.inc"
};

static u16 _gMistParkingAnimation1323CIndices[20] = {
#include "assets/mist_parking_animation_1323C_indices.inc"
};

AnimationSet gMistParkingAnimation1323C = {
    _gMistParkingAnimation1323CRecords,
    _gMistParkingAnimation1323CIndices,
    { NULL, _gMistParkingAnimation1323CBank1, NULL, NULL, _gMistParkingAnimation1323CBank4, NULL, NULL, NULL },
};

static void _mistParkingInitCutsceneModelPitch(Task* task);
static void _mistParkingAdvanceCutsceneModelPitch(Task* task);

/// Pitch progression in actor-angle units, advanced once per active callback.
enum {
    MIST_PARKING_CUTSCENE_MODEL_PITCH_START = -120,
    MIST_PARKING_CUTSCENE_MODEL_PITCH_STEP  = 15,
    MIST_PARKING_CUTSCENE_MODEL_PITCH_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 8
};

/// Applies the previous shared placement while recording this callback's pitch.
///
/// Requires a live TMD task; `killCountdown` is a signed angle in 4096 units
/// per turn. Copy the complete placement before changing its pitch so the
/// model receives the preceding update. The placement call borrows the snapshot
/// synchronously, and the shared placement retains the new pitch for the next call.
static inline void _mistParkingApplyPreviousCutsceneModelPlacement(Task* task)
{
    ActorTransform previousPlacement = D_mist_parking_8018FC3C;

    D_mist_parking_8018FC3C.rot.vx = task->killCountdown;
    actorMsgPlaceEulerZyx(task, 0, &previousPlacement, 0);
}

void mistParkingControlPlayerHeadAim(s32 mode)
{
    Task* task = D_mist_parking_80195324;

    if (task == NULL) {
        return;
    }
    switch (mode) {
        case MIST_PARKING_HEAD_AIM_FOLLOW_ANIMATION:
        case MIST_PARKING_HEAD_AIM_FORCE:
            task->spawnArg1.value = mode;
            break;
        default:
            taskKill(D_mist_parking_80195324);
            D_mist_parking_80195324 = NULL;
            break;
    }
}

void mistParkingQueueDelayedDisplayModeExit(s32 delayTicks)
{
    enum { MIST_PARKING_DISPLAY_EXIT_DESCRIPTOR_INDEX = 5 };

    displayQueueModeTask(taskGetDescAt(D_mist_parking_8018D75C, MIST_PARKING_DISPLAY_EXIT_DESCRIPTOR_INDEX), delayTicks, 0, STAGE_ENTRY_RELOAD);
}

void mistParkingDelayDisplayModeExitTask(Task* task)
{
    s32 ticksLeft;

    ticksLeft             = task->spawnArg1.value - 1;
    task->spawnArg1.value = ticksLeft;
    if (ticksLeft < 0) {
        taskKill(task);
        stageRequestModeTaskExit();
    }
}

void mistParkingSelectDialogueResource(s32 resourceOrdinal)
{
    enum {
        MIST_PARKING_DEPARTURE_FONT_VRAM_X = 320,
        MIST_PARKING_DEPARTURE_FONT_VRAM_Y = 256,
        MIST_PARKING_PRIZE_FONT_VRAM_X     = 704,
        MIST_PARKING_PRIZE_FONT_VRAM_Y     = 0
    };

    capReset();
    switch (resourceOrdinal) {
        case MIST_PARKING_DIALOGUE_DEPARTURE_MENU:
            Gp_CapFile = NULL;
            capSelectLoadedFile(MIST_PARKING_DIALOGUE_DEPARTURE_MENU);
            capSetTexturePage(MIST_PARKING_DEPARTURE_FONT_VRAM_X, MIST_PARKING_DEPARTURE_FONT_VRAM_Y);
            break;
        case MIST_PARKING_DIALOGUE_PRIZES:
            Gp_CapFile = NULL;
            capSelectLoadedFile(MIST_PARKING_DIALOGUE_PRIZES);
            capSetTexturePage(MIST_PARKING_PRIZE_FONT_VRAM_X, MIST_PARKING_PRIZE_FONT_VRAM_Y);
            break;
    }
}

void mistParkingSetConversationProgress(s32 progress)
{
    gameFlagSetNibble(GAME_FLAG_0F1, progress);
}

void mistParkingResetCutsceneTaskHandles(s32 unused)
{
    D_mist_parking_80195320 = NULL;
    D_mist_parking_80195324 = NULL;
}

/// Plays one of the two departure movies and restores game presentation.
///
/// `spawnArg1.value` selects stream 100 when zero, 101 otherwise, in the current
/// room. The matching slot (0..14) must already be loaded. Requires state 0..5
/// and exclusive display presentation while movie workspace replaces image memory.
/// Start requests cancellation; both paths wait for CD idle before restoring
/// saved VRAM images and model buffers, clearing the resident image workspace,
/// releasing the task and resuming the game loop. No work allocation is retained.
static void _mistParkingPlayDepartureMovieTask(Task* task)
{
    enum {
        MIST_PARKING_MOVIE_PREPARE                = 0,
        MIST_PARKING_MOVIE_QUEUE                  = 1,
        MIST_PARKING_MOVIE_WAIT_READY             = 2,
        MIST_PARKING_MOVIE_PLAY                   = 3,
        MIST_PARKING_MOVIE_WAIT_IDLE              = 4,
        MIST_PARKING_MOVIE_RESTORE                = 5,
        MIST_PARKING_DEPARTURE_MOVIE_ID           = 100,
        MIST_PARKING_ALTERNATE_DEPARTURE_MOVIE_ID = 101,
        MIST_PARKING_MOVIE_MUSIC_FADE_TICKS       = 10
    };
    u8          commandArgs[4];
    GameLoc     movieKey;
    CdCmdQueue* cdQueue;
    s16         movieSlot;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case MIST_PARKING_MOVIE_PREPARE:
            // Save the displaced VRAM images before reserving movie workspace.
            stageMusicRequestAreaStop(MIST_PARKING_MOVIE_MUSIC_FADE_TICKS);
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state = task->state + 1;
            return;
        case MIST_PARKING_MOVIE_QUEUE:
            movieKey = gGameSession->location;
            if (task->spawnArg1.value != 0) {
                movieKey.loc.view = MIST_PARKING_ALTERNATE_DEPARTURE_MOVIE_ID;
            } else {
                movieKey.loc.view = MIST_PARKING_DEPARTURE_MOVIE_ID;
            }
            movieSlot = streamFindMovieSlot(&movieKey.loc, 0, 0);
            // PLAY_STREAM uses byte zero; the other argument bytes are left untouched.
            commandArgs[0] = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
            task->state = task->state + 1;
            return;
        case MIST_PARKING_MOVIE_WAIT_READY:
            if (cdQueue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case MIST_PARKING_MOVIE_PLAY:
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            return;
        case MIST_PARKING_MOVIE_WAIT_IDLE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            return;
        case MIST_PARKING_MOVIE_RESTORE:
            // Restore before giving the image workspace and display back to gameplay.
            if (streamPollGameRestore(0, 0) == 0) {
                return;
            }
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            SetDispMask(1);
            taskKill(task);
            displayResumeGameLoop();
            return;
    }
}

/// Transfers the departure movie selector to a display task and releases the launcher.
///
/// The room launcher supplies `spawnArg1.value` (zero selects stream 100,
/// nonzero stream 101). Switches to task-only flipping and queues the current
/// camera and packets before teardown. Requires the room's live display resources.
static void _mistParkingStartDepartureMovieTask(Task* task)
{
    displaySpawnTaskFromTable(D_mist_parking_8018FC24, 1, task->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

/// Shows and places the conversation's model, then seeds the delayed pitch progression.
static void _mistParkingInitCutsceneModelPitch(Task* task)
{
    TmdObject* model = task->extra.tmd;

    model->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    task->killCountdown = MIST_PARKING_CUTSCENE_MODEL_PITCH_START;
    actorMsgPlaceEulerZyx(task, 0, &D_mist_parking_8018FC3C, 0);
    task->state = task->state + 1;
}

/// Advances the model's delayed pitch and holds it at one eighth of a turn.
///
/// Requires initialization and a live model root. The shared placement lags the
/// counter by one positive update; the signed halfword can overshoot the limit
/// for one update before being clamped on the next.
static void _mistParkingAdvanceCutsceneModelPitch(Task* task)
{
    if (task->killCountdown > 0) {
        // Apply the previous pitch before publishing the next one.
        _mistParkingApplyPreviousCutsceneModelPlacement(task);
    }

    // The negative angular seed delays movement; it is not a frame countdown.
    if (task->killCountdown < MIST_PARKING_CUTSCENE_MODEL_PITCH_LIMIT) {
        task->killCountdown = task->killCountdown + MIST_PARKING_CUTSCENE_MODEL_PITCH_STEP;
    } else {
        task->killCountdown = MIST_PARKING_CUTSCENE_MODEL_PITCH_LIMIT;
    }
}

#include "../../shared/actor_messages_place_euler_zyx.inc.c"

void mistParkingCutsceneModelPitchTask(Task* task)
{
    enum {
        MIST_PARKING_CUTSCENE_MODEL_INITIALIZE = 0,
        MIST_PARKING_CUTSCENE_MODEL_PITCH      = 1,
        MIST_PARKING_CUTSCENE_MODEL_RELEASE    = 2
    };
    TaskFunc stateHandlers[] = {
        [MIST_PARKING_CUTSCENE_MODEL_INITIALIZE] = _mistParkingInitCutsceneModelPitch,
        [MIST_PARKING_CUTSCENE_MODEL_PITCH]      = _mistParkingAdvanceCutsceneModelPitch,
        [MIST_PARKING_CUTSCENE_MODEL_RELEASE]    = taskKill
    };

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers[task->state](task);
    }
}
