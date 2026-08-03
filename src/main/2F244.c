#include "common.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/2F244", func_8003EA44);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003EC44);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003EE68);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F034);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F450);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F5A4);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F690);

s32 func_8003F6F8(void)
{
    D_80062698->field_1c |= 0x80000000;
    return 0;
}

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F71C);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F7A8);

s32 func_8003F848(void)
{
    D_80062698->field_1c |= 0x20000000;
    return 0;
}

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F86C);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F900);

s32 func_8003F944(void)
{
    return (D_80062698->field_1c & 0x48000000) != 0;
}

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F964);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F9AC);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F9F4);

void func_8003FA3C(u8 arg0)
{
    D_80062698->field_1a = arg0;
}

INCLUDE_ASM("main/nonmatchings/2F244", func_8003FA4C);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003FB20);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003FB70);

u8 func_8003FC18(void)
{
    return D_80062698->field_12;
}

void func_8003FC30(u8 arg0)
{
    GStruct17* temp;

    temp = D_80062698;
    if (temp->field_15 == 1) {
        temp->field_11 = arg0;
        func_8003F450(0);
    }
}

void func_8003FC6C(void)
{
    D_80062698->field_17 = 0;
    D_80062698->field_1a = 0xFF;
}

void func_8003FC8C(GStruct0* arg0)
{
    func_8002C9B0(0);
    if (func_8001CDF0() != 0) {
        arg0->field_30 += 1;
    } else {
        D_80071620[0].field_A = 1;
        func_8003EE68();
        arg0->field_30 += 2;
    }
}

void func_8003FCF8(GStruct0* arg0)
{
    func_8002C9B0(0);
    if (func_8001D82C() != 0) {
        D_80071620[0].field_A = 1;
        func_8003EE68();
        arg0->field_30 += 1;
    }
}

INCLUDE_ASM("main/nonmatchings/2F244", func_8003FD58);

void func_8003FE00(GStruct0* arg0)
{
    if (func_8001D82C() != 0) {
        arg0->field_30 += 1;
    }
}

void func_8003FE40(GStruct0* arg0)
{
    if (func_8001D0E8() != 0) {
        D_80070F68.field_1e  = 0;
        D_80070F68.field_10d = 0;
        D_80070F68.field_100 = 1;
        func_8003DE14(-1, 0, 0);
        func_8002D0A4(arg0);
    }
}

INCLUDE_ASM("main/nonmatchings/2F244", func_8003FE9C);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003FF14);

INCLUDE_ASM("main/nonmatchings/2F244", func_8004017C);

INCLUDE_ASM("main/nonmatchings/2F244", func_800405E0);

INCLUDE_ASM("main/nonmatchings/2F244", func_80040820);

INCLUDE_ASM("main/nonmatchings/2F244", func_800408C0);

void func_800408F4(void)
{
    D_80068FA0.field_234 = 1;
}

INCLUDE_ASM("main/nonmatchings/2F244", func_80040904);

void func_800409B0(GStruct0* arg0)
{
    func_8002D0A4(arg0);
}

INCLUDE_ASM("main/nonmatchings/2F244", func_800409D0);

INCLUDE_ASM("main/nonmatchings/2F244", func_800410F0);

INCLUDE_ASM("main/nonmatchings/2F244", func_80041700);

INCLUDE_ASM("main/nonmatchings/2F244", func_800418C0);

void func_80041B4C(GStruct27* arg0)
{
    if (arg0->field_18 != NULL) {
        Mem_Free2(arg0->field_18, 1);
        arg0->field_18 = NULL;
    }
}

INCLUDE_ASM("main/nonmatchings/2F244", func_80041B88);

INCLUDE_ASM("main/nonmatchings/2F244", func_80041BFC);

INCLUDE_ASM("main/nonmatchings/2F244", func_80041C50);

INCLUDE_ASM("main/nonmatchings/2F244", func_80041D3C);

INCLUDE_ASM("main/nonmatchings/2F244", func_80041D84);

INCLUDE_ASM("main/nonmatchings/2F244", func_80041DF4);

void func_80041E4C(void)
{
    GStruct27* node;

    node = D_800711B8;
    ResetGraph(1);
    F179D4_ClearOTag(0);
    F179D4_ClearOTag(1);
    while (node != NULL) {
        if (node->field_18 != NULL) {
            node->field_18 = NULL;
        }
        node = node->next;
    }
}

void func_80041EB4(void)
{
    GStruct27* node;
    void* mem;

    node = D_800711B8;
    Mem_InitAux();
    func_8001BB7C();
    while (node != NULL) {
        if (node->field_18 == NULL) {
            if (!(node->field_C & 4)) {
                mem = Mem_Calloc(node->field_10->field_4 * 2, 1);
                if (mem != NULL) {
                    node->field_18 = mem;
                    node->field_14 = 0;
                    func_800410F0(node);
                    func_800410F0(node);
                }
            }
        }
        node = node->next;
    }
}

INCLUDE_ASM("main/nonmatchings/2F244", func_80041F58);

INCLUDE_ASM("main/nonmatchings/2F244", func_80041FF8);

INCLUDE_ASM("main/nonmatchings/2F244", func_80042058);
