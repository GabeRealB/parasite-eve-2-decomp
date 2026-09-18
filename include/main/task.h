#ifndef TASK_H
#define TASK_H

#include "common.h"

// =============================================================================
// Types — cooperative task system (src/main/task.c)
// =============================================================================

struct Task;

/// Callback function in a `_Task`.
typedef void (*TaskFunc)(struct Task*);

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

/// Intrusive linked list node for a `Task`.
///
/// The head node is not an element in the linked list and points to the first
/// and the last elements.
typedef struct _TaskNode {
    struct Task*      next;
    struct _TaskNode* prev;
} TaskNode;

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
/// Killing a task that owns a body is spread over two steps, so nothing frees it
/// while its callback is still running: `Task_Kill` releases the body (for a TMD
/// model, from `Task_CountdownCallback` once `killCountdown` runs out) and marks
/// the task with `spawnType` 0xFF, and the exec pass that sees the mark unlinks
/// and frees the task once the callback has returned.
typedef struct Task {
    TaskNode     node;          // Intrusive list links; a task is its own list node
    struct Task* parent;        // Owning task; NULL when the task sits at the top level
    struct Task* firstChild;    // Head of the child ring; NULL when childless
    struct Task* nextSibling;   // Next child in that ring; the task itself when it is an only child
    TaskFunc     callback;      // Per-frame entry point, called by the exec passes
    TaskFunc     exitCallback;  // Runs as the task is torn down; spawned tasks get `Task_Kill`
    TaskIdMap*   work;          // The per-task work block, allocated by the spawner and freed on kill
    void*        spawnArg2;     // Second spawn argument; its meaning is the spawned type's
    void*        field_24;      // The task's `GpMsgEntry` id/handler table, walked by `Gp_DispatchMsg`
    u8           spawnType;     // Body kind (0 none, 1 TMD model, 2 2D display); 0xFF marks a task to collect
    u8           priority;      // List position, lower runs earlier; also the bucket the filtered passes select by
    s16          killCountdown; // Frames left before the body is released, counted down by `Task_CountdownCallback`
    void*        extra;         // The body the task owns, attached and released according to `spawnType`
    s32          state;         // Task state; handlers dispatch through `funcs[state]` of a copied `TaskFuncTableN`
    s32          spawnArg1;     // First spawn argument; its meaning is the spawned type's
    u8           flags;         // Flag byte; `Task_RequestKill` writes 0xFF here and `Task_PollKill` consumes it
    byte         unknown_39[3];
    s32          extraState;    // Kill-request payload, set by `Task_RequestKill` and returned by `Task_PollKill`
    byte         unknown_40[8];
} Task;
STATIC_ASSERT_SIZEOF(Task, 0x48);

/// Descriptor used to spawn a task. Indexed via `Task_DescBanks[bank][type]`.
typedef struct _TaskDesc {
    /* 0x0 */ u16      flags;    // low byte type 0/1/2; bit 0x100 type-1 setup
    /* 0x2 */ u16      priority; // low byte → Task::priority
    /* 0x4 */ TaskFunc callback;
    /* 0x8 */ s32      setupArg; // type-1 setup arg
} TaskDesc;
STATIC_ASSERT_SIZEOF(TaskDesc, 0xc);

// =============================================================================
// Functions — src/main/task.c
// =============================================================================

Task*     Task_SpawnFromDesc(TaskDesc* desc, s32 arg1, s32 arg2, TaskNode* list);
Task*     Task_SpawnFromTable(TaskDesc* table, s32 idx, s32 arg2, s32 arg3);
Task*     Task_Spawn(s32 bank, s32 type, s32 arg2, s32 arg3);
Task*     Task_SpawnOnDefaultList(TaskDesc* table, s32 idx, s32 arg2, s32 arg3);
Task*     Task_SpawnOnDefaultListA(s32 bank, s32 type, s32 arg2, s32 arg3);
void      Task_Kill(Task* task);
void      Task_KillChildren(Task* task);
void      Task_CallExit(Task* task);
void      Task_DetachFromParent(Task* task);
void      Task_Reparent(Task* parent, Task* task);
void      Task_InitList(TaskNode* node);
void      Task_ExecList(TaskNode* node);
void      Task_ExecDefaultList(TaskNode* node);
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
void      Task_CountdownCallback(Task* task);
s32       TaskIdMap_RemapIndex(s32 arg0, s32 arg1, s32 arg2);

// =============================================================================
// Globals
// =============================================================================

extern TaskDesc* Task_DescBanks[];
extern TaskNode* Task_ActiveList;
extern TaskNode  Task_DefaultList;
extern TaskNode  D_8007A110;

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
