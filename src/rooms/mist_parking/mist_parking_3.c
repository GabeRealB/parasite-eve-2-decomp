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

void func_mist_parking_801837B8(Task*);
void func_mist_parking_8018397C(Task*);

TaskDesc D_mist_parking_8018FC24[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_mist_parking_8018397C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_parking_801837B8, { .value = 0 } },
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

static void func_mist_parking_801839CC(Task* task);
static void func_mist_parking_80183A28(Task* task);

void func_mist_parking_80183634(s32 arg0)
{
    Task* t = D_mist_parking_80195324;

    if (t == NULL) {
        return;
    }
    switch (arg0) {
        case 0:
        case 1:
            t->spawnArg1.value = arg0;
            break;
        default:
            taskKill(D_mist_parking_80195324);
            D_mist_parking_80195324 = NULL;
            break;
    }
}

void func_mist_parking_80183688(s32 arg0)
{
    displayQueueModeTask(taskGetDescAt(D_mist_parking_8018D75C, 5U), arg0, 0, STAGE_ENTRY_RELOAD);
}

void func_mist_parking_801836CC(Task* arg0)
{
    s32 temp_v0;

    temp_v0               = arg0->spawnArg1.value - 1;
    arg0->spawnArg1.value = temp_v0;
    if (temp_v0 < 0) {
        taskKill(arg0);
        stageRequestModeTaskExit();
    }
}

void func_mist_parking_80183708(s32 arg0)
{
    capReset();
    switch (arg0) {
        case 1:
            Gp_CapFile = 0;
            capSelectLoadedFile(1);
            capSetTexturePage(0x140, 0x100);
            break;
        case 2:
            Gp_CapFile = 0;
            capSelectLoadedFile(2);
            capSetTexturePage(0x2C0, 0);
            break;
    }
}

void func_mist_parking_80183780(s32 arg0)
{
    gameFlagSetNibble(GAME_FLAG_0F1, arg0);
}

/// Drops the handles in `D_mist_parking_80195320` and
/// `D_mist_parking_80195324` without killing their tasks. Its caller passes
/// an argument, which is unused.
void func_mist_parking_801837A4(s32 arg0)
{
    D_mist_parking_80195320 = 0;
    D_mist_parking_80195324 = 0;
}

void func_mist_parking_801837B8(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            Stage_RequestMidiFromMap(0xA);
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state = task->state + 1;
            return;
        case 1:
            key = gGameSession->location;
            if (task->spawnArg1.value != 0) {
                key.loc.view = 0x65;
            } else {
                key.loc.view = 0x64;
            }
            slot         = streamFindMovieSlot(&key.loc, 0, 0);
            slotParam[0] = slot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 3:
            if (cdCmdIsIdle() & 0xFFFF) {
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
        case 4:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
                return;
            }
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            SetDispMask(1);
            taskKill(task);
            displayResumeGameLoop();
            return;
    }
}

/// Spawns the display task `D_mist_parking_8018FC24` with the task's
/// `spawnArg1`, sets `gDisplayState.control.flags.flipMode`, respawns the view tasks and kills itself.
void func_mist_parking_8018397C(Task* arg0)
{
    Display_SpawnWithOt(D_mist_parking_8018FC24, 1, arg0->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

static void func_mist_parking_801839CC(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    obj->flags         &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    task->killCountdown = -0x78;
    actorMsgPlaceEulerZyx(task, 0, &D_mist_parking_8018FC3C, 0);
    task->state = task->state + 1;
}

static void func_mist_parking_80183A28(Task* task)
{
    ActorTransform placement;

    if (task->killCountdown > 0) {
        placement                      = D_mist_parking_8018FC3C;
        D_mist_parking_8018FC3C.rot.vx = task->killCountdown;
        actorMsgPlaceEulerZyx(task, 0, &placement, 0);
    }

    if (task->killCountdown < 0x200) {
        task->killCountdown = task->killCountdown + 0xF;
    } else {
        task->killCountdown = 0x200;
    }
}

#include "../../shared/actor_messages_place_euler_zyx.inc.c"

/// Runs the parking-lot cap cutscene's sub-state handler for `task`, unless the
/// global suspend flag is set.
void func_mist_parking_80183B40(Task* task)
{
    TaskFunc states[3] = { func_mist_parking_801839CC, func_mist_parking_80183A28, taskKill };

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states[task->state](task);
    }
}
