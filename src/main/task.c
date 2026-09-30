#include "task.h"

#include "types.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "gameplay/display.h"
#include "gameplay/model_objects.h"

/* Define BSS before API headers to preserve first-declaration order. */
/// Borrowed head of the selected execution list for spawning and tail unlinking.
///
/// The selected head is an initialized bare `TaskNode`, not an embedded task
/// node. It must remain alive while selected; this pointer owns neither the
/// head nor its tasks. NULL is the pre-initialization value, not an empty list.
///
/// Initialization selects its head. Unfiltered walks select their head on entry
/// without restoring the previous selection; filtered walks restore it,
/// including on an early stop. Callers making temporary switches save and
/// restore it explicitly. The default frame walk selects `gTaskDefaultList`
/// before invoking callbacks; ordinary spawns use the current selection.
static TaskNode* _gTaskActiveList;

TaskNode gTaskDefaultList;

/// Unreferenced.
static u8 D_800716E8[8];

#include "main/task.h"

/// Completed body teardown awaiting execution-list collection.
///
/// Stored in `Task::bodyKind` once its owned body has been freed, or when
/// teardown has no body to release. `TASK_BODY_NONE` instead denotes a live
/// task without a body. Teardown leaves `Task::extra` unchanged; its pointer
/// must no longer be dereferenced.
///
/// Walkers unlink and free marked tasks after callback dispatch or the filter
/// decision, unless a stop request ends the walk first. Immediate teardown
/// unlinks and frees the task without setting this marker.
enum { TASK_BODY_RELEASED = 0xFF };

/// Values used by the task stop and deferred-model-release protocols.
enum {
    TASK_STATUS_STOP_REQUESTED     = 0xFF,
    TASK_MODEL_RELEASE_DELAY_TICKS = 2
};

/// Links `task` into `list` ahead of the first task whose priority is higher,
/// so the list stays in ascending priority order and equal priorities keep
/// the order they were spawned in.
static inline void _taskInsert(TaskNode* list, Task* task, u32 priority);

static Task* Task_SpawnFromDesc(TaskDesc* desc, TaskSpawnArg arg1, TaskSpawnArg arg2, TaskNode* list);

static void Task_Unlink(Task* state);

static void Task_Free(Task* state);

s32 D_8005ED8C = 0;

/// Links `task` into `list` ahead of the first task whose priority is higher,
/// so the list stays in ascending priority order and equal priorities keep
/// the order they were spawned in.
static inline void _taskInsert(TaskNode* list, Task* task, u32 priority)
{
    Task*      curr;
    TaskNode** link;

    for (curr = list->next; curr != NULL; curr = curr->node.next) {
        if (priority < curr->priority) {
            break;
        }
    }
    if (curr == NULL) {
        link = &list->prev;
    } else {
        link = &curr->node.prev;
    }
    task->node.next = (*link)->next;
    (*link)->next   = task;
    task->node.prev = *link;
    *link           = &task->node;
}

static Task* Task_SpawnFromDesc(TaskDesc* desc, TaskSpawnArg arg1, TaskSpawnArg arg2, TaskNode* list)
{
    Task*    task;
    TaskBody extra;
    u16      flags;
    s32      attachFlags;
    u8       priority;
    s32      kind;

    task = memCalloc(sizeof(Task), 0);
    if (task == NULL) {
        return NULL;
    }

    flags = desc->flags;
    switch (flags & 0xFF) {
        case TASK_BODY_TMD:
            attachFlags = 0;
            if (flags & 0x100) {
                attachFlags = 1;
            }
            if (D_8005ED8C != 0) {
                attachFlags |= 2;
            }
            extra.tmd = Gp_AttachTmdFlags(task, desc->arg.model, attachFlags);
            break;
        case TASK_BODY_DISP2D:
            extra.coordBody = gpAttachDisp2d(task);
            break;
        case TASK_BODY_NONE:
        default:
            extra.allocation = NULL;
            break;
    }

    // A descriptor that asks for a body gets no task when the body cannot be attached.
    if ((desc->flags & 0xFF) == TASK_BODY_NONE || extra.allocation != NULL) {
        task->callback     = desc->callback;
        priority           = desc->priority;
        task->exitCallback = taskKill;
        task->priority     = priority;
        kind               = desc->flags & 0xFF;
        task->extra        = extra;
        task->spawnArg1    = arg1;
        task->spawnArg2    = arg2;
        task->parent       = NULL;
        task->firstChild   = NULL;
        task->nextSibling  = task;
        task->bodyKind     = kind;

        _taskInsert(list, task, priority);
    } else {
        memFree(task);
        task = NULL;
    }
    return task;
}

void taskKill(Task* task)
{
    Task*      start;
    Task*      cur;
    Task*      temp;
    Task*      next;
    TaskNode*  previousList;
    TaskNode** pp;
    TaskNode*  prev;
    TmdObject* model;
    s32        type;
    s32        t;
    Task*      p;
    Task*      n;

    temp = task->firstChild;
    if (temp != NULL) {
        start = temp;
        cur   = start;
        do {
            cur->parent = NULL;
            cur->exitCallback(cur);
            cur = cur->nextSibling;
        } while (cur != start);
    }

    p = task->parent;
    if (p != NULL) {
        n = task->nextSibling;
        if (n == task) {
            p->firstChild = NULL;
        } else {
            if (p->firstChild == task) {
                p->firstChild = n;
            }
            cur = task;
            if (task->nextSibling != task) {
                do {
                    cur = cur->nextSibling;
                } while (cur->nextSibling != task);
            }
            cur->nextSibling = task->nextSibling;
        }
    }

    if (task->work != NULL) {
        memFree(task->work);
    }

    if (gDisplayState.immediateTaskFree == 0) {
        type = task->bodyKind;
        if (type == TASK_BODY_TMD) {
            goto case1;
        }
        if (type < TASK_BODY_DISP2D) {
            goto def_case;
        }
        if (type == TASK_BODY_DISP2D) {
            goto case2;
        }
        goto def_case;

    case1:
        task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->killCountdown     = TASK_MODEL_RELEASE_DELAY_TICKS;
        task->callback          = taskCountdownCallback;
        task->state             = 0;
        task->exitCallback      = taskNoopCallback;
        return;

    case2:
        modelObjectUnlinkDisp2d(&task->extra.coordBody->link);
        task->killCountdown = 1;
        task->callback      = taskNoopCallback;
        task->exitCallback  = taskNoopCallback;
        task->killCountdown--;
        if (task->killCountdown != 0) {
            return;
        }
        if (task->bodyKind == TASK_BODY_TMD) {
            goto cu1;
        }
        if (task->bodyKind != type) {
            goto cu_def;
        }
        goto cu2;

    def_case:
        task->killCountdown = 1;
        task->callback      = taskNoopCallback;
        task->exitCallback  = taskNoopCallback;
        task->killCountdown--;
        if (task->killCountdown != 0) {
            return;
        }
        if (task->bodyKind == TASK_BODY_TMD) {
            goto cu1;
        }
        if (task->bodyKind == TASK_BODY_DISP2D) {
            goto cu2;
        }
        goto cu_def;

    cu1:
        model = task->extra.tmd;
        modelObjectUnlinkTmd(&model->link);
        gpFreeTmd(model);
        goto cu_def;

    cu2:
        gpFreeDisp2d(task->extra.coordBody);

    cu_def:
        task->bodyKind = TASK_BODY_RELEASED;
        return;
    }

    t = task->bodyKind;
    if (t == TASK_BODY_TMD) {
        goto imm1;
    }
    if (t == TASK_BODY_DISP2D) {
        goto imm2;
    }
    goto imm_unlink;

imm1:
    modelObjectUnlinkTmd(&task->extra.tmd->link);
    gpFreeTmd(task->extra.tmd);
    goto imm_unlink;

imm2:
    modelObjectUnlinkDisp2d(&task->extra.coordBody->link);
    gpFreeDisp2d(task->extra.coordBody);

imm_unlink:
    previousList     = _gTaskActiveList;
    next             = task->node.next;
    _gTaskActiveList = &gTaskDefaultList;
    if (next == NULL) {
        pp = &gTaskDefaultList.prev;
    } else {
        pp = &next->node.prev;
    }
    prev       = task->node.prev;
    *pp        = prev;
    prev->next = task->node.next;
    memFree(task);
    _gTaskActiveList = previousList;
}

Task* Task_SpawnFromTable(TaskDesc* descriptor, s32 arg1, TaskSpawnArg arg2, TaskSpawnArg arg3)
{
    return Task_SpawnFromDesc(&descriptor[arg1], arg2, arg3, _gTaskActiveList);
}

Task* Task_Spawn(s32 arg0, TaskSpawnArg arg1, TaskSpawnArg arg2, TaskSpawnArg arg3)
{
    TaskDesc* ptr;

    if (arg0 >= 0) {
        ptr = gTaskDescBanks[arg0];
        ptr = &ptr[arg1.value];
    } else {
        ptr = arg1.pointer;
    }
    return Task_SpawnFromDesc(ptr, arg2, arg3, _gTaskActiveList);
}

void Task_KillChildren(Task* task)
{
    Task* start;
    Task* cur;
    Task* temp;

    temp = task->firstChild;
    if (temp != NULL) {
        start = temp;
        cur   = start;
        do {
            cur->parent = NULL;
            cur->exitCallback(cur);
            cur = cur->nextSibling;
        } while (cur != start);
    }
    task->firstChild = NULL;
}

void Task_CallExit(Task* task)
{
    task->exitCallback(task);
}

void Task_DetachFromParent(Task* task)
{
    Task* parent;
    Task* next;
    Task* cur;

    parent = task->parent;
    if (parent == NULL) {
        return;
    }

    next = task->nextSibling;
    if (next == task) {
        parent->firstChild = NULL;
    } else {
        if (parent->firstChild == task) {
            parent->firstChild = next;
        }
        cur = task;
        if (task->nextSibling != task) {
            do {
                cur = cur->nextSibling;
            } while (cur->nextSibling != task);
        }
        cur->nextSibling  = task->nextSibling;
        task->nextSibling = task;
    }
    task->parent = NULL;
}

void Task_Reparent(Task* arg0, Task* arg1)
{
    Task* parent;
    Task* next;
    Task* cur;
    Task* temp;

    parent = arg1->parent;
    if (parent != NULL) {
        next = arg1->nextSibling;
        if (next == arg1) {
            parent->firstChild = NULL;
        } else {
            if (parent->firstChild == arg1) {
                parent->firstChild = next;
            }
            cur = arg1;
            if (arg1->nextSibling != arg1) {
                do {
                    cur = cur->nextSibling;
                } while (cur->nextSibling != arg1);
            }
            cur->nextSibling  = arg1->nextSibling;
            arg1->nextSibling = arg1;
        }
        arg1->parent = NULL;
    }
    arg1->parent = arg0;
    temp         = arg0->firstChild;
    if (temp == NULL) {
        arg0->firstChild = arg1;
        return;
    }
    cur  = temp;
    arg0 = temp;
    if (cur->nextSibling != cur) {
        do {
            cur = cur->nextSibling;
        } while (cur->nextSibling != arg0);
    }
    arg1->nextSibling = arg0;
    cur->nextSibling  = arg1;
}

void Game_SetPtrSlot(void* ptr, s32 index)
{
    gGameSession->ptrSlots[index] = ptr;
}

struct Task* gameGetPtrSlot(s32 slot)
{
    return gGameSession->ptrSlots[slot];
}

void Task_InitList(TaskNode* node)
{
    _gTaskActiveList = node;
    node->next       = NULL;
    node->prev       = node;
}

void Task_ExecList(TaskNode* node)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr; // The indirection is required.

    curr             = node->next;
    _gTaskActiveList = node;
    if (curr != NULL) {
        tmp_ptr = &gDisplayState;
    loop_2:
        curr->callback(curr);
        if (tmp_ptr->stopTaskWalk == 1) {
            tmp_ptr->stopTaskWalk = 0;
            return;
        }
        if (curr->bodyKind == TASK_BODY_RELEASED) {
            next                  = curr->node.next;
            tmp_ptr->stopTaskWalk = 0;
            Task_Unlink(curr);
            Task_Free(curr);
            curr = next;
        } else {
            curr = curr->node.next;
        }
        if (curr != NULL) {
            goto loop_2;
        }
    }
}

TaskDesc* Task_GetDesc(u32 idx1, u32 idx2)
{
    TaskDesc* base = gTaskDescBanks[idx1];
    return base + idx2;
}

TaskDesc* Task_GetDescAt(TaskDesc* base, u32 idx)
{
    return base + idx;
}

void Task_RequestKill(Task* task, s32 arg1)
{
    Task* start;
    Task* cur;
    Task* temp;

    task->status           = TASK_STATUS_STOP_REQUESTED;
    task->extraState.value = arg1;
    task->callback         = taskNoopCallback;

    temp = task->firstChild;
    if (temp != NULL) {
        start = temp;
        cur   = start;
        do {
            cur->parent = NULL;
            cur->exitCallback(cur);
            cur = cur->nextSibling;
        } while (cur != start);
    }
    task->firstChild = NULL;
}

s32 Task_PollKill(Task* task, s32* arg1)
{
    s32 result;

    result = 0;
    if (task->status == TASK_STATUS_STOP_REQUESTED) {
        if (arg1 != NULL) {
            *arg1 = task->extraState.value;
        }
        task->exitCallback(task);
        result = 1;
    }
    return result;
}

TaskNode* Task_GetActiveList(void)
{
    return _gTaskActiveList;
}

void Task_SetActiveList(TaskNode* node)
{
    _gTaskActiveList = node;
}

void Task_ResetDefaultList(void)
{
    _gTaskActiveList      = &gTaskDefaultList;
    gTaskDefaultList.next = NULL;
    gTaskDefaultList.prev = &gTaskDefaultList;
}

static void Task_Unlink(Task* state)
{
    Task*      next;
    TaskNode*  head;
    TaskNode** pp;
    TaskNode*  prev;

    next = state->node.next;
    head = _gTaskActiveList;
    do {
        pp = &head->prev;
        if (next != NULL) {
            pp = &next->node.prev;
        }
    } while (0);
    prev       = state->node.prev;
    *pp        = prev;
    prev->next = state->node.next;
}

static void Task_Free(Task* state)
{
    memFree(state);
}

void Task_ExecDefaultList(TaskNode* unused)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr; // The indirection is required.

    curr             = gTaskDefaultList.next;
    _gTaskActiveList = &gTaskDefaultList;
    if (curr != NULL) {
        tmp_ptr = &gDisplayState;
    loop_2:
        curr->callback(curr);
        if (tmp_ptr->stopTaskWalk == 1) {
            tmp_ptr->stopTaskWalk = 0;
            return;
        }
        if (curr->bodyKind == TASK_BODY_RELEASED) {
            next                  = curr->node.next;
            tmp_ptr->stopTaskWalk = 0;
            Task_Unlink(curr);
            Task_Free(curr);
            curr = next;
        } else {
            curr = curr->node.next;
        }
        if (curr != NULL) {
            goto loop_2;
        }
    }
}

void Task_ExecListFiltered(TaskNode* node, s32 arg1)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr;
    TaskNode*     previousList;
    s32           filter;

    curr             = node->next;
    previousList     = _gTaskActiveList;
    _gTaskActiveList = node;
    if (curr != NULL) {
        filter  = arg1 & 0xFF;
        tmp_ptr = &gDisplayState;
    loop_2:
        if (curr->priority == (u8)filter) {
            curr->callback(curr);
        }
        if (tmp_ptr->stopTaskWalk == 1) {
            tmp_ptr->stopTaskWalk = 0;
            goto end;
        }
        if (curr->bodyKind == TASK_BODY_RELEASED) {
            next                  = curr->node.next;
            tmp_ptr->stopTaskWalk = 0;
            Task_Unlink(curr);
            Task_Free(curr);
            curr = next;
        } else {
            curr = curr->node.next;
        }
        if (curr != NULL) {
            goto loop_2;
        }
    }
end:
    _gTaskActiveList = previousList;
}

void Task_CallExitFiltered(TaskNode* node, s32 arg1)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr;
    TaskNode*     previousList;
    s32           filter;

    curr             = node->next;
    previousList     = _gTaskActiveList;
    _gTaskActiveList = node;
    if (curr != NULL) {
        filter  = arg1 & 0xFF;
        tmp_ptr = &gDisplayState;
    loop_2:
        if (curr->priority == (u8)filter) {
            Task_CallExit(curr);
        }
        if (tmp_ptr->stopTaskWalk == 1) {
            tmp_ptr->stopTaskWalk = 0;
            goto end;
        }
        if (curr->bodyKind == TASK_BODY_RELEASED) {
            next                  = curr->node.next;
            tmp_ptr->stopTaskWalk = 0;
            Task_Unlink(curr);
            Task_Free(curr);
            curr = next;
        } else {
            curr = curr->node.next;
        }
        if (curr != NULL) {
            goto loop_2;
        }
    }
end:
    _gTaskActiveList = previousList;
}

void taskCountdownCallback(Task* task)
{
    TmdObject* model;

    task->killCountdown--;
    if (task->killCountdown != 0) {
        return;
    }

    switch (task->bodyKind) {
        case TASK_BODY_TMD:
            model = task->extra.tmd;
            modelObjectUnlinkTmd(&model->link);
            gpFreeTmd(model);
            task->bodyKind = TASK_BODY_RELEASED;
            break;
        case TASK_BODY_DISP2D:
            gpFreeDisp2d(task->extra.coordBody);
            task->bodyKind = TASK_BODY_RELEASED;
            break;
        default:
            task->bodyKind = TASK_BODY_RELEASED;
            break;
    }
}

void Game_ClearPtrSlots(void)
{
    s32 i;

    for (i = (s32)ARRAY_SIZE(gGameSession->ptrSlots) - 1; i >= 0; i--) {
        gGameSession->ptrSlots[i] = NULL;
    }
}

void Mem_CopyUnaligned(void* src, void* dest, u32 count)
{
    u32 i;
    u32 alignment;
    u8* ptr;
    u8* dst;
    u32 remaining;

    ptr       = (u8*)src;
    dst       = (u8*)dest;
    remaining = count;

    while ((remaining & 0xFFFF) >= 4) {
        /* Alignment depends on address bits, not the pointed-to value. */
        alignment = (uintptr)ptr & 3;

        switch (alignment) {
            case 0:
                *(u32*)dst = *(u32*)ptr;
                ptr       += 4;
                dst       += 4;
                remaining -= 4;
                break;

            case 1:
                *dst++     = *ptr++;
                *(u16*)dst = *(u16*)ptr;
                ptr       += 2;
                dst       += 2;
                remaining -= 3;
                break;

            case 2:
                *(u16*)dst = *(u16*)ptr;
                ptr       += 2;
                dst       += 2;
                remaining -= 2;
                break;

            case 3:
                *dst       = *ptr;
                ptr       += 1;
                dst       += 1;
                remaining -= 1;
                break;
        }
    }

    remaining &= 0xFFFF;
    i          = 0;
    while ((i & 0xFFFF) < remaining) {
        *dst++ = *ptr++;
        i++;
    }
}
