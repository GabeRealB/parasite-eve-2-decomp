#ifndef GAMEPLAY_PRIVATE_LOADING_H
#define GAMEPLAY_PRIVATE_LOADING_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room-resource loading, view setup and sprite-list construction.

/// Phases shared by the base-resource and additional-file loading passes.
enum {
    LOADING_AREA_INIT  = 0,
    LOADING_AREA_QUEUE = 1,
    LOADING_AREA_WAIT  = 2
};

/// Phase for `Gp_PollAreaCdLoads` (0 init, 1 walk/enqueue, 2 wait idle).
/// `Gp_LoadWaitAreaCd` clears it when phase 1 (`func_800AA120`) finishes
/// so phase 2 can start.
extern s16 Gp_AreaCdPhase;

/// Phase for `func_800AA120`. `Gp_LoadWaitAreaCd` clears it when entering
/// its own phase 1.
extern u16 D_80114C70;

/// Phase for `Gp_LoadWaitAreaCd` (0 init, 1 `func_800AA120`, 2 `Gp_PollAreaCdLoads`).
/// `Gp_LoadWaitSave` clears it when advancing to this task state.
extern u16 D_80114C74;

/// Queues additional files selected by the layout's placements; returns 1 when finished.
u16 Gp_PollAreaCdLoads(void);

/// Queues the layout's base resources and their texture relocation; returns 1 when finished.
u16 func_800AA120(void);

extern const TaskFuncTable3 Gp_SessionStates;

extern const TaskFuncTable8 Gp_LoadStateFns;

extern const TaskFuncTable3 Gp_RoomObjStates;

void Gp_EnqueueViewCd(Task* task);

/// Applies the companion texture relocation to a task's model and its cached packets.
///
/// A live TMD task must have a valid `extra.tmd`; other body kinds are unchanged.
/// Sets an encoded texture-page displacement of 4 (256 VRAM words horizontally)
/// and a CLUT displacement of 6 rows. Rebuilds both buffer halves when present,
/// preserving the half selector; a missing buffer retains the offsets for its
/// later construction. Does not allocate, draw, change task state or kill it.
/// Existing buffers and borrowed sources must satisfy `tmdBuildBufferHalf`'s
/// contract, with no GPU work still reading packets that will be rebuilt.
void companionRelocateModelTextures(Task* companionTask);

void Gp_FinishLoadWait(Task* task);

void Gp_LoadViewAndCd(u8 arg0);

void Gp_LoadViewImages(void);

void Gp_EnqueueStageCd(void);

void Gp_EnqueueCompanionCd(u8 type, u8 variant);

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
extern ViewCountTable* Gp_ViewCountTables[];

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
extern ViewIndexTable* Gp_ViewIndexTables[];

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
extern WorldCollisionStageResources* Gp_RoomObjTables[];

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`. Each
/// entry is an array of `DirectionWarpEntry*`, indexed 1-based by
/// `GameSession.location.loc.area` / `GameLocationKey.area`.
extern DirectionWarpEntry** Gp_WarpTables[];

void func_800AA548(s32 arg0);

void Gp_BeginSessionTask(Task* arg0);

void Gp_LoadWaitBoot(Task* task);

void Gp_LoadWaitStage(Task* task);

void Gp_LoadState2(Task* task);

void Gp_LoadWaitCompanion(Task* task);

void Gp_LoadWaitSave(Task* task);

void Gp_LoadWaitAreaCd(Task* task);

void Gp_FadeGrayHold(Task* task);

/// Refreshes room collision/view state and queues the current view's clipping packets.
///
/// State 1 of `loadingRoomResourcesTask`. A live `task` caches the last logical
/// view in `spawnArg1.value`; a change invalidates and rebuilds the view cache.
/// A nonzero `roomObjsDirty` reapplies the camera and replaces borrowed collision
/// resources before clearing the request. Clipping commands are queued on every
/// call. Requires valid loaded view/room resources, live old lists until clearing,
/// sufficient current-frame packet storage and a 1024-tag depth-sorted OT.
void loadingUpdateRoomResourcesTask(Task* task);

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
extern ViewCameraTable* Gp_ViewTables[];

extern DR_STP D_80114C50;

void Gp_ViewLoadImage(Task* task);

extern s16 D_80114C40;

void Gp_ViewBeginLoad(Task* task);

#endif // GAMEPLAY_PRIVATE_LOADING_H
