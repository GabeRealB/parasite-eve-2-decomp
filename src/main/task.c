#include "common.h"

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

    task = Mem_Calloc(0x48, 0);
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
    extra = Gp_AttachTmdFlags(task, (TmdSource*)desc->setupArg, flags_a2);
    goto merge;

case2:
    extra = Gp_AttachDisp2d(task);

merge:
    if (((u8)desc->flags == 0) || (extra != NULL)) {
        task->callback     = desc->callback;
        priority           = *(u8*)&desc->priority;
        task->exitCallback = Task_Kill;
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
            curr = (Task*)&curr->node.prev;
        } else {
            curr = (Task*)&list->prev;
        }
        task->node.next           = (*(TaskNode**)curr)->next;
        (*(TaskNode**)curr)->next = task;
        task->node.prev           = *(TaskNode**)curr;
        *(TaskNode**)curr         = &task->node;
    } else {
        Mem_Free(task);
        task = NULL;
    }
    return task;
}

void Task_Kill(Task* arg0)
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

    p = arg0->parent;
    if (p != NULL) {
        n = arg0->nextSibling;
        if (n == arg0) {
            p->firstChild = NULL;
        } else {
            if (p->firstChild == arg0) {
                p->firstChild = n;
            }
            cur = arg0;
            if (arg0->nextSibling != arg0) {
                do {
                    cur = cur->nextSibling;
                } while (cur->nextSibling != arg0);
            }
            cur->nextSibling = arg0->nextSibling;
        }
    }

    if (arg0->idMap != NULL) {
        Mem_Free(arg0->idMap);
    }

    if (Display_State.skipTeardown == 0) {
        type = arg0->spawnType;
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
        ((TmdObject*)arg0->extra)->field_C |= 0x80;
        arg0->killCountdown                 = 2;
        arg0->callback                      = Task_CountdownCallback;
        arg0->state                         = 0;
        arg0->exitCallback                  = (TaskFunc)func_8002DEC4;
        return;

    case2:
        Gp_UnlinkDisp2d(arg0->extra);
        arg0->killCountdown = 1;
        arg0->callback      = (TaskFunc)func_8002DEC4;
        arg0->exitCallback  = (TaskFunc)func_8002DEC4;
        arg0->killCountdown--;
        if (arg0->killCountdown != 0) {
            return;
        }
        if (arg0->spawnType == 1) {
            goto cu1;
        }
        if (arg0->spawnType != type) {
            goto cu_def;
        }
        goto cu2;

    def_case:
        arg0->killCountdown = 1;
        arg0->callback      = (TaskFunc)func_8002DEC4;
        arg0->exitCallback  = (TaskFunc)func_8002DEC4;
        arg0->killCountdown--;
        if (arg0->killCountdown != 0) {
            return;
        }
        if (arg0->spawnType == 1) {
            goto cu1;
        }
        if (arg0->spawnType == 2) {
            goto cu2;
        }
        goto cu_def;

    cu1:
        extra = arg0->extra;
        Gp_UnlinkTmd(extra);
        Gp_FreeTmd(extra);
        goto cu_def;

    cu2:
        Gp_FreeDisp2d(arg0->extra);

    cu_def:
        arg0->spawnType = 0xFF;
        return;
    }

    t = arg0->spawnType;
    if (t == 1) {
        goto imm1;
    }
    if (t == 2) {
        goto imm2;
    }
    goto imm_unlink;

imm1:
    Gp_UnlinkTmd(arg0->extra);
    Gp_FreeTmd(arg0->extra);
    goto imm_unlink;

imm2:
    Gp_UnlinkDisp2d(arg0->extra);
    Gp_FreeDisp2d(arg0->extra);

imm_unlink:
    saved           = Task_ActiveList;
    next            = arg0->node.next;
    Task_ActiveList = &Task_DefaultList;
    if (next == NULL) {
        pp = &Task_DefaultList.prev;
    } else {
        pp = &next->node.prev;
    }
    prev       = arg0->node.prev;
    *pp        = prev;
    prev->next = arg0->node.next;
    Mem_Free(arg0);
    Task_ActiveList = saved;
}

Task* Task_SpawnFromTable(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    return Task_SpawnFromDesc(&arg0[arg1], arg2, arg3, Task_ActiveList);
}

Task* Task_Spawn(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TaskDesc* ptr;

    if (arg0 >= 0) {
        ptr = Task_DescBanks[arg0];
        ptr = &ptr[arg1];
    } else {
        ptr = (TaskDesc*)arg1;
    }
    return Task_SpawnFromDesc(ptr, arg2, arg3, Task_ActiveList);
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

void Game_SetPtrSlot(void* arg0, s32 arg1)
{
    Game_Session->field_C[arg1] = arg0;
}

void* Game_GetPtrSlot(s32 arg0)
{
    return Game_Session->field_C[arg0];
}

void Task_InitList(TaskNode* node)
{
    Task_ActiveList = node;
    node->next      = NULL;
    node->prev      = node;
}

void Task_ExecList(TaskNode* node)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr; // The indirection is required.

    curr            = node->next;
    Task_ActiveList = node;
    if (curr != NULL) {
        tmp_ptr = &Display_State;
    loop_2:
        curr->callback(curr);
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            return;
        }
        if (curr->spawnType == 0xFF) {
            next               = curr->node.next;
            tmp_ptr->field_10b = 0;
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
    TaskDesc* base = Task_DescBanks[idx1];
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

    arg0->flags      = 0xFF;
    arg0->extraState = arg1;
    arg0->callback   = (TaskFunc)func_8002DEC4;

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
    if (arg0->flags == 0xFF) {
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
    return Task_ActiveList;
}

void Task_SetActiveList(TaskNode* node)
{
    Task_ActiveList = node;
}

void Task_ResetDefaultList(void)
{
    Task_ActiveList       = &Task_DefaultList;
    Task_DefaultList.next = NULL;
    Task_DefaultList.prev = &Task_DefaultList;
}

void Task_Unlink(Task* state)
{
    Task*      next;
    TaskNode*  head;
    TaskNode** pp;
    TaskNode*  prev;

    next = state->node.next;
    head = Task_ActiveList;
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
    Mem_Free(state);
}

void Task_ExecDefaultList(TaskNode* node)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr; // The indirection is required.

    curr            = Task_DefaultList.next;
    Task_ActiveList = &Task_DefaultList;
    if (curr != NULL) {
        tmp_ptr = &Display_State;
    loop_2:
        curr->callback(curr);
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            return;
        }
        if (curr->spawnType == 0xFF) {
            next               = curr->node.next;
            tmp_ptr->field_10b = 0;
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
    saved           = Task_ActiveList;
    Task_ActiveList = node;
    if (curr != NULL) {
        filter  = arg1 & 0xFF;
        tmp_ptr = &Display_State;
    loop_2:
        if (curr->priority == (u8)filter) {
            curr->callback(curr);
        }
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            goto end;
        }
        if (curr->spawnType == 0xFF) {
            next               = curr->node.next;
            tmp_ptr->field_10b = 0;
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
    Task_ActiveList = saved;
}

void Task_CallExitFiltered(TaskNode* node, s32 arg1)
{
    Task*         next;
    Task*         curr;
    DisplayState* tmp_ptr;
    TaskNode*     saved;
    s32           filter;

    curr            = node->next;
    saved           = Task_ActiveList;
    Task_ActiveList = node;
    if (curr != NULL) {
        filter  = arg1 & 0xFF;
        tmp_ptr = &Display_State;
    loop_2:
        if (curr->priority == (u8)filter) {
            Task_CallExit(curr);
        }
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            goto end;
        }
        if (curr->spawnType == 0xFF) {
            next               = curr->node.next;
            tmp_ptr->field_10b = 0;
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
    Task_ActiveList = saved;
}

void Task_CountdownCallback(Task* arg0)
{
    void* temp_s0;

    arg0->killCountdown--;
    if (arg0->killCountdown != 0) {
        return;
    }

    switch (arg0->spawnType) {
        case 1:
            temp_s0 = arg0->extra;
            Gp_UnlinkTmd(temp_s0);
            Gp_FreeTmd(temp_s0);
            arg0->spawnType = 0xFF;
            break;
        case 2:
            Gp_FreeDisp2d(arg0->extra);
            arg0->spawnType = 0xFF;
            break;
        default:
            arg0->spawnType = 0xFF;
            break;
    }
}

void Game_ClearPtrSlots(void)
{
    s32 i;

    for (i = 0xF; i >= 0; i--) {
        Game_Session->field_C[i] = NULL;
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
