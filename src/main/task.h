#ifndef MAIN_PRIVATE_TASK_H
#define MAIN_PRIVATE_TASK_H

#include "types.h"

#include "main/task_types.h"

/// The task tables a spawn selects by bank number instead of by address, one
/// entry per bank.
///
/// `taskSpawn` and `taskGetDesc` pick a bank here and then index the
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

/// Initializes an empty execution-list head and selects it for spawning.
///
/// `listHead` must be non-NULL writable bare-head storage, kept live while
/// selected. Sets next to NULL and prev to itself without releasing any tasks
/// previously reachable through it. Selection persists after return.
void taskInitList(TaskNode* listHead);

/// Runs task callbacks in list order and collects tasks whose bodies are released.
///
/// Requires a live initialized bare head and live linked tasks. Selects the head
/// without restoring the previous selection; callbacks must restore temporary
/// switches so tail collection uses the owning head. After each callback, a
/// stop request equal to one is cleared and returns before collection. Otherwise
/// a released task is unlinked and freed in this walk. Callback cursor fields
/// must remain readable and unchanged by release/reuse until advancement, unless
/// the callback requests a stop. New tasks inserted after the cursor can run
/// in the same walk. Neither the head nor the entry list is copied.
void taskExecList(TaskNode* listHead);

/// Runs and selects the default frame list, with the collection contract of `taskExecList`.
///
/// The implementation ignores its `unusedListHead` argument and reads
/// `gTaskDefaultList` directly. The main loop supplies no argument while the
/// display path supplies the default head. Keep the legacy unprototyped
/// declaration to preserve both matched call sequences. Initialization must
/// precede dispatch; the default head stays selected after this call.
void taskExecDefaultList();

/// Runs matching-priority callbacks and collects every released task in a list.
///
/// Requires the live-list and callback-cursor contract of `taskExecList`.
/// Compares only `priority & 0xFF`; nonmatching tasks still undergo collection.
/// Selects the supplied head during dispatch and tail unlinking, then restores
/// the previous selection, including after a cleared stop request ends the walk.
void taskExecListForPriority(TaskNode* listHead, s32 priority);

/// Inert task callback for idle tasks and stop or teardown handoffs.
///
/// `unusedTask` is ignored, retaining the `TaskFunc` signature. Installed as
/// `callback`, it suppresses frame updates; installed as `exitCallback`, it
/// suppresses repeated teardown. The caller owns task and resource release.
void taskNoopCallback(Task* unusedTask);

/// Inert callback for resident task bank 0, slot 12.
///
/// `unusedTask` is ignored and may be NULL. The callback retains the `TaskFunc`
/// interface without changing task state or releasing resources.
void taskNoopBank0Slot12(Task* unusedTask);

/// Frame callback that dispatches the task's current exit handler.
///
/// Resident bank 0, slot 0x18 uses this callback. Requires a live non-NULL
/// `task` with a non-NULL exit handler whose code remains loaded; the handler
/// must not be this callback. Follows `taskCallExit`'s lifetime contract:
/// cleanup is entirely the handler's responsibility, and it may release the
/// task before returning.
void taskExitCallback(Task* task);

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

/// Launches an external debug task when enabled, then tears down the launcher.
///
/// Resident bank 5, slot 1 installs this callback on a bodyless task.
/// Nonzero `gDisplayState.debugMode` requests descriptor 0 at 0x80725C54
/// on the currently selected execution list, with both spawn payloads zero.
/// That address must provide a readable `TaskDesc`; its callback code and
/// any borrowed model geometry must remain loaded while used. The backing
/// debug image and descriptor contents are unproven.
///
/// Spawn failure is ignored. Every call then performs `taskKill` on the live,
/// non-NULL `task`, following its teardown contract even when debug is disabled.
/// The task and its resources may be released before return.
void taskDebugLaunchCallback(Task* task);

#endif // MAIN_PRIVATE_TASK_H
