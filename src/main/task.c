#include "task.h"

#include "types.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd.h"

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

/// Low byte of a signed priority selector, compared with the stored task byte.
enum { TASK_PRIORITY_MASK = 0xFF };

/// Exact stop request consumed by a walker before collection or advancement.
enum { TASK_WALK_STOP_REQUESTED = 1 };

static Task* _taskSpawnFromDesc(TaskDesc* desc, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2, TaskNode* listHead);

static void _taskUnlinkFromSelectedList(Task* task);

static void _taskFree(Task* task);

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

/// Creates and inserts a task from a synchronously borrowed descriptor.
///
/// `desc` and `listHead` must be live and non-NULL; the head must already be
/// initialized and ordered by ascending priority. Only body kinds 0, 1 and 2
/// are accepted. Task allocation or required body attachment failure returns
/// NULL and leaves the execution list unchanged. Primitive-buffer allocation
/// failure alone can leave a valid model body with a NULL buffer.
///
/// Copies the callback, low-byte priority and both payload words. The task owns
/// its attached body; callback code and borrowed model geometry must outlive
/// their use. No descriptor pointer is retained and no task callback runs here.
/// Equal priorities retain spawn order, so a spawn after a walk's cursor can
/// run in that same walk. Pointer payload ownership belongs to the callback.
static Task* _taskSpawnFromDesc(TaskDesc* desc, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2, TaskNode* listHead)
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
    TaskBody body;
    u16      descriptorFlags;
    s32      bufferFlags;
    u8       priority;
    s32      bodyKind;

    task = memCalloc(sizeof(*task), false);
    if (task == NULL) {
        return NULL;
    }

    descriptorFlags = desc->header.fields.flags;
    switch (descriptorFlags & TASK_DESC_BODY_KIND_MASK) {
        case TASK_BODY_TMD:
            bufferFlags = 0;
            if (descriptorFlags & TASK_DESC_SKIP_AUTO_MODEL_BUFFER) {
                // Descriptor bit 8 becomes creation bit 0, suppressing allocation and recovery.
                bufferFlags = TMD_CREATE_SKIP_AUTO_BUFFER;
            }
            if (gTaskDeferModelBufferAllocation) {
                bufferFlags |= TASK_SPAWN_DEFER_MODEL_BUFFER;
            }
            body.tmd = modelObjectAttachTmdWithBufferFlags(task, desc->data.model, bufferFlags);
            break;
        case TASK_BODY_COORD:
            body.coordBody = modelObjectAttachCoordBody(task);
            break;
        case TASK_BODY_NONE:
        default:
            body.allocation = NULL;
            break;
    }

    // A descriptor that asks for a body gets no task when the body cannot be attached.
    if ((desc->header.fields.flags & TASK_DESC_BODY_KIND_MASK) == TASK_BODY_NONE || body.allocation != NULL) {
        task->callback     = desc->callback;
        priority           = desc->header.fields.priority;
        task->exitCallback = taskKill;
        task->priority     = priority;
        bodyKind           = desc->header.fields.flags & TASK_DESC_BODY_KIND_MASK;
        task->extra        = body;
        task->spawnArg1    = spawnArg1;
        task->spawnArg2    = spawnArg2;
        task->parent       = NULL;
        task->firstChild   = NULL;
        task->nextSibling  = task;
        task->bodyKind     = bodyKind;

        _taskInsert(listHead, task, priority);
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

/// Releases a task's body synchronously and marks the task for walker collection.
///
/// `task` must be non-NULL, live and not already released. The caller finishes
/// child/parent teardown and work cleanup before making the task collectible.
/// An owned TMD body must be on the live model list; it is unlinked here and
/// freed with its primitive buffer. A coordinate body must already be unlinked
/// from its refresh list before this call, which frees only its allocation.
/// Other body kinds release no body allocation.
///
/// Both handlers become inert and the signed halfword countdown is initialized
/// to one and decremented once. At zero, the body is released and `bodyKind`
/// becomes `TASK_BODY_RELEASED`; a nonzero result leaves it unchanged. The task
/// allocation and its execution links remain live for the walker to collect.
/// `extra` retains its released pointer and must no longer be dereferenced.
static inline void _taskReleaseBodySynchronously(Task* task)
{
    TmdObject* model;

    _taskStopForInlineBodyRelease(task);
    if (task->killCountdown == 0) {
        switch (task->bodyKind) {
            case TASK_BODY_TMD:
                model = task->extra.tmd;
                modelObjectUnlinkTmd(&model->link);
                modelObjectFreeTmd(model);
                break;
            case TASK_BODY_COORD:
                modelObjectFreeCoordBody(task->extra.coordBody);
                break;
        }
        task->bodyKind = TASK_BODY_RELEASED;
    }
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

    Task* firstChild;
    Task* ringCursor;
    Task* childHead;
    s32   bodyKind;
    s32   immediateBodyKind;
    Task* parent;
    Task* nextSibling;

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
        switch (bodyKind) {
            case TASK_BODY_TMD:
                // Suppress active model drawing during the deferred release window.
                task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                task->killCountdown     = TASK_MODEL_RELEASE_DELAY_TICKS;
                task->callback          = taskCountdownCallback;
                task->state             = 0;
                task->exitCallback      = taskNoopCallback;
                break;
            case TASK_BODY_COORD:
                // Coordinate bodies leave their refresh list before synchronous release.
                modelObjectUnlinkCoordBody(&task->extra.coordBody->link);
                _taskReleaseBodySynchronously(task);
                break;
            case TASK_BODY_NONE:
            default:
                _taskReleaseBodySynchronously(task);
                break;
        }
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

Task* taskSpawnFromTable(TaskDesc* table, s32 index, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2)
{
    return _taskSpawnFromDesc(&table[index], spawnArg1, spawnArg2, _gTaskActiveList);
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
    return _taskSpawnFromDesc(ptr, arg2, arg3, _gTaskActiveList);
}

/// Dispatches exits around a nonempty child ring with parents cleared first.
///
/// `firstChild` heads a closed ring of live tasks with loaded, non-NULL handlers.
/// Each handler must preserve the child's sibling-link storage until the
/// post-handler read and keep the remaining traversal intact, even when freeing
/// the child.
/// The ring head is a pointer comparison boundary and is not dereferenced again
/// after its own dispatch. The ring owner's child head is left for the caller.
static inline void _taskCallChildRingExits(Task* firstChild)
{
    Task* child;

    child = firstChild;
    // Clear the parent before dispatch; read the preserved successor afterward.
    do {
        child->parent = NULL;
        child->exitCallback(child);
        child = child->nextSibling;
    } while (child != firstChild);
}

void taskCallChildExits(Task* parentTask)
{
    Task* firstChild;
    Task* childHead;

    childHead = parentTask->firstChild;
    if (childHead != NULL) {
        firstChild = childHead;
        _taskCallChildRingExits(firstChild);
    }
    parentTask->firstChild = NULL;
}

void taskCallExit(Task* task)
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

void taskInitList(TaskNode* listHead)
{
    _gTaskActiveList = listHead;
    listHead->next   = NULL;
    listHead->prev   = listHead;
}

/// Advances a task walk, collecting its cursor when body teardown is complete.
///
/// `cursor` must be non-NULL with readable body-kind and execution-link storage
/// after dispatch. A marked cursor must still be a linked primary-heap task
/// allocation with its work, body and teardown relationships already released.
/// Its predecessor is a bare head or a live task node; its successor is a live
/// task or NULL. The selected head must own the cursor when it is the tail.
/// `display` must remain live and writable through collection.
///
/// Saves the successor before unlinking and freeing a `TASK_BODY_RELEASED`
/// cursor, clearing `display->stopTaskWalk` only in that branch. Otherwise reads
/// the current successor without unlinking or freeing. Returns that borrowed
/// task, or NULL at the list end, without dispatching a callback. Callers consume
/// a stop request equal to one before reaching this helper. An exit that already
/// freed an unmarked cursor must retain its inspected bytes without reuse until
/// advancement; this helper does not extend the released allocation's lifetime.
static inline Task* _taskCollectReleasedAndAdvance(Task* cursor, DisplayState* display)
{
    Task* nextTask;

    if (cursor->bodyKind == TASK_BODY_RELEASED) {
        nextTask              = cursor->node.next;
        display->stopTaskWalk = 0;
        _taskUnlinkFromSelectedList(cursor);
        _taskFree(cursor);
        return nextTask;
    }
    return cursor->node.next;
}

void taskExecList(TaskNode* listHead)
{
    Task*         currentTask;
    DisplayState* display;

    currentTask      = listHead->next;
    _gTaskActiveList = listHead;
    if (currentTask != NULL) {
        display = &gDisplayState;
    dispatchTask:
        currentTask->callback(currentTask);
        // A callback can replace the world; stop before reading its cursor again.
        if (display->stopTaskWalk == TASK_WALK_STOP_REQUESTED) {
            display->stopTaskWalk = 0;
            return;
        }
        currentTask = _taskCollectReleasedAndAdvance(currentTask, display);
        if (currentTask != NULL) {
            goto dispatchTask;
        }
    }
}

TaskDesc* Task_GetDesc(u32 idx1, u32 idx2)
{
    TaskDesc* base = gTaskDescBanks[idx1];
    return base + idx2;
}

TaskDesc* taskGetDescAt(TaskDesc* table, u32 index)
{
    return table + index;
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

TaskNode* taskGetActiveList(void)
{
    return _gTaskActiveList;
}

void taskSetActiveList(TaskNode* listHead)
{
    _gTaskActiveList = listHead;
}

void taskResetDefaultList(void)
{
    _gTaskActiveList      = &gTaskDefaultList;
    gTaskDefaultList.next = NULL;
    gTaskDefaultList.prev = &gTaskDefaultList;
}

/// Removes a task's execution links using the selected head for a tail task.
///
/// The task must be live and linked. Its predecessor can be a bare head or an
/// embedded node. When it has no successor, the selected list must own it;
/// otherwise the successor supplies the backlink. Does not dispatch, release
/// resources or clear the removed task's own links.
static void _taskUnlinkFromSelectedList(Task* task)
{
    Task*      nextTask;
    TaskNode*  selectedListHead;
    TaskNode** backlinkSlot;
    TaskNode*  predecessorNode;

    nextTask              = task->node.next;
    selectedListHead      = _gTaskActiveList;
    backlinkSlot          = nextTask == NULL ? &selectedListHead->prev : &nextTask->node.prev;
    predecessorNode       = task->node.prev;
    *backlinkSlot         = predecessorNode;
    predecessorNode->next = task->node.next;
}

/// Releases only the primary-heap task allocation after execution-list unlinking.
///
/// Its owned work, body and teardown relationships must already be released.
/// No handlers run here; the pointer is invalid on return.
static void _taskFree(Task* task)
{
    memFree(task);
}

void taskExecDefaultList(TaskNode* unusedListHead)
{
    Task*         currentTask;
    DisplayState* display;

    currentTask      = gTaskDefaultList.next;
    _gTaskActiveList = &gTaskDefaultList;
    if (currentTask != NULL) {
        display = &gDisplayState;
    dispatchTask:
        currentTask->callback(currentTask);
        // A callback can replace the world; stop before reading its cursor again.
        if (display->stopTaskWalk == TASK_WALK_STOP_REQUESTED) {
            display->stopTaskWalk = 0;
            return;
        }
        currentTask = _taskCollectReleasedAndAdvance(currentTask, display);
        if (currentTask != NULL) {
            goto dispatchTask;
        }
    }
}

void taskExecListForPriority(TaskNode* listHead, s32 priority)
{
    Task*         currentTask;
    DisplayState* display;
    TaskNode*     savedListHead;
    s32           selectedPriority;

    currentTask      = listHead->next;
    savedListHead    = _gTaskActiveList;
    _gTaskActiveList = listHead;
    if (currentTask != NULL) {
        selectedPriority = priority & TASK_PRIORITY_MASK;
        display          = &gDisplayState;
    dispatchTask:
        if (currentTask->priority == selectedPriority) {
            currentTask->callback(currentTask);
        }
        // A callback can replace the world; stop before reading its cursor again.
        if (display->stopTaskWalk == TASK_WALK_STOP_REQUESTED) {
            display->stopTaskWalk = 0;
            goto restoreSelection;
        }
        currentTask = _taskCollectReleasedAndAdvance(currentTask, display);
        if (currentTask != NULL) {
            goto dispatchTask;
        }
    }
restoreSelection:
    _gTaskActiveList = savedListHead;
}

void taskCallExitForPriority(TaskNode* listHead, s32 priority)
{
    Task*         currentTask;
    DisplayState* display;
    TaskNode*     savedListHead;
    s32           selectedPriority;

    currentTask      = listHead->next;
    savedListHead    = _gTaskActiveList;
    _gTaskActiveList = listHead;
    if (currentTask != NULL) {
        selectedPriority = priority & TASK_PRIORITY_MASK;
        display          = &gDisplayState;
    dispatchTask:
        if (currentTask->priority == selectedPriority) {
            taskCallExit(currentTask);
        }
        // A callback can replace the world; stop before reading its cursor again.
        if (display->stopTaskWalk == TASK_WALK_STOP_REQUESTED) {
            display->stopTaskWalk = 0;
            goto restoreSelection;
        }
        currentTask = _taskCollectReleasedAndAdvance(currentTask, display);
        if (currentTask != NULL) {
            goto dispatchTask;
        }
    }
restoreSelection:
    _gTaskActiveList = savedListHead;
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

void gameClearTaskSlots(void)
{
    s32 slot;

    for (slot = (s32)ARRAY_SIZE(gGameSession->ptrSlots) - 1; slot >= 0; slot--) {
        gGameSession->ptrSlots[slot] = NULL;
    }
}

void memCopyBytes(const void* source, void* destination, u32 sizeBytes)
{
    /// Only the low halfword of the requested byte count is copied.
    enum { MEMORY_COPY_BYTE_COUNT_MASK = 0xFFFF };

    u32       tailByteIndex;
    u32       sourceAlignment;
    const u8* sourceCursor;
    u8*       destinationCursor;
    u32       bytesRemaining;

    sourceCursor      = source;
    destinationCursor = destination;
    bytesRemaining    = sizeBytes;

    /// Copies and advances through the next source word boundary.
    ///
    /// Requires at least four remaining bytes and disjoint readable/writable
    /// regions with the same residue modulo four. Cursor/count arguments must
    /// be stable distinct lvalues outside those regions, with no evaluation
    /// side effects: const u8*, u8* and u32. Alignment is a u32 residue in 0..3,
    /// evaluated once. The selected chunk advances both pointers by 4, 3, 2 or
    /// 1 bytes; the arguments have no captured surrounding identifiers.
#define MEMORY_COPY_ALIGNED_CHUNK(sourceCursor, destinationCursor, bytesRemaining, sourceAlignment) \
    do {                                                                                            \
        switch ((sourceAlignment)) {                                                                \
            case 0:                                                                                 \
                *(u32*)(destinationCursor) = *(const u32*)(sourceCursor);                           \
                (sourceCursor)            += sizeof(u32);                                           \
                (destinationCursor)       += sizeof(u32);                                           \
                (bytesRemaining)          -= sizeof(u32);                                           \
                break;                                                                              \
                                                                                                    \
            case 1:                                                                                 \
                *(destinationCursor)++     = *(sourceCursor)++;                                     \
                *(u16*)(destinationCursor) = *(const u16*)(sourceCursor);                           \
                (sourceCursor)            += sizeof(u16);                                           \
                (destinationCursor)       += sizeof(u16);                                           \
                (bytesRemaining)          -= 1 + sizeof(u16);                                       \
                break;                                                                              \
                                                                                                    \
            case 2:                                                                                 \
                *(u16*)(destinationCursor) = *(const u16*)(sourceCursor);                           \
                (sourceCursor)            += sizeof(u16);                                           \
                (destinationCursor)       += sizeof(u16);                                           \
                (bytesRemaining)          -= sizeof(u16);                                           \
                break;                                                                              \
                                                                                                    \
            case 3:                                                                                 \
                *(destinationCursor) = *(sourceCursor);                                             \
                (sourceCursor)      += 1;                                                           \
                (destinationCursor) += 1;                                                           \
                (bytesRemaining)    -= 1;                                                           \
                break;                                                                              \
        }                                                                                           \
    } while (0)

    while ((bytesRemaining & MEMORY_COPY_BYTE_COUNT_MASK) >= sizeof(u32)) {
        // Wide accesses follow source alignment; the destination must share it.
        sourceAlignment = (uintptr)sourceCursor & (sizeof(u32) - 1);

        MEMORY_COPY_ALIGNED_CHUNK(sourceCursor, destinationCursor, bytesRemaining, sourceAlignment);
    }

#undef MEMORY_COPY_ALIGNED_CHUNK

    // Fewer than four effective bytes remain; finish without wide accesses.
    bytesRemaining &= MEMORY_COPY_BYTE_COUNT_MASK;
    tailByteIndex   = 0;
    while ((tailByteIndex & MEMORY_COPY_BYTE_COUNT_MASK) < bytesRemaining) {
        *destinationCursor++ = *sourceCursor++;
        tailByteIndex++;
    }
}
