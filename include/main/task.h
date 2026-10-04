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
/// Initialize with `Task_ResetDefaultList` before spawning. An empty list has
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

Task* Task_SpawnFromTable(TaskDesc* table, s32 idx, TaskSpawnArg arg2, TaskSpawnArg arg3);

Task* Task_Spawn(s32 bank, TaskSpawnArg type, TaskSpawnArg arg2, TaskSpawnArg arg3);

static __inline__ Task* Task_SpawnPtr(s32 bank, s32 type, s32 arg2, const void* data)
{
    return Task_Spawn(bank, type, arg2, data);
}

Task* Task_SpawnOnDefaultList(TaskDesc* table, s32 idx, TaskSpawnArg arg2, TaskSpawnArg arg3);

Task* Task_SpawnOnDefaultListA(s32 bank, TaskSpawnArg type, TaskSpawnArg arg2, TaskSpawnArg arg3);

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

void Task_KillChildren(Task* task);

void Task_CallExit(Task* task);

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

void Task_CallExitFiltered(TaskNode* node, s32 filter);

TaskDesc* Task_GetDesc(u32 bank, u32 type);

TaskDesc* Task_GetDescAt(TaskDesc* base, u32 idx);

void Task_RequestKill(Task* task, s32 arg1);

s32 Task_PollKill(Task* task, s32* out);

TaskNode* Task_GetActiveList(void);

void Task_SetActiveList(TaskNode* node);

void Task_ResetDefaultList(void);

#endif // MAIN_TASK_H
