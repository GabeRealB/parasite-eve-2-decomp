#ifndef MAIN_PRIVATE_TASK_H
#define MAIN_PRIVATE_TASK_H

#include "types.h"

#include "main/task_types.h"

/// The task tables a spawn selects by bank number instead of by address, one
/// entry per bank.
///
/// `Task_Spawn` and `Task_GetDesc` pick a bank here and then index the
/// `TaskDesc` run it points at. Several banks share one table.
extern TaskDesc* gTaskDescBanks[15];

extern TaskNode gTaskDisplayList;

extern TaskDesc D_80062780[];

extern TaskDesc D_800626AC[];

extern TaskDesc D_800670D0[];

extern TaskDesc D_800676A8[];

extern TaskDesc D_80067734[];

extern TaskDesc D_80067828[];

extern TaskDesc D_800678F4[];

extern TaskDesc D_80068B7C[];

void Task_InitList(TaskNode* node);

void Task_ExecList(TaskNode* node);

/// Runs the default frame list. The body reloads `gTaskDefaultList` itself, so
/// the argument is not read.
/// Legacy ABI: GameMain_Loop passes no argument, while the display path
/// passes the default-list pointer. The implementation ignores that argument.
/// Keep this declaration unprototyped to preserve both original call sequences.
void Task_ExecDefaultList();

void Task_ExecListFiltered(TaskNode* node, s32 filter);

/// Inert task callback for idle tasks and stop or teardown handoffs.
///
/// `unusedTask` is ignored, retaining the `TaskFunc` signature. Installed as
/// `callback`, it suppresses frame updates; installed as `exitCallback`, it
/// suppresses repeated teardown. The caller owns task and resource release.
void taskNoopCallback(Task* unusedTask);

/// Task callback that counts a task's `killCountdown` down and releases the body
/// it owns when the count reaches zero: a TMD model comes off the model list and
/// has its buffer and object freed, a coordinate body is freed, and a task owning
/// neither is only marked. The mark is `bodyKind` 0xFF, which the next exec pass
/// collects the task on.
void taskCountdownCallback(Task* task);

s32 TaskIdMap_RemapIndex(s32 arg0, s32 arg1, s32 arg2);

void Task_KillMaybeSpawn(Task* task);

#endif // MAIN_PRIVATE_TASK_H
