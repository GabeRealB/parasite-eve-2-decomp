#include "common.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/1C034", func_8002B834);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BA9C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BB9C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BBC8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BC0C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BCA8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BD24);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BDB8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BE0C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BEA8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BF10);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BF58);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BFD4);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002BFE0);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C028);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C090);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C1D8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C5A4);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C868);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C8E4);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C9B0);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C9E0);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CA0C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CA54);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CAB8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CB04);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CCB8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CFA0);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002CFDC);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D03C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D0A4);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D0CC);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D14C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D214);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D22C);

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

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D40C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D41C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D428);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D444);

void func_8002D474(GStruct0* state)
{
    Mem_Free(state);
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D494);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D544);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D61C);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D6EC);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D780);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002D7A8);
