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

/// Intrusive linked list node for a `Task`.
///
/// The head node is not an element in the linked list and points to the first
/// and the last elements.
typedef struct _TaskNode {
    struct _Task*     next;
    struct _TaskNode* prev;
} TaskNode;

/// 2-byte table entry (id + type). Indexed via TaskIdMap.
typedef struct _GPairU8 {
    /* 0x0 */ u8 field_0; // id
    /* 0x1 */ u8 field_1; // type
} GPairU8;
STATIC_ASSERT_SIZEOF(GPairU8, 0x2);

/// Index + pointer into a GPairU8 table. Allocated (Mem_Calloc(8)) and stored
/// at Task::field_1C by Task_AllocIdMap; read by func_80042F54 / func_80042DF8.
typedef struct _TaskIdMap {
    /* 0x0 */ u16      field_0; // index into field_4
    /* 0x2 */ byte     pad_2[2];
    /* 0x4 */ GPairU8* field_4; // id/type table
} TaskIdMap;
STATIC_ASSERT_SIZEOF(TaskIdMap, 0x8);

/// Cooperative task. Field roles: see also `STRUCT_FIELDS.md`.
typedef struct _Task {
    /* 0x00 */ TaskNode      node;     // list links
    /* 0x08 */ struct _Task* field_8;  // parent (NULL if root)
    /* 0x0C */ struct _Task* field_c;  // first child (NULL if none)
    /* 0x10 */ struct _Task* field_10; // next sibling (self if only child)
    /* 0x14 */ TaskFunc      field_14; // per-frame callback
    /* 0x18 */ TaskFunc      field_18; // exit / kill callback
    /* 0x1C */ TaskIdMap*    field_1C; // optional id map (freed on kill)
    /* 0x20 */ void*         field_20; // spawn arg2 (often UiObject*)
    /* 0x24 */ byte          unknown_24[4];
    /* 0x28 */ u8            field_28; // spawn type (desc flags low byte)
    /* 0x29 */ u8            field_29; // list priority (lower = earlier)
    /* 0x2A */ s16           field_2a; // deferred-kill countdown
    /* 0x2C */ void*         field_2c; // spawn "extra" (GameActorExt*, …)
    /* 0x30 */ s32           field_30; // generic state
    /* 0x34 */ s32           field_34; // spawn arg1 (menu/ctx/mode)
    /* 0x38 */ u8            field_38; // small flag
    /* 0x39 */ byte          unknown_39[3];
    /* 0x3C */ s32           field_3c; // extra state
    /* 0x40 */ byte          unknown_40[8];
} Task;
STATIC_ASSERT_SIZEOF(Task, 0x48);

/// Descriptor used to spawn a task. Indexed via `Task_DescBanks[bank][type]`.
typedef struct _TaskDesc {
    /* 0x0 */ u16      flags;    // low byte type 0/1/2; bit 0x100 type-1 setup
    /* 0x2 */ u16      field_2;  // low byte → Task::field_29 priority
    /* 0x4 */ TaskFunc callback; // per-frame entry
    /* 0x8 */ s32      field_8;  // type-1 setup arg
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
void      func_8002D7A8(void* src, void* dest, u32 count);

// =============================================================================
// Globals
// =============================================================================

extern TaskDesc* Task_DescBanks[];
extern TaskNode* Task_ActiveList;
extern TaskNode  Task_DefaultList;

#endif // TASK_H
