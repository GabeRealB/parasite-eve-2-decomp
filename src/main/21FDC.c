#include "common.h"

#include <psyq/libmcrd.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/21FDC", func_800317DC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800319E4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80031B1C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80031C5C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80031DA4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80031F94);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800322B0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003245C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80032578);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800327A4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800328FC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80032AB0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80032D54);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80032F5C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800330D8);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003380C);

void func_800338A8(void)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    ptr1 = D_80060DD8;
    ptr0 = D_80060DF0;
    i    = 0;
    ch   = 0x5F;
    do {
        if (i >= 0xC) {
            *ptr0 = ch;
            *ptr1 = ch;
        }
        ptr1++;
        i++;
        ptr0++;
    } while (i < 0x14);
    *ptr0 = 0;
    *ptr1 = 0;
}

void func_800338F4(s32 arg0)
{
    u8* src;
    u8* dst;
    s32 i;

    if (arg0 == 0) {
        src = D_80060DD8;
        dst = D_80060DF0;
    } else {
        src = D_80060DF0;
        dst = D_80060DD8;
    }

    for (i = 0; i < 0x15; i++) {
        *dst++ = *src++;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033944);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800339C4);

void func_80033A28(GStruct47* arg0, s32 arg1)
{
    s16          sum;
    register u8* ptr asm("v1");
    u32          count;
    u32          i;

    sum   = 0;
    ptr   = arg0->field_4;
    count = arg1 - 4;
    i     = 0;
    if (count != 0) {
        do {
            i   += 1;
            sum += (s8)*ptr;
            ptr += 1;
        } while (i < count);
    }
    arg0->field_0 = sum;
    arg0->field_2 = ~sum;
}

void func_80033A70(void)
{
    GStruct23* p;

    p            = &D_80072168;
    p->field_21  = 0;
    p->field_1a8 = 0;
    p->field_1aa = 0;
    p->field_1ab = 0;
    p->field_1a9 = 0;
    p->field_25  = 0;
    func_800260B0(1);
    func_800429C8(0);
}

void func_80033AB8(void)
{
    u8(*a)[0x6C];
    u8(*b)[0xB0];
    u8(*c)[0x24];
    u8(*d)[0xE4];
    u8(*e)[0xA4];

    a = D_800733F0;
    Mem_Set(a, 0, 0x6C);
    b = D_800734C8;
    Mem_Set(b, 0, 0xB0);
    c = D_80073628;
    Mem_Set(c, 0, 0x24);
    d = D_80073670;
    Mem_Set(d, 0, 0xE4);
    e = D_80073838;
    Mem_Set(e, 0, 0xA4);
    Mem_Set(a + 1, 0xFF, 0x6C);
    Mem_Set(b + 1, 0xFF, 0xB0);
    Mem_Set(c + 1, 0xFF, 0x24);
    Mem_Set(d + 1, 0xFF, 0xE4);
    Mem_Set(e + 1, 0xFF, 0xA4);
}

// TODO
void func_80033BBC(void)
{
    MemCardInit(0); // 0 = No control routine
    MemCardStart();
    func_800303AC();
}

s32 func_80033BEC(GStruct47* arg0, s32 arg1)
{
    s16          sum;
    register u8* ptr asm("a2");
    u32          count;
    u32          i;

    sum   = 0;
    ptr   = arg0->field_4;
    count = arg1 - 4;
    i     = 0;
    if (count != 0) {
        do {
            i   += 1;
            sum += (s8)*ptr;
            ptr += 1;
        } while (i < count);
    }
    return ((u16)arg0->field_0 ^ (sum & 0xFFFF)) == 0;
}

void func_80033C38(void)
{
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033C40);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033CC0);

void func_80033D3C(void)
{
    GStruct47* temp;
    GStruct53* p;
    GStruct53* base;
    s16        next;
    s16        sum;
    u32        i;

    sum  = 0;
    i    = 1;
    base = D_800610FC;
    p    = base + 1;
    do {
        temp = p->field_0;
        p   += 1;
        i   += 1;
        next = sum + *(u8*)temp;
        sum  = next;
    } while (i < 9U);
    D_80072168.field_940 = next;
    D_80072168.field_942 = ~next;
}

s32 func_80033D88(void)
{
    s32        sum;
    u32        i;
    GStruct53* p;
    GStruct53* base;

    sum  = 0;
    i    = 1;
    base = D_800610FC;
    p    = base + 1;
    do {
        sum += *(u8*)p->field_0;
        p   += 1;
        i   += 1;
    } while (i < 9);
    return ((u16)D_80072AA8 ^ (sum & 0xFFFF)) == 0;
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033DD4);

void func_80033E58(void)
{
    u32        i;
    u32        j;
    GStruct53* p;
    GStruct53* base;
    u8*        src;
    s32        size;
    u8*        dest;

    i    = 1;
    base = D_800610FC;
    p    = base + 1;
    do {
        src  = (u8*)p->field_0;
        size = p->field_4;
        j    = 0;
        dest = src + size;
        while (j < (u32)size) {
            j    += 1;
            *dest = *src;
            src  += 1;
            dest += 1;
        }
        i += 1;
        p += 1;
    } while (i < 9);
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033EB0);

void func_80033F6C(GStruct0* arg0)
{
    GStruct0*  child;
    GStruct37* obj;
    GStruct37* flag;

    child = arg0->field_c;
    if (child != NULL) {
        obj          = child->field_20;
        flag         = arg0->field_20;
        obj->field_0 = 0;
        func_80048838(obj, obj->field_28);
        flag->field_0 = 1;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033FB8);

s32 func_80034028(GStruct23* arg0, GStruct21* arg1)
{
    if (arg0->field_5C2 != 0) {
        return 0;
    }
    if (arg0->field_23 != 0) {
        return 0;
    }
    return arg0->field_93C == arg1->field_A1C;
}

void func_80034070(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_0   = 0x10;
    arg1->field_4   = 0;
    arg1->field_18  = 0;
    arg1->field_C   = 0;
    arg1->field_A18 = 0x34;
    arg1->field_A20 = 0;
    arg0->field_30++;
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_800340A4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003415C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003429C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800343D0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800344B4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800345CC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800346AC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003477C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034894);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034938);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034A40);

void func_80034B38(GStruct0* arg0)
{
    if (arg0->field_2a != 0) {
        func_8002CCB8(arg0);
    }
}

void func_80034B68(GStruct0* arg0, GStruct21* arg1)
{
    s32        syncResult;
    GStruct0*  child;
    GStruct37* obj;
    GStruct37* flag;

    arg1->field_8 = 3;
    if (func_8003092C(arg0, 3, arg1->field_0) != 0) {
        arg0->field_30 = 0x13;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            if (arg1->field_14 != syncResult) {
                child = arg0->field_c;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = syncResult;
                }
                arg0->field_30 = 2;
            }
            return;
        case 0:
            return;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034C54);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034D50);

void func_80034E3C(GStruct0* arg0, GStruct21* arg1)
{
    s32        syncResult;
    s32        rslt;
    GStruct0*  child;
    GStruct37* obj;
    GStruct37* flag;

    arg1->field_8 = 0x13;
    if (func_8003092C(arg0, 0x13, arg1->field_0) != 0) {
        arg0->field_30 = 0x13;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            rslt = arg1->field_14;
            if (rslt == syncResult) {
                child = arg0->field_c;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = rslt;
                }
                arg0->field_30 = 0x14;
            }
            return;
        case 0:
            return;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034F2C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034FB4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800350B0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035180);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003527C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035358);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035464);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035574);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035684);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035764);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035844);

void func_8003591C(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_8 = 0xE;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0xE, 0) != 0) {
        arg0->field_30 = 0x13;
    }
}

void func_80035960(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_8 = 0xD;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0xD, 0) != 0) {
        arg0->field_30 = 0x13;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_800359A4);

void func_80035A94(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_0        = 0x10;
    arg1->field_8        = 0x8;
    arg1->field_A20      = 1;
    arg1->field_4        = 0;
    arg1->field_18       = 0;
    arg1->field_C        = 0;
    D_80070F68.field_101 = 0;
    arg0->field_30      += 1;
}

void func_80035AD4(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_24 = 9;
    arg1->field_28 = -1;
    arg0->field_30 = 7;
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035AF0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035C2C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035D14);

void func_80035E18(GStruct0* arg0)
{
    if (arg0->field_2a != 0) {
        func_8002CCB8(arg0);
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035E48);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035ED4);

void func_80035FD8(GStruct0* arg0, GStruct21* arg1)
{
    s32        syncResult;
    GStruct0*  child;
    GStruct37* obj;
    GStruct37* flag;

    arg1->field_8 = 3;
    if (func_8003092C(arg0, 3, arg1->field_0) != 0) {
        arg0->field_30 = 3;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            if (arg1->field_14 != syncResult) {
                child = arg0->field_c;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = syncResult;
                }
                arg0->field_30 = 7;
            }
            return;
        case 0:
            return;
    }
}

void func_800360C8(GStruct0* arg0, GStruct21* arg1)
{
    s32        syncResult;
    s32        rslt;
    GStruct0*  child;
    GStruct37* obj;
    GStruct37* flag;

    arg1->field_8 = 0xA;
    if (func_8003092C(arg0, 0xA, arg1->field_0) != 0) {
        arg0->field_30 = 3;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            rslt = arg1->field_14;
            if (rslt == syncResult) {
                child = arg0->field_c;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = rslt;
                }
                arg0->field_30 = 0xA;
            }
            return;
        case 0:
            return;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_800361C0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800362A4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800363AC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036488);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800365B0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800366BC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800367CC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800368DC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036968);

void func_80036A1C(void)
{
    char pad[0x10];
}

void func_80036A2C(void* arg0, GStruct20* arg1)
{
    func_80048C10(arg0, arg1);
    if (arg1->field_0 == 1) {
        func_80048D58(arg1, arg1->field_1c + 2, 0);
    }
}

void func_80036A70(GStruct0* arg0)
{
    GStruct20* obj;
    GStruct46* menu;

    obj  = arg0->field_20;
    menu = &D_8006116C;
    func_80047F40(obj, D_80013B64);
    if (arg0->field_30 == 0) {
        func_800489A0(menu, obj);
        menu->field_A  = 1;
        menu->field_10 = 0;
        menu->field_9  = 0;
        func_8004917C(menu, 1);
        arg0->field_30 += 1;
    } else {
        func_80048C10(menu, obj);
        if (obj->field_0 == 1) {
            func_80048D58(obj, obj->field_1c + 2, 0);
        }
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036B2C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036C04);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036CF0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036D98);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036E78);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036F18);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036FB8);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80037068);
