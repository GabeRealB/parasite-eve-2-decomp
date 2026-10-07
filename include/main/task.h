#ifndef MAIN_TASK_H
#define MAIN_TASK_H

#include "types.h"

#include "main/task_types.h"

/// Resident head of the scheduler's default priority-ordered execution list.
///
/// The normal frame walk selects this head before dispatching callbacks.
/// Ordinary spawns join the currently selected list; the default-list spawn
/// helpers temporarily select this head even when called from another list.
///
/// Initialize with `taskResetDefaultList` before spawning. An empty list has
/// `next == NULL` and `prev == &gTaskDefaultList`; zeroed BSS alone does not
/// satisfy this invariant. The head has program lifetime and is never a `Task`.
///
/// Reset discards the links without invoking exit callbacks or releasing any
/// allocations. Boot and session resets pair it with heap reinitialization.
extern TaskNode gTaskDefaultList;

/// Defers primitive-buffer allocation for newly spawned TMD task bodies.
///
/// `false` uses the descriptor's normal buffer policy; `true` defers allocation
/// without disabling later missing-buffer recovery. A descriptor that disables
/// automatic buffers keeps that policy in either case. Existing bodies and
/// direct model attachments are unaffected.
///
/// This resident 32-bit Boolean starts `false`. The menu sets it while rebuilding
/// weapon attachments before the auxiliary heap is reused for the view, then
/// clears it before returning. Callers must clear it after a temporary override.
extern bool gTaskDeferModelBufferAllocation;

/// Six task descriptors. Entry 5 is a model descriptor whose model is not
/// fixed: callers store the model in its `data.model` just before spawning effect
/// 0x80005, which spawns its task from that entry.
extern TaskDesc D_800626EC[6];

/// Task descriptors of bank 1. Entry 0x32 is a model descriptor whose model is
/// not fixed: callers store the model in its `data.model` just before spawning
/// `EFFECT_FLYING_BODY_PART`, which spawns its task from that entry.
extern TaskDesc D_800670D0[];

extern TaskDesc Stage_MusicTaskDesc;

/// Spawns an indexed descriptor onto the currently selected execution list.
///
/// `table[index]` must be a live, non-terminator descriptor; the signed element
/// index is unchecked. The selected head must be non-NULL and initialized,
/// with tasks ordered by ascending byte priority. Equal priorities retain spawn
/// order. A task inserted after a running walk's cursor can run in that walk.
///
/// Reads the descriptor synchronously and copies the two payload words without
/// invoking the callback or retaining the descriptor. The callback interprets
/// payloads and determines pointer ownership/lifetime. Keep callback code and
/// borrowed model geometry loaded while used. The task owns any attached body;
/// its initial exit callback is `taskKill`. Returns NULL if task allocation or
/// required body attachment fails, including an unrecognized nonzero body kind.
/// A missing primitive buffer alone does not fail model spawning.
Task* taskSpawnFromTable(TaskDesc* table, s32 index, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2);

/// Spawns a bank entry or a directly supplied descriptor onto the selected execution list.
///
/// For `bank` in 0..14, `selector.value` is an unchecked signed element index
/// in that bank's table. Every negative bank instead uses `selector.pointer`
/// as a live, non-NULL `TaskDesc*`, without indexing it. The selected descriptor
/// must not be a terminator. The current list head must be live, initialized
/// and ordered by ascending byte priority; equal priorities retain spawn order.
///
/// Reads the descriptor synchronously, retaining no pointer and invoking no
/// callback. Copies both payload words; their interpretation and pointer
/// ownership/lifetime belong to the callback. Keep callback code and borrowed
/// model geometry loaded while used. The new task owns its body and starts
/// with `taskKill` as its exit handler. Insertion after a walk's cursor can
/// dispatch the task in that same walk. Returns NULL on task allocation or
/// required body-attachment failure; a missing model primitive buffer alone
/// does not fail spawning.
Task* taskSpawn(s32 bank, TaskSpawnArg selector, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2);

static __inline__ Task* Task_SpawnPtr(s32 bank, s32 type, s32 arg2, const void* data)
{
    return taskSpawn(bank, type, arg2, data);
}

/// Spawns an indexed descriptor onto the default execution list, restoring the selected list.
///
/// `gTaskDefaultList` must be initialized. `table[index]` and both payload words
/// follow `taskSpawnFromTable`'s bounds, lifetime and ownership contract; the
/// signed element index is unchecked. Payload interpretation belongs to the
/// callback. Returns the new task or NULL on allocation or body-attachment
/// failure, restoring the previous list selection in either case. The callback
/// can run in a later default-list walk, or this walk if insertion follows its cursor.
Task* taskSpawnFromTableOnDefaultList(TaskDesc* table, s32 index, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2);

/// Spawns a bank entry or direct descriptor onto the default execution list.
///
/// `gTaskDefaultList` must be initialized. `bank`, `selector` and the payload
/// words follow `taskSpawn`'s bounds, lifetime and ownership contract: banks
/// 0..14 select an unchecked signed index, and every negative bank selects a
/// live descriptor pointer. The callback is not invoked during spawning.
///
/// Returns the task or NULL on allocation/body-attachment failure, restoring
/// the previous list selection in either case. A missing model primitive buffer
/// alone does not fail spawning. Insertion after the current default-list walk's
/// cursor can dispatch the new task in that same walk.
Task* taskSpawnOnDefaultList(s32 bank, TaskSpawnArg selector, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2);

/// Performs default task teardown, releasing its resources and arranging collection.
///
/// `task` must be non-NULL, live and not already torn down. Each child receives
/// its own `exitCallback` with its `parent` cleared; this task then leaves its
/// parent's sibling ring and releases non-NULL primary-heap `work`. Callers must
/// first release nested resources and clear borrowed work. This is the initial
/// exit handler; calling it directly does not dispatch a replacement handler.
///
/// Normal teardown suppresses repeated exit dispatch. A TMD model stops active
/// drawing but stays linked and allocated for two countdown-callback invocations
/// before release. A coordinate body is unlinked and freed during this call;
/// bodyless tasks have no body to release. Non-model tasks receive an inert frame
/// callback and are marked `bodyKind` 0xFF here; models receive that mark when
/// their countdown releases them. A walker can collect a marked task after the
/// dispatched callback returns, during the same walk, unless `stopTaskWalk` ends
/// it first. Released work and body pointer slots are left unchanged.
///
/// With `gDisplayState.immediateTaskFree` nonzero, the body and task are released
/// before this call returns. Any task freed at its execution list's tail must
/// belong to `gTaskDefaultList`, including recursively torn-down children; the
/// previously selected list is restored. Child exit handlers must preserve their
/// sibling successor through the caller's post-handler read, even when releasing
/// the child immediately. Callers must not access the released task or resources.
void taskKill(Task* task);

/// Dispatches each child's exit handler, then clears the parent's child head.
///
/// `parentTask` must be non-NULL and remain live throughout the call. A nonempty
/// child list must be a closed sibling ring of live tasks with loaded, non-NULL
/// exit handlers. Each child's `parent` is cleared before its handler runs, so
/// default teardown leaves the sibling traversal intact. Cleanup follows each
/// handler's contract; a replacement handler may leave its child allocated.
///
/// The successor is read after the handler returns. Handlers must preserve that
/// link's storage and the remaining ring until consumed, including when freeing
/// a child immediately; released storage must not be reused during traversal.
/// Sibling links are not reset to self-links. The parent keeps its own resources,
/// handlers, parent relationship and execution-list membership. Its child head
/// is cleared even when there were no children; no task is allocated here.
void taskCallChildExits(Task* parentTask);

/// Dispatches a live task's current exit callback once.
///
/// `task` and its handler must be non-NULL, with handler code still loaded.
/// Cleanup follows that handler's contract; this call itself neither marks nor
/// collects the task. A replacement handler can suppress or customize teardown,
/// and a handler can release the task before returning. Do not assume the task
/// or its resources remain live after this call.
void taskCallExit(Task* task);

/// Removes a live task from its parent's circular child ring.
///
/// `task` must be non-NULL and remain live. When it has a parent, that parent
/// must be live and its `firstChild` must head a closed ring containing this
/// task. Each sibling is live and names that same parent. An only child is the
/// task whose `nextSibling` points at itself; removing it clears the parent's
/// child head. Removing any other child keeps the remaining order and advances
/// the head only when this task is the head. The task then has no parent, and
/// its `nextSibling` points at itself.
///
/// A parentless task is left unchanged, including its sibling link. The task
/// keeps its own children, execution-list membership and resources, so a later
/// teardown of the former parent does not dispatch this task. Nothing is
/// allocated or released, and no handler runs.
void taskDetachFromParent(Task* task);

/// Moves a live task to the end of a parent's circular child ring for teardown.
///
/// Both arguments must be non-NULL and remain live throughout the call. The
/// destination and any existing source ring must be closed rings of live tasks,
/// with each child's `parent` identifying its ring owner. A parentless task must
/// have `nextSibling == task`. The caller must avoid cycles in the teardown tree;
/// this function performs no validation and NULL does not request detachment.
///
/// Removal advances the old ring's head when needed, or clears it for an only
/// child. Insertion preserves an existing destination head; an empty destination
/// receives this task as its only child. Using the current parent removes and
/// appends the task again, which can change child teardown order. The task keeps
/// its own children, execution-list membership and resources. No allocation,
/// release, callback dispatch or coordinate attachment occurs.
void taskReparent(Task* newParent, Task* task);

/// Dispatches matching-priority exits and collects every released task in a list.
///
/// `listHead` must be a live, initialized bare head. Only the low byte of the
/// signed `priority` selects exit callbacks, but every visited task is checked
/// for body-release marking and collection, including nonmatching tasks.
/// The head is selected for callbacks and tail unlinking; the prior selection
/// is restored on every return. Callbacks must restore temporary list switches.
/// A stop request equal to one is cleared and ends the walk before collection
/// or cursor advancement, including after the callback marks the task.
///
/// After dispatch, the walker reads the cursor's body kind and forward link.
/// A handler freeing that task must preserve their storage until advancement,
/// or request a stop before returning. In particular, immediate grenade exits
/// rely on primary-heap release retaining those bytes with no intervening reuse.
/// Spawns can extend the walk; this is not a snapshot of the entry list.
void taskCallExitForPriority(TaskNode* listHead, s32 priority);

/// Borrows the indexed descriptor from a task bank without spawning a task.
///
/// `bank` must be in 0..14 and `index` is an unsigned element index into that
/// bank's live table; neither is checked. The returned entry is not copied and
/// its table must remain loaded while used. Banks 11..13 share bank 2's table,
/// so those results alias. A terminator must not be used as a spawn recipe.
TaskDesc* taskGetDesc(u32 bank, u32 index);

/// Borrows an entry at an unchecked unsigned element index in a descriptor table.
///
/// `table` must remain live, and the selected entry must exist. No descriptor is
/// copied and no task is spawned. Callers using the result must keep the table
/// loaded; a terminator is not a spawn recipe.
TaskDesc* taskGetDescAt(TaskDesc* table, u32 index);

/// Suspends a task with a result for its polling caller and dispatches its children's exits.
///
/// `task` must be non-NULL and remain live throughout this call. Stores the
/// signed 32-bit `result`, sets the stop-request status 0xFF and replaces the
/// frame callback with an inert handler. The result's meaning belongs to the
/// task and its caller; zero is a result value, not an incomplete request.
///
/// Each child's parent is cleared before its exit handler runs; traversal and
/// handler lifetime follow `taskCallChildExits`'s contract. Clears the child
/// head afterward. Keeps this task's own work, body, parent relationship and
/// execution-list membership, without invoking its own exit handler. A caller
/// completes the handoff with `taskPollKill`; its exit handler must stay loaded.
void taskRequestKill(Task* task, s32 result);

/// Dispatches a requested task exit, optionally returning its stored result.
///
/// `task` must be live and non-NULL with a loaded, non-NULL exit handler. Returns
/// false unless its status is 0xFF, leaving the output and task unchanged.
/// On a request, copies the signed result to writable `resultOut` when non-NULL
/// before dispatching the exit, then returns true. Store the output outside any
/// resources the handler can release if it must remain readable afterward.
///
/// The Boolean reports dispatch, not the result value or completed release.
/// Cleanup follows the handler's contract: it may retain the task, defer body
/// release or free it immediately. The status is not cleared. Stop polling
/// after success; the task and its resources may no longer be live.
bool taskPollKill(Task* task, s32* resultOut);

/// Borrows the currently selected head used for spawning and tail unlinking.
///
/// May return NULL before task-list initialization. An empty initialized list
/// instead has a non-NULL bare head whose next link is NULL. Owns neither the
/// head nor its tasks; callers save this value when temporarily switching lists.
TaskNode* taskGetActiveList(void);

/// Selects an existing task-list head without initializing or walking it.
///
/// Spawning and tail unlinking require a live, initialized, non-NULL bare head.
/// Selection borrows its storage and persists until changed; temporary users
/// must save and restore the previous selection. Does not alter any list links.
void taskSetActiveList(TaskNode* listHead);

/// Discards the default list's links and selects its empty head.
///
/// Sets next to NULL and prev to the head. Does not dispatch exits or release
/// discarded tasks and resources; boot/session callers pair this with heap
/// reset. Do not use it as live-task teardown.
void taskResetDefaultList(void);

#endif // MAIN_TASK_H
