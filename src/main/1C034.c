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
    D_80070F68.field_101 = 0;
}

void func_8002BBC8(void)
{
    func_8002D428();
    func_80028718();
    Mem_Init();
    func_8002CFDC(0, 9, 0, 0);
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BC0C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BCA8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BD24);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BDB8);

void func_8002BE0C(GStruct0* arg0)
{
    if (D_80070F68.field_101 == 0) {
        func_8002CFDC(0, 2, 0, 0);
        func_8003DB48(0x5010);
        func_8002CCB8(arg0);
        return;
    }
    D_80070F68.field_10b = 1;
    func_8002CCB8(arg0);
    func_8002D428();
    func_80028718();
    Mem_Init();
    func_8002CFDC(0, 9, 0, 0);
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BEA8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BF10);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BF58);

void func_8002BFD4(void)
{
    D_8005ED68 = 0;
}

void func_8002BFE0(GStruct0* arg0)
{
    if (func_8001D344() != 0) {
        func_8002CFDC(0, 0x11, 1, 0);
        func_8002CCB8(arg0);
    }
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C028);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C090);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C1D8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C5A4);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C868);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C8E4);

void func_8002C9B0(s32 arg0)
{
    volatile GStruct25* p;

    p = &D_80071620[arg0];
    p->field_A = 0x3D;
}

void func_8002C9E0(s32 arg0)
{
    volatile GStruct25* p;

    p = &D_80071620[arg0];
    p->field_A = 0;
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CA0C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CA54);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CAB8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CB04);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CCB8);

GStruct0* func_8002CFA0(GStruct2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    return func_8002CB04(&arg0[arg1], arg2, arg3, D_800716D8);
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CFDC);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D03C);

void func_8002D0A4(GStruct0* arg0)
{
    arg0->field_18(arg0);
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D0CC);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D14C);

void func_8002D214(void* arg0, s32 arg1)
{
    D4F564_8005ED64->field_C[arg1] = arg0;
}

void* func_8002D22C(s32 arg0)
{
    return D4F564_8005ED64->field_C[arg0];
}

void func_8002D248(GStruct0Node* node)
{
    D_800716D8 = node;
    node->next = NULL;
    node->prev = node;
}

void func_8002D25C(GStruct0Node* node)
{
    GStruct0* next;
    GStruct0* curr;
    GStruct1* tmp_ptr; // The indirection is required.

    curr       = node->next;
    D_800716D8 = node;
    if (curr != NULL) {
        tmp_ptr = &D_80070F68;
    loop_2:
        curr->field_14(curr);
        if (tmp_ptr->field_10b == 1) {
            tmp_ptr->field_10b = 0;
            return;
        }
        if (curr->field_28 == 0xFF) {
            next               = curr->node.next;
            tmp_ptr->field_10b = 0;
            func_8002D444(curr);
            func_8002D474(curr);
            curr = next;
        } else {
            curr = curr->node.next;
        }
        if (curr != NULL) {
            goto loop_2;
        }
    }
}

GStruct2* func_8002D304(u32 idx1, u32 idx2)
{
    GStruct2* base = D_8005EF74[idx1];
    return base + idx2;
}

GStruct2* func_8002D32C(GStruct2* base, u32 idx)
{
    return base + idx;
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D340);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D3BC);

GStruct0Node* func_8002D40C(void)
{
    return D_800716D8;
}

void func_8002D41C(GStruct0Node* node)
{
    D_800716D8 = node;
}

void func_8002D428(void)
{
    D_800716D8 = &D_800716E0;
    D_800716E0.next = NULL;
    D_800716E0.prev = &D_800716E0;
}

void func_8002D444(GStruct0* state)
{
    GStruct0* next;
    GStruct0Node* head;
    GStruct0Node** pp;
    GStruct0Node* prev;

    next = state->node.next;
    head = D_800716D8;
    do {
        pp = &head->prev;
        if (next != NULL) {
            pp = &next->node.prev;
        }
    } while (0);
    prev = state->node.prev;
    *pp = prev;
    prev->next = state->node.next;
}

void func_8002D474(GStruct0* state)
{
    Mem_Free(state);
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D494);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D544);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D61C);

void func_8002D6EC(GStruct0* arg0)
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
