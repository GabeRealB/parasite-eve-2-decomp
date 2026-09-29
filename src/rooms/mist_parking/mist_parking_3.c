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
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

extern GpXformArg D_mist_parking_8018FC3C;

static s32 func_mist_parking_80183AC4(Task* task, s32 arg1, GpXformArg* placement, s32 arg3);

void func_mist_parking_801837B8(Task*);
void func_mist_parking_8018397C(Task*);

TaskDesc D_mist_parking_8018FC24[2] = {
    { 0, 192, func_mist_parking_8018397C, { .model = NULL } },
    { 0, 192, func_mist_parking_801837B8, { .model = NULL } },
};

GpXformArg D_mist_parking_8018FC3C = { { 2105, -910, -3460, 0 }, { 20, 1081, 0, 0 } };

SVECTOR D_mist_parking_8018FC54[2] = {
    { -4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_mist_parking_8018FC64[6] = {
    { 4800, 100, -5558, 0 },
    { 4800, -1463, -5558, 0 },
    { 4800, -1463, -6558, 0 },
    { 4800, 100, -6558, 0 },
    { 5800, 100, -5558, 0 },
    { 5800, -1463, -5558, 0 },
};

GpGridFace D_mist_parking_8018FC94[2] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 1, 4, 0 }, 1, 0 },
};

s16 D_mist_parking_8018FCAC[3] = {
    0,
    1,
    -1,
};

s16* D_mist_parking_8018FCB4[1] = {
    D_mist_parking_8018FCAC,
};

GpGridParams D_mist_parking_8018FCB8 = { NULL, D_mist_parking_8018FC54, D_mist_parking_8018FC64, D_mist_parking_8018FC94, D_mist_parking_8018FCB4, -4800, 6558, 1, 1, 4000, 2 };

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} MistParkingPoseBank1271C;

MistParkingPoseBank1271C D_mist_parking_8018FCDC = { .poses = {
#include "assets/mist_parking_animation_129F8_bank1.inc"
                                                     } };

GpPackedSvec D_mist_parking_8018FD24[46] = {
#include "assets/mist_parking_animation_129F8_bank4.inc"
};

GpAnimRec D_mist_parking_8018FDDC[109] = {
#include "assets/mist_parking_animation_129F8_records.inc"
};

u16 D_mist_parking_8018FF90[20] = {
#include "assets/mist_parking_animation_129F8_indices.inc"
};

GpAnimSet D_mist_parking_8018FFB8 = {
    D_mist_parking_8018FDDC,
    D_mist_parking_8018FF90,
    { NULL, D_mist_parking_8018FCDC.words, NULL, NULL, D_mist_parking_8018FD24, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} MistParkingPoseBank12A20;

MistParkingPoseBank12A20 D_mist_parking_8018FFE0 = { .poses = {
#include "assets/mist_parking_animation_12DCC_bank1.inc"
                                                     } };

GpPackedSvec D_mist_parking_80190040[84] = {
#include "assets/mist_parking_animation_12DCC_bank4.inc"
};

GpAnimRec D_mist_parking_80190190[117] = {
#include "assets/mist_parking_animation_12DCC_records.inc"
};

u16 D_mist_parking_80190364[20] = {
#include "assets/mist_parking_animation_12DCC_indices.inc"
};

GpAnimSet D_mist_parking_8019038C = {
    D_mist_parking_80190190,
    D_mist_parking_80190364,
    { NULL, D_mist_parking_8018FFE0.words, NULL, NULL, D_mist_parking_80190040, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} MistParkingPoseBank12DF4;

MistParkingPoseBank12DF4 D_mist_parking_801903B4 = { .poses = {
#include "assets/mist_parking_animation_1323C_bank1.inc"
                                                     } };

GpPackedSvec D_mist_parking_801903F0[99] = {
#include "assets/mist_parking_animation_1323C_bank4.inc"
};

GpAnimRec D_mist_parking_8019057C[150] = {
#include "assets/mist_parking_animation_1323C_records.inc"
};

u16 D_mist_parking_801907D4[20] = {
#include "assets/mist_parking_animation_1323C_indices.inc"
};

GpAnimSet D_mist_parking_801907FC = {
    D_mist_parking_8019057C,
    D_mist_parking_801907D4,
    { NULL, D_mist_parking_801903B4.words, NULL, NULL, D_mist_parking_801903F0, NULL, NULL, NULL },
};

static void func_mist_parking_801839CC(Task* task);
static void func_mist_parking_80183A28(Task* task);

void func_mist_parking_80183634(s32 arg0)
{
    Task* t = D_mist_parking_80195324;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1.value = arg0;
    return;
kill:
    taskKill(D_mist_parking_80195324);
    D_mist_parking_80195324 = NULL;
}

void func_mist_parking_80183688(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(D_mist_parking_8018D75C, 5U), arg0, 0, 0);
}

void func_mist_parking_801836CC(Task* arg0)
{
    s32 temp_v0;

    temp_v0               = arg0->spawnArg1.value - 1;
    arg0->spawnArg1.value = temp_v0;
    if (temp_v0 < 0) {
        taskKill(arg0);
        Stage_SetEndingFlag();
    }
}

void func_mist_parking_80183708(s32 arg0)
{
    Gp_ResetCap();
    switch (arg0) {
        case 1:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x140, 0x100);
            break;
        case 2:
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x2C0, 0);
            break;
    }
}

void func_mist_parking_80183780(s32 arg0)
{
    GameFlag_SetNibble(0xF1, arg0);
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

    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            Stage_RequestMidiFromMap(0xA);
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state = task->state + 1;
            return;
        case 1:
            key = gGameSession->at4;
            if (task->spawnArg1.value != 0) {
                key.loc.view = 0x65;
            } else {
                key.loc.view = 0x64;
            }
            slot         = Stream_FindSlot(key.raw.data, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(0x61, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->field_1FA == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = task->state + 1;
            return;
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
                return;
            }
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            taskKill(task);
            Display_ResetHeapWrapper();
            return;
    }
}

/// Spawns the display task `D_mist_parking_8018FC24` with the task's
/// `spawnArg1`, sets `gDisplayState.at100.flags.flipMode`, respawns the view tasks and kills itself.
void func_mist_parking_8018397C(Task* arg0)
{
    Display_SpawnWithOt(D_mist_parking_8018FC24, 1, arg0->spawnArg1.value, 0);
    gDisplayState.at100.flags.flipMode = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

static void func_mist_parking_801839CC(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    obj->flags         &= 0xFF7F;
    task->killCountdown = -0x78;
    func_mist_parking_80183AC4(task, 0, &D_mist_parking_8018FC3C, 0);
    task->state = task->state + 1;
}

static void func_mist_parking_80183A28(Task* task)
{
    GpXformArg placement;

    if (task->killCountdown > 0) {
        placement                      = D_mist_parking_8018FC3C;
        D_mist_parking_8018FC3C.rot.vx = task->killCountdown;
        func_mist_parking_80183AC4(task, 0, &placement, 0);
    }

    if (task->killCountdown < 0x200) {
        task->killCountdown = task->killCountdown + 0xF;
    } else {
        task->killCountdown = 0x200;
    }
}

/// Places the task's model at `placement`: its position becomes the
/// coordinate frame's translation, its angles the frame's rotation, from
/// which `RotMatrixZYX` rebuilds the matrix; clearing `flg` makes the frame
/// be recomputed.
static s32 func_mist_parking_80183AC4(Task* task, s32 arg1, GpXformArg* placement, s32 arg3)
{
    GpCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Runs the parking-lot cap cutscene's sub-state handler for `task`, unless the
/// global suspend flag is set.
void func_mist_parking_80183B40(Task* task)
{
    TaskFunc states[3] = { func_mist_parking_801839CC, func_mist_parking_80183A28, taskKill };

    if (Gp_StateF0.field_4 == 0) {
        states[task->state](task);
    }
}
