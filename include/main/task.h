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

/// Six task descriptors. Entry 5 is a model descriptor whose model is not
/// fixed: callers store the model in its `arg` just before spawning effect
/// 0x80005, which spawns its task from that entry.
extern TaskDesc D_800626EC[6];

extern TaskDesc Stage_MusicTaskDesc;

Task* Task_SpawnFromTable(TaskDesc* table, s32 idx, TaskSpawnArg arg2, TaskSpawnArg arg3);

Task* Task_Spawn(s32 bank, TaskSpawnArg type, TaskSpawnArg arg2, TaskSpawnArg arg3);

static __inline__ Task* Task_SpawnPtr(s32 bank, s32 type, s32 arg2, const void* data)
{
    return Task_Spawn(bank, type, arg2, data);
}

Task* Task_SpawnOnDefaultList(TaskDesc* table, s32 idx, TaskSpawnArg arg2, TaskSpawnArg arg3);

Task* Task_SpawnOnDefaultListA(s32 bank, TaskSpawnArg type, TaskSpawnArg arg2, TaskSpawnArg arg3);

/// Kills a task and frees it: hands each child its own `exitCallback` with the
/// child's `parent` cleared, unlinks the task from its parent's child ring, frees
/// its `work` block, releases the body it owns according to `bodyKind`, then
/// unlinks and frees the task itself. Every task is spawned with this as its
/// `exitCallback`, so a child tears itself down the same way.
///
/// The task's own free is the part that waits, so that a task calling this from
/// its own callback is not freed while that callback is still running: the body
/// goes, and the task is marked `bodyKind` 0xFF for the next exec pass to
/// collect. With `gDisplayState.immediateTaskFree` set, the body is released and the
/// task unlinked and freed here instead.
void taskKill(Task* task);

void Task_KillChildren(Task* task);

void Task_CallExit(Task* task);

void Task_DetachFromParent(Task* task);

void Task_Reparent(Task* parent, Task* task);

void Task_CallExitFiltered(TaskNode* node, s32 filter);

TaskDesc* Task_GetDesc(u32 bank, u32 type);

TaskDesc* Task_GetDescAt(TaskDesc* base, u32 idx);

void Task_RequestKill(Task* task, s32 arg1);

s32 Task_PollKill(Task* task, s32* out);

TaskNode* Task_GetActiveList(void);

void Task_SetActiveList(TaskNode* node);

void Task_ResetDefaultList(void);

#endif // MAIN_TASK_H
