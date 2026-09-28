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

u16 Gp_PollAreaCdLoads(void);

u16 func_800AA120(void);

extern const TaskFuncTable3 Gp_SessionStates;

extern const TaskFuncTable8 Gp_LoadStateFns;

extern const TaskFuncTable3 Gp_RoomObjStates;

void Gp_EnqueueViewCd(Task* task);

void Gp_PumpTmdStream(Task* task);

void Gp_FinishLoadWait(Task* task);

void Gp_LoadViewAndCd(u8 arg0);

void Gp_LoadViewImages(void);

void Gp_EnqueueStageCd(void);

void Gp_EnqueueCompanionCd(u8 type, u8 variant);

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`.
extern GpViewCountTbl* Gp_ViewCountTables[];

/// Per-stage pointer table. Index is `GpAreaKey.stage - 1`.
/// Each entry is an array of `GpRoomCoordRec*`, indexed by `field_2 - 1`.
extern GpRoomCoordRec** Gp_RoomCoordTables[];

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`.
extern GpViewIndexTbl* Gp_ViewIndexTables[];

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`.
extern GpRoomObjTbl* Gp_RoomObjTables[];

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`. Each
/// entry is an array of `GpWarpRec*`, indexed 1-based by
/// `GameSession.at4.loc.area` / `GpAreaKey.area`.
extern GpWarpRec** Gp_WarpTables[];

void func_800AA548(s32 arg0);

void Gp_BeginSessionTask(Task* arg0);

void Gp_LoadWaitBoot(Task* task);

void Gp_LoadWaitStage(Task* task);

void Gp_LoadState2(Task* task);

void Gp_LoadWaitCompanion(Task* task);

void Gp_LoadWaitSave(Task* task);

void Gp_LoadWaitAreaCd(Task* task);

void Gp_FadeGrayHold(Task* task);

void Gp_RoomObjState1(Task* task);

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`.
extern GpViewTbl* Gp_ViewTables[];

extern DR_STP D_80114C50;

void Gp_ViewLoadImage(Task* task);

extern s16 D_80114C40;

void Gp_ViewBeginLoad(Task* task);

#endif // GAMEPLAY_PRIVATE_LOADING_H
