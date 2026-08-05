#include "common.h"

#include <psyq/rand.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/201E0", func_8002F9E0);

s32 func_8002FB84(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    u8                  sp10[0x40];
    GStruct38           sp50[2];
    u8*                 cur;
    s32                 temp;
    register UiObject*  obj asm("s3");
    register s32        x asm("s2");
    register s32        y asm("s1");
    register GStruct38* p asm("s0");
    register u8*        buf asm("s4");
    register s32        ret asm("s5");
    register s32        four asm("s6");
    register s32        a6 asm("s7");
    s32                 a5;

    a5   = arg5;
    a6   = arg6;
    obj  = arg0;
    x    = arg1;
    y    = arg2;
    p    = sp50;
    buf  = sp10;
    four = 4;
    cur  = arg3;

    do {
        ret = func_8002F9E0(&cur, sp10);
        if (obj != NULL) {
            if (obj->field_8 != 5) {
                sp50[0].field_0 = obj->field_20 + x;
                sp50[0].field_2 = (obj->field_22 + y) - 3;
                temp            = (s16)obj->field_14;
                sp50[0].field_8 = arg4;
                sp50[0].field_4 = temp + 1;
                p->field_C      = 4;
                sp50[0].field_D = a6;
                sp50[0].field_E = a5;
                func_8002E53C(p, buf);
            }
        } else {
            sp50[1].field_0 = x;
            sp50[1].field_2 = y;
            p[1].field_4    = four;
            sp50[1].field_8 = arg4;
            p[1].field_C    = four;
            sp50[1].field_D = a6;
            sp50[1].field_E = a5;
            func_8002E53C(&sp50[1], buf);
        }
        x  = arg1;
        y += 0xF;
    } while (ret != -1);

    return 0;
}

s32 func_8002FCBC(u8* arg0)
{
    GStruct38 sp10;

    sp10.field_C = 4;
    sp10.field_0 = 0;
    sp10.field_2 = 0;
    sp10.field_4 = 0;
    sp10.field_8 = 0;
    sp10.field_D = 2;
    sp10.field_E = 0;
    func_8002EDFC(&sp10, arg0);
    return -sp10.field_0;
}

s32 func_8002FD08(u8* arg0)
{
    u8         sp10[0x40];
    GStruct38  sp50;
    s32        maxWidth;
    s32        height;
    GStruct38* p;
    u8*        buf;
    u8*        cur;
    s32        ret;
    s32        tmp;
    s8         c;

    maxWidth = 0;
    height   = maxWidth;
    p        = &sp50;
    cur      = arg0;
    buf      = sp10;

    do {
        ret = func_8002F9E0(&cur, sp10);

        c            = 4;
        sp50.field_0 = 0;
        sp50.field_2 = 0;
        sp50.field_4 = 0;
        sp50.field_8 = 0;
        tmp          = c;
        p->field_C   = tmp;
        c            = 2;
        p->field_D   = c;
        sp50.field_E = 0;
        func_8002EDFC(p, buf);

        if (maxWidth < -sp50.field_0) {
            do {
            } while (0);
            maxWidth = -sp50.field_0;
        }
        height += 0xF;
        cur     = buf;
    } while (ret != -1);

    return (height << 16) | maxWidth;
}

s32 func_8002FDCC(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    GStruct38 sp10;
    GStruct38 sp20;
    s32       temp;

    if (arg0 != NULL) {
        if (arg0->field_8 == 5) {
            return 0;
        }
    } else {
        sp20.field_0 = arg1;
        sp20.field_2 = arg2;
        sp20.field_4 = 4;
        sp20.field_8 = arg4;
        sp20.field_C = 4;
        sp20.field_D = arg6;
        sp20.field_E = arg5;
        func_8002E53C(&sp20, arg3);
        return arg1;
    }
    sp10.field_0 = arg0->field_20 + arg1;
    sp10.field_2 = (arg0->field_22 + arg2) - 3;
    temp         = (s16)arg0->field_14;
    sp10.field_8 = arg4;
    sp10.field_C = 4;
    sp10.field_D = arg6;
    sp10.field_E = arg5;
    sp10.field_4 = temp + 1;
    func_8002E53C(&sp10, arg3);
    return sp10.field_0 - (s16)arg0->field_20;
}

void func_8002FEAC(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6)
{
    func_8002FDCC(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

INCLUDE_ASM("main/nonmatchings/201E0", func_8002FEE0);

void func_80030074(void)
{
    RECT rect;

    rect.x = 0x100;
    rect.y = 0xF3;
    rect.w = 0x40;
    rect.h = 1;
    LoadImage(&rect, D_80060910);

    rect.x = 0x3D0;
    rect.y = 0x1FF;
    rect.w = 0x30;
    rect.h = 1;
    LoadImage(&rect, D_800609B0);
}

void func_800300EC(u8* arg0, s32 arg1)
{
    s32 i;

    i = 0;
    do {
        *arg0 = D_80060DC8[i];
        i++;
        arg0++;
    } while (i < 0xC);

    *arg0   = D_80060E08[arg1];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    arg0[1] = 0;
}

void func_800301FC(void)
{
    u8(*a)[0x6C];
    u8(*b)[0xB0];
    u8(*c)[0x24];
    u8(*d)[0xE4];
    u8(*e)[0xA4];
    McSaveData* p;
    s32         one;
    s32         two;
    s32         idx;

    Mem_Set(&D_80073B88, 0, 0x40);
    Mem_Set(D_80073B88.field_40, 0xFF, 0x40);
    Mem_Set(D_80073980, 0, 0x100);
    Mem_Set(&D_80073980[0x100], 0xFF, 0x100);

    a = D_800733F0;
    Mem_Set(a, 0, 0x6C);
    do {
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
        p = &Mc_SaveData;
    } while (0);

    one          = 1;
    p->field_6   = 0x14;
    two          = 2;
    p->field_7   = one;
    p->field_4   = one;
    p->field_5   = one;
    p->field_8   = 7;
    p->field_9   = one;
    p->field_5C5 = two;
    p->field_22  = one;
    func_8004C4D0();
    idx                         = p->field_22 - 1;
    (&D_80073B88)[idx].field_21 = two;
}

void func_800303AC(void)
{
    McBufferSlot*             base;
    register McBufferSlot*    slot asm("t1");
    McBufferSlot*             end;
    register McChecksumBlock* block asm("t0");
    register s32              size asm("a3");
    register u8*              fptr asm("a0");
    register s16              sum asm("a2");
    register u32              fi asm("v1");
    register u32              i asm("a1");
    register u32              count asm("a0");
    register s32              fill asm("t2");
    register volatile u8*     cptr asm("v1");
    s32                       tmp;
    s32                       cond;

    fill = -1;
    base = Mc_BufferSlots;
    slot = base + 1;
    end  = base + 9;
    do {
        size = slot->field_4;
        fptr = (u8*)slot->field_0;
        cond = size;
        fi   = 0;
        if (cond != 0) {
            do {
                *fptr = 0;
                fi   += 1;
                fptr += 1;
            } while (fi < (u32)size);
        }
        cond = size;
        fi   = 0;
        if (cond != 0) {
            do {
                *fptr = fill;
                fi   += 1;
                fptr += 1;
            } while (fi < (u32)size);
        }
        sum   = 0;
        block = slot->field_0;
        asm("");
        i     = 0;
        count = size - 4;
        cptr  = block->field_4;
        if (count != 0) {
            do {
                i    += 1;
                tmp   = (s8)*cptr;
                sum   = sum + tmp;
                cptr += 1;
            } while (i < count);
        }
        slot          += 1;
        block->field_2 = ~sum;
        block->field_0 = sum;
    } while ((u32)slot < (u32)end);

    Display_State.field_10e = 1;
    func_800301FC();

    Mc_SaveData.field_21  = 0;
    Mc_SaveData.field_1a8 = 0;
    Mc_SaveData.field_1aa = 0;
    Mc_SaveData.field_1ab = 0;
    Mc_SaveData.field_1a9 = 0;
    Mc_SaveData.field_25  = 0;
    func_800260B0(1);
    func_800429C8(0);
}
