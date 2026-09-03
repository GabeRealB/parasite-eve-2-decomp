#include "common.h"

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/text.h"
#include "main/ui.h"

void Ui_DispatchObjectState(Task* arg0);
void Ui_InsetRect2(void* arg0, RECT* arg1, RECT* arg2);
void func_80044C34(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3);
void Ui_DrawPanel(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3);

void func_80044698(void)
{
}

void Ui_DrawWindowBorder(RECT* arg0, s32 arg1, s32 arg2)
{
    RECT         sp10;
    POLY_GT4*    p;
    POLY_GT4*    p2;
    DR_MODE*     dr;
    s16          temp;
    s16          mid;
    u16          y;
    register s32 color asm("v0");
    s32          color0;
    s32          color2;
    register s32 color3 asm("a1");
    s32          val;
    s32          t;
    register s32 c asm("a0");
    u8           u;
    u8           h;
    s16          sx;
    u16          ux;
    DR_MODE*     r;
    s32          y0r;

    p  = (POLY_GT4*)Gpu_PrimCursor;
    p2 = p + 1;

    temp  = arg0->x;
    p->x2 = temp;
    p->x0 = temp;

    temp   = arg0->x + arg0->w;
    p2->x2 = temp;
    p2->x0 = temp;

    mid    = arg0->w >> 1;
    p2->x3 = mid;
    p2->x1 = mid;
    p->x3  = mid;
    p->x1  = mid;

    temp   = arg0->y + arg0->h;
    p->y1  = temp;
    p->y0  = temp;
    p2->y1 = temp;
    p2->y0 = temp;

    y      = arg0->y;
    p->y3  = y;
    p->y2  = y;
    p2->y3 = y;
    p2->y2 = y;

    r              = (DR_MODE*)(p + 2);
    Gpu_PrimCursor = (DR_TPAGE*)r;
    if (p->x0 < p2->x0) {
        y0r = p->y0;
        if (p->y2 < y0r) {
            dr = r;
            goto body;
        }
    }
    goto end;
body:
    Gpu_PrimCursor = (DR_TPAGE*)(dr + 1);

    sp10.h = 0xFF;
    sp10.w = 0xFF;
    sp10.y = 0;
    sp10.x = 0;
    setTexWindow(dr, &sp10);
    addPrim(Gpu_CurrentOt + arg2, dr);

    if ((arg1 & 0xF) == 4) {
        p->tpage  = 0x1E;
        p2->tpage = 0x1E;
        p->clut   = 0x3C84;
        p2->clut  = 0x3C84;
    } else {
        p->tpage  = 0x1E;
        p2->tpage = 0x1E;
        p->clut   = 0x3C0F;
        p2->clut  = 0x3C0F;
    }

    if (arg1 & 0x10000) {
        color  = 0x606060;
        color0 = 0x505050;
        color2 = 0x808080;
        color3 = 0x707070;
        goto store_colors;
    }
    if ((arg1 & 0xF) == 4) {
        color = rsin(Display_State.field_8 << 6) + 0x1000;
        val   = color >> 7;

        color = 0xB0;
        t     = color - val;
        c     = t & 0xFF;
        if (t <= 0) {
            t = 1;
            SOFT_TOUCH_REG(t);
            c = t & 0xFF;
        }
        color          = 0x80;
        t              = color - val;
        color          = (c << 16) | (c << 8) | c;
        *(u32*)&p->r1  = color;
        *(u32*)&p2->r1 = color;
        if (t <= 0) {
            t = 1;
        }
        c              = t & 0xFF;
        color          = 0x40;
        t              = color - val;
        color          = (c << 16) | (c << 8) | c;
        *(u32*)&p->r0  = color;
        *(u32*)&p2->r0 = color;
        if (t <= 0) {
            t = 1;
        }
        c              = t & 0xFF;
        color          = 0x30;
        t              = color - val;
        color          = (c << 16) | (c << 8) | c;
        *(u32*)&p->r3  = color;
        *(u32*)&p2->r3 = color;
        if (t <= 0) {
            t = 1;
        }
        color          = t & 0xFF;
        color0         = (color << 16) | (color << 8) | color;
        *(u32*)&p2->r2 = color0;
        *(u32*)&p->r2  = color0;
    } else {
        color  = 0xA8A8A8;
        color0 = 0x808080;
        color2 = 0x404040;
        color3 = 0x303030;
    store_colors:
        *(u32*)&p->r1  = color;
        *(u32*)&p2->r1 = color;
        *(u32*)&p->r0  = color0;
        *(u32*)&p2->r0 = color0;
        *(u32*)&p->r3  = color2;
        *(u32*)&p2->r3 = color2;
        *(u32*)&p2->r2 = color3;
        *(u32*)&p->r2  = color3;
    }

    p->v1 = 0;
    p->v0 = 0;
    h     = arg0->h;
    p->v3 = h;
    p->v2 = h;

    p2->v1 = 0;
    p2->v0 = 0;
    h      = arg0->h;
    p2->v3 = h;
    p2->v2 = h;

    if (p->x0 < 0) {
        sx = p2->x0;
        ux = p2->x0;
        if (sx < 0) {
            p->x3 = ux;
            p->x1 = ux;
        } else {
            p->x3 = 0;
            p->x1 = 0;
        }
        setlen(p, 0xC);
        setcode(p, 0x3C);
        p->u2 = 0;
        p->u0 = 0;
        u     = (u8)p->x1 - (u8)p->x0;
        p->u3 = u;
        p->u1 = u;
        addPrim(Gpu_CurrentOt + arg2, p);
    }

    if (p2->x0 >= 0) {
        sx = p->x0;
        ux = p->x0;
        if (sx >= 0) {
            p2->x3 = ux;
            p2->x1 = ux;
            p2->u3 = 0;
            p2->u1 = 0;
        } else {
            p2->x3 = 0;
            p2->x1 = 0;
            u      = p->u1 & 0x1F;
            p2->u3 = u;
            p2->u1 = u;
        }
        setlen(p2, 0xC);
        setcode(p2, 0x3C);
        u      = p2->u1 + ((u8)p2->x0 - (u8)p2->x1);
        p2->u2 = u;
        p2->u0 = u;
        addPrim(Gpu_CurrentOt + arg2, p2);
    }

    dr             = (DR_MODE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(dr + 1);
    sp10.x         = 0;
    sp10.y         = 0;
    sp10.w         = 0x20;
    sp10.h         = 0x20;
    setTexWindow(dr, &sp10);
    addPrim(Gpu_CurrentOt + arg2, dr);
end:
    return;
}

INCLUDE_ASM("main/nonmatchings/ui", func_80044C34);

void Ui_DrawPanel(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3)
{
    RECT              sp10;
    RECT              sp18;
    POLY_F4*          poly;
    DR_TPAGE*         dr;
    s16               temp;
    u16               x;
    u16               y;
    u16               t;
    register DR_AREA* p asm("s0");

    if (arg0->field_4 >= 0) {
        if (arg3 != 0) {
            p              = (DR_AREA*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
            sp10.x         = arg2->x + 0xA0;
            temp           = arg2->y + 0x78;
            sp10.y         = temp;
            sp10.w         = arg2->w;
            sp10.h         = arg2->h;
            sp10.y         = temp + (Display_State.field_1f * 0x110);
            SetDrawArea(p, &sp10);
            addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 3, p);
        }
        func_80044C34(arg0, arg1, arg2, arg3);
        if (arg3 != 0) {
            s32 tw;
            tw = 0x140;
            TOUCH_REG(tw);
            sp18.w         = tw;
            sp18.h         = 0xF0;
            p              = (DR_AREA*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
            sp18.x         = 0;
            sp18.y         = Display_State.field_1f * 0x110;
            SetDrawArea(p, &sp18);
            addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 1, p);
        }
        if (arg0->field_4 & 0x10000) {
            poly           = (POLY_F4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
            setlen(poly, 5);
            setcode(poly, 0x2A);
            poly->b0 = 0;
            poly->g0 = 0;
            poly->r0 = 0;
            x        = arg2->x;
            poly->x2 = x;
            poly->x0 = x;
            t        = arg2->x + arg2->w;
            poly->x3 = t;
            poly->x1 = t;
            y        = arg2->y;
            poly->y1 = y;
            poly->y0 = y;
            t        = arg2->y + arg2->h;
            poly->y3 = t;
            poly->y2 = t;
            addPrim(Gpu_CurrentOt + (s16)arg0->field_14, poly);

            dr             = Gpu_PrimCursor;
            Gpu_PrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(Gpu_CurrentOt + (s16)arg0->field_14, dr);
        }
    }
}

void Ui_SetupClip(UiPanel* arg0)
{
    RECT     sp10;
    RECT     sp18;
    DR_AREA* p;

    Ui_InsetRect2(arg0, &arg0->field_C, &sp18);
    if ((arg0->field_4 & 0xF) == 2) {
        sp18.y += 9;
        sp18.h -= 0xB;
        sp18.x += 2;
        sp18.w -= 4;
    } else {
        sp18.y += 2;
        sp18.h -= 4;
        sp18.x += 2;
        sp18.w -= 4;
    }
    arg0->field_1C = -(sp18.w >> 1);
    arg0->field_1E = arg0->field_1C + sp18.w;
    arg0->field_18 = -(sp18.h >> 1);
    arg0->field_1A = arg0->field_18 + sp18.h;
    arg0->field_20 = sp18.x - arg0->field_1C;
    arg0->field_22 = sp18.y - arg0->field_18;

    p              = (DR_AREA*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    sp10.x         = 0;
    sp10.w         = 0;
    sp10.h         = 0;
    sp10.y         = Display_State.field_1f * 0x110;
    SetDrawArea(p, &sp10);
    addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 3, p);

    p              = (DR_AREA*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    sp10.x         = 0;
    sp10.w         = 0x140;
    sp10.h         = 0xF0;
    sp10.y         = Display_State.field_1f * 0x110;
    SetDrawArea(p, &sp10);
    addPrim(Gpu_CurrentOt + (s16)arg0->field_14, p);
}

void Ui_ScaleRect(UiPanel* arg0, RECT* arg1, s32 arg2, s32 arg3)
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

void Ui_LayoutAndClip(UiPanel* arg0)
{
    RECT sp10;
    RECT sp18;
    RECT sp20;
    s32  var_a2;

    {
        RECT* arg1;

        arg1 = &sp10;
        switch (arg0->field_8) {
            case 1:
                var_a2 = 9 - arg0->field_16;
                if (var_a2 <= 0) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(arg0, arg1, var_a2, 0);
                goto after_fill;
            case 2:
                break;
            case 3:
            case 4:
                var_a2 = 9 - arg0->field_16;
                if ((u32)(var_a2 - 1) >= 8U) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(arg0, arg1, var_a2, 1);
                goto after_fill;
        }
        arg1->x = arg0->field_C.x;
        arg1->y = arg0->field_C.y;
        arg1->w = arg0->field_C.w;
        arg1->h = arg0->field_C.h;
    }
after_fill: {
    RECT* arg1;

    arg1 = &sp10;
    Ui_InsetRect2(arg0, &arg0->field_C, &sp20);
    if ((arg0->field_4 & 0xF) == 2) {
        sp20.y += 9;
        sp20.h -= 0xB;
        sp20.x += 2;
        sp20.w -= 4;
    } else {
        sp20.y += 2;
        sp20.h -= 4;
        sp20.x += 2;
        sp20.w -= 4;
    }
    arg0->field_1C = -(sp20.w >> 1);
    arg0->field_1E = arg0->field_1C + sp20.w;
    arg0->field_18 = -(sp20.h >> 1);
    arg0->field_1A = arg0->field_18 + sp20.h;
    arg0->field_20 = sp20.x - arg0->field_1C;
    arg0->field_22 = sp20.y - arg0->field_18;
    if (arg1 != NULL) {
        Ui_InsetRect2(arg0, arg1, &sp18);
    }
    Ui_DrawPanel(arg0, &sp10, &sp18, 1);
}
}

void Ui_LayoutAndDraw(UiPanel* arg0)
{
    RECT sp10;
    RECT sp18;
    RECT sp20;
    s32  var_a2;

    {
        RECT* arg1;

        arg1 = &sp10;
        switch (arg0->field_8) {
            case 1:
                var_a2 = 9 - arg0->field_16;
                if (var_a2 <= 0) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(arg0, arg1, var_a2, 0);
                goto after_fill;
            case 2:
                break;
            case 3:
            case 4:
                var_a2 = 9 - arg0->field_16;
                if ((u32)(var_a2 - 1) >= 8U) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(arg0, arg1, var_a2, 1);
                goto after_fill;
        }
        arg1->x = arg0->field_C.x;
        arg1->y = arg0->field_C.y;
        arg1->w = arg0->field_C.w;
        arg1->h = arg0->field_C.h;
    }
after_fill: {
    RECT* arg1;

    arg1 = &sp10;
    Ui_InsetRect2(arg0, &arg0->field_C, &sp20);
    if ((arg0->field_4 & 0xF) == 2) {
        sp20.y += 9;
        sp20.h -= 0xB;
        sp20.x += 2;
        sp20.w -= 4;
    } else {
        sp20.y += 2;
        sp20.h -= 4;
        sp20.x += 2;
        sp20.w -= 4;
    }
    arg0->field_1C = -(sp20.w >> 1);
    arg0->field_1E = arg0->field_1C + sp20.w;
    arg0->field_18 = -(sp20.h >> 1);
    arg0->field_1A = arg0->field_18 + sp20.h;
    arg0->field_20 = sp20.x - arg0->field_1C;
    arg0->field_22 = sp20.y - arg0->field_18;
    if (arg1 != NULL) {
        Ui_InsetRect2(arg0, arg1, &sp18);
    }
    Ui_DrawPanel(arg0, &sp10, &sp18, 0);
}
}

void Ui_LayoutAndDrawAlt(UiPanel* arg0)
{
    RECT sp10;
    RECT sp18;
    RECT sp20;
    s32  var_a2;

    {
        RECT* arg1;

        arg1 = &sp10;
        switch (arg0->field_8) {
            case 1:
                var_a2 = 9 - arg0->field_16;
                if (var_a2 <= 0) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(arg0, arg1, var_a2, 0);
                goto after_fill;
            case 2:
                break;
            case 3:
            case 4:
                var_a2 = 9 - arg0->field_16;
                if ((u32)(var_a2 - 1) >= 8U) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(arg0, arg1, var_a2, 1);
                goto after_fill;
        }
        arg1->x = arg0->field_C.x;
        arg1->y = arg0->field_C.y;
        arg1->w = arg0->field_C.w;
        arg1->h = arg0->field_C.h;
    }
after_fill: {
    RECT* arg1;

    arg1 = &sp10;
    Ui_InsetRect2(arg0, &arg0->field_C, &sp20);
    if ((arg0->field_4 & 0xF) == 2) {
        sp20.y += 9;
        sp20.h -= 0xB;
        sp20.x += 2;
        sp20.w -= 4;
    } else {
        sp20.y += 2;
        sp20.h -= 4;
        sp20.x += 2;
        sp20.w -= 4;
    }
    arg0->field_1C = -(sp20.w >> 1);
    arg0->field_1E = arg0->field_1C + sp20.w;
    arg0->field_18 = -(sp20.h >> 1);
    arg0->field_1A = arg0->field_18 + sp20.h;
    arg0->field_20 = sp20.x - arg0->field_1C;
    arg0->field_22 = sp20.y - arg0->field_18;
    if (arg1 != NULL) {
        Ui_InsetRect2(arg0, arg1, &sp18);
    }
    Ui_DrawPanel(arg0, &sp10, &sp18, 1);
}
}

void Ui_SetListClip(UiList* arg0, UiPanel* arg1, s32 arg2)
{
    RECT     sp10;
    DR_AREA* p;
    s32      i;
    s16      temp;

    if (arg2 == 0) {
        for (i = 0; i < 2; i++) {
            p              = (DR_AREA*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
            sp10.x         = arg1->field_20 + (arg1->field_1C + 0xA0);
            temp           = arg1->field_22 + (arg1->field_18 + 0x78) + (Display_State.field_1f * 0x110);
            sp10.y         = temp;
            sp10.y         = temp + arg0->field_17;
            sp10.w         = arg1->field_1E - arg1->field_1C;
            temp           = ((s16)arg1->field_1A - (s16)arg1->field_18 - arg0->field_17) / arg0->field_7;
            sp10.h         = temp;
            sp10.h         = temp * arg0->field_7;
            SetDrawArea(p, &sp10);
            addPrim(Gpu_CurrentOt + (i + (s16)arg1->field_14) + 1, p);
        }
    } else {
        for (i = 0; i < 2; i++) {
            p              = (DR_AREA*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
            sp10.w         = 0x140;
            sp10.x         = 0;
            sp10.h         = 0xF0;
            sp10.y         = Display_State.field_1f * 0x110;
            SetDrawArea(p, &sp10);
            addPrim(Gpu_CurrentOt + (i + (s16)arg1->field_14) + 1, p);
        }
    }
}

void Ui_DrawCursor(UiPanel* arg0, s32 arg1, s32 arg2)
{
    SPRT_8*   p;
    DR_TPAGE* dr;
    s32       n;
    s32       y;
    s32       row;
    s32       half;
    s32       t;

    n = (u32)Display_State.field_c >> 3;
    if (arg0->field_0 != 0) {
        p              = (SPRT_8*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
        p->x0          = arg0->field_20 + arg1 - 8;
        y              = arg0->field_22;
        p->clut        = 0x3C0A;
        setlen(p, 3);
        setcode(p, 0x75);
        p->y0 = y + arg2 - 2;
        arg2  = n / 3;
        row   = arg2;
        arg2  = n - row * 3;
        half  = row / 2;
        half  = row - half * 2;
        SOFT_TOUCH_REG2(half, arg2);
        t     = arg2 * 8 - 0x18;
        p->u0 = t;
        t     = half * 8 + 0x30;
        p->v0 = t;
        addPrim(Gpu_CurrentOt + 4, p);
        dr             = Gpu_PrimCursor;
        Gpu_PrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, 0x1E);
        addPrim(Gpu_CurrentOt + 4, dr);
    }
}

void Ui_DrawCaret(UiList* arg0, UiPanel* arg1, s32 arg2)
{
    POLY_G3*         p;
    s16              x;
    s32              y;
    u32              mask;
    u32              mask_hi;
    register u_long* ot asm("a2");
    s32              y0;
    u16              t;
    s32              f1a;
    s32              tmp;

    p              = (POLY_G3*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setPolyG3(p);

    x     = arg1->field_C.x + arg1->field_C.w - 5;
    p->x2 = x;
    p->x1 = x;
    p->x0 = x;

    y     = arg1->field_22;
    p->y2 = y;
    p->y1 = y;
    p->y0 = y;

    if (arg2 == 0) {
        y     = y + arg1->field_18;
        p->y0 = y;
        if (arg1->field_0 == 1) {
            s32 tmp;

            tmp   = (((u32)Display_State.field_c >> 3) & 3) - 3;
            tmp   = y - tmp;
            p->y0 = tmp;
        }
        p->y0 = p->y0 + arg0->field_17;
        p->x1 = p->x1 - 4;
        t     = p->y0 + 5;
        p->x2 = p->x2 + 5;
        p->y2 = t;
        p->y1 = t;
    } else {
        f1a   = arg1->field_1A;
        tmp   = y + 2;
        y0    = f1a + tmp;
        p->y0 = y0;
        if (arg1->field_0 == 1) {
            register s32 tmp asm("v0");
            s32          c;

            c     = 0xFFFD;
            tmp   = ((u32)Display_State.field_c >> 3) & 3;
            tmp   = tmp + c;
            tmp   = y0 + tmp;
            p->y0 = tmp;
        }
        {
            register u16 tv0 asm("v0");
            u16          tv1;

            tv0   = p->x1;
            tv1   = ((volatile POLY_G3*)p)->y0;
            tv0   = tv0 - 3;
            p->x1 = tv0;
            tv0   = p->x2;
            tv1   = tv1 - 4;
            p->y2 = tv1;
            p->y1 = tv1;
            p->x2 = tv0 + 4;
        }
    }

    mask    = 0xFFFFFF;
    p->r0   = 0x9F;
    p->g0   = 0x7F;
    p->b0   = 0xBF;
    p->r2   = 0xDF;
    p->r1   = 0xDF;
    p->g2   = 0xCF;
    p->g1   = 0xCF;
    p->b2   = 0xFF;
    p->b1   = 0xFF;
    ot      = Gpu_CurrentOt;
    mask_hi = 0xFF000000;
    p->tag  = (p->tag & mask_hi) | (ot[(s16)arg1->field_14 + 1] & mask);
    ot[(s16)arg1->field_14 + 1] =
        (ot[(s16)arg1->field_14 + 1] & mask_hi) | ((u32)p & mask);
}

void Ui_UpdateLayoutSize(UiPanel* arg0, s32 arg1, s32 arg2)
{
    RECT sp10;

    if (arg1 > 0) {
        arg0->field_C.w = (arg0->field_C.w - (arg0->field_1E - arg0->field_1C)) + arg1;
    }
    if (arg2 > 0) {
        arg0->field_C.h = (arg0->field_C.h - (arg0->field_1A - arg0->field_18)) + arg2;
    }
    Ui_InsetRect2(arg0, &arg0->field_C, &sp10);
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
}

/// Signed overlay of UiList so field_5/field_7 load with lb (visible-row counts).
typedef struct {
    /* 0x00 */ u8  pad0[4];
    /* 0x04 */ u8  field_4;
    /* 0x05 */ s8  field_5;
    /* 0x06 */ s8  field_6;
    /* 0x07 */ s8  field_7;
    /* 0x08 */ u8  pad8;
    /* 0x09 */ u8  field_9;
    /* 0x0A */ u8  field_A;
    /* 0x0B */ u8  padB;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s8  field_16;
    /* 0x17 */ s8  field_17;
} UiListSignedRows;

/// Signed overlay of UiPanel layout halfwords (field_18..field_1E can be negative).
typedef struct {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ RECT field_C;
    /* 0x14 */ u16  field_14;
    /* 0x16 */ s16  field_16;
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
    /* 0x1C */ s16  field_1C;
    /* 0x1E */ s16  field_1E;
    /* 0x20 */ u16  field_20;
    /* 0x22 */ u16  field_22;
} UiPanelSignedLayoutFull;

void Ui_LayoutListPanel(UiList* arg0_, UiPanel* arg1_)
{
    UiListSignedRows*        arg0;
    UiPanelSignedLayoutFull* arg1;
    RECT                     sp10;
    s16                      temp_v0;
    u8                       temp_a2_u8;
    s8                       temp_v1;
    s32                      temp_v1_2;
    s32                      height;
    s32                      temp_a2;

    arg0 = (UiListSignedRows*)arg0_;
    arg1 = (UiPanelSignedLayoutFull*)arg1_;

    if (arg0->field_5 == 0) {
        arg0->field_5 = arg0->field_4;
    } else if (arg0->field_4 < arg0->field_5) {
        arg0->field_5 = arg0->field_4;
    }

    {
        s32          f5;
        s32          f7;
        register s32 f18 asm("a0");
        s32          f1a;
        s32          w;
        s32          x;
        u32          xv;

        f5              = arg0->field_5;
        f7              = arg0->field_7;
        f5              = f5 * f7;
        temp_a2         = 0x98;
        w               = arg1->field_C.w;
        x               = arg1->field_C.x;
        f18             = arg1->field_18;
        temp_a2         = temp_a2 - (x + w);
        f1a             = arg1->field_1A;
        f5              = f5 - (f1a - f18);
        arg1->field_C.h = arg1->field_C.h + f5;
        xv              = *(u16*)&arg1->field_C.x;
        if (temp_a2 < 0) {
            arg1->field_C.x = xv + temp_a2;
        }
    }
    temp_a2 = 0x70 - (arg1->field_C.y + arg1->field_C.h);
    if (temp_a2 < 0) {
        arg1->field_C.y += temp_a2;
    }

    Ui_InsetRect2(arg1, &arg1->field_C, &sp10);
    if ((arg1->field_4 & 0xF) == 2) {
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
    arg1->field_1C = -(sp10.w >> 1);
    arg1->field_1E = arg1->field_1C + sp10.w;
    arg1->field_18 = -(sp10.h >> 1);
    arg1->field_1A = arg1->field_18 + sp10.h;
    arg1->field_20 = sp10.x - arg1->field_1C;
    arg1->field_22 = sp10.y - arg1->field_18;

    arg0->field_17 = 0;
    sp10.x         = arg1->field_20 + arg1->field_1C;
    sp10.y         = arg1->field_22 + arg1->field_18;
    sp10.w         = arg1->field_1E - arg1->field_1C;
    temp_v0        = arg1->field_1A - arg1->field_18;
    height         = temp_v0;
    sp10.h         = temp_v0;
    height         = height - arg0->field_17;
    if (arg0->field_7 == 0) {
        arg0->field_7 = 0xA;
    }
    temp_a2_u8 = arg0->field_4;
    temp_v1    = arg0->field_7;
    if (height >= (temp_a2_u8 * temp_v1)) {
        arg0->field_5 = temp_a2_u8;
    } else {
        arg0->field_5 = height / temp_v1;
        if (arg0->field_5 <= 0) {
            arg0->field_5 = 1;
        }
    }
    temp_v1_2 = arg0->field_4;
    if (arg0->field_10 >= temp_v1_2) {
        arg0->field_10 = temp_v1_2 - 1;
        SOFT_COMPILER_BARRIER();
        temp_v1_2 = arg0->field_4;
    }
    if (arg0->field_5 >= temp_v1_2) {
        arg0->field_9 = 0;
    }
    arg0->field_A                   = 0;
    *(volatile s16*)&arg0->field_14 = 0;
    arg0->field_16                  = 0;
    *(volatile s32*)&arg0->field_C  = 0;
    if (Mc_SaveData.field_1ab != 0) {
        arg0->field_10 = 0;
        arg0->field_9  = 0;
    }
}

void func_80046B34(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    TILE*    p;
    LINE_F3* l;
    s32      y;
    u16      t;

    if ((arg5 != 0) && (arg3 >= 2)) {
        p              = (TILE*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
        p->x0          = arg0->field_20 + arg1 + 1;
        y              = arg0->field_22;
        p->w           = arg3 - 1;
        p->h           = arg4 - 1;
        *(u32*)&p->r0  = arg5;
        setlen(p, 3);
        p->y0 = y + arg2 + 1;
        setcode(p, 0x60);
        addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 1, p);
    }

    l              = (LINE_F3*)Gpu_PrimCursor;
    l->x2          = arg0->field_20 + arg1 + 1;
    t              = arg0->field_20 + (arg1 + arg3);
    l->x1          = t;
    l->x0          = t;
    Gpu_PrimCursor = (DR_TPAGE*)(l + 1);
    l->y0          = arg0->field_22 + arg2;
    t              = arg0->field_22 + (arg2 + arg4);
    l->y2          = t;
    l->y1          = t;
    *(u32*)&l->r0  = ((arg6 & 1) == 0) ? 0x506058 : 0x101810;
    setLineF3(l);
    addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 1, l);

    l              = (LINE_F3*)Gpu_PrimCursor;
    t              = arg0->field_20 + arg1;
    l->x1          = t;
    l->x2          = t;
    l->x0          = arg0->field_20 + (arg1 + arg3) - 1;
    Gpu_PrimCursor = (DR_TPAGE*)(l + 1);
    t              = arg0->field_22 + arg2;
    l->y1          = t;
    l->y0          = t;
    l->y2          = arg0->field_22 + (arg2 + arg4);
    *(u32*)&l->r0  = ((arg6 & 1) == 0) ? 0x101810 : 0x506058;
    setLineF3(l);
    addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 1, l);
}

/// Overlay of UiPanel / UiObject at the layout halfwords that Ui_DrawListHighlight
/// loads as signed (field_1C / field_1E are written with potentially negative
/// values by Ui_InsetLayout; this function needs lh, not lhu).
typedef struct {
    /* 0x00 */ u8  pad0[0x14];
    /* 0x14 */ u16 field_14;
    /* 0x16 */ u8  pad16[6];
    /* 0x1C */ s16 field_1C;
    /* 0x1E */ s16 field_1E;
    /* 0x20 */ u16 field_20;
    /* 0x22 */ u16 field_22;
} UiPanelSignedLayout;

void Ui_DrawListHighlight(UiList* arg0, UiPanel* arg1, s32 arg2)
{
    TILE*                p;
    s32                  y;
    s32                  x1;
    s32                  width;
    s32                  h;
    register u32         color asm("t4");
    UiPanelSignedLayout* a1;
    u16                  f14;

    a1    = (UiPanelSignedLayout*)arg1;
    color = 0x1741F;
    SOFT_TOUCH_REG2(color, a1);
    f14          = a1->field_14;
    h            = arg0->field_7;
    x1           = a1->field_1C;
    a1->field_14 = f14 + 1;
    SOFT_COMPILER_BARRIER();
    width = a1->field_1E - x1;
    if ((width - 1) >= 2) {
        p              = (TILE*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
        p->x0          = a1->field_20 + x1 + 1;
        y              = a1->field_22;
        p->w           = width - 2;
        p->h           = h - 1;
        setlen(p, 3);
        *(u32*)&p->r0 = color;
        setcode(p, 0x60);
        arg2  = arg2 - h;
        p->y0 = y + arg2 + 1;
        addPrim(Gpu_CurrentOt + (s16)a1->field_14 + 1, p);
    }
    a1->field_14 = (u16)(a1->field_14 - 1);
}

INCLUDE_ASM("main/nonmatchings/ui", func_80046EEC);

void Ui_DrawHBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4* p;
    s16       temp;
    s32       y;

    if (arg1 < arg2) {
        p     = (POLY_FT4*)Gpu_PrimCursor;
        temp  = arg0->field_20 + arg1;
        p->x2 = temp;
        p->x0 = temp;
        {
            u16          f20;
            register s32 next asm("v1");
            register s32 ur asm("a1");

            f20            = arg0->field_20;
            next           = (s32)(p + 1);
            Gpu_PrimCursor = (DR_TPAGE*)next;
            ur             = 0x6F;
            temp           = f20 + arg2;
            arg2           = 0x68;
            p->x3          = temp;
            p->x1          = temp;
            y              = arg0->field_22;
            p->v0          = 0x50;
            p->v1          = 0x50;
            p->v2          = 0x57;
            p->v3          = 0x57;
            p->tpage       = 0x1E;
            p->clut        = 0x3C03;
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
        addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 2, p);
    }
}

void Ui_DrawVBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4* p;
    s16       temp;
    s32       y;

    if (arg1 < arg2) {
        s32          xoff;
        register s32 left asm("v1");
        s32          base;

        xoff  = arg3;
        p     = (POLY_FT4*)Gpu_PrimCursor;
        base  = arg0->field_20 + xoff;
        left  = base - 3;
        temp  = base + 5;
        p->x2 = left;
        p->x0 = left;
        p->x3 = temp;
        p->x1 = temp;
        {
            u16          f22;
            register s32 next asm("v1");
            register s32 ur asm("v1");
            register s32 ul asm("a2");

            f22            = arg0->field_22;
            next           = (s32)(p + 1);
            Gpu_PrimCursor = (DR_TPAGE*)next;
            ur             = 0x77;
            ul             = 0x70;
            temp           = f22 + arg1;
            p->y1          = temp;
            p->y0          = temp;
            y              = arg0->field_22;
            p->v0          = 0x50;
            p->v1          = 0x50;
            p->v2          = 0x57;
            p->v3          = 0x57;
            p->tpage       = 0x1E;
            p->clut        = 0x3C03;
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
        addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 2, p);
    }
}

void Ui_DrawTextUnderline(UiPanel* arg0, s32 arg1, s32 arg2, char* arg3, s32 arg4)
{
    TextDrawReq       sp10;
    POLY_F4*          p;
    s32               x;
    s32               y;
    s32               textX;
    s32               color;
    s32               t20;
    s32               tmp0;
    s32               tmp1;
    s32               f22;
    s32               fourth;
    register s32      otIdx asm("s0");
    register UiPanel* self asm("s3");

    self            = arg0;
    color           = arg4;
    otIdx           = (s16)self->field_14;
    tmp0            = (s16)self->field_20;
    tmp1            = (s16)self->field_22;
    sp10.glyphTable = 5;
    sp10.centerMode = 0;
    sp10.field_E    = 0;
    x               = arg1 + tmp0;
    y               = arg2 + tmp1;
    sp10.x          = x + 2;
    sp10.y          = y + 5;
    otIdx           = otIdx + 1;
    sp10.otIndex    = otIdx;
    sp10.field_8    = color;
    func_8002E53C(&sp10, (u8*)arg3);

    p              = (POLY_F4*)Gpu_PrimCursor;
    p->x2          = x;
    p->x0          = x;
    textX          = (u16)sp10.x;
    Gpu_PrimCursor = (DR_TPAGE*)((POLY_FT4*)p + 1);
    *(s32*)&p->r0  = 0x21002;
    p->y3          = y + 7;
    p->y2          = y + 7;
    setcode(p, 0x28);
    setlen(p, 5);
    p->y1 = y;
    p->y0 = y;
    p->x3 = textX;
    p->x1 = textX + 3;
    addPrim(Gpu_CurrentOt + otIdx, p);

    t20    = (s16)self->field_20;
    f22    = (s16)self->field_22;
    fourth = f22 - 7;
    Ui_DrawHBar(self, x - t20, (s16)sp10.x - t20, y - fourth);
}

void Ui_DrawTextColored(UiPanel* arg0, char* arg1)
{
    RECT     sp18;
    RECT*    r;
    s32      var_a2;
    s32      color;
    s32      x;
    s32      y;
    Task*    child;
    UiPanel* related;

    color = 0x505040;
    if (arg0->field_0 == 1) {
        color = 0x806020;
    }
    child = ((UiObject*)arg0)->owner->firstChild;
    if (child != NULL) {
        related = (UiPanel*)child->spawnArg2;
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
            Ui_ScaleRect(arg0, r, var_a2, 0);
            break;
        case 2:
            goto block_default;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            Ui_ScaleRect(arg0, r, var_a2, 1);
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
    Ui_DrawTextUnderline(arg0, x - (s16)arg0->field_20, y - (s16)arg0->field_22, arg1, color);
    arg0->field_14 = (u16)(arg0->field_14 + 1);
}

void Ui_DrawText(UiPanel* arg0, char* arg1)
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
            Ui_ScaleRect(arg0, r, var_a2, 0);
            break;
        case 2:
            goto block_default;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            Ui_ScaleRect(arg0, r, var_a2, 1);
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
    Ui_DrawTextUnderline(arg0, x - (s16)arg0->field_20, y - (s16)arg0->field_22, arg1, color);
    arg0->field_14 = (u16)(arg0->field_14 + 1);
}

UiObject* Ui_SpawnTextBlock(TextBlockDesc* arg0_, s32 arg1, s32 arg2, s32 arg3)
{
    union {
        TaskDesc desc;
        RECT     rect;
    } sp;
    Task*          task;
    UiObject*      obj;
    UiObject*      result;
    TextLineNode*  node;
    s32            count;
    s32            maxWidth;
    s32            width;
    s32            field_8;
    s16            new_var;
    s32            cb;
    TextBlockDesc* arg0;
    s32            dummy;

    arg0   = arg0_;
    result = NULL;
    SOFT_TOUCH_REG(arg0);
    if (arg0->count > 0) {
        obj              = NULL;
        sp.desc.flags    = D_80067678.field_10;
        sp.desc.priority = D_80067678.field_12;
        field_8          = D_80067678.field_18;
        sp.desc.callback = Ui_DispatchObjectState;
        sp.desc.setupArg = field_8;
        task             = Task_SpawnFromTable(&sp.desc, (s32)obj, (s32)arg0, (s32)obj);
        dummy            = 1;
        if (task != NULL) {
            obj = (UiObject*)Mem_Calloc(0x30, (s32)obj);
            if (obj != NULL) {
                task->spawnArg2    = obj;
                task->exitCallback = Ui_FreeAndKill;
                obj->owner         = task;
                obj->status        = dummy;
                obj->field_4       = D_80067678.field_0;
                obj->field_C       = D_80067678.field_4;
                obj->field_E       = D_80067678.field_6;
                obj->field_10      = D_80067678.field_8;
                obj->field_12      = D_80067678.field_A;
                obj->drawOrder     = D_80067678.field_C & 0xFFFC;
                cb                 = D_80067678.field_14;
                obj->timer         = dummy;
                obj->callback      = cb;
            } else {
                Task_Kill(task);
            }
        }
        result = obj;
        if (result != NULL) {
            count    = arg0->count;
            node     = arg0->lines;
            maxWidth = 0;
            dummy    = dummy;
            if (arg0->field_8 == 0) {
                result->field_4 = 3;
            }
            if (count > 0) {
                do {
                    width = Text_MeasureWidth(node->text);
                    if (maxWidth < width) {
                        maxWidth = width;
                    }
                    node   = node->next;
                    count -= 1;
                } while (count > 0);
            }
            Ui_InsetRect2(result, (RECT*)&result->field_C, &sp.rect);
            if ((result->field_4 & 0xF) == 2) {
                sp.rect.y += 9;
                sp.rect.h -= 0xB;
                sp.rect.x += 2;
                sp.rect.w -= 4;
            } else {
                sp.rect.y += 2;
                sp.rect.h -= 4;
                sp.rect.x += 2;
                sp.rect.w -= 4;
            }
            result->field_1C = -(sp.rect.w >> 1);
            result->field_1E = result->field_1C + sp.rect.w;
            count            = result->field_1E;
            maxWidth        -= (s16)count - (s16)result->field_1C;
            result->field_18 = -(sp.rect.h >> 1);
            result->field_1A = result->field_18 + sp.rect.h;
            result->baseX    = sp.rect.x - result->field_1C;
            new_var          = sp.rect.y;
            result->field_10 = (result->field_10 + maxWidth) + 0xC;
            result->field_C  = -((s16)result->field_10 / 2);
            result->baseY    = new_var - (s16)result->field_18;
            maxWidth         = arg0->count * 0xF;
            maxWidth        -= (s16)result->field_1A - (s16)result->field_18;
            result->field_12 = result->field_12 + maxWidth;
            result->field_E  = -((s16)result->field_12 / 2);
            USE_REG(dummy);
        }
    }
    arg0->field_2 = 0;
    return result;
}

void Ui_DrawTextInRect(RECT* arg0, s32 arg1, s32 arg2, char* arg3)
{
    UiPanel  sp18;
    s32      pad[2];
    RECT     sp48;
    RECT     sp50;
    UiPanel* self;
    RECT*    r;
    s32      var_a2;
    s32      color;
    s32      x;
    s32      y;
    s32      two;
    s16      temp_t0;
    s16      temp_t1;

    two           = 2;
    sp18.field_8  = two;
    sp18.field_14 = arg1 - 3;
    sp18.field_4  = arg2;
    temp_t0       = arg0->x + two;
    sp48.x        = temp_t0;
    temp_t1       = arg0->y + two;
    sp48.y        = temp_t1;
    sp48.w        = ((arg0->w + arg0->x) - temp_t0) - 1;
    sp48.h        = ((arg0->h + arg0->y) - temp_t1) - 1;
    func_80044C34(&sp18, arg0, &sp48, 0);
    if (arg3 != NULL) {
        color = 0x707060;
        self  = &sp18;
        r     = &sp50;
        switch (self->field_8) {
            case 1:
                var_a2 = 9 - self->field_16;
                if (var_a2 <= 0) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(self, r, var_a2, 0);
                break;
            case 2:
                goto block_default;
            case 3:
            case 4:
                var_a2 = 9 - self->field_16;
                if ((u32)(var_a2 - 1) >= 8U) {
                    var_a2 = 1;
                }
                Ui_ScaleRect(self, r, var_a2, 1);
                break;
            default:
            block_default:
                r->x = self->field_C.x;
                r->y = self->field_C.y;
                r->w = self->field_C.w;
                r->h = self->field_C.h;
                break;
        }
        x              = sp50.x;
        y              = sp50.y;
        x              = x + 1;
        y              = y + 1;
        self->field_14 = (u16)(self->field_14 - 1);
        Ui_DrawTextUnderline(self, x - (s16)self->field_20, y - (s16)self->field_22, arg3, color);
        self->field_14 = (u16)(self->field_14 + 1);
    }
}

void Ui_SizeFromText(UiPanel* arg0, u8* arg1, s32 arg2, s32 arg3)
{
    struct {
        union {
            s32 as32;
            struct {
                u16 w;
                u16 h;
            } hw;
        } dims;
        s32  pad;
        RECT rect;
    } sp;
    s32 t;
    s32 u;

    sp.dims.as32 = Text_MeasureMultiLine(arg1);
    Ui_InsetRect2(arg0, &arg0->field_C, &sp.rect);
    if ((arg0->field_4 & 0xF) == 2) {
        sp.rect.y += 9;
        sp.rect.h -= 0xB;
        sp.rect.x += 2;
        sp.rect.w -= 4;
    } else {
        sp.rect.y += 2;
        sp.rect.h -= 4;
        sp.rect.x += 2;
        sp.rect.w -= 4;
    }
    arg0->field_1C = -(sp.rect.w >> 1);
    arg0->field_1E = arg0->field_1C + sp.rect.w;
    arg0->field_18 = -(sp.rect.h >> 1);
    arg0->field_1A = arg0->field_18 + sp.rect.h;
    arg0->field_20 = sp.rect.x - arg0->field_1C;
    arg0->field_22 = sp.rect.y - arg0->field_18;
    t              = arg2 + 5;
    u              = arg3 + 1;
    Ui_UpdateLayoutSize(arg0, sp.dims.hw.w + t, sp.dims.hw.h + u);
    arg0->field_C.x = -(arg0->field_C.w / 2);
    arg0->field_C.y = -(arg0->field_C.h / 2) - 0x14;
}

UiObject* Ui_SpawnFromDesc(UiObjectDesc* arg0, s32 arg1, s32 arg2, s32 arg3, UiObject* arg4)
{
    TaskDesc  desc;
    Task*     task;
    UiObject* obj;
    s32       field_8;

    obj           = NULL;
    desc.flags    = arg0->field_10;
    desc.priority = arg0->field_12;
    field_8       = arg0->field_18;
    desc.callback = Ui_DispatchObjectState;
    desc.setupArg = field_8;
    task          = Task_SpawnFromTable(&desc, (s32)obj, arg1, (s32)obj);
    if (task != NULL) {
        obj = (UiObject*)Mem_Calloc(0x30, (s32)obj);
        if (obj != NULL) {
            task->spawnArg2    = obj;
            task->exitCallback = Ui_FreeAndKill;
            obj->owner         = task;
            obj->status        = arg2;
            obj->field_4       = arg0->field_0;
            obj->field_C       = arg0->field_4;
            obj->field_E       = arg0->field_6;
            obj->field_10      = arg0->field_8;
            obj->field_12      = arg0->field_A;
            obj->drawOrder     = arg0->field_C & 0xFFFC;
            obj->callback      = arg0->field_14;
            obj->timer         = arg3;
            if (arg4 != NULL) {
                Task_Reparent(arg4->owner, task);
            }
        } else {
            Task_Kill(task);
        }
    }
    return obj;
}

void Ui_TeardownTree(UiObject* arg0, Task* arg1)
{
    Task* temp_s0;
    Task* child;

    temp_s0 = arg0->owner;
    child   = temp_s0->firstChild;
    if (child != NULL) {
        do {
            Ui_TeardownTree(child->spawnArg2, child);
            child = temp_s0->firstChild;
        } while (child != NULL);
    }
    if (arg0->mode != 3) {
        Task_DetachFromParent(temp_s0);
        arg0->mode = 3;
    }
}

void Ui_FreeAndKill(Task* arg0)
{
    if (arg0->spawnArg2 != NULL) {
        Mem_Free(arg0->spawnArg2);
    }
    Task_Kill(arg0);
}

void Ui_SetState4(Task* arg0, Task* arg1)
{
    arg0->parent = (Task*)4;
}

void Ui_ClampAnimOrClose(UiPanel* arg0, s32 arg1, s32 arg2)
{
    s16 temp_v1;

    if ((arg2 != 0) && (arg0->field_8 >= 5)) {
        temp_v1 = arg0->field_16;
        if ((temp_v1 < 0) || ((arg2 + 9) < temp_v1)) {
            arg0->field_16 = (s16)(arg2 + 9);
        }
    } else {
        Ui_StartCloseAnim(arg0, (void*)arg1);
    }
}

void Ui_StartCloseAnim(UiPanel* arg0, void* arg1)
{
    if (arg0->field_8 != 2) {
        if ((u16)arg0->field_16 >= 0xA) {
            arg0->field_16 = 9;
        }
        arg0->field_8 = 1;
    }
}

void Ui_InitList(UiList* arg0, UiMiniObj* arg1)
{
    RECT     sp;
    UiPanel* a1;
    s16      temp_v0;
    u8       temp_a2;
    s8       temp_v1;
    s32      temp_v1_2;
    s32      height;

    a1             = (UiPanel*)arg1;
    arg0->field_17 = 0;
    sp.x           = a1->field_20 + a1->field_1C;
    sp.y           = a1->field_22 + a1->field_18;
    sp.w           = a1->field_1E - a1->field_1C;
    temp_v0        = a1->field_1A - a1->field_18;
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
        SOFT_COMPILER_BARRIER();
        temp_v1_2 = arg0->field_4;
    }
    if ((s8)arg0->field_5 >= temp_v1_2) {
        arg0->field_9 = 0;
    }
    arg0->field_A                   = 0;
    *(volatile s16*)&arg0->field_14 = 0;
    arg0->field_16                  = 0;
    *(volatile s32*)&arg0->field_C  = 0;
    if (Mc_SaveData.field_1ab != 0) {
        arg0->field_10 = 0;
        arg0->field_9  = 0;
    }
    TOUCH_MEM(sp);
}

void Ui_ComputeVisibleRows(UiList* arg0, s32 arg1)
{
    RECT     sp;
    UiPanel* a1;
    s16      temp_v0;
    u8       temp_a2;
    s8       temp_v1;
    s32      temp_v1_2;

    a1      = (UiPanel*)arg1;
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
        SOFT_COMPILER_BARRIER();
        temp_v1_2 = arg0->field_4;
    }
    if ((s8)arg0->field_5 >= temp_v1_2) {
        arg0->field_9 = 0;
    }
    arg0->field_A = 0;
    TOUCH_MEM(sp);
}

void Ui_UpdateListNoAnim(void* arg0, void* arg1)
{
    func_80046EEC(arg0, arg1, 0);
}

void Ui_ComputeVisibleRowsEx(UiList* arg0, UiPanel* arg1, s32 arg2)
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
        SOFT_COMPILER_BARRIER();
        temp_v1_2 = arg0->field_4;
    }
    if ((s8)arg0->field_5 >= temp_v1_2) {
        arg0->field_9 = 0;
    }
    arg0->field_A = 0;
    TOUCH_MEM(sp);
}

void Ui_SmoothCursor(UiMiniObj* arg0, s32 arg1, s32 arg2)
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
    Ui_DrawCursor(arg0, targetX - arg0->field_20, targetY - arg0->field_22);
}

s32 Ui_LookupTable(void* arg0, s32 arg1)
{
    return D_8006763C[arg1];
}

s32 Ui_Scale15(s32 arg0)
{
    return (arg0 << 4) - arg0;
}

void Ui_DrawTitle(UiPanel* arg0, char* arg1)
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
            Ui_ScaleRect(arg0, r, var_a2, 0);
            break;
        case 2:
            goto block_default;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            Ui_ScaleRect(arg0, r, var_a2, 1);
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
    Ui_DrawTextUnderline(arg0, x - (s16)arg0->field_20, y - (s16)arg0->field_22, arg1, color);
    arg0->field_14 = (u16)(arg0->field_14 + 1);
}

void Ui_DrawTextAtLayout(UiPanel* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    TextDrawReq sp;
    s32         temp;

    if (arg0->field_8 == 2) {
        arg0->field_14 = (u16)(arg0->field_14 - 1);
        sp.x           = arg0->field_20 + arg1;
        sp.y           = arg0->field_22 + arg2;
        temp           = (s16)arg0->field_14;
        sp.field_8     = arg4;
        sp.glyphTable  = 0;
        sp.centerMode  = (s8)arg6;
        sp.otIndex     = temp + 1;
        sp.field_E     = (s8)arg5;
        func_8002E53C(&sp, arg3);
        arg0->field_14 = (u16)(arg0->field_14 + 1);
    }
}

void Ui_ClampDialogRect(UiPanel* arg0, UiPanel* arg1, UiPanel* arg2)
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

void Ui_SizeFromTextPlain(UiPanel* arg0, u8* arg1)
{
    Ui_SizeFromText(arg0, arg1, 0, 0);
}

void Ui_SizeFromTextWide(UiPanel* arg0, u8* arg1)
{
    Ui_SizeFromText(arg0, arg1, 0x20, 0);
}

s32 Ui_IsStateDone(Task* arg0)
{
    return (s32)arg0->parent >= 4;
}

void Ui_InsertDrawTPage(s32 arg0, s32 arg1)
{
    DR_TPAGE* p;

    p              = Gpu_PrimCursor;
    Gpu_PrimCursor = p + 1;
    setDrawTPage(p, 0, 1, 0x1E | ((arg1 & 3) << 5));
    addPrim(Gpu_CurrentOt + arg0, p);
}

void Ui_SetListScrollFlag(UiList* arg0, s32 arg1)
{
    if (arg1 == 0) {
        arg0->field_A &= 0xFD;
        return;
    }
    arg0->field_A |= 2;
}

void Ui_AllocTile(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5)
{
    TILE* p;
    s32   y;
    u32   color;

    color = arg5;

    if ((color != 0) && (arg3 >= 2)) {
        p              = (TILE*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
        p->x0          = arg0->field_20 + arg1 + 1;
        y              = arg0->field_22;
        p->w           = arg3 - 1;
        p->h           = arg4 - 1;
        *(u32*)&p->r0  = color;
        setlen(p, 3);
        p->y0 = y + arg2 + 1;
        setcode(p, 0x60);
        addPrim(Gpu_CurrentOt + (s16)arg0->field_14 + 1, p);
    }
}

void Ui_LayoutWithMode0(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5)
{
    func_80046B34(arg0, arg1, arg2, arg3, arg4, arg5, 0);
}

void Ui_LayoutWithMode1(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5)
{
    func_80046B34(arg0, arg1, arg2, arg3, arg4, arg5, 1);
}

void Ui_InsetRect2(void* arg0, RECT* arg1, RECT* arg2)
{
    arg2->x = arg1->x + 2;
    arg2->y = arg1->y + 2;
    arg2->w = (arg1->w + arg1->x) - arg2->x - 1;
    arg2->h = (arg1->h + arg1->y) - arg2->y - 1;
}

void Ui_InsetLayout(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3)
{
    RECT sp10;

    Ui_InsetRect2(arg0, &arg0->field_C, &sp10);
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
        Ui_InsetRect2(arg0, arg1, arg2);
    }
}

void Ui_ComputeAnimRect(UiPanel* arg0, RECT* arg1)
{
    s32 var_a2;

    switch (arg0->field_8) {
        case 1:
            var_a2 = 9 - arg0->field_16;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            Ui_ScaleRect(arg0, arg1, var_a2, 0);
            return;
        case 2:
            break;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            Ui_ScaleRect(arg0, arg1, var_a2, 1);
            return;
    }
    arg1->x = arg0->field_C.x;
    arg1->y = arg0->field_C.y;
    arg1->w = arg0->field_C.w;
    arg1->h = arg0->field_C.h;
}

void Ui_AnimOpenStep(UiPanel* arg0, void* arg1)
{
    if (arg0->field_16 == 0) {
        arg0->field_16 = 9;
        arg0->field_8 += 1;
        Ui_DrawAndCallback(arg0, arg1);
    } else {
        if (arg0->field_16 > 0) {
            arg0->field_16 += 9;
        }
        arg0->field_8 = 5;
        Ui_ClipAndCallback(arg0, arg1);
    }
}

void Ui_DrawAndCallback(UiPanel* arg0, void* arg1)
{
    s32 temp_s2;

    temp_s2       = arg0->field_0;
    arg0->field_0 = temp_s2 << 0x10;
    Ui_LayoutAndClip(arg0);
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

void Ui_LayoutDrawAndCallback(UiPanel* arg0, void* arg1)
{
    Ui_LayoutAndDraw(arg0);
    arg0->field_24(arg1);
}

void Ui_TickAnimCounter(UiPanel* arg0, void* arg1)
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
    Ui_LayoutAndDrawAlt(arg0);
    arg0->field_24(arg1);
}

void Ui_AnimCloseStep(UiPanel* arg0, void* arg1)
{
    s32 temp_s1;

    temp_s1 = arg0->field_0;
    if (arg0->field_16 >= 0) {
        arg0->field_16 += Display_State.field_10a;
    }
    if ((u16)arg0->field_16 >= 9U) {
        arg0->field_16 = -1;
        arg0->field_8 += 1;
        Ui_ClipAndCallback(arg0, arg1);
        return;
    }
    arg0->field_0 <<= 0x10;
    Ui_LayoutAndDrawAlt(arg0);
    arg0->field_24(arg1);
    if (arg0->field_0 == (temp_s1 << 0x10)) {
        arg0->field_0 = temp_s1;
    }
}

void Ui_ClipAndCallback(UiPanel* arg0, void* arg1)
{
    s16 temp_a0;
    s16 temp_v0;
    s32 temp_s0;
    s32 temp_s2;

    temp_s2       = arg0->field_0;
    temp_s0       = temp_s2 << 0x10;
    arg0->field_0 = temp_s0;
    Ui_SetupClip(arg0);
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
        Ui_StartCloseAnim(arg0, arg1);
    }
}

void Ui_DispatchObjectState(Task* arg0)
{
    UiPanelFuncTable6 sp;
    UiPanel*          temp;

    sp   = Ui_ObjectStates;
    temp = arg0->spawnArg2;
    sp.funcs[temp->field_8](temp, arg0);
}

s32 Ui_GetCursorFixed(void)
{
    struct {
        s16 unk0;
        s16 unk2;
    } sp;

    sp.unk0 = D_80067648 >> 8;
    sp.unk2 = D_8006764C >> 8;
    return *(s32*)&sp;
}

void Ui_DrawFlatCaret(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    POLY_F3*      p;
    u16           temp_v0;
    u32           mask;
    u32           mask_hi;
    register u32* ot asm("a2");

    p              = (POLY_F3*)Gpu_PrimCursor;
    temp_v0        = arg0->field_20 + arg1;
    p->x2          = temp_v0;
    p->x1          = temp_v0;
    p->x0          = temp_v0;
    Gpu_PrimCursor = (DR_TPAGE*)((POLY_G3*)p + 1);
    temp_v0        = arg0->field_22 + arg2;
    p->y2          = temp_v0;
    p->y1          = temp_v0;
    p->y0          = temp_v0;
    if (arg4 == 0) {
        register u16 tv0 asm("v0");
        u16          tv1;

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
        u16          tv1;

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
    ot      = Gpu_CurrentOt;
    mask_hi = 0xFF000000;
    p->tag  = (p->tag & mask_hi) | (ot[(s16)arg0->field_14 + 1] & mask);
    ot[(s16)arg0->field_14 + 1] =
        (ot[(s16)arg0->field_14 + 1] & mask_hi) | ((u32)p & mask);
}

void Ui_WaitCdThenOverlay(Task* arg0)
{
    UiPanel* temp_s0;

    temp_s0 = arg0->spawnArg2;
    if (CdCmd_IsIdle() != 0) {
        func_801D4B64(arg0);
        return;
    }
    temp_s0->field_16 += Display_State.field_10a;
}

void Ui_DrawDialogLine(DialogPrompt* arg0, UiObject* arg1)
{
    DialogListCtx* temp_s3;
    DialogOption*  var_a3;
    s32            var_v0;
    s16            temp;

    temp_s3 = (DialogListCtx*)arg1->owner->spawnArg1;
    var_v0  = arg0->field_8;
    var_a3  = temp_s3->field_4;
    if (var_v0 > 0) {
        do {
            var_a3  = var_a3->next;
            var_v0 -= 1;
        } while (var_v0 > 0);
    }
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, var_a3->text, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            temp           = 6;
            arg1->field_2C = (s8)(u8)arg0->field_8 + 1;
            arg1->field_2E = temp;
            return;
        }
        if ((temp_s3->field_C & 1) && (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0)) {
            arg1->field_2C = -1;
            arg1->field_2E = -1;
        }
    }
}

void Ui_ListTaskCallback(Task* arg0)
{
    UiObject*      obj;
    SelectMenuCtx* ctx;
    UiList*        menu;
    char*          text;
    u8             base;
    s16            status;
    Task*          parent;
    Task*          child;

    obj           = (UiObject*)arg0->spawnArg2;
    ctx           = (SelectMenuCtx*)arg0->spawnArg1;
    menu          = &D_80067654;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        base          = ctx->field_0;
        menu->field_5 = base;
        menu->field_4 = base;
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        menu->field_A = 1;
        arg0->state  += 1;
    }
    text = ctx->field_8;
    if (text != NULL) {
        Ui_DrawText((UiPanel*)obj, text);
    }
    func_80046EEC(menu, obj, 0);
    if (obj->status == 1) {
        status = obj->field_2E;
        if ((status == 6) || (status == -1)) {
            ctx->field_2 = obj->field_2C;
            parent       = obj->owner;
            child        = parent->firstChild;
            if (child != NULL) {
                do {
                    Ui_TeardownTree((UiObject*)child->spawnArg2, child);
                    child = parent->firstChild;
                } while (child != NULL);
            }
            if (obj->mode != 3) {
                Task_DetachFromParent(parent);
                obj->mode = 3;
            }
        }
    }
}

void Ui_SetHolderParam(s32 arg0, s32 arg1, s32 arg2)
{
    if (Wip_UiHolder != NULL) {
        Wip_UiHolder->field_28->field_34 = arg0;
    }
}

void Ui_SetHolderParamAlt(s32 arg0, s32 arg1, s32 arg2)
{
    if (Wip_UiHolder != NULL) {
        Wip_UiHolder->field_28->field_34 = arg0;
    }
}
