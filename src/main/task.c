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

/// Mask selecting the low-byte body kind from a task descriptor's flags halfword.
///
/// Excludes the upper-byte options from attachment dispatch and `Task::bodyKind`.
/// Selectors are 0 (no body), 1 (TMD model) and 2 (coordinate body). Zero permits
/// a bodyless spawn; every nonzero selector requires successful attachment, so
/// other selector values fail to spawn.
enum { TASK_DESC_BODY_KIND_MASK = 0xFF };

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

/// Values used by the task stop protocol.
enum {
    TASK_STATUS_STOP_REQUESTED = 0xFF
};

static Task* Task_SpawnFromDesc(TaskDesc* desc, TaskSpawnArg arg1, TaskSpawnArg arg2, TaskNode* list);

static void Task_Unlink(Task* state);

static void Task_Free(Task* state);

bool gTaskDeferModelBufferAllocation = false;

/// Inserts a new task into its execution list, preserving ascending priority and spawn order among equals.
///
/// `listHead` must be a non-NULL bare head of a live, NULL-terminated list already
/// ordered by priority.
/// An empty head has `next == NULL` and `prev == listHead`; otherwise its `prev`
/// points to the last task's node. `task` must be live and not already linked.
/// `priority` is the task's initialized byte priority, 0..255, widened to `u32`
/// for the unsigned comparison. Insertion overwrites both of the task's links
/// and updates the successor's back link, or the head's tail link when appending.
/// The head and tasks must remain live while linked. Insertion performs no
/// allocation, callback dispatch or release, and does not select the active list.
static inline void _taskInsert(TaskNode* listHead, Task* task, u32 priority)
{
    Task*      nextTask;
    TaskNode** backlinkSlot;
    TaskNode*  predecessorNode;

    for (nextTask = listHead->next; nextTask != NULL; nextTask = nextTask->node.next) {
        if (priority < nextTask->priority) {
            break;
        }
    }

    // The insertion slot holds the predecessor, which may be the bare list head.
    if (nextTask == NULL) {
        backlinkSlot = &listHead->prev;
    } else {
        backlinkSlot = &nextTask->node.prev;
    }
    task->node.next       = (*backlinkSlot)->next;
    predecessorNode       = *backlinkSlot;
    predecessorNode->next = task;
    task->node.prev       = *backlinkSlot;
    *backlinkSlot         = &task->node;
}

static Task* Task_SpawnFromDesc(TaskDesc* desc, TaskSpawnArg arg1, TaskSpawnArg arg2, TaskNode* list)
{
    /// Spawn-time creation flag that defers a TMD body's initial primitive buffer.
    ///
    /// Bit 1 of the signed 32-bit buffer-flags argument, added while
    /// `gTaskDeferModelBufferAllocation` is set. A nonzero creation argument
    /// skips auxiliary-heap buffer allocation but still creates the model body
    /// and its coordinates. This bit does not set `TMD_OBJECT_SKIP_AUTO_BUFFER`;
    /// recovery remains eligible unless `TASK_DESC_SKIP_AUTO_MODEL_BUFFER` also
    /// supplies bit 0. No allocation is scheduled by this flag: a later
    /// buffer-allocation pass must run.
    enum { TASK_SPAWN_DEFER_MODEL_BUFFER = 1 << 1 };

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

    flags = desc->header.fields.flags;
    switch (flags & TASK_DESC_BODY_KIND_MASK) {
        case TASK_BODY_TMD:
            attachFlags = 0;
            if (flags & TASK_DESC_SKIP_AUTO_MODEL_BUFFER) {
                // Descriptor bit 8 becomes creation bit 0, suppressing allocation and recovery.
                attachFlags = 1;
            }
            if (gTaskDeferModelBufferAllocation) {
                attachFlags |= TASK_SPAWN_DEFER_MODEL_BUFFER;
            }
            extra.tmd = modelObjectAttachTmdWithBufferFlags(task, desc->data.model, attachFlags);
            break;
        case TASK_BODY_COORD:
            extra.coordBody = modelObjectAttachCoordBody(task);
            break;
        case TASK_BODY_NONE:
        default:
            extra.allocation = NULL;
            break;
    }

    // A descriptor that asks for a body gets no task when the body cannot be attached.
    if ((desc->header.fields.flags & TASK_DESC_BODY_KIND_MASK) == TASK_BODY_NONE || extra.allocation != NULL) {
        task->callback     = desc->callback;
        priority           = desc->header.fields.priority;
        task->exitCallback = taskKill;
        task->priority     = priority;
        kind               = desc->header.fields.flags & TASK_DESC_BODY_KIND_MASK;
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

/// Collects a torn-down task immediately instead of leaving it to a list walker.
///
/// `task` must be non-NULL, live, allocated from the primary heap and still
/// linked in an execution list. The caller must finish child/parent teardown
/// and release its work and body first; their pointer slots need not be cleared.
/// This helper repairs execution-list links and frees only the task allocation,
/// without dispatching callbacks or setting the deferred-collection marker.
///
/// A task with no successor must belong to `gTaskDefaultList`: that head is
/// selected for collection regardless of the previous selection. The predecessor
/// may be a bare head or an embedded task node. The previous list selection is
/// restored after release; the caller must not access the freed task.
static inline void _taskCollectImmediately(Task* task)
{
    TaskNode*  savedListHead;
    Task*      nextTask;
    TaskNode** backlinkSlot;
    TaskNode*  predecessorNode;

    savedListHead    = _gTaskActiveList;
    nextTask         = task->node.next;
    _gTaskActiveList = &gTaskDefaultList;

    // Repair the successor's back link, or the selected default head's tail link.
    if (nextTask == NULL) {
        backlinkSlot = &_gTaskActiveList->prev;
    } else {
        backlinkSlot = &nextTask->node.prev;
    }
    predecessorNode       = task->node.prev;
    *backlinkSlot         = predecessorNode;
    predecessorNode->next = task->node.next;
    memFree(task);
    _gTaskActiveList = savedListHead;
}

/// Stops frame updates and repeat exit dispatch for body teardown in the current call.
///
/// `task` must be non-NULL and remain allocated through the caller's teardown.
/// Both handlers become `taskNoopCallback`, and the signed `killCountdown`
/// is initialized to one and decremented to zero within this call.
/// The caller releases the body and marks the task for execution-list collection;
/// coordinate bodies are unlinked from their refresh list before this helper.
/// The task allocation remains live on return.
static inline void _taskStopForInlineBodyRelease(Task* task)
{
    /// Initial countdown for non-model body teardown completed within the same call.
    ///
    /// Stored in the signed 16-bit `Task::killCountdown`, then decremented once
    /// after both callbacks become inert. The resulting zero permits coordinate
    /// body release or bodyless-task marking before `taskKill` returns; it counts
    /// that synchronous decrement rather than future scheduler dispatches.
    enum { TASK_SYNCHRONOUS_BODY_RELEASE_COUNTDOWN = 1 };

    task->killCountdown = TASK_SYNCHRONOUS_BODY_RELEASE_COUNTDOWN;
    task->callback      = taskNoopCallback;
    task->exitCallback  = taskNoopCallback;
    task->killCountdown--;
}

void taskKill(Task* task)
{
    /// Countdown callback ticks before a stopped TMD model is released.
    ///
    /// Normal teardown disables active drawing and retains the linked model
    /// and its owned storage for this many countdown callback invocations.
    /// Walks that skip the task do not advance the delay; immediate teardown
    /// bypasses it.
    enum { TASK_MODEL_RELEASE_DELAY_TICKS = 2 };

    Task*      firstChild;
    Task*      ringCursor;
    Task*      childHead;
    TmdObject* model;
    s32        bodyKind;
    s32        immediateBodyKind;
    Task*      parent;
    Task*      nextSibling;

    // Children lose their parent before dispatch so their exit handlers leave
    // this sibling ring intact. The successor is read after each handler returns.
    childHead = task->firstChild;
    if (childHead != NULL) {
        firstChild = childHead;
        ringCursor = firstChild;
        do {
            ringCursor->parent = NULL;
            ringCursor->exitCallback(ringCursor);
            ringCursor = ringCursor->nextSibling;
        } while (ringCursor != firstChild);
    }

    // Remove this task from its parent's ring without rewriting its own links.
    parent = task->parent;
    if (parent != NULL) {
        nextSibling = task->nextSibling;
        if (nextSibling == task) {
            parent->firstChild = NULL;
        } else {
            if (parent->firstChild == task) {
                parent->firstChild = nextSibling;
            }
            ringCursor = task;
            if (task->nextSibling != task) {
                do {
                    ringCursor = ringCursor->nextSibling;
                } while (ringCursor->nextSibling != task);
            }
            ringCursor->nextSibling = task->nextSibling;
        }
    }

    if (task->work != NULL) {
        memFree(task->work);
    }

    if (gDisplayState.immediateTaskFree == 0) {
        bodyKind = task->bodyKind;
        if (bodyKind == TASK_BODY_TMD) {
            goto scheduleModelRelease;
        }
        if (bodyKind < TASK_BODY_COORD) {
            goto stopBodylessTask;
        }
        if (bodyKind == TASK_BODY_COORD) {
            goto unlinkCoordBody;
        }
        goto stopBodylessTask;

    scheduleModelRelease:
        // Suppress active model drawing during the deferred release window.
        task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->killCountdown     = TASK_MODEL_RELEASE_DELAY_TICKS;
        task->callback          = taskCountdownCallback;
        task->state             = 0;
        task->exitCallback      = taskNoopCallback;
        return;

    unlinkCoordBody:
        // Coordinate bodies leave their refresh list before inline release.
        modelObjectUnlinkCoordBody(&task->extra.coordBody->link);
        _taskStopForInlineBodyRelease(task);
        if (task->killCountdown != 0) {
            return;
        }
        if (task->bodyKind == TASK_BODY_TMD) {
            goto releaseModel;
        }
        if (task->bodyKind != bodyKind) {
            goto markBodyReleased;
        }
        goto releaseCoordBody;

    stopBodylessTask:
        _taskStopForInlineBodyRelease(task);
        if (task->killCountdown != 0) {
            return;
        }
        if (task->bodyKind == TASK_BODY_TMD) {
            goto releaseModel;
        }
        if (task->bodyKind == TASK_BODY_COORD) {
            goto releaseCoordBody;
        }
        goto markBodyReleased;

    releaseModel:
        model = task->extra.tmd;
        modelObjectUnlinkTmd(&model->link);
        modelObjectFreeTmd(model);
        goto markBodyReleased;

    releaseCoordBody:
        modelObjectFreeCoordBody(task->extra.coordBody);

    markBodyReleased:
        task->bodyKind = TASK_BODY_RELEASED;
        return;
    }

    // Immediate teardown bypasses the draw delay and walker collection.
    immediateBodyKind = task->bodyKind;
    switch (immediateBodyKind) {
        case TASK_BODY_TMD:
            modelObjectUnlinkTmd(&task->extra.tmd->link);
            modelObjectFreeTmd(task->extra.tmd);
            break;
        case TASK_BODY_COORD:
            modelObjectUnlinkCoordBody(&task->extra.coordBody->link);
            modelObjectFreeCoordBody(task->extra.coordBody);
            break;
    }
    _taskCollectImmediately(task);
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

/// Detaches a task from its parent's circular child ring while keeping it live.
///
/// `task` must be non-NULL and live throughout the call. If it has a parent,
/// that live parent's `firstChild` must head a closed ring containing `task`,
/// with live siblings whose `parent` identifies that owner. Removal preserves
/// the remaining siblings' order and advances the head only when removing it;
/// removing an only child clears the head. The detached task has `parent == NULL`
/// and `nextSibling == task`.
///
/// A parentless task is left unchanged, including its sibling link. The task
/// keeps its children, execution-list links and resources; no handlers run.
static inline void _taskDetachForReparent(Task* task)
{
    Task* oldParent;
    Task* nextSibling;
    Task* predecessor;

    oldParent = task->parent;
    if (oldParent != NULL) {
        nextSibling = task->nextSibling;
        if (nextSibling == task) {
            oldParent->firstChild = NULL;
        } else {
            if (oldParent->firstChild == task) {
                oldParent->firstChild = nextSibling;
            }
            // The child ring has only forward links, so locate the predecessor.
            predecessor = task;
            while (predecessor->nextSibling != task) {
                predecessor = predecessor->nextSibling;
            }
            predecessor->nextSibling = task->nextSibling;
            task->nextSibling        = task;
        }
        task->parent = NULL;
    }
}

void taskDetachFromParent(Task* task)
{
    _taskDetachForReparent(task);
}

void taskReparent(Task* newParent, Task* task)
{
    Task* childHead;
    Task* lastChild;
    Task* firstChild;

    // Restore the old parent's ring before changing the teardown relationship.
    _taskDetachForReparent(task);

    task->parent = newParent;
    childHead    = newParent->firstChild;
    if (childHead == NULL) {
        newParent->firstChild = task;
        return;
    }

    // Append after the last child, retaining the destination ring's head.
    lastChild  = childHead;
    firstChild = childHead;
    if (lastChild->nextSibling != lastChild) {
        do {
            lastChild = lastChild->nextSibling;
        } while (lastChild->nextSibling != firstChild);
    }
    task->nextSibling      = firstChild;
    lastChild->nextSibling = task;
}

void gameSetTaskSlot(struct Task* task, s32 slot)
{
    gGameSession->ptrSlots[slot] = task;
}

struct Task* gameGetTaskSlot(s32 slot)
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

    // Releasing dispatch. A model is unlinked, then freed; a coordinate body is
    // freed with its link unchanged.
    switch (task->bodyKind) {
        case TASK_BODY_TMD:
            model = task->extra.tmd;
            modelObjectUnlinkTmd(&model->link);
            modelObjectFreeTmd(model);
            task->bodyKind = TASK_BODY_RELEASED;
            break;
        case TASK_BODY_COORD:
            modelObjectFreeCoordBody(task->extra.coordBody);
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
