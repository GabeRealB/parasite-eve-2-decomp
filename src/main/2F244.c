#include "common.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"
#include "psyq/libpress.h"

INCLUDE_ASM("main/nonmatchings/2F244", func_8003EA44);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003EC44);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003EE68);

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F034);

void func_8003F450(s32 arg0)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;
    u_long*       ot;
    u32           mode;

    temp            = &Display_State;
    saved           = D_800710A0;
    buf             = temp->field_114 ^ 1;
    temp->field_114 = buf;
    D_800710A0      = D5F414_OrderingTables + buf * C5F414_OTAG_ENTRIES;
    ClearOTagR(D_800710A0, C5F414_OTAG_ENTRIES);
    ot              = D_800710A0;
    *ot             = C5F414_OTAG_END_PRIM;
    D_800710A0      = ot + 0x20;
    temp->field_103 = 0;
    temp->field_1f  = *(u8*)&temp->field_118;
    mode            = D_80062698->field_11;
    switch (mode) {
        case 3:
        case 0x20:
            Task_ExecDefaultList(&Task_DefaultList);
            break;
        case 2:
            func_800AC688();
            func_8009850C(&D_80070EE8[temp->field_114]);
            break;
        case 1:
            Task_ExecListFiltered(&Task_DefaultList, 0x62);
            func_800AC688();
            func_80097AC0(&D_80070EE8[temp->field_114]);
            break;
    }
    D_800710A0 = saved;
}

INCLUDE_ASM("main/nonmatchings/2F244", func_8003F5A4);

void func_8003F690(void)
{
    DisplayState* temp;

    func_8003E6E4();
    temp            = &Display_State;
    temp->field_1e  = 1;
    temp->field_103 = 2;
    temp->field_118 = temp->field_114 ^ 1;
    Task_InitList(&D_8007A110);
    Task_SpawnFromTable(&D_8006269C, 0, 0, 0);
}

s32 func_8003F6F8(void)
{
    D_80062698->field_1c |= 0x80000000;
    return 0;
}

s32 func_8003F71C(s32 arg0, s32 arg1)
{
    GStruct17* temp;
    s32        mask;

    mask = 0x40000000;
    if (!(D_80062698->field_1c & mask)) {
        Pad_SetCooldown(0);
        temp            = D_80062698;
        temp->field_20  = arg0;
        temp->field_24  = 0;
        temp->field_28  = 0;
        temp->field_11  = arg1;
        temp->field_1c |= mask;
    }
    return (u8)D4F564_8005ED64->field_4;
}

s32 func_8003F7A8(s32 arg0)
{
    GStruct17* temp;
    s32        mask;
    s32        ret;

    mask = 0x40000000;
    ret  = -1;
    if (!(D_80062698->field_1c & mask)) {
        Pad_SetCooldown(0);
        temp                  = D_80062698;
        temp->field_20        = arg0;
        temp->field_24        = 0;
        temp->field_28        = 0;
        temp->field_11        = 7;
        temp->field_1c       |= mask;
        ret                   = (u8)D4F564_8005ED64->field_4;
        D_80062698->field_1c |= 0x80000000;
    }
    return ret;
}

s32 func_8003F848(void)
{
    D_80062698->field_1c |= 0x20000000;
    return 0;
}

s32 func_8003F86C(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0) {
        D_80062698->field_18 = 0x20;
    } else {
        D_80062698->field_18 = arg2;
    }
    if (arg0 != 0) {
        D_80062698->field_18 = -D_80062698->field_18;
    }
    D_80062698->field_19 = 0;
    if (arg1 != 0) {
        D_80062698->field_19 |= 1;
    }
    if (arg3 != 0) {
        D_80062698->field_19 |= 2;
    }
    return 0;
}

s32 func_8003F900(void)
{
    GStruct17* temp;
    u8         temp_a0;

    temp    = D_80062698;
    temp_a0 = temp->field_17;
    if (temp_a0 == 0) {
        return 0;
    }
    if (temp_a0 >= temp->field_1a) {
        return 1;
    }
    return -1;
}

s32 func_8003F944(void)
{
    return (D_80062698->field_1c & 0x48000000) != 0;
}

void func_8003F964(void)
{
    if (D_80062698->field_13 == 0) {
        func_8003E904();
        D_80062698->field_13 = 1;
    }
}

void func_8003F9AC(void)
{
    if (D_80062698->field_14 == 0) {
        func_8003E9A4();
        D_80062698->field_14 = 1;
    }
}

void func_8003F9F4(void)
{
    if (D_80062698->field_14 == 1) {
        func_8003E9C4();
        D_80062698->field_14 = 0;
    }
}

void func_8003FA3C(u8 arg0)
{
    D_80062698->field_1a = arg0;
}

void func_8003FA4C(s32 arg0)
{
    switch (arg0) {
        case 0:
            Display_State.field_103 = 1;
            Display_State.field_100 = 0;
            func_8003DE14(0, 0, 0);
            return;
        case 1:
            Display_State.field_103 = (u8)arg0;
            Display_State.field_100 = 3;
            func_8003DE14(-1, 0, 0);
            return;
        case 2:
            Display_State.field_103 = 1;
            Display_State.field_100 = 2;
            func_8003DE14(-1, 0, 0);
            return;
        case 3:
            Display_State.field_103 = 2;
            return;
    }
}

s32 func_8003FB20(void)
{
    GStruct17* temp;
    u32        flags;
    s32        val;

    temp  = D_80062698;
    flags = temp->field_1c;
    if (!(flags & 0x40000000)) {
        temp->field_1c = flags | 0x50000000;
        val            = (u8)D4F564_8005ED64->field_4;
        temp->field_24 = 0;
        temp->field_28 = 0;
        temp->field_11 = 3;
        temp->field_20 = val;
    }
    return 0;
}

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

void func_8003FC8C(Task* arg0)
{
    Pad_SetCooldown(0);
    if (func_8001CDF0() != 0) {
        arg0->field_30 += 1;
    } else {
        Pad_States[0].field_A = 1;
        func_8003EE68();
        arg0->field_30 += 2;
    }
}

void func_8003FCF8(Task* arg0)
{
    Pad_SetCooldown(0);
    if (func_8001D82C() != 0) {
        Pad_States[0].field_A = 1;
        func_8003EE68();
        arg0->field_30 += 1;
    }
}

void func_8003FD58(Task* arg0)
{
    u32 temp_v1;

    Display_State.field_103 = 2;
    temp_v1                 = D_80062698->field_C;
    if (temp_v1 < 5U) {
        if (temp_v1 < 3U) {
            if (temp_v1 != 1) {
                goto block_3;
            }
        }
    } else {
    block_3:
        func_800144F8(D4F564_8005ED64->field_7, D4F564_8005ED64->field_6);
        func_80041EB4();
        func_800ACAA8();
    }
    func_80042364(0, 0, 4);
    arg0->field_30 = (s32)(arg0->field_30 + 1);
    func_8003FE00(arg0);
}

void func_8003FE00(Task* arg0)
{
    if (func_8001D82C() != 0) {
        arg0->field_30 += 1;
    }
}

void func_8003FE40(Task* arg0)
{
    if (func_8001D0E8() != 0) {
        Display_State.field_1e  = 0;
        Display_State.field_10d = 0;
        Display_State.field_100 = 1;
        func_8003DE14(-1, 0, 0);
        Task_CallExit(arg0);
    }
}

void func_8003FE9C(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_80013E98;
    sp.funcs[arg0->field_30](arg0);
    func_8003EA44();
}

INCLUDE_ASM("main/nonmatchings/2F244", func_8003FF14);

INCLUDE_ASM("main/nonmatchings/2F244", func_8004017C);

INCLUDE_ASM("main/nonmatchings/2F244", func_800405E0);

void func_80040820(void)
{
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    if (p->field_214 == 0) {
        if (p->field_234 != 0) {
            DecDCTvlcBuild((u16*)((u8*)D4CB64_ImgBuffers + 0x8800));
            p->field_234 = 0;
        }
        if ((p->field_200 != 0) && (p->field_234 == 0)) {
            func_800405E0();
        }
    } else if (p->field_200 != 0) {
        func_8004017C();
    }
}

void func_800408C0(void* arg0)
{
    CdCmdQueue* p;

    D_8007A35C   = 0;
    p            = &CdCmd_Queue;
    p->field_200 = 1;
    p->field_1FE = 0;
    p->field_202 = 0;
    D_8007A360   = arg0;
    D_8007A358   = 0;
}

void func_800408F4(void)
{
    CdCmd_Queue.field_234 = 1;
}

void func_80040904(void)
{
    s32         temp;
    CdCmdQueue* p;

    temp = 0x140 / (D_8007A35E * 16);
    p    = &CdCmd_Queue;
    if (D_8007A35C == temp - 1) {
        p->field_1EC = 0;
        DecDCToutCallback(0);
    } else {
        D_8007A35C = D_8007A35C + 1;
        DecDCTout(D4CB64_ImgBuffers->buffers[D_8007A35C], 0x780);
    }
}

void func_800409B0(Task* arg0)
{
    Task_CallExit(arg0);
}

extern u8 D_800105AC;
extern u8 D_800105F4;
extern u8 D_80010718;
extern u8 D_80010A90;
extern u8 D_80010BF4;
extern u8 D_80010E1C;
extern u8 D_80010EF4;
extern u8 D_80010F08;
extern u8 D_8001108C;
extern u8 D_800110A0;
extern u8 D_800112DC;
extern u8 D_800113B4;
extern u8 D_800113E8;
extern u8 D_800114FC;
extern u8 D_80011530;
extern u8 D_80011678;
extern u8 D_800117BC;
extern u8 D_80011994;
extern u8 D_800119A8;
extern u8 D_80011AF4;
extern u8 D_80011B08;
extern u8 D_8001237C;
extern u8 D_80012520;
extern u8 D_80099994;
extern u8 D_80099B94;
extern u8 D_80099D40;
extern u8 D_80099FF4;
extern u8 D_8009A348;
extern u8 D_8009A57C;
extern u8 D_8009AF90;
extern u8 D_8009B2F4;
extern u8 D_8009B500;
extern u8 D_8009BD00;
extern u8 D_8009C024;
extern u8 D_8009C414;
extern u8 D_8009CED0;
extern u8 D_8009D0DC;
extern u8 D_8009D388;
extern u8 D_8009D518;
extern u8 D_8009D718;
extern u8 D_8009D900;
extern u8 D_8009DB00;
extern u8 D_8009DCB8;
extern u8 D_8009DE48;
extern u8 D_8009E048;
extern u8 D_8009E274;
extern u8 D_8009E4A0;
extern u8 D_8009E770;
extern u8 D_8009EAA4;
extern u8 D_80136224;
extern u8 D_80136500;
extern u8 D_8013685C;
extern u8 D_80136C00;
extern u8 D_8013700C;
extern u8 D_80137300;
extern u8 D_801375F8;
extern u8 D_801379B4;
extern u8 D_80138004;
extern u8 D_801386EC;

void func_800409D0(GStruct33* arg0)
{
    u32*         stream;
    u32          id;
    u32          dims;
    void*        handler;
    s32          flag;
    register u32 tmp asm("v0");

    stream = arg0->field_20;
    if (arg0->field_0 == 0) {
        tmp  = *(u32*)&D4F564_8005ED64->field_4;
        tmp  = (tmp & 0xFFFF0000) ^ 0x02100000;
        flag = tmp < 1;
        goto read_id;

        for (;;) {
            switch (id) {
                case 0x20:
                case 0x22:
                    handler = &D_80010A90;
                    break;
                case 0x60:
                case 0x62:
                    handler = &D_80010BF4;
                    break;
                case 0xC0:
                    handler = &D_80010E1C;
                    break;
                case 0xC4:
                    handler = &D_8009EAA4;
                    break;
                case 5:
                    handler = &D_80099B94;
                    break;
                case 0x45:
                    handler = &D_80099994;
                    break;
                case 0x21:
                case 0x121:
                    handler = &D_800105F4;
                    break;
                case 0x61:
                case 0x161:
                    handler = &D_80010718;
                    break;
                case 0x38:
                    handler = &D_80010F08;
                    break;
                case 0x8038:
                    handler = &D_80136224;
                    break;
                case 0x10038:
                    handler = &D_8013700C;
                    break;
                case 0x20038:
                    handler = &D_801379B4;
                    break;
                case 0x3A:
                    handler = &D_80010EF4;
                    break;
                case 0x1003A:
                    handler = &D_80137300;
                    break;
                case 0x78:
                    handler = &D_800110A0;
                    break;
                case 0x8078:
                    handler = &D_8013685C;
                    break;
                case 0x10078:
                    handler = &D_801375F8;
                    break;
                case 0x20078:
                    handler = &D_80138004;
                    break;
                case 0x7A:
                    handler = &D_8001108C;
                    break;
                case 0xC8:
                    handler = &D_800112DC;
                    break;
                case 0x40C8:
                    handler = &D_8009AF90;
                    if (flag != 0) {
                        handler = &D_8009B2F4;
                    }
                    break;
                case 0x200C8:
                    handler = &D_801386EC;
                    break;
                case 0x31:
                case 0x39:
                case 0x131:
                    handler = &D_800113E8;
                    break;
                case 0x8039:
                    handler = &D_80136500;
                    break;
                case 0x3B:
                    handler = &D_800113B4;
                    break;
                case 0x71:
                case 0x79:
                case 0x171:
                    handler = &D_80011530;
                    break;
                case 0x8079:
                    handler = &D_80136C00;
                    break;
                case 0x7B:
                    handler = &D_800114FC;
                    break;
                case 0x4039:
                    handler = &D_80099D40;
                    if (flag != 0) {
                        handler = &D_8009A348;
                    }
                    break;
                case 0x4079:
                    handler = &D_80099FF4;
                    if (flag != 0) {
                        handler = &D_8009A57C;
                    }
                    break;
                case 0:
                    handler = &D_80011678;
                    break;
                case 0x40:
                    handler = &D_800117BC;
                    break;
                case 0x18:
                    handler = &D_800119A8;
                    break;
                case 0x1A:
                    handler = &D_80011994;
                    break;
                case 0x58:
                    handler = &D_80011AF4;
                    break;
                case 0x5A:
                    handler = &D_80011B08;
                    break;
                case 0x4078:
                    handler = &D_8009C414;
                    if (flag != 0) {
                        handler = &D_8009C024;
                    }
                    break;
                case 0x4038:
                    handler = &D_8009B500;
                    if (flag != 0) {
                        handler = &D_8009BD00;
                    }
                    break;
                case 0x120:
                    handler = &D_8009E048;
                    break;
                case 0x122:
                    handler = &D_8009E274;
                    break;
                case 0x160:
                    handler = &D_8009E4A0;
                    break;
                case 0x162:
                    handler = &D_8009E770;
                    break;
                case 0x1C:
                    handler = &D_8009D388;
                    break;
                case 0x1E:
                    handler = &D_8009DCB8;
                    break;
                case 0x5C:
                    handler = &D_8009D518;
                    break;
                case 0x5E:
                    handler = &D_8009DE48;
                    break;
                case 0x30:
                    handler = &D_8009CED0;
                    break;
                case 0x130:
                    handler = &D_8001237C;
                    break;
                case 0x70:
                    handler = &D_8009D0DC;
                    break;
                case 0x170:
                    handler = &D_80012520;
                    break;
                case 0x156:
                    handler = &D_8009D718;
                    break;
                case 4:
                    handler = &D_8009DB00;
                    break;
                case 0x44:
                    handler = &D_8009D900;
                    break;
                default:
                    handler = &D_800105AC;
                    break;
            }

            stream++;
            *stream = (u32)handler;
            stream++;
            dims = *stream;
            stream++;
            id      = dims & 0xFFFF;
            stream += (dims >> 16) * id;
            id      = *stream;

            while (1) {
                if (id != -2U) {
                    break;
                }
                stream++;
            read_id:
                id = *stream;
                if (id == -1U) {
                    goto done;
                }
            }
        }
    done:
        arg0->field_0 = 1;
    }
}

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

s32 func_80041B88(GStruct27* arg0)
{
    s32   result;
    void* mem;

    result = 0;
    if (arg0->field_18 == NULL) {
        mem            = Mem_Calloc(arg0->field_10->field_4 * 2, 1);
        arg0->field_18 = mem;
        if (mem != NULL) {
            arg0->field_14 = 0;
            func_800410F0(arg0);
            func_800410F0(arg0);
            result = 1;
        }
    }
    return result;
}

s32 func_80041BFC(void)
{
    GStruct27* node;
    s32        result;

    result = 0;
    node   = D_800711B8.next;
    while (node != NULL) {
        if (node->field_18 != NULL) {
            result += node->field_10->field_4 * 2;
        }
        node = node->next;
    }
    return result;
}

INCLUDE_ASM("main/nonmatchings/2F244", func_80041C50);

void func_80041D3C(GStruct27* arg0)
{
    GStruct27* node;

    node = D_800711B8.next;
    while (node != NULL) {
        node->field_C |= 0x80;
        node           = node->next;
    }
    arg0->field_30++;
}

void func_80041D84(GStruct27* arg0)
{
    GStruct27* node;

    node = D_800711B8.next;
    while (node != NULL) {
        if (node->field_18 != NULL) {
            Mem_Free2(node->field_18, 1);
            node->field_18 = NULL;
        }
        node = node->next;
    }
    arg0->field_30++;
}

void func_80041DF4(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80013EDC;
    sp.funcs[arg0->field_30](arg0);
}

void func_80041E4C(void)
{
    GStruct27* node;

    node = D_800711B8.next;
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
    void*      mem;

    node = D_800711B8.next;
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

void func_80041F58(Task* arg0)
{
    GStruct27* node;
    void*      mem;

    node = D_800711B8.next;
    while (node != NULL) {
        if (node->field_18 == NULL) {
            mem = Mem_Calloc(node->field_10->field_4 * 2, 1);
            if (mem != NULL) {
                node->field_18 = mem;
                node->field_14 = 0;
                node->field_C &= ~0x80;
                func_800410F0(node);
                func_800410F0(node);
            }
        }
        node = node->next;
    }
    Task_Kill(arg0);
}

void func_80041FF8(GStruct27* node)
{
    while (node != NULL) {
        if (node->field_C & 8) {
            if (node->field_18 != NULL) {
                func_800418C0(node);
            }
        }
        node = node->next;
    }
}

void func_80042058(GStruct27* node)
{
    while (node != NULL) {
        if (!(node->field_C & 0x80)) {
            if (node->field_18 != NULL) {
                func_800418C0(node);
            }
        }
        node = node->next;
    }
}
