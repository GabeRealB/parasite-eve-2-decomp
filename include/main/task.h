#ifndef TASK_H
#define TASK_H

#include "common.h"

// =============================================================================
// Types — cooperative task system (src/main/task.c)
// =============================================================================

struct Task;

/// A function the task system calls with the task that owns the slot.
///
/// Both the per-frame step and the teardown are one of these (`Task::callback`
/// and `Task::exitCallback`), as are the per-state handlers a dispatcher picks
/// out by `Task::state`. The task is the only argument: whatever a body needs
/// it reaches through that, in `Task::work`, `Task::extra` or `Task::spawnArg2`.
typedef void (*TaskFunc)(struct Task* task);

/// Fixed-size table of `TaskFunc` callbacks. Copied onto the stack by state
/// dispatchers (e.g. `GameFlow_DispatchTable`) so the call uses a local jump table.
typedef struct {
    TaskFunc funcs[3];
} TaskFuncTable3;

typedef struct {
    TaskFunc funcs[4];
} TaskFuncTable4;

typedef struct {
    TaskFunc funcs[5];
} TaskFuncTable5;

typedef struct {
    TaskFunc funcs[6];
} TaskFuncTable6;

typedef struct {
    TaskFunc funcs[7];
} TaskFuncTable7;

typedef struct {
    TaskFunc funcs[8];
} TaskFuncTable8;

typedef struct {
    TaskFunc funcs[9];
} TaskFuncTable9;

typedef struct {
    TaskFunc funcs[10];
} TaskFuncTable10;

typedef struct {
    TaskFunc funcs[11];
} TaskFuncTable11;

typedef struct {
    TaskFunc funcs[12];
} TaskFuncTable12;

typedef struct {
    TaskFunc funcs[14];
} TaskFuncTable14;

typedef struct {
    TaskFunc funcs[15];
} TaskFuncTable15;

typedef struct {
    TaskFunc funcs[16];
} TaskFuncTable16;

typedef struct {
    TaskFunc funcs[18];
} TaskFuncTable18;

/// Intrusive list link for a `Task`, and the type a task list is headed by.
///
/// The link is the task's first member, so a pointer to one is also the task
/// that carries it. A head is a bare node belonging to no task: its `next` is
/// the first task on the list, and its `prev` the last, which is the head
/// itself while the list is empty. The head is the only node that is not a task,
/// and a walk reaches it through `prev` alone, so `next` names a task while
/// `prev` names a node.
typedef struct TaskNode {
    struct Task*     next; // Following task, or NULL past the last
    struct TaskNode* prev; // Preceding node, or the head at the front
} TaskNode;
STATIC_ASSERT_SIZEOF(TaskNode, 0x8);

/// 2-byte table entry (id + type). Indexed via TaskIdMap.
typedef struct _TaskIdPair {
    /* 0x0 */ u8 id;
    /* 0x1 */ u8 type;
} TaskIdPair;
STATIC_ASSERT_SIZEOF(TaskIdPair, 0x2);

/// Index + pointer into a TaskIdPair table. Allocated (Mem_Calloc(8)) and stored
/// at Task::work by Task_AllocIdMap; read by Stage_ApplyTableEntryWhenIdle / Stage_LoadOrCountdownTask.
typedef struct _TaskIdMap {
    /* 0x0 */ u16         index;
    /* 0x2 */ byte        pad_2[2];
    /* 0x4 */ TaskIdPair* table;
} TaskIdMap;
STATIC_ASSERT_SIZEOF(TaskIdMap, 0x8);

/// A cooperatively scheduled game object. Actors, UI and loading steps are all
/// tasks, so one spawn, tick and kill path serves them all.
///
/// A task is a per-frame `callback` plus an optional body, and it belongs to two
/// structures at once: an intrusive list rooted at a `TaskNode`, which the exec
/// passes walk in `priority` order, and an optional parent/child tree whose
/// children form a ring (`firstChild` / `nextSibling`). The spawn helpers build
/// one from a `TaskDesc`, which supplies its `callback` and `priority`;
/// `spawnArg1`, `spawnArg2` and `extra` carry whatever the spawned type needs.
///
/// Several further slots are the task's own storage that the task system borrows
/// to run its protocol, so they hold whatever the spawned type puts there between
/// those uses: `status` carries a stop request, `extraState` the word that
/// request hands back, and `killCountdown` the delay before the body goes.
///
/// Killing a task that owns a body is spread over two steps, so nothing frees it
/// while its callback is still running: `taskKill` releases the body — a TMD model
/// only once its `killCountdown` has run out, a 2D display straight away — and
/// marks the task with `spawnType` 0xFF, and the exec pass that sees the mark
/// unlinks and frees the task once the callback has returned.
typedef struct Task {
    TaskNode     node;          // Intrusive list links; a task is its own list node
    struct Task* parent;        // Owning task; NULL when the task sits at the top level
    struct Task* firstChild;    // Head of the child ring; NULL when childless
    struct Task* nextSibling;   // Next child in that ring; the task itself when it is an only child
    TaskFunc     callback;      // Per-frame entry point, called by the exec passes
    TaskFunc     exitCallback;  // Runs as the task is torn down
    void*        work;          // Per-task work block, freed on kill; whatever the spawned type needs
    void*        spawnArg2;     // Second spawn argument; its meaning is the spawned type's
    void*        msgTable;      // Table of id/handler records the task answers messages with
    u8           spawnType;     // Body kind (0 none, 1 TMD model, 2 2D display); 0xFF marks a task to collect
    u8           priority;      // List position; lower runs earlier, and selects which pass picks the task up
    s16          killCountdown; // Frames left before the body is released; the task's own timer otherwise
    void*        extra;         // The body the task owns, attached and released according to `spawnType`
    s32          state;         // Index a handler dispatches on to pick its per-state function
    s32          spawnArg1;     // First spawn argument; its meaning is the spawned type's
    u8           status;        // The task's own byte; the task system records a stop request in it as 0xFF
    byte         unknown_39[3];
    s32          extraState;    // Word handed back with a stop request; the task's own payload otherwise
    byte         unknown_40[8];
} Task;
STATIC_ASSERT_SIZEOF(Task, 0x48);

/// One entry of a task table: what a spawn helper turns into a running `Task`.
///
/// The shared tables are reached by name — `Task_DescBanks[bank][type]` for the
/// banks, a package's own table for its rooms and actors — and every spawn path
/// ends in `Task_SpawnFromDesc`, which reads these four fields and nothing else.
/// A table that is walked rather than indexed ends on an entry whose `flags` is
/// all ones.
///
/// The argument is the descriptor's own: a kind-1 descriptor names the model its
/// task attaches, and one that attaches no model keeps whatever it needs there.
typedef struct {
    u16      flags;    // Body kind in the low byte (0 none, 1 TMD model, 2 2D display), plus bit 8 to attach the model without allocating its buffer
    u16      priority; // List position the spawned task takes; its low byte is what `Task::priority` gets
    TaskFunc callback; // Per-frame entry point the spawned task runs
    union {
        void* model;   // Kind 1: the model the task attaches
        s32   value;   // The descriptor's own value, where it attaches no model
    } arg;
} TaskDesc;
STATIC_ASSERT_SIZEOF(TaskDesc, 0xc);

// =============================================================================
// Functions — src/main/task.c
// =============================================================================

Task* Task_SpawnFromDesc(TaskDesc* desc, s32 arg1, s32 arg2, TaskNode* list);
Task* Task_SpawnFromTable(TaskDesc* table, s32 idx, s32 arg2, s32 arg3);
Task* Task_Spawn(s32 bank, s32 type, s32 arg2, s32 arg3);
Task* Task_SpawnOnDefaultList(TaskDesc* table, s32 idx, s32 arg2, s32 arg3);
Task* Task_SpawnOnDefaultListA(s32 bank, s32 type, s32 arg2, s32 arg3);
/// Kills a task and frees it: hands each child its own `exitCallback` with the
/// child's `parent` cleared, unlinks the task from its parent's child ring, frees
/// its `work` block, releases the body it owns according to `spawnType`, then
/// unlinks and frees the task itself. Every task is spawned with this as its
/// `exitCallback`, so a child tears itself down the same way.
///
/// The task's own free is the part that waits, so that a task calling this from
/// its own callback is not freed while that callback is still running: the body
/// goes, and the task is marked `spawnType` 0xFF for the next exec pass to
/// collect. With `gDisplayState.skipTeardown` set, the body is released and the
/// task unlinked and freed here instead.
void taskKill(Task* task);
void Task_KillChildren(Task* task);
void Task_CallExit(Task* task);
void Task_DetachFromParent(Task* task);
void Task_Reparent(Task* parent, Task* task);
void Task_InitList(TaskNode* node);
void Task_ExecList(TaskNode* node);
/// Runs the default frame list. The body reloads `gTaskDefaultList` itself, so
/// the argument is not read.
void      Task_ExecDefaultList(TaskNode* unused);
void      Task_ExecListFiltered(TaskNode* node, s32 filter);
void      Task_CallExitFiltered(TaskNode* node, s32 filter);
TaskDesc* Task_GetDesc(u32 bank, u32 type);
TaskDesc* Task_GetDescAt(TaskDesc* base, u32 idx);
void      Task_RequestKill(Task* task, s32 arg1);
s32       Task_PollKill(Task* task, s32* out);
TaskNode* Task_GetActiveList(void);
void      Task_SetActiveList(TaskNode* node);
void      Task_ResetDefaultList(void);
void      Task_Unlink(Task* task);
void      Task_Free(Task* task);
/// Task callback that counts a task's `killCountdown` down and releases the body
/// it owns when the count reaches zero: a TMD model comes off the model list and
/// has its buffer and object freed, a 2D display is freed, and a task owning
/// neither is only marked. The mark is `spawnType` 0xFF, which the next exec pass
/// collects the task on.
void taskCountdownCallback(Task* task);
s32  TaskIdMap_RemapIndex(s32 arg0, s32 arg1, s32 arg2);

// =============================================================================
// Globals
// =============================================================================

extern TaskDesc* Task_DescBanks[];

/// The task list the running code is working on: the list a spawned task joins
/// and the list an unlinked node is taken out of.
///
/// It points at `gTaskDefaultList` unless something has switched it, and the
/// default frame walk switches it back. A walk over another list points this
/// at that list, so a task spawned from inside a callback joins the list its
/// callback is running on rather than the main one; callers that must leave
/// the value as they found it save it first and put it back afterwards.
extern TaskNode* gTaskActiveList;

/// Head of the main task list: the list the frame walk runs and the list a
/// spawned task joins unless something has switched the active list away from
/// it.
///
/// It is a bare `TaskNode` rather than a task, so the head belongs to none of
/// the elements it anchors. Tearing the task system down — at boot and at the
/// start of a session — empties the list outright instead of draining it,
/// because the tasks still on it are reclaimed by reinitializing the heaps
/// they came from.
extern TaskNode gTaskDefaultList;

/// Head of the side list the display code runs on. A display mode makes it the
/// active list, so the tasks spawned for that mode land here rather than on the
/// main list, and the display frame walks it.
extern TaskNode gTaskDisplayList;

extern TaskFuncTable5       GameFlow_States5;
extern TaskFuncTable3       GameFlow_States3;
extern const TaskFuncTable6 Display_TaskStates;
extern TaskFuncTable3       Tmd_TaskStates;
extern TaskFuncTable4       Stage_TaskStates;

extern TaskDesc D_8006268C;
extern TaskDesc D_8006269C;
extern TaskDesc D_80062774;

extern TaskIdPair* D_8006273C[];
extern TaskIdPair* D_80062750[];

#endif // TASK_H
