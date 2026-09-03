#ifndef TASK_H
#define TASK_H

#include "common.h"

// =============================================================================
// Types — cooperative task system (src/main/task.c)
// =============================================================================

struct _Task;

/// Callback function in a `_Task`.
typedef void (*TaskFunc)(struct _Task*);

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
    TaskFunc funcs[16];
} TaskFuncTable16;

/// Intrusive linked list node for a `Task`.
///
/// The head node is not an element in the linked list and points to the first
/// and the last elements.
typedef struct _TaskNode {
    struct _Task*     next;
    struct _TaskNode* prev;
} TaskNode;

/// 2-byte table entry (id + type). Indexed via TaskIdMap.
typedef struct _TaskIdPair {
    /* 0x0 */ u8 id;
    /* 0x1 */ u8 type;
} TaskIdPair;
STATIC_ASSERT_SIZEOF(TaskIdPair, 0x2);

/// Index + pointer into a TaskIdPair table. Allocated (Mem_Calloc(8)) and stored
/// at Task::idMap by Task_AllocIdMap; read by Stage_ApplyTableEntryWhenIdle / Stage_LoadOrCountdownTask.
typedef struct _TaskIdMap {
    /* 0x0 */ u16         index;
    /* 0x2 */ byte        pad_2[2];
    /* 0x4 */ TaskIdPair* table;
} TaskIdMap;
STATIC_ASSERT_SIZEOF(TaskIdMap, 0x8);

/// Cooperative task. Field roles: see also `STRUCT_FIELDS.md`.
typedef struct _Task {
    /* 0x00 */ TaskNode      node;
    /* 0x08 */ struct _Task* parent;
    /* 0x0C */ struct _Task* firstChild;
    /* 0x10 */ struct _Task* nextSibling;
    /* 0x14 */ TaskFunc      callback;
    /* 0x18 */ TaskFunc      exitCallback;
    /* 0x1C */ TaskIdMap*    idMap;
    /* 0x20 */ void*         spawnArg2;
    /* 0x24 */ void*         field_24;
    /* 0x28 */ u8            spawnType;
    /* 0x29 */ u8            priority;
    /* 0x2A */ s16           killCountdown;
    /* 0x2C */ void*         extra;
    /* 0x30 */ s32           state;
    /* 0x34 */ s32           spawnArg1;
    /* 0x38 */ u8            flags;
    /* 0x39 */ byte          unknown_39[3];
    /* 0x3C */ s32           extraState;
    /* 0x40 */ byte          unknown_40[8];
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
