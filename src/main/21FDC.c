#include "common.h"

#include <psyq/libmcrd.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/21FDC", func_800317DC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800319E4);

void func_80031B1C(GStruct0* arg0, GStruct21* arg1)
{
    s32        ret;
    u32        status;
    s32        idx;
    GStruct37* obj;
    GStruct62* entry;
    GStruct62* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        status         = MemCardCreateFile(arg1->field_C, D_80060DD8, 1);
        arg1->field_14 = status;
        switch (status) {
            case 0:
                arg0->field_30 = 0xA;
                break;
            case 1:
                arg0->field_30 = 0x14;
                break;
            case 4:
                arg0->field_30 = 0x15;
                break;
            case 7:
                arg0->field_30 = 0x19;
                break;
            case 2:
            case 3:
            case 5:
            case 6:
            default:
                arg0->field_30 = 0x2A;
                break;
        }
    }

    obj           = arg0->field_20;
    idx           = arg1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

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

u16* func_8003380C(s8* arg0, u16* arg1)
{
    u16* lower;
    u16* upper;
    u16* symbol;
    s32  ch;
    s32  idx;
    u8   ch_u;

    ch_u = *arg0;
    if (*arg0 != 0) {
        lower  = D_80060E80;
        upper  = D_80060E48;
        symbol = D_80060EB8;
        do {
            ch = (s8)ch_u;
            if (ch >= 0x61) {
                idx  = (ch - 0x61) * 2;
                idx += (s32)lower;
                goto store;
            }
            if (ch >= 0x41) {
                idx  = (ch - 0x41) * 2;
                idx += (s32)upper;
                goto store;
            }
            if (ch >= 0x20) {
                idx  = (ch - 0x20) * 2;
                idx += (s32)symbol;
            store:
                *arg1 = *(u16*)idx;
            }
            arg0++;
            ch_u = *arg0;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

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

void func_80033944(void)
{
    s16 sum;
    u8* ptr;
    s32 limit;
    s32 i;
    s16 tmp;

    sum                 = 0;
    ptr                 = (u8*)&D_80072168;
    ptr                += 4;
    limit               = 0x38;
    i                   = 0;
    D_80072168.field_1C = 0;
    D_80072168.field_1E = 0xFFFF;
    do {
        i   += 1;
        tmp  = (s8)*ptr;
        sum  = sum + tmp;
        ptr += 1;
    } while (i < limit);
    D_80072168.field_1C = sum;
    D_80072168.field_1E = ~sum;
    func_800339C4(&D_80072168);
}

s32 func_800339C4(GStruct23* arg0)
{
    register s16 sum asm("v1");
    volatile u8* ptr;
    s32          limit;
    s32          i;
    s32          tmp;

    if ((u32)(arg0->field_12 - 1) >= 0x10U) {
        return 0;
    }
    sum   = 0;
    ptr   = &arg0->field_4;
    limit = 0x38;
    i     = 0;
    do {
        i   += 1;
        tmp  = (s8)*ptr;
        sum  = sum + tmp;
        ptr += 1;
    } while (i < limit);
    return ((u16)arg0->field_1C ^ (sum & 0xFFFF)) == 0;
}

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

s32 func_80033C40(void)
{
    GStruct53*   base;
    u8*          src;
    u8*          dest;
    s32          size;
    register s32 flags asm("a3");
    u32          i;
    register u32 j asm("a0");
    s32          idx;

    flags = 0;
    i     = 0;
    base  = D_800610FC;
    do {
        idx  = 8 - i;
        src  = (u8*)base[idx].field_0;
        size = base[idx].field_4;
        j    = 0;
        dest = src + size;
        if (size != 0) {
            do {
                if (*src != *dest) {
                    flags |= 1;
                }
                j    += 1;
                src  += 1;
                dest += 1;
            } while (j < (u32)size);
        }
        i     += 1;
        flags *= 2;
    } while (i < 8U);
    return flags | 0x103;
}

void func_80033CC0(void)
{
    GStruct47*   temp;
    GStruct53*   p;
    GStruct53*   base;
    s16          sum;
    s32          inv;
    u32          count;
    u32          i;
    register u32 j asm("a0");
    u8*          ptr;

    i    = 1;
    inv  = 0xFFFF;
    base = D_800610FC;
    p    = base + 1;
    do {
        sum   = 0;
        j     = 0;
        temp  = p->field_0;
        count = p->field_4;
        ptr   = temp->field_4;
        count = count - 4;
        if (count != 0) {
            do {
                j   += 1;
                sum += (s8)*ptr;
                ptr += 1;
            } while (j < count);
        }
        p            += 1;
        i            += 1;
        temp->field_2 = inv - sum;
        temp->field_0 = sum;
    } while (i < 9U);
}

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

s32 func_80033DD4(void)
{
    GStruct47*   temp;
    GStruct53*   p;
    GStruct53*   base;
    s16          sum;
    u32          count;
    u32          i;
    register u32 j asm("a0");
    u8*          ptr;
    s32          flag;

    flag = 1;
    i    = 1;
    base = D_800610FC;
    p    = base + 1;
    do {
        sum   = 0;
        j     = 0;
        temp  = p->field_0;
        count = p->field_4;
        ptr   = temp->field_4;
        count = count - 4;
        if (count != 0) {
            do {
                j   += 1;
                sum += (s8)*ptr;
                ptr += 1;
            } while (j < count);
        }
        if ((u16)temp->field_0 != (sum & 0xFFFF)) {
            flag = 0;
        }
        i += 1;
        p += 1;
    } while (i < 9U);
    return flag;
}

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

void func_80033EB0(GStruct0* arg0, s32 arg1)
{
    s32        ret;
    GStruct37* obj;
    GStruct62* entry;
    GStruct62* base;

    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[arg1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

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

void func_80033FB8(s32 arg0, GStruct21* arg1)
{
    s16          sum;
    s32          count;
    u8*          src;
    s16*         dst;
    register s32 i asm("v1");

    sum   = 0;
    count = 0x200;
    if (arg0 == 0) {
        src = D_80060EFC;
        dst = &D_80072AA4;
    } else {
        src = (u8*)arg1->field_18;
        dst = (s16*)&arg1->field_A1C;
    }

    i      = 0;
    dst[0] = sum;
    dst[1] = ~sum;
    if (count != 0) {
        do {
            i   += 1;
            sum += (s8)*src;
            src += 1;
        } while (i < count);
    }
    dst[0] = sum;
    dst[1] = ~sum;
}

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

void func_800346AC(GStruct0* arg0, GStruct21* arg1)
{
    s32        ret;
    GStruct37* obj;
    GStruct62* entry;
    GStruct62* base;

    arg1->field_4 = 0;
    arg0->field_30++;
    arg1->field_8 = 4;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[4];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003477C);

void func_80034894(GStruct0* arg0, GStruct21* arg1)
{
    s32 ret;

    arg1->field_8 = 7;
    ret           = func_8003062C(arg0, 7, arg1->field_0);
    switch (ret) {
        case 0:
            break;
        case 1:
            arg0->field_2a = 0xC;
            arg0->field_30 = 0x27;
            break;
        case -1:
            arg0->field_30 = 0x13;
            break;
    }
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

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

void func_80034C54(GStruct0* arg0, GStruct21* arg1)
{
    s32        ret;
    s32        syncResult;
    GStruct0*  child;
    GStruct37* obj;
    GStruct37* flag;

    arg1->field_8 = 9;
    ret           = func_800307AC(arg0, 9, arg1->field_0);
    switch (ret) {
        case 0:
            break;
        case 1:
            arg0->field_30 = 0x16;
            break;
        case -1:
            arg0->field_2a = 0xC;
            arg0->field_30 = 0x29;
            break;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_14 != 0) {
                child          = arg0->field_c;
                arg0->field_30 = 2;
                if (child != NULL) {
                    obj          = child->field_20;
                    flag         = arg0->field_20;
                    obj->field_0 = 0;
                    func_80048838(obj, obj->field_28);
                    flag->field_0 = syncResult;
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

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

void func_80034F2C(GStruct0* arg0, GStruct21* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, arg1->field_8, 0) != 0) {
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
        *ptr0          = 0;
        *ptr1          = 0;
        arg0->field_30 = 0x13;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034FB4);

void func_800350B0(GStruct0* arg0, GStruct21* arg1)
{
    s32        ret;
    GStruct37* obj;
    GStruct62* entry;
    GStruct62* base;
    s32        idx;

    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = 0x1F;
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035180);

void func_8003527C(GStruct0* arg0, GStruct21* arg1)
{
    s32        ret;
    GStruct37* obj;
    GStruct62* entry;
    GStruct62* base;

    arg1->field_4 = 4;
    arg1->field_8 = 1;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[1];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_80035358(GStruct0* arg0, GStruct21* arg1)
{
    register GStruct21* a1 asm("s1");
    register GStruct0*  a0 asm("s2");
    register s32        openIdx asm("s0");
    register GStruct37* obj asm("s0");
    register s32        modeIdx asm("s1");
    s32                 ret;
    s32                 openResult;
    GStruct62*          entry;
    GStruct62*          base;

    a1      = arg1;
    openIdx = a1->field_A14;
    MemCardClose();
    a0           = arg0;
    openResult   = MemCardOpen(a1->field_C, a1->field_30[openIdx], 1);
    a1->field_14 = openResult;
    if (openResult == 0) {
        a1->field_4  = 0;
        a0->field_30 = a0->field_30 + 1;
    } else {
        a0->field_30 = 0x18;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035464);

void func_80035574(GStruct0* arg0, GStruct21* arg1)
{
    register GStruct21* a1 asm("s1");
    register GStruct0*  a0 asm("s0");
    register GStruct37* obj asm("s0");
    register s32        modeIdx asm("s1");
    register s32        ret asm("s3");
    s32                 temp_v0;
    GStruct62*          entry;
    GStruct62*          base;

    a1 = arg1;
    a0 = arg0;
    if (a1->field_14 == 0) {
        MemCardClose();
        temp_v0       = a1->field_A14 + 1;
        a1->field_A14 = temp_v0;
        if (temp_v0 < a1->field_288) {
            a0->field_30 = 0x22;
        } else {
            a0->field_30 = a0->field_30 + 1;
        }
    } else {
        a0->field_30 = 0x18;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035684);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035764);

void func_80035844(GStruct0* arg0, GStruct21* arg1)
{
    s32        ret;
    GStruct37* obj;
    GStruct62* entry;
    GStruct62* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 <= 0) {
        arg0->field_30 = 0xF;
    }
    arg1->field_8 = 4;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[4];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

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

void func_80035C2C(GStruct0* arg0, GStruct21* arg1)
{
    s32        ret;
    GStruct37* obj;
    GStruct62* entry;
    GStruct62* base;
    s32        idx;

    MemCardClose();
    idx           = arg1->field_8;
    obj           = arg0->field_20;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[idx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->field_30 = 4;
    if (arg0->field_20 != NULL) {
        ((GStruct37*)arg0->field_20)->field_2E = -1;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035D14);

void func_80035E18(GStruct0* arg0)
{
    if (arg0->field_2a != 0) {
        func_8002CCB8(arg0);
    }
}

void func_80035E48(GStruct0* arg0, GStruct21* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0xF;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0xF, 0) != 0) {
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
        *ptr0          = 0;
        *ptr1          = 0;
        arg0->field_30 = 3;
    }
}

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

void func_800365B0(GStruct0* arg0, GStruct21* arg1)
{
    register GStruct21* a1 asm("s1");
    register GStruct0*  a0 asm("s2");
    register s32        openIdx asm("s0");
    register GStruct37* obj asm("s0");
    register s32        modeIdx asm("s1");
    s32                 ret;
    s32                 openResult;
    GStruct62*          entry;
    GStruct62*          base;

    a1      = arg1;
    openIdx = a1->field_A14;
    MemCardClose();
    a0           = arg0;
    openResult   = MemCardOpen(a1->field_C, a1->field_30[openIdx], 1);
    a1->field_14 = openResult;
    if (openResult == 0) {
        a1->field_4  = 0;
        a0->field_30 = a0->field_30 + 1;
    } else {
        a0->field_30 = 6;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_800366BC);

void func_800367CC(GStruct0* arg0, GStruct21* arg1)
{
    register GStruct21* a1 asm("s1");
    register GStruct0*  a0 asm("s0");
    register GStruct37* obj asm("s0");
    register s32        modeIdx asm("s1");
    register s32        ret asm("s3");
    s32                 temp_v0;
    GStruct62*          entry;
    GStruct62*          base;

    a1 = arg1;
    a0 = arg0;
    if (a1->field_14 == 0) {
        MemCardClose();
        temp_v0       = a1->field_A14 + 1;
        a1->field_A14 = temp_v0;
        if (temp_v0 < a1->field_288) {
            a0->field_30 = 0x14;
        } else {
            a0->field_30 = a0->field_30 + 1;
        }
    } else {
        a0->field_30 = 0x6;
    }
    obj           = a0->field_20;
    modeIdx       = a1->field_8;
    ret           = func_80048E10(obj, 1);
    obj->field_2E = 0;
    func_80048E38(obj, D_8001398C);
    base  = D_80060D08;
    entry = &base[modeIdx];
    func_8002FDCC(obj, obj->field_1C + 2, -2, entry->field_0, ret, 1, 0);
    func_8002FDCC(obj, obj->field_1C + 2, 0xF, entry->field_4, ret, 1, 0);
}

void func_800368DC(GStruct0* arg0, GStruct21* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0x17;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0x17, 0) != 0) {
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
        *ptr0          = 0;
        *ptr1          = 0;
        arg0->field_30 = 3;
    }
}

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

void func_80036B2C(GStruct60* arg0, GStruct37* arg1)
{
    s16 var_v0;
    s32 temp;
    s8  temp2;

    temp2 = arg0->field_8;
    temp  = arg1->field_28->field_34;
    func_800330D8(arg1, temp, temp2, 0, arg0->field_1A + 7);
    if (arg0->field_C == 1) {
        if (func_8002C868(0, 1, D_8005ED70) != 0) {
            func_8005414C(0x16, 0, 0);
            arg1->field_2E = 6;
            var_v0         = (s8)(u8)arg0->field_8;
            goto block_5;
        }
        if (func_8002C868(0, 1, D_8005ED74) != 0) {
            func_8005414C(0x3B, 0, 0);
            arg1->field_2E = 6;
            var_v0         = -1;
        block_5:
            arg1->field_2C = var_v0;
        }
    }
}

void func_80036C04(GStruct0* arg0)
{
    GStruct20* obj;
    GStruct46* menu;
    GStruct64* ctx;
    s32        temp;

    obj  = arg0->field_20;
    ctx  = (GStruct64*)arg0->field_34;
    menu = &D_80061194;
    func_80047F40(obj, D_80013B64);
    if (arg0->field_30 == 0) {
        func_800489A0(menu, obj);
        menu->field_A  = 1;
        menu->field_10 = ctx->field_290;
        temp           = (u8)menu->field_10 - menu->field_5 + 1;
        menu->field_9  = temp;
        if ((s8)temp < 0) {
            menu->field_9 = 0;
        }
        func_8004917C(menu, 1);
        arg0->field_30 += 1;
    } else {
        func_80048C10(menu, obj);
        if (obj->field_0 == 1) {
            func_80048D58(obj, obj->field_1c + 2, 0);
        }
    }
}

void func_80036CF0(GStruct0* arg0)
{
    void*      obj;
    s32        data;
    GStruct46* menu;
    s32        val;

    obj = arg0->field_20;
    if (arg0->field_30 == 0) {
        arg0->field_2a  = (u16)arg0->field_34;
        data            = arg0->field_8->field_34;
        arg0->field_30 += 1;
        arg0->field_34  = data;
    }
    data = arg0->field_34;
    func_80048E38(obj, D_80013BB4);
    if (arg0->field_2a == 1) {
        menu = &D_80061194;
    } else {
        menu = &D_8006116C;
    }
    val = menu->field_10;
    func_800330D8(obj, data, val, 0, 0);
}

void func_80036D98(GStruct60* arg0, GStruct37* arg1)
{
    s32 temp;

    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, D_80060A54, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (func_8002C868(0, 1, D_8005ED70) != 0) {
            func_8005414C(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        } else if (func_8002C868(0, 1, D_8005ED74) != 0) {
            func_8005414C(0x15, 0, 0);
            arg0->field_B  = temp;
            arg0->field_22 = 0x41;
        }
    }
}

void func_80036E78(GStruct60* arg0, GStruct37* arg1)
{
    s32 temp;

    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, D_80060A64, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (func_8002C868(0, 1, D_8005ED70) != 0) {
            func_8005414C(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        }
    }
}

void func_80036F18(GStruct60* arg0, GStruct37* arg1)
{
    s32 temp;

    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, D_80060A5C, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (func_8002C868(0, 1, D_8005ED70) != 0) {
            func_8005414C(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        }
    }
}

void func_80036FB8(GStruct60* arg0, GStruct37* arg1)
{
    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, D_80060A58, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (func_8002C868(0, 1, D_8005ED70 | D_8005ED74) != 0) {
            func_8005414C(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = -1;
        }
    }
}

void func_80037068(GStruct0* arg0)
{
    GStruct30* obj;
    GStruct46* menu;
    s32        mode;

    mode = arg0->field_34;
    obj  = arg0->field_20;
    if (mode == 2) {
        goto block_2;
    }
    if (mode >= 3) {
        goto block_default;
    }
    if (mode != 1) {
        goto block_default;
    }
    menu = &D_80061284;
    goto block_done;
block_2:
    menu = &D_800612AC;
    goto block_done;
block_default:
    menu = &D_8006125C;
block_done:
    if (arg0->field_30 == 0) {
        func_80046830(menu, obj);
        obj->field_C.y -= obj->field_C.h / 2;
        if (arg0->field_34 != 3) {
            menu->field_10 = 0;
        } else {
            menu->field_10 = 1;
        }
        menu->field_9 = 0;
        func_8004917C(menu, 1);
        arg0->field_30 += 1;
    } else {
        func_80048C10(menu, obj);
    }
}
