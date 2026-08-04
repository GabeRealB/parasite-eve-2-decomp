#include "common.h"
#include "main/task.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/task", Task_SpawnFromDesc);

INCLUDE_ASM("main/nonmatchings/task", Task_Kill);

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

INCLUDE_ASM("main/nonmatchings/task", func_8002D14C);

void func_8002D214(void* arg0, s32 arg1)
{
    D4F564_8005ED64->field_C[arg1] = arg0;
}

void* func_8002D22C(s32 arg0)
{
    return D4F564_8005ED64->field_C[arg0];
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

void func_8002D780(void)
{
    s32 i;

    for (i = 0xF; i >= 0; i--) {
        D4F564_8005ED64->field_C[i] = NULL;
    }
}

INCLUDE_ASM("main/nonmatchings/task", func_8002D7A8);
