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
/// dispatchers (e.g. `func_8002C028`) so the call uses a local jump table.
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

/// Intrusive linked list node for a `Task`.
///
/// The head node is not an element in the linked list and points to the first
/// and the last elements.
typedef struct _TaskNode {
    struct _Task*     next;
    struct _TaskNode* prev;
} TaskNode;

/// 2-byte table entry (id + type). Indexed via GStruct63.
typedef struct _GPairU8 {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GPairU8;
STATIC_ASSERT_SIZEOF(GPairU8, 0x2);

/// Index + pointer into a GPairU8 table. Allocated (Mem_Calloc(8)) and stored
/// at Task::field_1C by func_80042B00; read by func_80042F54 / func_80042DF8.
typedef struct _GStruct63 {
    /* 0x0 */ u16      field_0; // index into field_4
    /* 0x2 */ byte     pad_2[2];
    /* 0x4 */ GPairU8* field_4;
} GStruct63;
STATIC_ASSERT_SIZEOF(GStruct63, 0x8);

typedef struct _Task {
    TaskNode      node;
    struct _Task* field_8;
    struct _Task* field_c;
    struct _Task* field_10;
    TaskFunc      field_14;
    TaskFunc      field_18;
    GStruct63*    field_1C;
    void*         field_20;
    byte          unknown_24[4];
    u8            field_28;
    u8            field_29;
    s16           field_2a;
    void*         field_2c;
    s32           field_30;
    s32           field_34;
    u8            field_38;
    byte          unknown_39[3];
    s32           field_3c;
    byte          unknown_40[8];
} Task;
STATIC_ASSERT_SIZEOF(Task, 0x48);

/// Descriptor used to spawn a task. Indexed via `Task_DescBanks[bank][type]`.
///
/// `flags` low byte is the spawn type (0 = bare, 1 = func_80099170, 2 =
/// func_80099098). Bit 0x100 is an extra arg to type-1 setup. Low byte of
/// `field_2` is the list priority (`Task::field_29`); lower values run earlier.
typedef struct _TaskDesc {
    u16      flags;
    u16      field_2;
    TaskFunc callback; // per-frame / state-machine entry
    s32      field_8;
} TaskDesc;
STATIC_ASSERT_SIZEOF(TaskDesc, 0xc);

// =============================================================================
// Functions — src/main/task.c
// =============================================================================

Task*     Task_SpawnFromDesc(TaskDesc* desc, s32 arg1, s32 arg2, TaskNode* list);
Task*     Task_SpawnFromTable(TaskDesc* table, s32 idx, s32 arg2, s32 arg3);
Task*     Task_Spawn(s32 bank, s32 type, s32 arg2, s32 arg3);
void      Task_Kill(Task* task);
void      Task_KillChildren(Task* task);
void      Task_CallExit(Task* task);
void      Task_DetachFromParent(Task* task);
void      func_8002D14C(Task* parent, Task* task);
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
void      func_8002D7A8(void* src, void* dest, u32 count);

// =============================================================================
// Globals
// =============================================================================

extern TaskDesc* Task_DescBanks[];
extern TaskNode* Task_ActiveList;
extern TaskNode  Task_DefaultList;

#endif // TASK_H
