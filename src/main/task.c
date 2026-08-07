#include "common.h"
#include "main/task.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

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
    extra = func_80099170(task, desc->field_8, flags_a2);
    goto merge;

case2:
    extra = func_80099098(task);

merge:
    if (((u8)desc->flags == 0) || (extra != NULL)) {
        task->field_14 = desc->callback;
        priority       = *(u8*)&desc->field_2;
        task->field_18 = Task_Kill;
        task->field_29 = priority;
        flags_lo       = (u8)desc->flags;
        task->field_2c = extra;
        task->field_34 = arg1;
        task->field_20 = (void*)arg2;
        task->field_8  = NULL;
        task->field_c  = NULL;
        task->field_10 = task;
        task->field_28 = flags_lo;
        curr           = list->next;
        if (curr != NULL) {
            priority &= 0xFF;
        loop:
            if ((u32)priority >= (u8)curr->field_29) {
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

    temp = arg0->field_c;
    if (temp != NULL) {
        start = temp;
        cur   = start;
        do {
            cur->field_8 = NULL;
            cur->field_18(cur);
            cur = cur->field_10;
        } while (cur != start);
    }

    {
        register Task* p asm("v1");
        register Task* n asm("a0");

        p = arg0->field_8;
        if (p != NULL) {
            n = arg0->field_10;
            if (n == arg0) {
                p->field_c = NULL;
            } else {
                if (p->field_c == arg0) {
                    p->field_c = n;
                }
                cur = arg0;
                if (arg0->field_10 != arg0) {
                    do {
                        cur = cur->field_10;
                    } while (cur->field_10 != arg0);
                }
                cur->field_10 = arg0->field_10;
            }
        }
    }

    if (arg0->field_1C != NULL) {
        Mem_Free(arg0->field_1C);
    }

    if (Display_State.field_123 == 0) {
        type = arg0->field_28;
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
        ((GameActorExt*)arg0->field_2c)->field_C |= 0x80;
        arg0->field_2a                            = 2;
        arg0->field_14                            = func_8002D6EC;
        arg0->field_30                            = 0;
        arg0->field_18                            = (TaskFunc)func_8002DEC4;
        return;

    case2:
        func_80099258(arg0->field_2c);
        arg0->field_2a = 1;
        arg0->field_14 = (TaskFunc)func_8002DEC4;
        arg0->field_18 = (TaskFunc)func_8002DEC4;
        arg0->field_2a--;
        if (arg0->field_2a != 0) {
            return;
        }
        if (arg0->field_28 == 1) {
            goto cu1;
        }
        if (arg0->field_28 != type) {
            goto cu_def;
        }
        goto cu2;

    def_case:
        arg0->field_2a = 1;
        arg0->field_14 = (TaskFunc)func_8002DEC4;
        arg0->field_18 = (TaskFunc)func_8002DEC4;
        arg0->field_2a--;
        if (arg0->field_2a != 0) {
            return;
        }
        if (arg0->field_28 == 1) {
            goto cu1;
        }
        if (arg0->field_28 == 2) {
            goto cu2;
        }
        goto cu_def;

    cu1:
        extra = arg0->field_2c;
        func_800991DC(extra);
        func_80099214(extra);
        goto cu_def;

    cu2:
        func_80099290(arg0->field_2c);

    cu_def:
        arg0->field_28 = 0xFF;
        return;
    }

    {
        register s32 t asm("v1");

        t = arg0->field_28;
        if (t == 1) {
            goto imm1;
        }
        if (t == 2) {
            goto imm2;
        }
    }
    goto imm_unlink;

imm1:
    func_800991DC(arg0->field_2c);
    func_80099214(arg0->field_2c);
    goto imm_unlink;

imm2:
    func_80099258(arg0->field_2c);
    func_80099290(arg0->field_2c);

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

    temp = arg0->field_c;
    if (temp != NULL) {
        start = temp;
        cur   = start;
        do {
            cur->field_8 = NULL;
            cur->field_18(cur);
            cur = cur->field_10;
        } while (cur != start);
    }
    arg0->field_c = NULL;
}

void Task_CallExit(Task* arg0)
{
    arg0->field_18(arg0);
}

void Task_DetachFromParent(Task* arg0)
{
    Task* parent;
    Task* next;
    Task* cur;

    parent = arg0->field_8;
    if (parent == NULL) {
        return;
    }

    next = arg0->field_10;
    if (next == arg0) {
        parent->field_c = NULL;
    } else {
        if (parent->field_c == arg0) {
            parent->field_c = next;
        }
        cur = arg0;
        if (arg0->field_10 != arg0) {
            do {
                cur = cur->field_10;
            } while (cur->field_10 != arg0);
        }
        cur->field_10  = arg0->field_10;
        arg0->field_10 = arg0;
    }
    arg0->field_8 = NULL;
}

void Task_Reparent(Task* arg0, Task* arg1)
{
    Task* parent;
    Task* next;
    Task* cur;
    Task* temp;

    parent = arg1->field_8;
    if (parent != NULL) {
        next = arg1->field_10;
        if (next == arg1) {
            parent->field_c = NULL;
        } else {
            if (parent->field_c == arg1) {
                parent->field_c = next;
            }
            cur = arg1;
            if (arg1->field_10 != arg1) {
                do {
                    cur = cur->field_10;
                } while (cur->field_10 != arg1);
            }
            cur->field_10  = arg1->field_10;
            arg1->field_10 = arg1;
        }
        arg1->field_8 = NULL;
    }
    arg1->field_8 = arg0;
    temp          = arg0->field_c;
    if (temp == NULL) {
        arg0->field_c = arg1;
        return;
    }
    cur  = temp;
    arg0 = temp;
    if (cur->field_10 != cur) {
        do {
            cur = cur->field_10;
        } while (cur->field_10 != arg0);
    }
    arg1->field_10 = arg0;
    cur->field_10  = arg1;
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
        curr->field_14(curr);
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            return;
        }
        if (curr->field_28 == 0xFF) {
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

    arg0->field_38 = 0xFF;
    arg0->field_3c = arg1;
    arg0->field_14 = (TaskFunc)func_8002DEC4;

    temp = arg0->field_c;
    if (temp != NULL) {
        start = temp;
        cur   = start;
        do {
            cur->field_8 = NULL;
            cur->field_18(cur);
            cur = cur->field_10;
        } while (cur != start);
    }
    arg0->field_c = NULL;
}

s32 Task_PollKill(Task* arg0, s32* arg1)
{
    s32 result;

    result = 0;
    if (arg0->field_38 == 0xFF) {
        if (arg1 != NULL) {
            *arg1 = arg0->field_3c;
        }
        arg0->field_18(arg0);
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
        curr->field_14(curr);
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            return;
        }
        if (curr->field_28 == 0xFF) {
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
        if (curr->field_29 == (u8)filter) {
            curr->field_14(curr);
        }
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            goto end;
        }
        if (curr->field_28 == 0xFF) {
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
        if (curr->field_29 == (u8)filter) {
            Task_CallExit(curr);
        }
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            goto end;
        }
        if (curr->field_28 == 0xFF) {
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

void func_8002D6EC(Task* arg0)
{
    void* temp_s0;

    arg0->field_2a--;
    if (arg0->field_2a != 0) {
        return;
    }

    switch (arg0->field_28) {
        case 1:
            temp_s0 = arg0->field_2c;
            func_800991DC(temp_s0);
            func_80099214(temp_s0);
            arg0->field_28 = 0xFF;
            break;
        case 2:
            func_80099290(arg0->field_2c);
            arg0->field_28 = 0xFF;
            break;
        default:
            arg0->field_28 = 0xFF;
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

void func_8002D7A8(void* src, void* dest, u32 count)
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
