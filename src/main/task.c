#include "common.h"

#include "gameplay/gameplay.h"
#include "main/unknown_syms.h"
#include "main/text.h"

Task* Task_SpawnFromDesc(TaskDesc* desc, s32 arg1, s32 arg2, TaskNode* list)
{
    Task*          task;
    s32            type;
    s32            flags_a2;
    void*          extra;
    u16            flags;
    s32            temp;
    u8             flags_lo;
    s32            priority;
    register Task* curr asm("a3");
    TaskNode**     link;

    task = memCalloc(0x48, 0);
    if (task == NULL) {
        return NULL;
    }

    flags = desc->flags;
    type  = flags & 0xFF;
    if (type == 1) {
        goto case1;
    }
    extra = NULL;
    if (type < 2) {
        goto merge;
    }
    if (type == 2) {
        goto case2;
    }
    goto merge;

case1:
    temp     = flags & 0x100;
    flags_a2 = (u32)temp > 0;
    if (D_8005ED8C != 0) {
        flags_a2 |= 2;
    }
    extra = Gp_AttachTmdFlags(task, desc->arg.model, flags_a2);
    goto merge;

case2:
    extra = gpAttachDisp2d(task);

merge:
    if (((u8)desc->flags == 0) || (extra != NULL)) {
        task->callback     = desc->callback;
        priority           = *(u8*)&desc->priority;
        task->exitCallback = taskKill;
        task->priority     = priority;
        flags_lo           = (u8)desc->flags;
        task->extra        = extra;
        task->spawnArg1    = arg1;
        task->spawnArg2    = (void*)arg2;
        task->parent       = NULL;
        task->firstChild   = NULL;
        task->nextSibling  = task;
        task->spawnType    = flags_lo;
        curr               = list->next;
        if (curr != NULL) {
            priority &= 0xFF;
        loop:
            if ((u32)priority >= (u8)curr->priority) {
                curr = curr->node.next;
                if (curr != NULL) {
                    goto loop;
                }
            }
        }
        if (curr != NULL) {
            link = &curr->node.prev;
        } else {
            link = &list->prev;
        }
        task->node.next = (*link)->next;
        (*link)->next   = task;
        task->node.prev = *link;
        *link           = &task->node;
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
    TaskNode*  saved;
    TaskNode** pp;
    TaskNode*  prev;
    void*      extra;
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

    if (gDisplayState.skipTeardown == 0) {
        type = task->spawnType;
        if (type == 1) {
            goto case1;
        }
        if (type < 2) {
            goto def_case;
        }
        if (type == 2) {
            goto case2;
        }
        goto def_case;

    case1:
        ((TmdObject*)task->extra)->flags |= 0x80;
        task->killCountdown               = 2;
        task->callback                    = taskCountdownCallback;
        task->state                       = 0;
        task->exitCallback                = textNoopCallback;
        return;

    case2:
        gpUnlinkDisp2d(task->extra);
        task->killCountdown = 1;
        task->callback      = textNoopCallback;
        task->exitCallback  = textNoopCallback;
        task->killCountdown--;
        if (task->killCountdown != 0) {
            return;
        }
        if (task->spawnType == 1) {
            goto cu1;
        }
        if (task->spawnType != type) {
            goto cu_def;
        }
        goto cu2;

    def_case:
        task->killCountdown = 1;
        task->callback      = textNoopCallback;
        task->exitCallback  = textNoopCallback;
        task->killCountdown--;
        if (task->killCountdown != 0) {
            return;
        }
        if (task->spawnType == 1) {
            goto cu1;
        }
        if (task->spawnType == 2) {
            goto cu2;
        }
        goto cu_def;

    cu1:
        extra = task->extra;
        gpUnlinkTmd(extra);
        gpFreeTmd(extra);
        goto cu_def;

    cu2:
        gpFreeDisp2d(task->extra);

    cu_def:
        task->spawnType = 0xFF;
        return;
    }

    t = task->spawnType;
    if (t == 1) {
        goto imm1;
    }
    if (t == 2) {
        goto imm2;
    }
    goto imm_unlink;

imm1:
    gpUnlinkTmd(task->extra);
    gpFreeTmd(task->extra);
    goto imm_unlink;

imm2:
    gpUnlinkDisp2d(task->extra);
    gpFreeDisp2d(task->extra);

imm_unlink:
    saved           = gTaskActiveList;
    next            = task->node.next;
    gTaskActiveList = &gTaskDefaultList;
    if (next == NULL) {
        pp = &gTaskDefaultList.prev;
    } else {
        pp = &next->node.prev;
    }
    prev       = task->node.prev;
    *pp        = prev;
    prev->next = task->node.next;
    memFree(task);
    gTaskActiveList = saved;
}

Task* Task_SpawnFromTable(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    return Task_SpawnFromDesc(&arg0[arg1], arg2, arg3, gTaskActiveList);
}

Task* Task_Spawn(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TaskDesc* ptr;

    if (arg0 >= 0) {
        ptr = gTaskDescBanks[arg0];
        ptr = &ptr[arg1];
    } else {
        ptr = (TaskDesc*)arg1;
    }
    return Task_SpawnFromDesc(ptr, arg2, arg3, gTaskActiveList);
}

void Task_KillChildren(Task* arg0)
{
    Task* start;
    Task* cur;
    Task* temp;

    temp = arg0->firstChild;
    if (temp != NULL) {
        start = temp;
        cur   = start;
        do {
            cur->parent = NULL;
            cur->exitCallback(cur);
            cur = cur->nextSibling;
        } while (cur != start);
    }
    arg0->firstChild = NULL;
}

void Task_CallExit(Task* arg0)
{
    arg0->exitCallback(arg0);
}

void Task_DetachFromParent(Task* arg0)
{
    Task* parent;
    Task* next;
    Task* cur;

    parent = arg0->parent;
    if (parent == NULL) {
        return;
    }

    next = arg0->nextSibling;
    if (next == arg0) {
        parent->firstChild = NULL;
    } else {
        if (parent->firstChild == arg0) {
            parent->firstChild = next;
        }
        cur = arg0;
        if (arg0->nextSibling != arg0) {
            do {
                cur = cur->nextSibling;
            } while (cur->nextSibling != arg0);
        }
        cur->nextSibling  = arg0->nextSibling;
        arg0->nextSibling = arg0;
    }
    arg0->parent = NULL;
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
    gTaskActiveList = node;
    node->next      = NULL;
    node->prev      = node;
}

void Task_ExecList(TaskNode* node)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr; // The indirection is required.

    curr            = node->next;
    gTaskActiveList = node;
    if (curr != NULL) {
        tmp_ptr = &gDisplayState;
    loop_2:
        curr->callback(curr);
        if (tmp_ptr->stopTaskWalk == 1) {
            tmp_ptr->stopTaskWalk = 0;
            return;
        }
        if (curr->spawnType == 0xFF) {
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

void Task_RequestKill(Task* arg0, s32 arg1)
{
    Task* start;
    Task* cur;
    Task* temp;

    arg0->status     = 0xFF;
    arg0->extraState = arg1;
    arg0->callback   = textNoopCallback;

    temp = arg0->firstChild;
    if (temp != NULL) {
        start = temp;
        cur   = start;
        do {
            cur->parent = NULL;
            cur->exitCallback(cur);
            cur = cur->nextSibling;
        } while (cur != start);
    }
    arg0->firstChild = NULL;
}

s32 Task_PollKill(Task* arg0, s32* arg1)
{
    s32 result;

    result = 0;
    if (arg0->status == 0xFF) {
        if (arg1 != NULL) {
            *arg1 = arg0->extraState;
        }
        arg0->exitCallback(arg0);
        result = 1;
    }
    return result;
}

TaskNode* Task_GetActiveList(void)
{
    return gTaskActiveList;
}

void Task_SetActiveList(TaskNode* node)
{
    gTaskActiveList = node;
}

void Task_ResetDefaultList(void)
{
    gTaskActiveList       = &gTaskDefaultList;
    gTaskDefaultList.next = NULL;
    gTaskDefaultList.prev = &gTaskDefaultList;
}

void Task_Unlink(Task* state)
{
    Task*      next;
    TaskNode*  head;
    TaskNode** pp;
    TaskNode*  prev;

    next = state->node.next;
    head = gTaskActiveList;
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

void Task_Free(Task* state)
{
    memFree(state);
}

void Task_ExecDefaultList(TaskNode* unused)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr; // The indirection is required.

    curr            = gTaskDefaultList.next;
    gTaskActiveList = &gTaskDefaultList;
    if (curr != NULL) {
        tmp_ptr = &gDisplayState;
    loop_2:
        curr->callback(curr);
        if (tmp_ptr->stopTaskWalk == 1) {
            tmp_ptr->stopTaskWalk = 0;
            return;
        }
        if (curr->spawnType == 0xFF) {
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
    TaskNode*     saved;
    s32           filter;

    curr            = node->next;
    saved           = gTaskActiveList;
    gTaskActiveList = node;
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
        if (curr->spawnType == 0xFF) {
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
    gTaskActiveList = saved;
}

void Task_CallExitFiltered(TaskNode* node, s32 arg1)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr;
    TaskNode*     saved;
    s32           filter;

    curr            = node->next;
    saved           = gTaskActiveList;
    gTaskActiveList = node;
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
        if (curr->spawnType == 0xFF) {
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
    gTaskActiveList = saved;
}

void taskCountdownCallback(Task* task)
{
    void* temp_s0;

    task->killCountdown--;
    if (task->killCountdown != 0) {
        return;
    }

    switch (task->spawnType) {
        case 1:
            temp_s0 = task->extra;
            gpUnlinkTmd(temp_s0);
            gpFreeTmd(temp_s0);
            task->spawnType = 0xFF;
            break;
        case 2:
            gpFreeDisp2d(task->extra);
            task->spawnType = 0xFF;
            break;
        default:
            task->spawnType = 0xFF;
            break;
    }
}

void Game_ClearPtrSlots(void)
{
    s32 i;

    for (i = 0xF; i >= 0; i--) {
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
