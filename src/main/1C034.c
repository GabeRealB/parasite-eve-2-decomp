#include "common.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/1C034", func_8002B834);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BA9C);

void func_8002BB9C(void)
{
    u8* ptr;
    u32 i;

    ptr = (u8*)D4F564_8005ED64;
    for (i = 0; i < sizeof(GStruct14); i++) {
        *ptr++ = 0;
    }
    Display_State.field_101 = 0;
}

void func_8002BBC8(void)
{
    Task_ResetDefaultList();
    func_80028718();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

void func_8002BC0C(Task* arg0)
{
    u8*         ptr;
    u32         i;
    s32         saved;
    CdCmdQueue* p;

    p     = &CdCmd_Queue;
    saved = D_80072189;
    ptr   = (u8*)D4F564_8005ED64;
    for (i = 0; i < sizeof(GStruct14); i++) {
        *ptr++ = 0;
    }
    Display_State.field_101 = 0;
    Display_State.field_12e = 1;
    p->field_248            = 1;
    p->field_244            = 1;
    D_800710A8.field_4      = 1;
    func_800303AC();
    do {
        D_80072189 = saved;
    } while (0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_8002BCA8(Task* arg0)
{
    void* temp_v0;

    func_800280F4(0);
    temp_v0        = func_800486F0(D_800611C8, 0, 1, 0, 0);
    arg0->field_20 = temp_v0;
    if (temp_v0 != 0) {
        Display_State.field_11e  = 0xFF;
        D4F564_8005ED64->field_2 = 1;
        arg0->field_2a           = 0x10;
        arg0->field_30           = arg0->field_30 + 1;
    }
}

void func_8002BD24(Task* arg0)
{
    UiObject* obj;

    obj = arg0->field_20;
    if (obj->field_2E == -1) {
        func_80048838(obj, obj->field_28);
        Display_State.field_11e  = 0;
        D4F564_8005ED64->field_2 = 0;
        if (D_80072311 == 1) {
            func_800260B0(0);
        } else {
            func_800260B0(1);
        }
        func_800429C8(0);
        arg0->field_2a = 0xC;
        arg0->field_30 = arg0->field_30 + 1;
    }
}

void func_8002BDB8(Task* arg0)
{
    arg0->field_2a--;
    if (arg0->field_2a != 0) {
        return;
    }
    Pad_SetCooldown(0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_8002BE0C(Task* arg0)
{
    if (Display_State.field_101 == 0) {
        Task_Spawn(0, 2, 0, 0);
        func_8003DB48(0x5010);
        Task_Kill(arg0);
        return;
    }
    Display_State.field_10b = 1;
    Task_Kill(arg0);
    Task_ResetDefaultList();
    func_80028718();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

void func_8002BEA8(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_800134BC;
    sp.funcs[arg0->field_30](arg0);
}

void func_8002BF10(Task* arg0)
{
    ((GStructOverlayAt4*)D4F564_8005ED64)->field_4 =
        ((GStructOverlayAt4*)&Mc_SaveData)->field_4;
    D_8007A394     = 0;
    arg0->field_30 = arg0->field_30 + 1;
}

void func_8002BF58(Task* arg0)
{
    u8 param1[8];
    u8 param2[8];

    if ((u8)func_80042500() == 0) {
        F12D18_8002252C(&D4F564_8005ED64->field_4, 0);
        param1[3] = 0;
        param1[2] = 0;
        param1[0] = 0;
        param2[0] = 0;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        arg0->field_30 = arg0->field_30 + 1;
    }
}

void func_8002BFD4(void)
{
    D_8005ED68 = 0;
}

void func_8002BFE0(Task* arg0)
{
    if (func_8001D344() != 0) {
        Task_Spawn(0, 0x11, 1, 0);
        Task_Kill(arg0);
    }
}

void func_8002C028(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800134D0;
    Pad_SetCooldown(0);
    sp.funcs[arg0->field_30](arg0);
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C090);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C1D8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C5A4);

s32 Pad_CheckButtons(s32 arg0, s32 arg1, s32 arg2)
{
    PadState* p;
    u16       val;

    p = (PadState*)&Pad_States[arg0];
    switch (arg1) {
        case 1:
            val = p->field_6;
            break;
        case 3:
            val = p->field_8;
            break;
        default:
            val = p->field_4;
            break;
    }
    if (arg1 == 2) {
        return (val & arg2) == arg2;
    }
    return (val & arg2) != 0;
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C8E4);

void Pad_SetCooldown(s32 arg0)
{
    volatile PadState* p;

    p          = &Pad_States[arg0];
    p->field_A = 0x3D;
}

void Pad_ClearCooldown(s32 arg0)
{
    volatile PadState* p;

    p          = &Pad_States[arg0];
    p->field_A = 0;
}

s32 Pad_ReadButtonsInv(s32 arg0)
{
    u16         sp;
    PadRawPort* base;

    base          = Pad_RawPorts;
    ((u8*)&sp)[1] = base[arg0].field_2;
    ((u8*)&sp)[0] = base[arg0].field_3;
    return (u16)~sp;
}

void Pad_ClearEvents(s32 arg0)
{
    PadState* p;
    s32       i;
    s32       j;
    s32       offset;
    PadEvent* entries;

    p      = (PadState*)&Pad_States[arg0];
    i      = 0;
    offset = 0x10;
    for (; i < 2; i++) {
        entries = p->field_10[i];
        for (j = 0; j < 8; j++) {
            entries[j].field_0 = 0;
            entries[j].field_1 = 0;
            entries[j].field_2 = 0;
        }
        offset += 0x20;
    }
    p->field_2 = 0;
}

s32 Pad_CheckSpecialCombo(void)
{
    volatile PadState* p;
    u16                val;
    s32                result;

    p   = Pad_States;
    val = p->field_4;
    if (val == 0x90F) {
        result = D_8005ED8A == 0x90F;
    } else {
        result = 0;
    }
    D_8005ED8A = val;
    if (p->field_A != 0) {
        D_8005ED8A = 0;
        result     = 0;
    }
    return result;
}

INCLUDE_ASM("main/nonmatchings/1C034", Task_SpawnFromDesc);

INCLUDE_ASM("main/nonmatchings/1C034", Task_Kill);

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

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D14C);

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

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D7A8);
