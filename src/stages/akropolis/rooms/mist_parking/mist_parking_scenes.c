#include "mist_parking_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
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
#include "../../../../shared/actor/actor_messages.h"

#define ROOM_PART declarations_mist_parking_scenes
#include "assets/rooms/mist/parking/declarations.inc"

static void _mistParkingPlayDepartureMovieTask(Task* task);
static void _mistParkingStartDepartureMovieTask(Task* task);

TaskDesc gMistParkingDepartureMovieTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistParkingStartDepartureMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistParkingPlayDepartureMovieTask, { .value = 0 } },
};

#define ROOM_PART _gMistParkingCutsceneModelPlacement
#include "assets/rooms/mist/parking/events.inc"
static SVECTOR _gMistParkingPierceCollisionPatchNormals[2] = {
#include "assets/mist_parking_collision_126F8_normals.inc"
};

static SVECTOR _gMistParkingPierceCollisionPatchVerts[6] = {
#include "assets/mist_parking_collision_126F8_verts.inc"
};

static WorldCollisionGridFace _gMistParkingPierceCollisionPatchFaces[2] = {
#include "assets/mist_parking_collision_126F8_faces.inc"
};

static s16 _gMistParkingPierceCollisionPatchCells[4] = {
#include "assets/mist_parking_collision_126F8_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
#define MIST_PARKING_PIERCE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gMistParkingPierceCollisionPatchCells[(halfwordOffset)])
static s16* _gMistParkingPierceCollisionPatchTable[1] = {
#include "assets/mist_parking_collision_126F8_table.inc"
};
#undef MIST_PARKING_PIERCE_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gMistParkingPierceCollisionPatch = {
    .viewCoord   = NULL,
    .normals     = _gMistParkingPierceCollisionPatchNormals,
    .vertices    = _gMistParkingPierceCollisionPatchVerts,
    .faces       = _gMistParkingPierceCollisionPatchFaces,
    .cellFaceIds = _gMistParkingPierceCollisionPatchTable,
    .xBias       = -4800,
    .zBias       = 6558,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gMistParkingPierceCollisionPatchFaces)
};

#include "assets/rooms/mist/parking/clips_3.inc"

static void _mistParkingInitCutsceneModelPitch(Task* task);
static void _mistParkingAdvanceCutsceneModelPitch(Task* task);

/// Pitch progression in actor-angle units, advanced once per active callback.
enum {
    CUTSCENE_MODEL_PITCH_START = -120,
    CUTSCENE_MODEL_PITCH_STEP  = 15,
    CUTSCENE_MODEL_PITCH_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 8
};

/// Applies the previous shared placement while recording this callback's pitch.
static inline void _mistParkingApplyPreviousCutsceneModelPlacement(Task* task)
{
    ActorTransform previousPlacement = _gMistParkingCutsceneModelPlacement;

    _gMistParkingCutsceneModelPlacement.rot.vx = task->killCountdown;
    actorMsgPlaceEulerZyx(task, 0, &previousPlacement, 0);
}

void mistParkingControlPlayerHeadAim(s32 mode)
{
    Task* task = gMistParkingPlayerHeadAimTask;

    if (task == NULL) {
        return;
    }
    switch (mode) {
        case MIST_PARKING_HEAD_AIM_FOLLOW_ANIMATION:
        case MIST_PARKING_HEAD_AIM_FORCE:
            task->spawnArg1.value = mode;
            break;
        default:
            taskKill(gMistParkingPlayerHeadAimTask);
            gMistParkingPlayerHeadAimTask = NULL;
            break;
    }
}

void mistParkingQueueDelayedDisplayModeExit(s32 delayTicks)
{
    enum { DISPLAY_EXIT_DESCRIPTOR_INDEX = 5 };

    displayQueueModeTask(taskGetDescAt(gMistParkingConversationTaskDescs, DISPLAY_EXIT_DESCRIPTOR_INDEX), delayTicks, 0, STAGE_ENTRY_RELOAD);
}

#include "mist_parking_display_mode_exit.inc.c"

void mistParkingDelayDisplayModeExitTask(Task* task)
{
    _mistParkingDelayDisplayModeExit(task);
}

void mistParkingSelectDialogueResource(s32 resourceOrdinal)
{
    enum {
        DEPARTURE_FONT_VRAM_X = 320,
        DEPARTURE_FONT_VRAM_Y = 256,
        PRIZE_FONT_VRAM_X     = 704,
        PRIZE_FONT_VRAM_Y     = 0
    };

    capReset();
    switch (resourceOrdinal) {
        case MIST_PARKING_DIALOGUE_DEPARTURE_MENU:
            gCapFile = NULL;
            capSelectLoadedFile(MIST_PARKING_DIALOGUE_DEPARTURE_MENU);
            capSetTexturePage(DEPARTURE_FONT_VRAM_X, DEPARTURE_FONT_VRAM_Y);
            break;
        case MIST_PARKING_DIALOGUE_PRIZES:
            gCapFile = NULL;
            capSelectLoadedFile(MIST_PARKING_DIALOGUE_PRIZES);
            capSetTexturePage(PRIZE_FONT_VRAM_X, PRIZE_FONT_VRAM_Y);
            break;
    }
}

void mistParkingSetConversationProgress(s32 progress)
{
    gameFlagSetNibble(EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE, progress);
}

void mistParkingResetCutsceneTaskHandles(s32 unused)
{
    gMistParkingCutsceneModelTask = NULL;
    gMistParkingPlayerHeadAimTask = NULL;
}

/// Plays one of the two departure movies and restores game presentation.
static void _mistParkingPlayDepartureMovieTask(Task* task)
{
    enum {
        MOVIE_PREPARE                = 0,
        MOVIE_QUEUE                  = 1,
        MOVIE_WAIT_READY             = 2,
        MOVIE_PLAY                   = 3,
        MOVIE_WAIT_IDLE              = 4,
        MOVIE_RESTORE                = 5,
        DEPARTURE_MOVIE_ID           = 100,
        ALTERNATE_DEPARTURE_MOVIE_ID = 101,
        MOVIE_MUSIC_FADE_TICKS       = 10
    };
    u8              commandArgs[4];
    GameLocationKey movieKey;
    CdCmdQueue*     cdQueue;
    s16             movieSlot;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case MOVIE_PREPARE:
            // Save the displaced VRAM images before reserving movie workspace.
            stageMusicRequestAreaStop(MOVIE_MUSIC_FADE_TICKS);
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state = task->state + 1;
            return;
        case MOVIE_QUEUE:
            movieKey = gGameSession->location;
            if (task->spawnArg1.value != 0) {
                movieKey.view = ALTERNATE_DEPARTURE_MOVIE_ID;
            } else {
                movieKey.view = DEPARTURE_MOVIE_ID;
            }
            movieSlot = streamFindMovieSlot(&movieKey, 0, 0);
            // PLAY_STREAM uses byte zero; the other argument bytes are left untouched.
            commandArgs[0] = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
            task->state = task->state + 1;
            return;
        case MOVIE_WAIT_READY:
            if (cdQueue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case MOVIE_PLAY:
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
        case MOVIE_WAIT_IDLE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            return;
        case MOVIE_RESTORE:
            // Restore before giving the image workspace and display back to gameplay.
            if (streamPollGameRestore(0, 0) == 0) {
                return;
            }
            memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
            SetDispMask(1);
            taskKill(task);
            displayResumeGameLoop();
            return;
    }
}

/// Transfers the departure movie selector to a display task and releases the launcher.
static void _mistParkingStartDepartureMovieTask(Task* task)
{
    displaySpawnTaskFromTable(gMistParkingDepartureMovieTaskDescs, 1, task->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

/// Shows and places the conversation's model, then seeds the delayed pitch progression.
static void _mistParkingInitCutsceneModelPitch(Task* task)
{
    TmdObject* model = task->extra.tmd;

    model->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    task->killCountdown = CUTSCENE_MODEL_PITCH_START;
    actorMsgPlaceEulerZyx(task, 0, &_gMistParkingCutsceneModelPlacement, 0);
    task->state = task->state + 1;
}

/// Advances the model's delayed pitch and holds it at one eighth of a turn.
static void _mistParkingAdvanceCutsceneModelPitch(Task* task)
{
    if (task->killCountdown > 0) {
        // Apply the previous pitch before publishing the next one.
        _mistParkingApplyPreviousCutsceneModelPlacement(task);
    }

    // The negative angular seed delays movement; it is not a frame countdown.
    if (task->killCountdown < CUTSCENE_MODEL_PITCH_LIMIT) {
        task->killCountdown = task->killCountdown + CUTSCENE_MODEL_PITCH_STEP;
    } else {
        task->killCountdown = CUTSCENE_MODEL_PITCH_LIMIT;
    }
}

#include "../../../../shared/actor/actor_messages_place_euler_zyx.inc.c"

void mistParkingCutsceneModelPitchTask(Task* task)
{
    enum {
        INITIALIZE = 0,
        PITCH      = 1,
        RELEASE    = 2
    };
    TaskFunc stateHandlers[] = {
        [INITIALIZE] = _mistParkingInitCutsceneModelPitch,
        [PITCH]      = _mistParkingAdvanceCutsceneModelPitch,
        [RELEASE]    = taskKill
    };

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers[task->state](task);
    }
}
