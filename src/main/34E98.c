#include "common.h"

#include "main/mem.h"
#include "main/unknown_syms.h"

void func_800498D4(Task* arg0);

void func_80044698(void)
{
}

INCLUDE_ASM("main/nonmatchings/34E98", func_800446A0);

INCLUDE_ASM("main/nonmatchings/34E98", func_80044C34);

INCLUDE_ASM("main/nonmatchings/34E98", func_800454E4);

INCLUDE_ASM("main/nonmatchings/34E98", func_800457F8);

void func_80045A3C(GStruct30* arg0, RECT* arg1, s32 arg2, s32 arg3)
{
    s16 temp;

    if (((u8)arg0->field_4 >> 4) == 1) {
        arg1->w = arg0->field_C.w;
        arg1->h = (arg0->field_C.h * arg2) >> 3;
        arg1->x = arg0->field_C.x;
        arg1->y = (arg0->field_C.y + arg0->field_C.h) - arg1->h;
    } else {
        arg1->w = (arg0->field_C.w * arg2) >> 3;
        temp    = arg0->field_C.h;
        if (temp >= 0xC) {
            temp = (((temp - 0xC) * arg2) >> 3) + 0xC;
        } else {
            temp = 0xC;
        }
        arg1->h = temp;
        arg1->x = arg0->field_C.x;
        arg1->y = (arg0->field_C.y + arg0->field_C.h) - arg1->h;
        arg1->x = arg0->field_C.x;
        arg1->w = arg0->field_C.w;
    }
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80045B24);

INCLUDE_ASM("main/nonmatchings/34E98", func_80045D24);

INCLUDE_ASM("main/nonmatchings/34E98", func_80045F24);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046124);

INCLUDE_ASM("main/nonmatchings/34E98", func_800463B4);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046508);

INCLUDE_ASM("main/nonmatchings/34E98", func_800466E4);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046830);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046B34);

/// Overlay of GStruct30 / UiObject at the layout halfwords that func_80046DEC
/// loads as signed (field_1C / field_1E are written with potentially negative
/// values by func_80049348; this function needs lh, not lhu).
typedef struct {
    /* 0x00 */ u8  pad0[0x14];
    /* 0x14 */ u16 field_14;
    /* 0x16 */ u8  pad16[6];
    /* 0x1C */ s16 field_1C;
    /* 0x1E */ s16 field_1E;
    /* 0x20 */ u16 field_20;
    /* 0x22 */ u16 field_22;
} GStruct30SignedLayout;

void func_80046DEC(UiList* arg0, GStruct30* arg1, s32 arg2)
{
    TILE*                           p;
    s32                             y;
    s32                             x1;
    s32                             width;
    s32                             h;
    register u32                    color asm("t4");
    register GStruct30SignedLayout* a1 asm("t0");
    u16                             f14;

    a1    = (GStruct30SignedLayout*)arg1;
    color = 0x1741F;
    asm("" : "+r"(color), "+r"(a1));
    f14          = a1->field_14;
    h            = arg0->field_7;
    x1           = a1->field_1C;
    a1->field_14 = f14 + 1;
    asm("" ::: "memory");
    width = a1->field_1E - x1;
    if ((width - 1) >= 2) {
        p          = (TILE*)D_80071190;
        D_80071190 = (DR_TPAGE*)(p + 1);
        p->x0      = a1->field_20 + x1 + 1;
        y          = a1->field_22;
        p->w       = width - 2;
        p->h       = h - 1;
        setlen(p, 3);
        *(u32*)&p->r0 = color;
        setcode(p, 0x60);
        arg2  = arg2 - h;
        p->y0 = y + arg2 + 1;
        addPrim(D_800710A0 + (s16)a1->field_14 + 1, p);
    }
    a1->field_14 = (u16)(a1->field_14 - 1);
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80046EEC);

void func_80047A0C(GStruct30* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4* p;
    s16       temp;
    s32       y;

    if (arg1 < arg2) {
        p     = (POLY_FT4*)D_80071190;
        temp  = arg0->field_20 + arg1;
        p->x2 = temp;
        p->x0 = temp;
        {
            register u16 f20 asm("v0");
            register s32 next asm("v1");
            register s32 ur asm("a1");

            f20        = arg0->field_20;
            next       = (s32)(p + 1);
            D_80071190 = (DR_TPAGE*)next;
            ur         = 0x6F;
            temp       = f20 + arg2;
            arg2       = 0x68;
            p->x3      = temp;
            p->x1      = temp;
            y          = arg0->field_22;
            p->v0      = 0x50;
            p->v1      = 0x50;
            p->v2      = 0x57;
            p->v3      = 0x57;
            p->tpage   = 0x1E;
            p->clut    = 0x3C03;
            setlen(p, 9);
            p->u1 = ur;
            p->u3 = ur;
            p->u0 = arg2;
            p->u2 = arg2;
            setcode(p, 0x2D);
        }
        y     = y + arg3;
        temp  = y - 4;
        y     = y + 3;
        p->y3 = y;
        p->y2 = y;
        p->y1 = temp;
        p->y0 = temp;
        addPrim(D_800710A0 + (s16)arg0->field_14 + 2, p);
    }
}

void func_80047B24(GStruct30* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4* p;
    s16       temp;
    s32       y;

    if (arg1 < arg2) {
        register s32 xoff asm("v1");
        register s32 left asm("v1");
        s32          base;

        xoff  = arg3;
        p     = (POLY_FT4*)D_80071190;
        base  = arg0->field_20 + xoff;
        left  = base - 3;
        temp  = base + 5;
        p->x2 = left;
        p->x0 = left;
        p->x3 = temp;
        p->x1 = temp;
        {
            register u16 f22 asm("v0");
            register s32 next asm("v1");
            register s32 ur asm("v1");
            register s32 ul asm("a2");

            f22        = arg0->field_22;
            next       = (s32)(p + 1);
            D_80071190 = (DR_TPAGE*)next;
            ur         = 0x77;
            ul         = 0x70;
            temp       = f22 + arg1;
            p->y1      = temp;
            p->y0      = temp;
            y          = arg0->field_22;
            p->v0      = 0x50;
            p->v1      = 0x50;
            p->v2      = 0x57;
            p->v3      = 0x57;
            p->tpage   = 0x1E;
            p->clut    = 0x3C03;
            setlen(p, 9);
            p->u1 = ur;
            p->u3 = ur;
            p->u0 = ul;
            p->u2 = ul;
            setcode(p, 0x2D);
        }
        y     = y + arg2;
        p->y3 = y;
        p->y2 = y;
        addPrim(D_800710A0 + (s16)arg0->field_14 + 2, p);
    }
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80047C40);

void func_80047D90(GStruct30* arg0, char* arg1)
{
    RECT       sp18;
    RECT*      r;
    s32        var_a2;
    s32        color;
    s32        x;
    s32        y;
    Task*      child;
    GStruct30* related;

    color = 0x505040;
    if (arg0->field_0 == 1) {
        color = 0x806020;
    }
    child = ((UiObject*)arg0)->field_28->field_c;
    if (child != NULL) {
        related = (GStruct30*)child->field_20;
        if (related->field_0 == 1) {
            if ((related->field_4 & 0xF) != 2) {
                color = 0x806020;
            }
        }
    }
    r = &sp18;
    switch (arg0->field_8) {
        case 1:
            var_a2 = 9 - arg0->field_16;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, r, var_a2, 0);
            break;
        case 2:
            goto block_default;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, r, var_a2, 1);
            break;
        default:
        block_default:
            r->x = arg0->field_C.x;
            r->y = arg0->field_C.y;
            r->w = arg0->field_C.w;
            r->h = arg0->field_C.h;
            break;
    }
    x              = sp18.x;
    y              = sp18.y;
    x              = x + 1;
    y              = y + 1;
    arg0->field_14 = (u16)(arg0->field_14 - 1);
    func_80047C40(arg0, x - (s16)arg0->field_20, y - (s16)arg0->field_22, arg1, color);
    arg0->field_14 = (u16)(arg0->field_14 + 1);
}

void func_80047F40(GStruct30* arg0, char* arg1)
{
    RECT  sp18;
    RECT* r;
    s32   var_a2;
    s32   color;
    s32   x;
    s32   y;

    color = 0x505040;
    if (arg0->field_0 == 1) {
        color = 0x806020;
    }
    r = &sp18;
    switch (arg0->field_8) {
        case 1:
            var_a2 = 9 - arg0->field_16;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, r, var_a2, 0);
            break;
        case 2:
            goto block_default;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, r, var_a2, 1);
            break;
        default:
        block_default:
            r->x = arg0->field_C.x;
            r->y = arg0->field_C.y;
            r->w = arg0->field_C.w;
            r->h = arg0->field_C.h;
            break;
    }
    x              = sp18.x;
    y              = sp18.y;
    x              = x + 1;
    y              = y + 1;
    arg0->field_14 = (u16)(arg0->field_14 - 1);
    func_80047C40(arg0, x - (s16)arg0->field_20, y - (s16)arg0->field_22, arg1, color);
    arg0->field_14 = (u16)(arg0->field_14 + 1);
}

INCLUDE_ASM("main/nonmatchings/34E98", func_800480A0);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048390);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048560);

UiObject* func_800486F0(UiObjectDesc* arg0, s32 arg1, s32 arg2, s32 arg3, UiObject* arg4)
{
    TaskDesc  desc;
    Task*     task;
    UiObject* obj;
    s32       field_8;

    obj           = NULL;
    desc.flags    = arg0->field_10;
    desc.field_2  = arg0->field_12;
    field_8       = arg0->field_18;
    desc.callback = func_800498D4;
    desc.field_8  = field_8;
    task          = Task_SpawnFromTable(&desc, (s32)obj, arg1, (s32)obj);
    if (task != NULL) {
        obj = (UiObject*)Mem_Calloc(0x30, (s32)obj);
        if (obj != NULL) {
            task->field_20 = obj;
            task->field_18 = func_800488B8;
            obj->field_28  = task;
            obj->field_0   = arg2;
            obj->field_4   = arg0->field_0;
            obj->field_C   = arg0->field_4;
            obj->field_E   = arg0->field_6;
            obj->field_10  = arg0->field_8;
            obj->field_12  = arg0->field_A;
            obj->field_14  = arg0->field_C & 0xFFFC;
            obj->field_24  = arg0->field_14;
            obj->field_16  = arg3;
            if (arg4 != NULL) {
                func_8002D14C(arg4->field_28, task);
            }
        } else {
            Task_Kill(task);
        }
    }
    return obj;
}

void func_80048838(UiObject* arg0, Task* arg1)
{
    Task* temp_s0;
    Task* child;

    temp_s0 = arg0->field_28;
    child   = temp_s0->field_c;
    if (child != NULL) {
        do {
            func_80048838(child->field_20, child);
            child = temp_s0->field_c;
        } while (child != NULL);
    }
    if (arg0->field_8 != 3) {
        Task_DetachFromParent(temp_s0);
        arg0->field_8 = 3;
    }
}

void func_800488B8(Task* arg0)
{
    if (arg0->field_20 != NULL) {
        Mem_Free(arg0->field_20);
    }
    Task_Kill(arg0);
}

void func_800488F8(Task* arg0)
{
    arg0->field_8 = (Task*)4;
}

void func_80048904(GStruct30* arg0, s32 arg1, s32 arg2)
{
    s16 temp_v1;

    if ((arg2 != 0) && (arg0->field_8 >= 5)) {
        temp_v1 = arg0->field_16;
        if ((temp_v1 < 0) || ((arg2 + 9) < temp_v1)) {
            arg0->field_16 = (s16)(arg2 + 9);
        }
    } else {
        func_80048964(arg0, (void*)arg1);
    }
}

void func_80048964(GStruct30* arg0, void* arg1)
{
    if (arg0->field_8 != 2) {
        if ((u16)arg0->field_16 >= 0xA) {
            arg0->field_16 = 9;
        }
        arg0->field_8 = 1;
    }
}

INCLUDE_ASM("main/nonmatchings/34E98", func_800489A0);

void func_80048AEC(UiList* arg0, s32 arg1)
{
    RECT       sp;
    GStruct30* a1;
    s16        temp_v0;
    u8         temp_a2;
    s8         temp_v1;
    s32        temp_v1_2;

    a1      = (GStruct30*)arg1;
    sp.x    = a1->field_20 + a1->field_1C;
    sp.y    = a1->field_22 + a1->field_18;
    sp.w    = a1->field_1E - a1->field_1C;
    temp_v0 = a1->field_1A - a1->field_18;
    arg1    = temp_v0;
    sp.h    = temp_v0;
    arg1    = arg1 - arg0->field_17;
    if (arg0->field_7 == 0) {
        arg0->field_7 = 0xA;
    }
    temp_a2 = arg0->field_4;
    temp_v1 = arg0->field_7;
    if (arg1 >= (temp_a2 * temp_v1)) {
        arg0->field_5 = temp_a2;
    } else {
        arg0->field_5 = arg1 / temp_v1;
        if ((s8)arg0->field_5 <= 0) {
            arg0->field_5 = 1;
        }
    }
    temp_v1_2 = arg0->field_4;
    if (arg0->field_10 >= temp_v1_2) {
        arg0->field_10 = temp_v1_2 - 1;
        asm("" ::: "memory");
        temp_v1_2 = arg0->field_4;
    }
    if ((s8)arg0->field_5 >= temp_v1_2) {
        arg0->field_9 = 0;
    }
    arg0->field_A = 0;
    asm("" : : "m"(sp));
}

void func_80048C10(void* arg0, void* arg1)
{
    func_80046EEC(arg0, arg1, 0);
}

void func_80048C30(UiList* arg0, GStruct30* arg1, s32 arg2)
{
    RECT sp;
    s16  temp_v0;
    u8   temp_a2;
    s8   temp_v1;
    s32  temp_v1_2;
    s32  height;

    arg0->field_17 = arg2;
    sp.x           = arg1->field_20 + arg1->field_1C;
    sp.y           = arg1->field_22 + arg1->field_18;
    sp.w           = arg1->field_1E - arg1->field_1C;
    temp_v0        = arg1->field_1A - arg1->field_18;
    height         = temp_v0;
    sp.h           = temp_v0;
    height         = height - arg0->field_17;
    if (arg0->field_7 == 0) {
        arg0->field_7 = 0xA;
    }
    temp_a2 = arg0->field_4;
    temp_v1 = arg0->field_7;
    if (height >= (temp_a2 * temp_v1)) {
        arg0->field_5 = temp_a2;
    } else {
        arg0->field_5 = height / temp_v1;
        if ((s8)arg0->field_5 <= 0) {
            arg0->field_5 = 1;
        }
    }
    temp_v1_2 = arg0->field_4;
    if (arg0->field_10 >= temp_v1_2) {
        arg0->field_10 = temp_v1_2 - 1;
        asm("" ::: "memory");
        temp_v1_2 = arg0->field_4;
    }
    if ((s8)arg0->field_5 >= temp_v1_2) {
        arg0->field_9 = 0;
    }
    arg0->field_A = 0;
    asm("" : : "m"(sp));
}

void func_80048D58(GStruct20* arg0, s32 arg1, s32 arg2)
{
    s32 i;
    s32 targetX;
    s32 targetY;
    s16 baseX;
    s16 baseY;
    u8  count;

    i         = 0;
    baseX     = arg0->field_20;
    baseY     = arg0->field_22;
    targetX   = arg1 + baseX;
    targetY   = arg2 + baseY;
    targetX <<= 8;
    targetY <<= 8;
    count     = Display_State.field_10a;
    if (count != 0) {
        do {
            i          += 1;
            D_80067648 += (targetX - D_80067648) >> 2;
            D_8006764C += (targetY - D_8006764C) >> 2;
        } while (i < count);
    }
    targetX = D_80067648 >> 8;
    targetY = D_8006764C >> 8;
    func_800463B4(arg0, targetX - arg0->field_20, targetY - arg0->field_22);
}

s32 func_80048E10(void* arg0, s32 arg1)
{
    return D_8006763C[arg1];
}

s32 func_80048E2C(s32 arg0)
{
    return (arg0 << 4) - arg0;
}

void func_80048E38(GStruct30* arg0, char* arg1)
{
    RECT  sp18;
    RECT* r;
    s32   var_a2;
    s32   color;
    s32   x;
    s32   y;

    color = 0x707060;
    r     = &sp18;
    switch (arg0->field_8) {
        case 1:
            var_a2 = 9 - arg0->field_16;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, r, var_a2, 0);
            break;
        case 2:
            goto block_default;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, r, var_a2, 1);
            break;
        default:
        block_default:
            r->x = arg0->field_C.x;
            r->y = arg0->field_C.y;
            r->w = arg0->field_C.w;
            r->h = arg0->field_C.h;
            break;
    }
    x              = sp18.x;
    y              = sp18.y;
    x              = x + 1;
    y              = y + 1;
    arg0->field_14 = (u16)(arg0->field_14 - 1);
    func_80047C40(arg0, x - (s16)arg0->field_20, y - (s16)arg0->field_22, arg1, color);
    arg0->field_14 = (u16)(arg0->field_14 + 1);
}

void func_80048F88(GStruct30* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    GStruct38 sp;
    s32       temp;

    if (arg0->field_8 == 2) {
        arg0->field_14 = (u16)(arg0->field_14 - 1);
        sp.field_0     = arg0->field_20 + arg1;
        sp.field_2     = arg0->field_22 + arg2;
        temp           = (s16)arg0->field_14;
        sp.field_8     = arg4;
        sp.field_C     = 0;
        sp.field_D     = (s8)arg6;
        sp.field_4     = temp + 1;
        sp.field_E     = (s8)arg5;
        func_8002E53C(&sp, arg3);
        arg0->field_14 = (u16)(arg0->field_14 + 1);
    }
}

void func_80049024(GStruct30* arg0, GStruct30* arg1, GStruct30* arg2)
{
    s32 temp;
    s32 limit;
    s16 new_var;

    limit           = 0x96;
    arg0->field_C.x = (arg1->field_18 + arg2->field_20) + 8;
    arg0->field_C.y = (arg1->field_1A + arg2->field_22) - 2;
    new_var         = arg0->field_C.x;
    temp            = limit - (new_var + arg0->field_C.w);
    if (temp < 0) {
        arg0->field_C.x = ((u16)new_var) + temp;
    }
    temp = 0x5A - (arg0->field_C.y + arg0->field_C.h);
    if (temp < 0) {
        arg0->field_C.y = ((u16)arg0->field_C.y) + temp;
    }
}

void func_800490A4(void* arg0, void* arg1)
{
    func_80048560(arg0, arg1, 0, 0);
}

void func_800490C8(void* arg0, void* arg1)
{
    func_80048560(arg0, arg1, 0x20, 0);
}

s32 func_800490EC(Task* arg0)
{
    return (s32)arg0->field_8 >= 4;
}

void func_80049100(s32 arg0, s32 arg1)
{
    DR_TPAGE* p;

    p          = D_80071190;
    D_80071190 = p + 1;
    setDrawTPage(p, 0, 1, 0x1E | ((arg1 & 3) << 5));
    addPrim(D_800710A0 + arg0, p);
}

void func_8004917C(UiList* arg0, s32 arg1)
{
    if (arg1 == 0) {
        arg0->field_A &= 0xFD;
        return;
    }
    arg0->field_A |= 2;
}

void func_800491AC(GStruct30* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5)
{
    TILE*        p;
    s32          y;
    register u32 color asm("t3");

    color = arg5;

    if ((color != 0) && (arg3 >= 2)) {
        p             = (TILE*)D_80071190;
        D_80071190    = (DR_TPAGE*)(p + 1);
        p->x0         = arg0->field_20 + arg1 + 1;
        y             = arg0->field_22;
        p->w          = arg3 - 1;
        p->h          = arg4 - 1;
        *(u32*)&p->r0 = color;
        setlen(p, 3);
        p->y0 = y + arg2 + 1;
        setcode(p, 0x60);
        addPrim(D_800710A0 + (s16)arg0->field_14 + 1, p);
    }
}

void func_80049288(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5)
{
    func_80046B34(arg0, arg1, arg2, arg3, arg4, arg5, 0);
}

void func_800492B8(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5)
{
    func_80046B34(arg0, arg1, arg2, arg3, arg4, arg5, 1);
}

void func_800492EC(void* arg0, RECT* arg1, RECT* arg2)
{
    arg2->x = arg1->x + 2;
    arg2->y = arg1->y + 2;
    arg2->w = (arg1->w + arg1->x) - arg2->x - 1;
    arg2->h = (arg1->h + arg1->y) - arg2->y - 1;
}

void func_80049348(GStruct30* arg0, RECT* arg1, RECT* arg2)
{
    RECT sp10;

    func_800492EC(arg0, &arg0->field_C, &sp10);
    if ((arg0->field_4 & 0xF) == 2) {
        sp10.y += 9;
        sp10.h -= 0xB;
        sp10.x += 2;
        sp10.w -= 4;
    } else {
        sp10.y += 2;
        sp10.h -= 4;
        sp10.x += 2;
        sp10.w -= 4;
    }
    arg0->field_1C = -(sp10.w >> 1);
    arg0->field_1E = arg0->field_1C + sp10.w;
    arg0->field_18 = -(sp10.h >> 1);
    arg0->field_1A = arg0->field_18 + sp10.h;
    arg0->field_20 = sp10.x - arg0->field_1C;
    arg0->field_22 = sp10.y - arg0->field_18;
    if (arg1 != NULL) {
        func_800492EC(arg0, arg1, arg2);
    }
}

void func_80049478(GStruct30* arg0, RECT* arg1)
{
    s32 var_a2;

    switch (arg0->field_8) {
        case 1:
            var_a2 = 9 - arg0->field_16;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, arg1, var_a2, 0);
            return;
        case 2:
            break;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, arg1, var_a2, 1);
            return;
    }
    arg1->x = arg0->field_C.x;
    arg1->y = arg0->field_C.y;
    arg1->w = arg0->field_C.w;
    arg1->h = arg0->field_C.h;
}

void func_80049554(GStruct30* arg0, void* arg1)
{
    if (arg0->field_16 == 0) {
        arg0->field_16 = 9;
        arg0->field_8 += 1;
        func_800495B4(arg0, arg1);
    } else {
        if (arg0->field_16 > 0) {
            arg0->field_16 += 9;
        }
        arg0->field_8 = 5;
        func_800497F4(arg0, arg1);
    }
}

void func_800495B4(GStruct30* arg0, void* arg1)
{
    s32 temp_s2;

    temp_s2       = arg0->field_0;
    arg0->field_0 = temp_s2 << 0x10;
    func_80045B24(arg0);
    arg0->field_24(arg1);
    arg0->field_16 -= Display_State.field_10a;
    if (arg0->field_16 <= 0) {
        arg0->field_16 = 0;
        if (arg0->field_8 == 1) {
            arg0->field_8 = 2;
        }
    }
    if (arg0->field_0 == (temp_s2 << 0x10)) {
        arg0->field_0 = temp_s2;
    }
}

void func_8004965C(GStruct30* arg0, void* arg1)
{
    func_80045D24(arg0);
    arg0->field_24(arg1);
}

void func_8004969C(GStruct30* arg0, void* arg1)
{
    if (arg0->field_16 >= 0) {
        arg0->field_16 += Display_State.field_10a;
    }
    if ((u16)arg0->field_16 >= 9U) {
        arg0->field_16 = 9;
        Task_CallExit(arg1);
        return;
    }
    arg0->field_0 = 0;
    func_80045F24(arg0);
    arg0->field_24(arg1);
}

void func_8004972C(GStruct30* arg0, void* arg1)
{
    s32 temp_s1;

    temp_s1 = arg0->field_0;
    if (arg0->field_16 >= 0) {
        arg0->field_16 += Display_State.field_10a;
    }
    if ((u16)arg0->field_16 >= 9U) {
        arg0->field_16 = -1;
        arg0->field_8 += 1;
        func_800497F4(arg0, arg1);
        return;
    }
    arg0->field_0 <<= 0x10;
    func_80045F24(arg0);
    arg0->field_24(arg1);
    if (arg0->field_0 == (temp_s1 << 0x10)) {
        arg0->field_0 = temp_s1;
    }
}

void func_800497F4(GStruct30* arg0, void* arg1)
{
    s16 temp_a0;
    s16 temp_v0;
    s32 temp_s0;
    s32 temp_s2;

    temp_s2       = arg0->field_0;
    temp_s0       = temp_s2 << 0x10;
    arg0->field_0 = temp_s0;
    func_800457F8(arg0);
    arg0->field_24(arg1);
    if (arg0->field_0 == temp_s0) {
        arg0->field_0 = temp_s2;
    }
    if (arg0->field_16 > 0) {
        temp_v0        = (u16)arg0->field_16 - Display_State.field_10a;
        arg0->field_16 = temp_v0;
        if (temp_v0 < 9) {
            arg0->field_16 = 9;
        }
    }
    temp_a0 = arg0->field_16;
    if (((temp_a0 < 0) && (arg0->field_0 == 1)) || (temp_a0 == 9)) {
        func_80048964(arg0, arg1);
    }
}

void func_800498D4(Task* arg0)
{
    GFunc30Table6 sp;
    GStruct30*    temp;

    sp   = D_80013F2C;
    temp = arg0->field_20;
    sp.funcs[temp->field_8](temp, arg0);
}

s32 func_80049950(void)
{
    struct {
        s16 unk0;
        s16 unk2;
    } sp;

    sp.unk0 = D_80067648 >> 8;
    sp.unk2 = D_8006764C >> 8;
    return *(s32*)&sp;
}

void func_80049980(GStruct30* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    POLY_F3*      p;
    u16           temp_v0;
    u32           mask;
    register u32  mask_hi asm("a1");
    register u32* ot asm("a2");

    p          = (POLY_F3*)D_80071190;
    temp_v0    = arg0->field_20 + arg1;
    p->x2      = temp_v0;
    p->x1      = temp_v0;
    p->x0      = temp_v0;
    D_80071190 = (DR_TPAGE*)((POLY_G3*)p + 1);
    temp_v0    = arg0->field_22 + arg2;
    p->y2      = temp_v0;
    p->y1      = temp_v0;
    p->y0      = temp_v0;
    if (arg4 == 0) {
        register u16 tv0 asm("v0");
        register u16 tv1 asm("v1");

        tv0   = p->x1;
        tv1   = ((volatile POLY_F3*)p)->y0;
        tv0   = tv0 - 4;
        p->x1 = tv0;
        tv0   = p->x2;
        tv1   = tv1 + 5;
        p->y2 = tv1;
        p->y1 = tv1;
        p->x2 = tv0 + 5;
    } else {
        register u16 tv0 asm("v0");
        register u16 tv1 asm("v1");

        tv0   = p->x1;
        tv1   = ((volatile POLY_F3*)p)->y0;
        tv0   = tv0 - 3;
        p->x1 = tv0;
        tv0   = p->x2;
        tv1   = tv1 - 4;
        p->y2 = tv1;
        p->y1 = tv1;
        p->x2 = tv0 + 4;
    }
    mask          = 0xFFFFFF;
    *(s32*)&p->r0 = arg3 * 2;
    setlen(p, 4);
    setcode(p, 0x20);
    ot      = D_800710A0;
    mask_hi = 0xFF000000;
    p->tag  = (p->tag & mask_hi) | (ot[(s16)arg0->field_14 + 1] & mask);
    ot[(s16)arg0->field_14 + 1] =
        (ot[(s16)arg0->field_14 + 1] & mask_hi) | ((u32)p & mask);
}

void func_80049A8C(Task* arg0)
{
    GStruct30* temp_s0;

    temp_s0 = arg0->field_20;
    if (func_8001D344() != 0) {
        func_801D4B64(arg0);
        return;
    }
    temp_s0->field_16 += Display_State.field_10a;
}

void func_80049AF0(GStruct60* arg0, UiObject* arg1)
{
    GStruct70* temp_s3;
    GStruct69* var_a3;
    s32        var_v0;
    s16        temp;

    temp_s3 = (GStruct70*)arg1->field_28->field_34;
    var_v0  = arg0->field_8;
    var_a3  = temp_s3->field_4;
    if (var_v0 > 0) {
        do {
            var_a3  = var_a3->field_4;
            var_v0 -= 1;
        } while (var_v0 > 0);
    }
    func_8002FDCC(arg1, arg0->field_18, arg0->field_1A, var_a3->field_0, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            temp           = 6;
            arg1->field_2C = (s8)(u8)arg0->field_8 + 1;
            arg1->field_2E = temp;
            return;
        }
        if ((temp_s3->field_C & 1) && (Pad_CheckButtons(0, 1, D_8005ED74 | D_8005ED78) != 0)) {
            arg1->field_2C = -1;
            arg1->field_2E = -1;
        }
    }
}

void func_80049C00(Task* arg0)
{
    UiObject*  obj;
    GStruct73* ctx;
    UiList*    menu;
    char*      text;
    u8         base;
    s16        status;
    Task*      parent;
    Task*      child;

    obj           = (UiObject*)arg0->field_20;
    ctx           = (GStruct73*)arg0->field_34;
    menu          = &D_80067654;
    obj->field_2E = 0;
    if (arg0->field_30 == 0) {
        base          = ctx->field_0;
        menu->field_5 = base;
        menu->field_4 = base;
        func_80046830(menu, (GStruct30*)obj);
        menu->field_A   = 1;
        arg0->field_30 += 1;
    }
    text = ctx->field_8;
    if (text != NULL) {
        func_80047F40((GStruct30*)obj, text);
    }
    func_80046EEC(menu, obj, 0);
    if (obj->field_0 == 1) {
        status = obj->field_2E;
        if ((status == 6) || (status == -1)) {
            ctx->field_2 = obj->field_2C;
            parent       = obj->field_28;
            child        = parent->field_c;
            if (child != NULL) {
                do {
                    func_80048838((UiObject*)child->field_20, child);
                    child = parent->field_c;
                } while (child != NULL);
            }
            if (obj->field_8 != 3) {
                Task_DetachFromParent(parent);
                obj->field_8 = 3;
            }
        }
    }
}

void func_80049D34(s32 arg0)
{
    if (D_80067694 != NULL) {
        D_80067694->field_28->field_34 = arg0;
    }
}

void func_80049D5C(s32 arg0)
{
    if (D_80067694 != NULL) {
        D_80067694->field_28->field_34 = arg0;
    }
}
