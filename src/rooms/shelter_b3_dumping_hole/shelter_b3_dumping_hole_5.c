#include "common.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/pad.h"
#include "main/stage.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/text.h"
#include "psyq/libgpu.h"
#include "rooms/shelter_b3_dumping_hole.h"

typedef struct {
    u8  field_0;
    u8  field_1;
    u8  _pad2[0x2];
    u8  field_4;
    u8  field_5;
    u8  _pad6[0x2];
    s32 field_8;
} DumpingHoleSpawnElem;

extern DumpingHoleSpawnElem* D_shelter_b3_dumping_hole_8018F4BC;
extern s16                   D_shelter_b3_dumping_hole_8018F4C6;

extern void func_shelter_b3_dumping_hole_8017FD9C(s32 arg0, s32 arg1);
extern void func_shelter_b3_dumping_hole_8017FE10(void);
extern void func_shelter_b3_dumping_hole_80181C8C(void);
s32         func_shelter_b3_dumping_hole_80181F80(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void        func_shelter_b3_dumping_hole_80182AA0(void);

typedef struct {
    u8  pad_00[0x2A];
    u16 field_2A;
    u8  pad_2C[0x4];
    s32 field_30;
    s32 field_34;
} Sub81A48;

void func_shelter_b3_dumping_hole_80181A48(Task* arg0)
{
    Sub81A48* s = (Sub81A48*)arg0;

    switch (s->field_30) {
        case 0:
            s->field_34  = 3;
            s->field_2A  = 8;
            s->field_30 += 1;
            break;
        case 1:
            if ((s16)(s->field_2A -= 1) < 0) {
                s->field_30 += 1;
            }
            Display_ClampField126(s->field_34);
            s->field_34 = -s->field_34;
            break;
        default:
            Display_ClampField126(0);
            taskKill(arg0);
            break;
    }
}

void func_shelter_b3_dumping_hole_80181B04(s16 arg0)
{
    func_shelter_b3_dumping_hole_8017FD9C(
        (s32) & ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[1], arg0);
}

void func_shelter_b3_dumping_hole_80181B44(void)
{
    func_shelter_b3_dumping_hole_8017FE10();
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80181B64);

void func_shelter_b3_dumping_hole_80181C8C(void)
{
    if (D_shelter_b3_dumping_hole_8018F4BC == NULL) {
        return;
    }
    if (D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8 == -1) {
        return;
    }
    if (Gp_CapBusy() != 0) {
        return;
    }
    func_shelter_b3_dumping_hole_80181F80(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8, 0x80, 1,
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_0 |
            ((D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_1 & 0x10)
             << 4));
    if (D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_4 & 1) {
        return;
    }
    func_shelter_b3_dumping_hole_80182AA0();
}

typedef struct {
    /* 0x00 */ char magic[0x8];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
} Reloc80181D68Hdr;

typedef struct {
    /* 0x00 */ u8  pad[0x8];
    /* 0x08 */ s32 field_8;
} Reloc80181D68Entry1;

typedef struct {
    /* 0x00 */ s16                 count;
    /* 0x02 */ u8                  pad[0xE];
    /* 0x10 */ Reloc80181D68Entry1 entries[1];
} Reloc80181D68Table1;

typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 entries[1];
} Reloc80181D68Table2;

extern char       D_shelter_b3_dumping_hole_8017D650[];
extern GlyphUvwh* D_shelter_b3_dumping_hole_8018F4B8;
extern s32        D_shelter_b3_dumping_hole_8018F4B4;

s32 func_shelter_b3_dumping_hole_80181D68(s32 arg0)
{
    Reloc80181D68Hdr*    hdr = (Reloc80181D68Hdr*)arg0;
    Reloc80181D68Entry1* r;
    s32*                 q;
    s32                  n1;
    s32                  n2;
    s32                  i;

    if (strncmp((char*)hdr, D_shelter_b3_dumping_hole_8017D650, 3) != 0) {
        return 0;
    }
    if (hdr->field_8 > 0) {
        hdr->field_8  += (s32)hdr;
        hdr->field_C  += (s32)hdr;
        hdr->field_10 += (s32)hdr;
        n1             = ((Reloc80181D68Table1*)hdr->field_C)->count;
        r              = &((Reloc80181D68Table1*)hdr->field_C)->entries[0];
        for (i = 0; i < n1; i++) {
            if (r->field_8 != -1) {
                r->field_8 += (s32)hdr;
            } else {
                r++;
            }
            r++;
        }
        n2 = ((Reloc80181D68Table2*)hdr->field_10)->count;
        q  = &((Reloc80181D68Table2*)hdr->field_10)->entries[0];
        for (i = 0; i < n2; i++) {
            if (*q != 0) {
                *q += (s32)hdr;
            }
            q++;
        }
    }
    D_shelter_b3_dumping_hole_8018F4B8 = (GlyphUvwh*)hdr->field_8;
    D_shelter_b3_dumping_hole_8018F4B4 = hdr->field_10 + 4;
    return 1;
}

extern s16 D_shelter_b3_dumping_hole_8018F4C0;
extern s16 D_shelter_b3_dumping_hole_8018F4C2;
extern s16 D_shelter_b3_dumping_hole_8018F4C4;
extern s16 D_shelter_b3_dumping_hole_8018F4C8;
extern s16 D_shelter_b3_dumping_hole_8018F4CA;
extern u8  D_shelter_b3_dumping_hole_8018F4D0;

s32 func_shelter_b3_dumping_hole_80182FD0(s32 arg0);
s16 func_shelter_b3_dumping_hole_80182C24(u16* arg0);
s16 func_shelter_b3_dumping_hole_80182D34(u16* arg0, s32 arg1);
s32 func_shelter_b3_dumping_hole_80182F18(u16* arg0);
s16 func_shelter_b3_dumping_hole_801829B4(u16* arg0);
s32 func_shelter_b3_dumping_hole_80182E50(s32 arg0);

s32 func_shelter_b3_dumping_hole_80181E70(s16 arg0, s16 arg1, s32 arg2)
{
    DumpingHoleSpawnElem* entry;

    entry                              = ((DumpingHoleSpawnElem**)D_shelter_b3_dumping_hole_8018F4B4)[arg0];
    D_shelter_b3_dumping_hole_8018F4BC = entry;
    if (entry == NULL) {
        return 1;
    }
    D_shelter_b3_dumping_hole_8018F4CA = arg1;
    D_shelter_b3_dumping_hole_8018F4C6 = func_shelter_b3_dumping_hole_80182FD0(1);
    D_shelter_b3_dumping_hole_8018F4C4 = arg2;
    D_shelter_b3_dumping_hole_8018F4C0 = func_shelter_b3_dumping_hole_80182C24(
        (u16*)D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8);
    D_shelter_b3_dumping_hole_8018F4C2 = func_shelter_b3_dumping_hole_801829B4(
        (u16*)D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8);
    D_shelter_b3_dumping_hole_8018F4C8 = func_shelter_b3_dumping_hole_80182E50(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8);
    D_shelter_b3_dumping_hole_8018F4D0 = 0x1E;
    return 0;
}

/// Caption drawing, shared in source with actor 215100: the glyph table the
/// resident text module provides, and the caption state set up above.
extern GlyphUvwh D_8010FB70[];
extern s16       D_shelter_b3_dumping_hole_8018B578;
extern s16       D_shelter_b3_dumping_hole_8018B57A;
extern u16       D_shelter_b3_dumping_hole_8018F4CC;
extern u16       D_shelter_b3_dumping_hole_8018F4CE;

s32 func_shelter_b3_dumping_hole_80181F80(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u16*       text;
    u16*       body;
    s32        title;
    s16        sc;
    u32        shifted;
    s32        titleWidth;
    s16        lineIdx;
    s16        x;
    s32        y;
    s16        i;
    u16        code;
    s16        centered;
    s32        palette;
    s16        t;
    s16        t2;
    s16        glyphY;
    s32        top;
    POLY_G4*   bg;
    POLY_G4*   bg2;
    DR_MODE*   dm;
    POLY_FT4*  ft;
    POLY_GT4*  gt;
    POLY_GT4*  gt2;
    GlyphUvwh* icon;

    lineIdx = 0;
    title   = arg3;
    text    = (u16*)arg0;
    x       = func_shelter_b3_dumping_hole_80182D34((u16*)arg0, 0) - 0xA0;
    y       = (u16)D_shelter_b3_dumping_hole_8018F4C2 - 0x78;

    bg             = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg + 1);
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
    bg->y0 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x1 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - D_shelter_b3_dumping_hole_8018F4C0 * 2 + 0xAB;
    bg->y1 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x2 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
    bg->y2 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8 + (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x3 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - D_shelter_b3_dumping_hole_8018F4C0 * 2 + 0xAB;
    bg->y3 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8 + (u16)D_shelter_b3_dumping_hole_8018F4C8;
    addPrim(&gGpuCurrentOt[3], bg);
    bg2            = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg2 + 1);
    *bg2           = *bg;
    addPrim(&gGpuCurrentOt[3], bg2);
    dm             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(dm + 1);
    setlen(dm, 1);
    dm->code[0] = 0xE100020A;
    addPrim(&gGpuCurrentOt[3], dm);

    body = text;
    if (title & 0xFF) {
        ft             = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(ft + 1);
        setlen(ft, 9);
        setcode(ft, 0x2D);
        title      = title - 1;
        top        = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - (u16)D_shelter_b3_dumping_hole_8018F4C8;
        ft->x0     = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        titleWidth = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w - 0xA7;
        ft->x1     = (u16)D_shelter_b3_dumping_hole_8018F4C0 + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->x2     = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
        ft->y2     = top - gDisplayState.vramYOffset;
        titleWidth = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w - 0xA7;
        ft->x3     = (u16)D_shelter_b3_dumping_hole_8018F4C0 + titleWidth;
        ft->y3     = top - gDisplayState.vramYOffset;
        ft->u0     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u;
        ft->v0     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v;
        ft->u1     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w;
        ft->v1     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v;
        ft->u2     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u;
        ft->v2     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->u3     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w;
        ft->v3     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
        addPrim(&gGpuCurrentOt[2], ft);
    }

    centered = 1;
    i        = 0;
    while (1) {
        code    = body[i];
        shifted = (u32)code << 16;
        sc      = (s32)shifted >> 16;
        if (sc == -1) {
            break;
        }
        if (sc == -2) {
            t2                                 = lineIdx + 1;
            lineIdx                            = t2;
            D_shelter_b3_dumping_hole_8018F4CE = y - 2;
            D_shelter_b3_dumping_hole_8018F4CC = x + 4;
            y                                 += func_shelter_b3_dumping_hole_80182F18(&body[i + 1]);
            if (centered != 0) {
                x = func_shelter_b3_dumping_hole_80182D34((u16*)arg0, t2) - 0xA0;
            } else {
                x = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA0;
            }
            i++;
            continue;
        } else if (sc == -3) {
            x += 3;
            i++;
            continue;
        } else if ((code & 0xFF00) == 0x8400) {
            icon           = &D_8010FB70[code & 0xFF];
            ft             = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(ft + 1);
            setlen(ft, 9);
            setcode(ft, 0x2D);
            ft->clut  = 0x3C00;
            ft->tpage = 0x1E;
            t         = (y - gDisplayState.vramYOffset) + 1;
            ft->x0    = x;
            ft->y0    = t - icon->h;
            ft->x1    = x + icon->w;
            ft->y1    = t - icon->h;
            ft->x2    = x;
            ft->y2    = t;
            ft->x3    = x + icon->w;
            ft->y3    = t;
            ft->u0    = icon->u;
            ft->v0    = icon->v;
            ft->u1    = icon->u + icon->w;
            ft->v1    = icon->v;
            ft->u2    = icon->u;
            ft->v2    = icon->v + icon->h;
            ft->u3    = icon->u + icon->w;
            ft->v3    = icon->v + icon->h;
            addPrim(&gGpuCurrentOt[2], ft);
            x += icon->w;
            i++;
            continue;
        } else {
            palette        = (shifted >> 26) & 3;
            code           = code & 0x3FF;
            glyphY         = y - gDisplayState.vramYOffset;
            gt             = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt + 1);
            setcode(gt, 0x3C);
            setlen(gt, 12);
            setShadeTex(gt, 1);
            setRGB0(gt, 0x70, 0x70, 0x70);
            setRGB1(gt, 0x70, 0x70, 0x70);
            setRGB2(gt, 0x70, 0x70, 0x70);
            setRGB3(gt, 0x70, 0x70, 0x70);
            setSemiTrans(gt, 1);
            gt->clut  = palette | 0x3D50;
            gt->x0    = x;
            gt->tpage = getTPage(0, 1, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
            gt->y0    = glyphY - D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->x1    = x + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->y1    = glyphY - D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->x2    = x;
            gt->y2    = glyphY;
            gt->x3    = x + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->y3    = glyphY;
            gt->u0    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u;
            gt->v0    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v;
            gt->u1    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->v1    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v;
            gt->u2    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u;
            gt->v2    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->u3    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->v3    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            addPrim(&gGpuCurrentOt[2], gt);
            gt2            = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt2 + 1);
            *gt2           = *gt;
            gt2->tpage     = getTPage(0, 2, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
            addPrim(&gGpuCurrentOt[2], gt2);
            x = D_shelter_b3_dumping_hole_8018F4B8[(s16)code].w + x - 1;
        }
        i++;
    }
    return 0;
}

/// Top Y of the caption block the text stream `arg0` holds: every line after
/// the first `-2` adds its height (the tallest glyph's `h + 2`, or 2 when empty)
/// and the total is subtracted from `D_shelter_b3_dumping_hole_8018F4C4`. Gameplay's
/// `Gp_CapTextTopY` is the same walk against a fixed 0xD0, and the two pins are
/// what that twin carries; unpinned the body lands at 92%.
s16 func_shelter_b3_dumping_hole_801829B4(u16* arg0)
{
    s32                 lineH;
    s32                 total;
    s32                 i;
    s32                 seenBreak;
    u16                 code;
    s32                 shifted;
    register s32        next asm("v1");
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");

    lineH     = 0;
    total     = lineH;
    i         = lineH;
    code      = arg0[0];
    shifted   = code << 16;
    seenBreak = lineH;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        do {
            v0tmp = seenBreak;
            if (shifted >> 16 == -2) {
                if (v0tmp != 0) {
                    if (lineH == 0) {
                        lineH = 2;
                    }
                    total += lineH;
                } else {
                    seenBreak = 1;
                }
                lineH = 0;
            } else if (shifted >> 16 != -3) {
                if (shifted >> 16 >= 0) {
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)D_shelter_b3_dumping_hole_8018F4B8);
                    if (lineH < glyph->h + 2) {
                        v0tmp = glyph->h;
                        TOUCH_REG(v0tmp);
                        lineH = v0tmp + 2;
                    }
                }
            }
            next    = i + 1;
            code    = arg0[(s16)next];
            i       = next;
            shifted = code << 16;
            v0tmp   = -1;
        } while (shifted >> 16 != v0tmp);
    }
    return (s16)(D_shelter_b3_dumping_hole_8018F4C4 - total);
}

typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u8  r;
    /* 0x05 */ u8  g;
    /* 0x06 */ u8  b;
    /* 0x07 */ u8  code;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  field_C;
    /* 0x0D */ u8  field_D;
    /* 0x0E */ u8  field_E;
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 field_12;
    /* 0x14 */ u8  field_14;
    /* 0x15 */ u8  field_15;
    /* 0x16 */ u8  field_16;
    /* 0x17 */ u8  pad_17;
    /* 0x18 */ s16 field_18;
    /* 0x1A */ s16 field_1A;
} Prim82AA0;

extern s32 D_shelter_b3_dumping_hole_8018B670;
extern s32 D_shelter_b3_dumping_hole_8018B674;

void func_shelter_b3_dumping_hole_80182AA0(void)
{
    Prim82AA0* prim;
    s32        c1;
    s32        c2;

    if (D_shelter_b3_dumping_hole_8018F4D0 != 0) {
        D_shelter_b3_dumping_hole_8018F4D0 -= 1;
        return;
    }
    prim           = (Prim82AA0*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 6);
    prim->code     = 0x30;
    c1             = (D_shelter_b3_dumping_hole_8018B670 << 7) / 15;
    prim->r        = c1;
    prim->g        = c1;
    prim->b        = c1;
    c1             = (D_shelter_b3_dumping_hole_8018B670 * 192) / 15;
    c2             = c1;
    prim->field_C  = c2;
    prim->field_D  = c2;
    prim->field_E  = c2;
    prim->field_14 = c2;
    prim->field_15 = c2;
    prim->field_16 = c2;
    prim->field_8  = D_shelter_b3_dumping_hole_8018F4CC + 3;
    prim->field_A  = D_shelter_b3_dumping_hole_8018F4CE;
    prim->field_10 = D_shelter_b3_dumping_hole_8018F4CC;
    prim->field_18 = D_shelter_b3_dumping_hole_8018F4CC + 7;
    prim->field_12 = D_shelter_b3_dumping_hole_8018F4CE - 7;
    prim->field_1A = D_shelter_b3_dumping_hole_8018F4CE - 7;
    addPrim(&gGpuCurrentOt[2], prim);
    if (D_shelter_b3_dumping_hole_8018B674 == 0) {
        D_shelter_b3_dumping_hole_8018B670 += 1;
        if (D_shelter_b3_dumping_hole_8018B670 >= 0xF) {
            D_shelter_b3_dumping_hole_8018B674 = 1;
        }
    } else {
        D_shelter_b3_dumping_hole_8018B670 -= 1;
        if (D_shelter_b3_dumping_hole_8018B670 < 9) {
            D_shelter_b3_dumping_hole_8018B674 = 0;
        }
    }
}

/// Horizontal centring offset of the caption line the text stream `arg0`
/// starts with: the widest line's pixel width subtracted from the 0x140 screen
/// width, halved, minus 5. The walk is the one `func_actor_215100_8014C360`
/// makes, and gameplay's `Gp_CapCenterX` compiles to the same 0x110 bytes with
/// only the glyph table symbol differing — `-2` closes a line and keeps the
/// running maximum, `-3` and `0x8400`-masked codes indent it by 3 and 0x10, and
/// each glyph code (non-negative, `& 0x3FF` indexing `D_shelter_b3_dumping_hole_8018F4B8`)
/// advances it by that glyph's `w - 1`.
///
/// The three pins are what gameplay's twin carries; leaving them out keeps the
/// block structure and instruction count but moves 71 register choices.
s16 func_shelter_b3_dumping_hole_80182C24(u16* arg0)
{
    register s32        lineW asm("t0");
    s32                 maxW;
    s32                 i;
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW   = 0;
    maxW    = lineW;
    i       = lineW;
    code    = arg0[0];
    shifted = code << 16;
    v0tmp   = -1;
    if (shifted >> 16 != v0tmp) {
        table = D_shelter_b3_dumping_hole_8018F4B8;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((lineW << 16) > (maxW << 16)) {
                    maxW = lineW;
                }
                lineW = 0;
                goto do_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            TOUCH_REG(masked);
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)maxW;
    return (0x140 - width) / 2 - 5;
}

/// Horizontal centring offset of line `arg1` of the caption text stream
/// `arg0`: that line's pixel width subtracted from 0x140, halved, minus 5.
/// Same walk as `func_actor_215100_8014C06C`, but keeps the width of the
/// selected line instead of the widest; gameplay's `Gp_CapCenterXLine`
/// compiles to the same bytes, pins included.
s16 func_shelter_b3_dumping_hole_80182D34(u16* arg0, s32 arg1)
{
    register s32        lineW asm("t1");
    s32                 selectedW;
    s32                 i;
    s32                 lineIndex;
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW     = 0;
    selectedW = lineW;
    i         = lineW;
    lineIndex = lineW;
    code      = arg0[0];
    shifted   = code << 16;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        table = D_shelter_b3_dumping_hole_8018F4B8;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((s16)lineIndex == arg1) {
                    selectedW = lineW;
                }
                lineW = 0;
                v0tmp = i + 1;
                i     = v0tmp;
                lineIndex++;
                goto after_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            TOUCH_REG(masked);
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
            after_inc:
                TOUCH_REG(v0tmp);
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)selectedW;
    return (0x140 - width) / 2 - 5;
}

typedef struct {
    /* 0x00 */ u8 pad[3];
    /* 0x03 */ u8 field_3;
} GlyphEntry;

s32 func_shelter_b3_dumping_hole_80182E50(s32 arg0)
{
    u16*        p = (u16*)arg0;
    short       acc;
    short       total;
    u16         i;
    u16         tok;
    s32         sh;
    s32         t;
    s32         ni;
    GlyphEntry* e;

    acc   = 0;
    total = acc;
    i     = total;
    tok   = *p;
    sh    = tok << 16;
    if ((sh >> 16) != -1) {
        do {
            t = sh >> 16;
            if (t == -2) {
                if (acc == 0) {
                    acc = 2;
                }
                total += acc;
                acc    = 0;
            } else if (t == -3) {
            } else if (t >= 0) {
                e = (GlyphEntry*)((tok & 0x3FF) * sizeof(GlyphEntry) + (s32)D_shelter_b3_dumping_hole_8018F4B8);
                if (acc < e->field_3 + 2) {
                    acc = e->field_3 + 2;
                }
            }
            ni  = (i = i + 1);
            tok = p[(s16)ni];
            sh  = tok << 16;
        } while ((sh >> 16) != -1);
    }
    return (s16)total;
}

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `func_800E6BB8` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `D_shelter_b3_dumping_hole_8018F4B8`) raises it to that glyph's `h + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
s32 func_shelter_b3_dumping_hole_80182F18(u16* arg0)
{
    s32                 height;
    s32                 i;
    s32                 cont;
    u16                 code;
    s32                 shifted;
    volatile GlyphUvwh* glyph;
    GlyphUvwh*          table;
    s32                 next;
    s32                 htmp;
    s32                 v0tmp;

    height  = 0;
    i       = height;
    cont    = 1;
    code    = arg0[0];
    table   = D_shelter_b3_dumping_hole_8018F4B8;
    shifted = code << 16;
    for (;;) {
        shifted = shifted >> 16;
        if (shifted == -2) {
            cont = 0;
        } else if (shifted == -1) {
            cont   = 0;
            height = 0xD;
        } else {
            if (shifted >= 0) {
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                if (height < glyph->h + 2) {
                    htmp   = glyph->h;
                    height = htmp + 2;
                    goto do_inc;
                }
                next = i + 1;
            } else {
            do_inc:
                next = i + 1;
            }
            i = next;
            TOUCH_REG(next);
            code = arg0[(s16)next];
        }
        v0tmp = cont;
        TOUCH_REG(v0tmp);
        if (v0tmp == 0) {
            break;
        }
        shifted = code << 16;
    }
    if (height == 0) {
        height = 2;
    }
    return height;
}

s32 func_shelter_b3_dumping_hole_80182FD0(s32 arg0)
{
    s32                   sentinel = -1;
    s32                   base     = (s32)D_shelter_b3_dumping_hole_8018F4BC;
    s32                   target   = D_shelter_b3_dumping_hole_8018F4CA;
    DumpingHoleSpawnElem* e        = (DumpingHoleSpawnElem*)(arg0 * sizeof(DumpingHoleSpawnElem) + base);

loop:
    if (e->field_8 != sentinel) {
        if (e->field_5 != target) {
            e++;
            arg0++;
            goto loop;
        }
    }
    return arg0;
}

void func_shelter_b3_dumping_hole_80183024(Task* arg0)
{
    if ((arg0->spawnArg1 -= 1) <= 0) {
        taskKill(arg0);
    }
    func_shelter_b3_dumping_hole_80181C8C();
}

void func_shelter_b3_dumping_hole_80183060(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->state = 1;
            break;
        case 1:
            arg0->spawnArg1 -= 1;
            if (arg0->spawnArg1 <= 0 || Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
                taskKill(arg0);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_shelter_b3_dumping_hole_80181C8C();
}
