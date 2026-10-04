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

/// Counts down a task and, on reaching zero, releases its body and marks it for collection.
///
/// Every dispatch decrements the signed `killCountdown`. The releasing dispatch
/// is the one that stores zero, so a positive count lasts that many dispatches.
/// A count that is already zero or negative releases only on a later dispatch
/// that stores zero. A TMD model is unlinked from the live model list and
/// freed, including any primitive buffer it owns. A coordinate body is freed
/// with its list link left unchanged. Any other kind receives only the mark.
/// `extra` keeps its pointer, which must not be dereferenced once the task is
/// marked.
///
/// The mark is `bodyKind` 0xFF. The walk that called this callback may unlink
/// and free the task on return, in that same walk. A stop request leaves the
/// marked task linked until a later walk reaches it without stopping. The mark
/// makes the task eligible for collection without reserving another pass. A
/// later dispatch before collection finds the count already off zero and leaves
/// the body alone. The task, its work block and its children stay allocated
/// across this call.
///
/// Normal model teardown installs this callback with a count of two after
/// suppressing the model's active drawing. System-bank descriptor 1 also names
/// it as the callback of a task that attaches no body.
void taskCountdownCallback(Task* task);

void Task_KillMaybeSpawn(Task* task);

#endif // MAIN_PRIVATE_TASK_H
