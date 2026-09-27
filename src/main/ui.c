#include "common.h"

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3E9C.h"
#include "gameplay/4CC.h"
#include "main/devkit.h"

static void Ui_DispatchObjectState(Task* arg0);
static void Ui_InsetRect2(void* arg0, RECT* arg1, RECT* arg2);
static void func_80044C34(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3);
static void Ui_DrawPanel(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3);
static void Ui_AnimCloseStep(UiPanel* arg0, void* arg1);

static void Ui_AnimOpenStep(UiPanel* arg0, void* arg1);
static void Ui_ClipAndCallback(UiPanel* arg0, void* arg1);
static void Ui_DrawAndCallback(UiPanel* arg0, void* arg1);
static void Ui_DrawListHighlight(UiList* arg0, UiPanel* arg1, s32 arg2, s32 arg3);
static void Ui_LayoutDrawAndCallback(UiPanel* arg0, void* arg1);
static void Ui_TickAnimCounter(UiPanel* arg0, void* arg1);
static void func_80044698(void);
static void Ui_DrawDialogLine(DialogPrompt* arg0, UiObject* arg1);
static void Ui_ListTaskCallback(Task* arg0);

TaskDesc D_800670D0[] = {
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, func_80714A48 },
    { 0x1, 0xC0, func_80707534, { &D_8075BED4 } },
    { 0x2, 0xC0, func_807075A0 },
    { 0x1, 0xC0, func_807077C0, { &D_8075BED4 } },
    { 0x2, 0xC0, func_807080C8 },
    { 0x2, 0xC0, func_80707870 },
    { 0x0, 0xC0, func_80707980 },
    { 0x0, 0x60, Gp_EnemyDispatch },
    { 0x1, 0x40, func_807077C0, { &D_8075BED4 } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0x41, Gp_UpdateRoomCoords },
    { 0x0, 0x51, func_800D96C8 },
    { 0x2, 0xC0, taskKill },
    { 0x0, 0xC0, func_807146AC },
    { 0x1, 0xC0, func_8071473C, { &D_8075BED4 } },
    { 0x2, 0xC0, func_8071489C, { &D_8072C8F0 } },
    { 0x0, 0x0, NULL },
    { 0x0, 0x0, NULL },
    { 0x1, 0xC0, func_807149F0, { &D_8075BED4 } },
    { 0x0, 0xC0, func_80707B14 },
    { 0x0, 0x2F, func_800B2910 },
    { 0x2, 0x60, taskKill },
    { 0x0, 0xC0, func_80707C38 },
    { 0x1, 0xC0, func_80707F84, { &D_8075BED4 } },
    { 0x2, 0xC0, func_80708070 },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0x0, NULL },
    { 0x0, 0x0, NULL },
    { 0x0, 0xC0, Tmd_DispatchTask },
    { 0x0, 0xC0, Tmd_AllocNodeBuffers },
    { 0x0, 0x60, func_800B5DB8 },
    { 0x0, 0xC0, func_80044698 },
    { 0x0, 0x70, func_800CFD78 },
    { 0x0, 0xC0, func_800CE22C },
    { 0x0, 0xC2, Gp_FadeTileTask },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, func_807127A8 },
    { 0x0, 0x0, NULL },
    { 0x0, 0x70, taskKill },
    { 0x0, 0xC0, func_800B65B0 },
    { 0x0, 0xC0, func_800B60C0 },
    { 0x0, 0xC0, func_800D9CC8 },
    { 0x0, 0xC0, func_8070A6E8 },
    { 0x0, 0xC0, func_80708778 },
    { 0x0, 0x2F, Gp_FadeWorkTask },
    { 0x1, 0x70, Gp_EffAttachTask37 },
};

static u8 D_80067334[] = "Where am I?";
static u8 D_80067340[] = "Square";
static u8 D_80067348[] = "Fire Escape";
static u8 D_80067354[] = "MIST Parking";
static u8 D_80067364[] = "Gas Station";
static u8 D_80067370[] = "Trailer Coach";
static u8 D_80067380[] = "Motel Lobby";
static u8 D_8006738C[] = "Refuge";
static u8 D_80067394[] = "Sterilization Room";
static u8 D_800673A8[] = "Underground Parking";
static u8 D_800673BC[] = "Laboratory";
static u8 D_800673C8[] = "Incinerator Control Room";
static u8 D_800673E4[] = "Pod Deck";
static u8 D_800673F0[] = "Nursery";
static u8 D_800673F8[] = "Tent";
static u8 D_80067400[] = "Opening";
static u8 D_80067408[] = "Motel Room 6";

u8* D_80067418[] = {
    D_80067334,
    D_80067340,
    D_80067348,
    D_80067354,
    D_80067364,
    D_80067370,
    D_80067380,
    D_8006738C,
    D_80067394,
    D_800673A8,
    D_800673BC,
    D_800673C8,
    D_800673E4,
    D_800673F0,
    D_800673F8,
    D_80067400,
    D_80067408,
};

/* Ｓｑｕａｒｅ */
static u8 D_8006745C[] = "\x82\x72\x82\x91\x82\x95\x82\x81\x82\x92\x82\x85";
/* Ｆｉｒｅ　Ｅｓｃａｐｅ */
static u8 D_8006746C[] = "\x82\x65\x82\x89\x82\x92\x82\x85\x81\x40\x82\x64\x82\x93\x82\x83\x82\x81\x82\x90\x82\x85";
/* ＭＩＳＴ　Ｐａｒｋｉｎｇ */
static u8 D_80067484[] = "\x82\x6C\x82\x68\x82\x72\x82\x73\x81\x40\x82\x6F\x82\x81\x82\x92\x82\x8B\x82\x89\x82\x8E\x82\x87";
/* Ｇａｓ　Ｓｔａｔｉｏｎ */
static u8 D_800674A0[] = "\x82\x66\x82\x81\x82\x93\x81\x40\x82\x72\x82\x94\x82\x81\x82\x94\x82\x89\x82\x8F\x82\x8E";
/* Ｔｒａｉｌｅｒ　Ｃｏａｃｈ */
static u8 D_800674B8[] = "\x82\x73\x82\x92\x82\x81\x82\x89\x82\x8C\x82\x85\x82\x92\x81\x40\x82\x62\x82\x8F\x82\x81\x82\x83\x82\x88";
/* Ｍｏｔｅｌ　Ｌｏｂｂｙ */
static u8 D_800674D4[] = "\x82\x6C\x82\x8F\x82\x94\x82\x85\x82\x8C\x81\x40\x82\x6B\x82\x8F\x82\x82\x82\x82\x82\x99";
/* Ｒｅｆｕｇｅ */
static u8 D_800674EC[] = "\x82\x71\x82\x85\x82\x86\x82\x95\x82\x87\x82\x85";
/* Ｓｔｅｒｉｌｉｚａｔｉｏｎ　Ｒｍ． */
static u8 D_800674FC[] = "\x82\x72\x82\x94\x82\x85\x82\x92\x82\x89\x82\x8C\x82\x89\x82\x9A\x82\x81\x82\x94\x82\x89\x82\x8F\x82\x8E\x81\x40\x82\x71\x82\x8D\x81\x44";
/* Ｕｎｄｅｒｇｒｏｕｎｄ　Ｐａｒｋ． */
static u8 D_80067520[] = "\x82\x74\x82\x8E\x82\x84\x82\x85\x82\x92\x82\x87\x82\x92\x82\x8F\x82\x95\x82\x8E\x82\x84\x81\x40\x82\x6F\x82\x81\x82\x92\x82\x8B\x81\x44";
/* Ｌａｂｏｒａｔｏｒｙ */
static u8 D_80067544[] = "\x82\x6B\x82\x81\x82\x82\x82\x8F\x82\x92\x82\x81\x82\x94\x82\x8F\x82\x92\x82\x99";
/* Ｉｎｃｉｎ．　Ｃｏｎｔｒｏｌ */
static u8 D_8006755C[] = "\x82\x68\x82\x8E\x82\x83\x82\x89\x82\x8E\x81\x44\x81\x40\x82\x62\x82\x8F\x82\x8E\x82\x94\x82\x92\x82\x8F\x82\x8C";
/* Ｐｏｄ　Ｄｅｃｋ */
static u8 D_8006757C[] = "\x82\x6F\x82\x8F\x82\x84\x81\x40\x82\x63\x82\x85\x82\x83\x82\x8B";
/* Ｎｕｒｓｅｒｙ */
static u8 D_80067590[] = "\x82\x6D\x82\x95\x82\x92\x82\x93\x82\x85\x82\x92\x82\x99";
/* Ｔｅｎｔ */
static u8 D_800675A0[] = "\x82\x73\x82\x85\x82\x8E\x82\x94";
/* Ｏｐｅｎｉｎｇ */
static u8 D_800675AC[] = "\x82\x6E\x82\x90\x82\x85\x82\x8E\x82\x89\x82\x8E\x82\x87";
/* Ｍｏｔｅｌ　Ｒｏｏｍ　６ */
static u8 D_800675BC[] = "\x82\x6C\x82\x8F\x82\x94\x82\x85\x82\x8C\x81\x40\x82\x71\x82\x8F\x82\x8F\x82\x8D\x81\x40\x82\x55";
/* Ｗｈｅｒｅ　ａｍ　Ｉ？ */
static u8 D_800675D8[] = "\x82\x76\x82\x88\x82\x85\x82\x92\x82\x85\x81\x40\x82\x81\x82\x8D\x81\x40\x82\x68\x81\x48";

u8* D_800675F0[] = {
    D_800675D8,
    D_8006745C,
    D_8006746C,
    D_80067484,
    D_800674A0,
    D_800674B8,
    D_800674D4,
    D_800674EC,
    D_800674FC,
    D_80067520,
    D_80067544,
    D_8006755C,
    D_8006757C,
    D_80067590,
    D_800675A0,
    D_800675AC,
    D_800675BC,
    NULL,
};
/// Unreferenced.
static s32 D_80067638    = 0x001C2824;
static s32 D_8006763C[1] = { 0x000D287F };
static s32 D_80067640    = 0x00606060;
/// Unreferenced.
static s32 D_80067644 = 0x0038443C;
static s32 D_80067648 = 0xFFFFFF56;
static s32 D_8006764C = 0xFFFFFF7E;

static UiListItemFunc D_80067650[] = { Ui_DrawDialogLine };
static UiList         D_80067654   = { D_80067650, 1, 1, 0, 0x0F };
static UiObjectDesc   D_80067678   = { 2, 0xFFD0, 0xFFE0, 0x60, 0x40, 0x20, 0, 0, 0xC0, Ui_ListTaskCallback, 0 };
WipUiHolder*          Wip_UiHolder = NULL;

static const UiPanelFuncTable6 Ui_ObjectStates = { {
    Ui_AnimOpenStep,
    Ui_DrawAndCallback,
    Ui_LayoutDrawAndCallback,
    Ui_TickAnimCounter,
    Ui_AnimCloseStep,
    Ui_ClipAndCallback,
} };

static void func_80044698(void)
{
}

/// A neutral grey colour word with every channel at the low byte of `level`.
static inline s32 _uiGrey(s32 level)
{
    level &= 0xFF;
    return (level << 16) | (level << 8) | level;
}

static void Ui_DrawWindowBorder(RECT* arg0, s32 arg1, s32 arg2)
{
    RECT      sp10;
    POLY_GT4* p;
    POLY_GT4* p2;
    DR_MODE*  dr;
    s32       val;
    s32       t;

    p  = (POLY_GT4*)gGpuPrimCursor;
    p2 = p + 1;

    p->x0 = p->x2 = arg0->x;
    p2->x0 = p2->x2 = arg0->x + arg0->w;
    p->x1 = p->x3 = p2->x1 = p2->x3 = arg0->w >> 1;
    p2->y0 = p2->y1 = p->y0 = p->y1 = arg0->y + arg0->h;
    p2->y2 = p2->y3 = p->y2 = p->y3 = arg0->y;

    gGpuPrimCursor = p + 2;
    if (p->x0 >= p2->x0 || p->y0 <= p->y2) {
        return;
    }

    dr             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;

    sp10.w = sp10.h = 0xFF;
    sp10.x = sp10.y = 0;
    setTexWindow(dr, &sp10);
    addPrim(gGpuCurrentOt + arg2, dr);

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
        PRIM_COLOR_WORD(p, 1)  = 0x606060;
        PRIM_COLOR_WORD(p2, 1) = 0x606060;
        PRIM_COLOR_WORD(p, 0)  = 0x505050;
        PRIM_COLOR_WORD(p2, 0) = 0x505050;
        PRIM_COLOR_WORD(p, 3)  = 0x808080;
        PRIM_COLOR_WORD(p2, 3) = 0x808080;
        PRIM_COLOR_WORD(p2, 2) = 0x707070;
        PRIM_COLOR_WORD(p, 2)  = 0x707070;
    } else if ((arg1 & 0xF) == 4) {
        val = (rsin(gDisplayState.animFrame << 6) + 0x1000) >> 7;

        t = 0xB0 - val;
        if (t <= 0) {
            t = 1;
        }
        PRIM_COLOR_WORD(p2, 1) = PRIM_COLOR_WORD(p, 1) = _uiGrey(t);

        t = 0x80 - val;
        if (t <= 0) {
            t = 1;
        }
        PRIM_COLOR_WORD(p2, 0) = PRIM_COLOR_WORD(p, 0) = _uiGrey(t);

        t = 0x40 - val;
        if (t <= 0) {
            t = 1;
        }
        PRIM_COLOR_WORD(p2, 3) = PRIM_COLOR_WORD(p, 3) = _uiGrey(t);

        t = 0x30 - val;
        if (t <= 0) {
            t = 1;
        }
        PRIM_COLOR_WORD(p, 2) = PRIM_COLOR_WORD(p2, 2) = _uiGrey(t);
    } else {
        PRIM_COLOR_WORD(p, 1)  = 0xA8A8A8;
        PRIM_COLOR_WORD(p2, 1) = 0xA8A8A8;
        PRIM_COLOR_WORD(p, 0)  = 0x808080;
        PRIM_COLOR_WORD(p2, 0) = 0x808080;
        PRIM_COLOR_WORD(p, 3)  = 0x404040;
        PRIM_COLOR_WORD(p2, 3) = 0x404040;
        PRIM_COLOR_WORD(p2, 2) = 0x303030;
        PRIM_COLOR_WORD(p, 2)  = 0x303030;
    }

    p->v0 = p->v1 = 0;
    p->v2 = p->v3 = arg0->h;
    p2->v0 = p2->v1 = 0;
    p2->v2 = p2->v3 = arg0->h;

    if (p->x0 < 0) {
        if (p2->x0 < 0) {
            p->x1 = p->x3 = p2->x0;
        } else {
            p->x1 = p->x3 = 0;
        }
        setPolyGT4(p);
        p->u0 = p->u2 = 0;
        p->u1 = p->u3 = p->x1 - p->x0;
        addPrim(gGpuCurrentOt + arg2, p);
    }

    if (p2->x0 >= 0) {
        if (p->x0 >= 0) {
            p2->x1 = p2->x3 = p->x0;
            p2->u1 = p2->u3 = 0;
        } else {
            p2->x1 = p2->x3 = 0;
            p2->u1 = p2->u3 = p->u1 & 0x1F;
        }
        setPolyGT4(p2);
        p2->u0 = p2->u2 = p2->u1 + (p2->x0 - p2->x1);
        addPrim(gGpuCurrentOt + arg2, p2);
    }

    dr             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setRECT(&sp10, 0, 0, 0x20, 0x20);
    setTexWindow(dr, &sp10);
    addPrim(gGpuCurrentOt + arg2, dr);
}

static void func_80044C34(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3)
{
    SPRT*     spr;
    POLY_FT4* p;
    TILE*     tile;
    DR_TPAGE* dr;
    s16       t;
    u16       x;
    u16       y;
    u8        color;

    spr = (SPRT*)gGpuPrimCursor;
    arg1->w++;
    arg1->h++;
    gGpuPrimCursor = (SPRT_8*)spr + 1;
    spr->x0        = arg1->x;
    spr->y0        = arg1->y;
    spr->u0        = 0;
    spr->v0        = 0x50;
    spr->clut      = 0x3C03;
    setlen(spr, 3);
    setcode(spr, 0x75);
    addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, spr);

    spr            = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = spr + 1;
    spr->x0        = arg1->x + arg1->w - 8;
    if (spr->x0 > arg1->x) {
        spr->y0   = arg1->y;
        spr->u0   = 0x10;
        spr->v0   = 0x50;
        spr->clut = 0x3C03;
        setlen(spr, 3);
        setcode(spr, 0x75);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, spr);
    }

    spr            = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = spr + 1;
    spr->x0        = arg1->x;
    spr->y0        = arg1->y + arg1->h - 8;
    if (arg1->y < spr->y0) {
        spr->u0   = 0x28;
        spr->v0   = 0x50;
        spr->clut = 0x3C03;
        setlen(spr, 3);
        setcode(spr, 0x75);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, spr);
    }

    spr            = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = spr + 1;
    spr->x0        = arg1->x + arg1->w - 8;
    spr->y0        = arg1->y + arg1->h - 8;
    if (arg1->y < spr->y0 && spr->x0 > arg1->x) {
        spr->u0   = 0x38;
        spr->v0   = 0x50;
        spr->clut = 0x3C03;
        setlen(spr, 3);
        setcode(spr, 0x75);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, spr);
    }

    p              = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    x              = arg1->x + 8;
    p->x2          = x;
    p->x0          = x;
    t              = arg1->x + arg1->w - 8;
    p->x3          = t;
    p->x1          = t;
    y              = arg1->y;
    p->y1          = y;
    p->y0          = y;
    t              = arg1->y + 8;
    p->y3          = t;
    p->y2          = t;
    if (p->x0 < p->x1) {
        setUV4(p, 0x8, 0x50, 0x10, 0x50, 0x8, 0x58, 0x10, 0x58);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, p);
    }

    p              = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    x              = arg1->x + 8;
    p->x2          = x;
    p->x0          = x;
    t              = arg1->x + arg1->w - 8;
    p->x3          = t;
    p->x1          = t;
    y              = arg1->y + arg1->h - 8;
    p->y1          = y;
    p->y0          = y;
    t              = arg1->y + arg1->h;
    p->y3          = t;
    p->y2          = t;
    if (p->x0 < p->x1 && p->y0 > arg1->y) {
        setUV4(p, 0x30, 0x50, 0x38, 0x50, 0x30, 0x58, 0x38, 0x58);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, p);
    }

    p              = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    x              = arg1->x;
    p->x2          = x;
    p->x0          = x;
    t              = x + 8;
    p->x3          = t;
    p->x1          = t;
    y              = arg1->y + 8;
    p->y1          = y;
    p->y0          = y;
    t              = arg1->y + arg1->h - 8;
    p->y3          = t;
    p->y2          = t;
    if (p->y0 < p->y2) {
        setUV4(p, 0x18, 0x50, 0x20, 0x50, 0x18, 0x57, 0x20, 0x57);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, p);
    }

    p              = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    t              = arg1->x + arg1->w;
    x              = t - 8;
    p->x2          = x;
    p->x0          = x;
    p->x3          = t;
    p->x1          = t;
    y              = arg1->y + 8;
    p->y1          = y;
    p->y0          = y;
    t              = arg1->y + arg1->h - 8;
    p->y3          = t;
    p->y2          = t;
    if (p->x0 > arg1->x && p->y0 < p->y2) {
        setUV4(p, 0x20, 0x50, 0x28, 0x50, 0x20, 0x57, 0x28, 0x57);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, p);
    }
    Ui_DrawWindowBorder(arg2, arg0->field_4, (s16)arg0->field_14 + 3);
    if (arg0->field_4 & 0x20000) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        color          = (9 - arg0->field_16) * 8;
        tile->b0       = color;
        tile->g0       = color;
        tile->r0       = color;
        tile->x0       = -0xA0;
        tile->y0       = -0x78;
        tile->w        = 0x140;
        tile->h        = 0xF0;
        setlen(tile, 3);
        setcode(tile, 0x62);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, tile);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setlen(dr, 1);
        dr->code[0] = 0xE1000240;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, dr);
    }
}

static void Ui_DrawPanel(UiPanel* arg0, RECT* arg1, RECT* arg2, s32 arg3)
{
    RECT      sp10;
    RECT      sp18;
    POLY_F4*  poly;
    DR_TPAGE* dr;
    u16       x;
    u16       y;
    u16       t;

    if (arg0->field_4 >= 0) {
        if (arg3 != 0) {
            DR_AREA* p;

            p              = (DR_AREA*)gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            setRECT(&sp10, arg2->x + 0xA0, arg2->y + 0x78, arg2->w, arg2->h);
            sp10.y += gDisplayState.drawBuffer * 0x110;
            SetDrawArea(p, &sp10);
            addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, p);
        }
        func_80044C34(arg0, arg1, arg2, arg3);
        if (arg3 != 0) {
            DR_AREA* p;

            p              = (DR_AREA*)gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            setRECT(&sp18, 0, gDisplayState.drawBuffer * 0x110, 0x140, 0xF0);
            SetDrawArea(p, &sp18);
            addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, p);
        }
        if (arg0->field_4 & 0x10000) {
            poly           = (POLY_F4*)gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
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
            addPrim(gGpuCurrentOt + (s16)arg0->field_14, poly);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(gGpuCurrentOt + (s16)arg0->field_14, dr);
        }
    }
}

static void Ui_SetupClip(UiPanel* arg0)
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

    p              = (DR_AREA*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    sp10.x         = 0;
    sp10.w         = 0;
    sp10.h         = 0;
    sp10.y         = gDisplayState.drawBuffer * 0x110;
    SetDrawArea(p, &sp10);
    addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 3, p);

    p              = (DR_AREA*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    sp10.x         = 0;
    sp10.w         = 0x140;
    sp10.h         = 0xF0;
    sp10.y         = gDisplayState.drawBuffer * 0x110;
    SetDrawArea(p, &sp10);
    addPrim(gGpuCurrentOt + (s16)arg0->field_14, p);
}

static void Ui_ScaleRect(UiPanel* arg0, RECT* arg1, s32 arg2, s32 arg3)
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

static void Ui_LayoutAndClip(UiPanel* arg0)
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

static void Ui_LayoutAndDraw(UiPanel* arg0)
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

static void Ui_LayoutAndDrawAlt(UiPanel* arg0)
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

static void Ui_SetListClip(UiList* arg0, UiPanel* arg1, s32 arg2)
{
    RECT     sp10;
    DR_AREA* p;
    s32      i;
    s16      temp;

    if (arg2 == 0) {
        for (i = 0; i < 2; i++) {
            p              = (DR_AREA*)gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            sp10.x         = arg1->field_20 + (arg1->field_1C + 0xA0);
            temp           = arg1->field_22 + (arg1->field_18 + 0x78) + (gDisplayState.drawBuffer * 0x110);
            sp10.y         = temp;
            sp10.y         = temp + arg0->field_17;
            sp10.w         = arg1->field_1E - arg1->field_1C;
            temp           = ((s16)arg1->field_1A - (s16)arg1->field_18 - arg0->field_17) / arg0->field_7;
            sp10.h         = temp;
            sp10.h         = temp * arg0->field_7;
            SetDrawArea(p, &sp10);
            addPrim(gGpuCurrentOt + (i + (s16)arg1->field_14) + 1, p);
        }
    } else {
        for (i = 0; i < 2; i++) {
            p              = (DR_AREA*)gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            sp10.w         = 0x140;
            sp10.x         = 0;
            sp10.h         = 0xF0;
            sp10.y         = gDisplayState.drawBuffer * 0x110;
            SetDrawArea(p, &sp10);
            addPrim(gGpuCurrentOt + (i + (s16)arg1->field_14) + 1, p);
        }
    }
}

static void Ui_DrawCursor(UiPanel* arg0, s32 arg1, s32 arg2)
{
    SPRT_8*   p;
    DR_TPAGE* dr;
    s32       n;
    s32       y;
    s32       row;
    s32       half;
    s32       t;

    n = (u32)gDisplayState.vsyncCount >> 3;
    if (arg0->field_0 != 0) {
        p              = (SPRT_8*)gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
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
        t     = arg2 * 8 - 0x18;
        p->u0 = t;
        t     = half * 8 + 0x30;
        p->v0 = t;
        addPrim(gGpuCurrentOt + 4, p);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, 0x1E);
        addPrim(gGpuCurrentOt + 4, dr);
    }
}

static void Ui_DrawCaret(UiList* arg0, UiPanel* arg1, s32 arg2)
{
    POLY_G3* p;
    s16      x;
    s32      y;
    s32      y0;
    u16      t;

    p              = (POLY_G3*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
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
        y    += arg1->field_18;
        p->y0 = y;
        if (arg1->field_0 == 1) {
            p->y0 -= (((u32)gDisplayState.vsyncCount >> 3) & 3) - 3;
        }
        p->y0 += arg0->field_17;
        p->x1 -= 4;
        t      = p->y0 + 5;
        p->x2 += 5;
        p->y2  = t;
        p->y1  = t;
    } else {
        y0    = y + 2;
        p->y0 = arg1->field_1A + y0;
        if (arg1->field_0 == 1) {
            p->y0 += (((u32)gDisplayState.vsyncCount >> 3) & 3) - 3;
        }
        p->x1 -= 3;
        t      = p->y0 - 4;
        p->x2 += 4;
        p->y2  = t;
        p->y1  = t;
    }

    p->r0 = 0x9F;
    p->g0 = 0x7F;
    p->b0 = 0xBF;
    p->r2 = 0xDF;
    p->r1 = 0xDF;
    p->g2 = 0xCF;
    p->g1 = 0xCF;
    p->b2 = 0xFF;
    p->b1 = 0xFF;
    addPrim(gGpuCurrentOt + (s16)arg1->field_14 + 1, p);
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
    s32                      height;
    s32                      overflow;
    s32                      growth;

    arg0 = (UiListSignedRows*)arg0_;
    arg1 = (UiPanelSignedLayoutFull*)arg1_;

    if (arg0->field_5 == 0) {
        arg0->field_5 = arg0->field_4;
    } else if (arg0->field_4 < arg0->field_5) {
        arg0->field_5 = arg0->field_4;
    }

    growth           = arg0->field_5 * arg0->field_7;
    growth          -= arg1->field_1A - arg1->field_18;
    arg1->field_C.h += growth;
    overflow         = 0x98 - (arg1->field_C.x + arg1->field_C.w);
    if (overflow < 0) {
        arg1->field_C.x += overflow;
    }
    overflow = 0x70 - (arg1->field_C.y + arg1->field_C.h);
    if (overflow < 0) {
        arg1->field_C.y += overflow;
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
    sp10.h         = arg1->field_1A - arg1->field_18;
    height         = sp10.h;
    height        -= arg0->field_17;
    if (arg0->field_7 == 0) {
        arg0->field_7 = 0xA;
    }
    if (height >= arg0->field_4 * arg0->field_7) {
        arg0->field_5 = arg0->field_4;
    } else {
        arg0->field_5 = height / arg0->field_7;
        if (arg0->field_5 <= 0) {
            arg0->field_5 = 1;
        }
    }
    if (arg0->field_10 >= arg0->field_4) {
        arg0->field_10 = arg0->field_4 - 1;
    }
    if (arg0->field_4 <= arg0->field_5) {
        arg0->field_9 = 0;
    }
    arg0->field_A  = 0;
    arg0->field_14 = 0;
    arg0->field_16 = 0;
    arg0->field_C  = 0;
    if (Mc_SaveData[0].cursorMode != 0) {
        arg0->field_10 = 0;
        arg0->field_9  = 0;
    }
}

/// Fills the inside of a w x h box at (x, y), relative to the panel's origin,
/// with a flat tile in the ordering-table slot after the panel's. Nothing is
/// drawn for a zero colour or a box too narrow to have an inside.
static inline void _uiFillTile(UiPanel* panel, s32 x, s32 y, s32 w, s32 h, u32 color)
{
    TILE* p;
    s32   top;

    if (color != 0 && w >= 2) {
        p                     = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor        = p + 1;
        p->x0                 = panel->field_20 + x + 1;
        top                   = panel->field_22;
        p->w                  = w - 1;
        p->h                  = h - 1;
        PRIM_COLOR_WORD(p, 0) = color;
        setlen(p, 3);
        p->y0 = top + y + 1;
        setcode(p, 0x60);
        addPrim(gGpuCurrentOt + (s16)panel->field_14 + 1, p);
    }
}

void func_80046B34(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    LINE_F3* l;
    u16      t;

    _uiFillTile(arg0, arg1, arg2, arg3, arg4, arg5);

    l                     = (LINE_F3*)gGpuPrimCursor;
    l->x2                 = arg0->field_20 + arg1 + 1;
    t                     = arg0->field_20 + (arg1 + arg3);
    l->x1                 = t;
    l->x0                 = t;
    gGpuPrimCursor        = l + 1;
    l->y0                 = arg0->field_22 + arg2;
    t                     = arg0->field_22 + (arg2 + arg4);
    l->y2                 = t;
    l->y1                 = t;
    PRIM_COLOR_WORD(l, 0) = ((arg6 & 1) == 0) ? PRIM_RGBC(0x58, 0x60, 0x50, 0) : PRIM_RGBC(0x10, 0x18, 0x10, 0);
    setLineF3(l);
    addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, l);

    l                     = (LINE_F3*)gGpuPrimCursor;
    t                     = arg0->field_20 + arg1;
    l->x1                 = t;
    l->x2                 = t;
    l->x0                 = arg0->field_20 + (arg1 + arg3) - 1;
    gGpuPrimCursor        = l + 1;
    t                     = arg0->field_22 + arg2;
    l->y1                 = t;
    l->y0                 = t;
    l->y2                 = arg0->field_22 + (arg2 + arg4);
    PRIM_COLOR_WORD(l, 0) = ((arg6 & 1) == 0) ? PRIM_RGBC(0x10, 0x18, 0x10, 0) : PRIM_RGBC(0x58, 0x60, 0x50, 0);
    setLineF3(l);
    addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, l);
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

static void Ui_DrawListHighlight(UiList* arg0, UiPanel* arg1, s32 arg2, s32 arg3)
{
    UiPanelSignedLayout* a1;
    s32                  h;
    s32                  x1;

    a1 = (UiPanelSignedLayout*)arg1;
    h  = arg0->field_7;
    x1 = a1->field_1C;
    a1->field_14++;
    _uiFillTile(arg1, x1, arg2 - h, a1->field_1E - x1 - 1, h, 0x1741F);
    a1->field_14--;
}

/// Eases the list cursor a quarter of the way toward (x, y) once per elapsed
/// tick, in 24.8 fixed point, and draws it at the result.
static inline void _uiListMoveCursor(UiPanelRender* panel, s32 x, s32 y)
{
    s32 i;
    s16 baseX;
    s16 baseY;
    s32 targetX;
    s32 targetY;
    u8  ticks;

    i         = 0;
    baseX     = panel->field_20;
    baseY     = panel->field_22;
    targetX   = x + baseX;
    targetY   = y + baseY;
    targetX <<= 8;
    targetY <<= 8;
    ticks     = gDisplayState.frameTicks;
    if (ticks != 0) {
        do {
            i++;
            D_80067648 += (targetX - D_80067648) >> 2;
            D_8006764C += (targetY - D_8006764C) >> 2;
        } while (i < ticks);
    }
    targetX = D_80067648 >> 8;
    targetY = D_8006764C >> 8;
    Ui_DrawCursor((UiPanel*)panel, targetX - panel->field_20, targetY - panel->field_22);
}

static void func_80046EEC(UiListRender* arg0, UiPanelRender* arg1, s32 arg2)
{
    s32 step;
    s32 playSound;
    s32 highlight;
    s32 itemData;
    s32 margin;
    s32 rowY;
    s32 highlightY;
    s32 cursorX;
    s32 cursorY;
    s32 rows;
    s32 item;
    s32 i;
    s32 inset;
    s32 h;
    s32 state;
    s32 sound;
    s32 center;
    s32 rowH;

    cursorY   = 0;
    step      = 0;
    playSound = 0;
    highlight = 0;
    margin    = arg0->field_5 >> 2;
    itemData  = D_80067640;
    if (margin < 2) {
        margin = 0;
    }
    arg0->field_20 = 0;
    arg0->field_22 = 0;
    arg0->field_18 = arg1->field_1C + 2;
    state          = arg1->state.w;
    if (state >= 2) {
        switch (state) {
            case 19:
                arg0->field_10 = arg0->field_9;
                break;
            case 18:
                arg0->field_10 = arg0->field_9 + arg0->field_5 - 1;
                break;
        }
    }
    if (arg0->field_10 < 0) {
        arg0->field_10 += arg0->field_4;
    }
    rows = arg0->field_5;
    if (rows < arg0->field_4) {
        if (arg0->field_6 != 0 || arg0->field_9 > 0) {
            Ui_DrawCaret((UiList*)arg0, (UiPanel*)arg1, 0);
        }
        if (arg0->field_6 != 0 || arg0->field_9 + arg0->field_5 < arg0->field_4) {
            Ui_DrawCaret((UiList*)arg0, (UiPanel*)arg1, 1);
        }
        arg0->field_1A = arg1->field_18 + arg0->field_7;
        if (arg0->field_14 > 0) {
            arg0->field_14 -= gDisplayState.frameTicks * 2;
            if (arg0->field_14 <= 0) {
                arg0->field_14 = 0;
                if (arg0->field_16 == 1) {
                    arg0->field_9++;
                    if (arg0->field_9 >= arg0->field_4) {
                        arg0->field_9 -= arg0->field_4;
                    }
                }
                arg0->field_16 = 0;
            } else {
                if (arg0->field_16 == 1) {
                    s32 top         = arg0->field_1A + 7;
                    cursorY         = top - arg0->field_7 + (arg0->field_5 - 1) * arg0->field_7;
                    arg0->field_1A -= arg0->field_7 - arg0->field_14;
                } else {
                    s32 top         = arg0->field_1A + 7;
                    cursorY         = top - arg0->field_7;
                    arg0->field_1A -= arg0->field_14;
                }
                rows++;
            }
        }
    } else {
        arg0->field_1A = arg1->field_18 + arg0->field_7;
    }
    arg0->field_1A += arg0->field_17;
    highlightY      = arg0->field_1A;
    rowY            = highlightY;
    if (arg0->field_4 == 0) {
        s32 top = highlightY + 7;

        cursorX = arg0->field_18 - 2;
        cursorY = top - arg0->field_7;
        _uiListMoveCursor(arg1, cursorX, cursorY);
        return;
    }
    if (arg0->field_14 != 0) {
        Ui_SetListClip((UiList*)arg0, (UiPanel*)arg1, 1);
    }
    item = arg0->field_9;
    for (i = 0; i < rows; i++) {
        if (item == arg0->field_10) {
            if (arg0->field_16 == 0) {
                if (arg1->state.w == 1) {
                    arg0->field_C  = 1;
                    highlight      = 1;
                    arg0->field_1C = itemData;
                    highlightY     = rowY;
                } else {
                    arg0->field_C  = 0;
                    arg0->field_1C = itemData;
                }
            }
            h       = arg0->field_7;
            center  = rowY - (h - 1) / 2;
            cursorY = center - 1;
            if (h == 8) {
                cursorY = center - 2;
            }
        } else {
            arg0->field_C  = 0;
            arg0->field_1C = itemData;
        }
        inset         = 0;
        rowH          = arg0->field_7;
        arg0->field_8 = item;
        if (rowH == 10) {
            inset = 3;
        } else if (rowH < 10) {
            inset = 2;
        } else if (rowH >= 16) {
            inset = rowH - 15;
        }
        arg0->field_1A = rowY - inset;
        if (arg0->field_A & 1) {
            arg0->funcs[0](arg0, arg1);
        } else {
            arg0->funcs[item](arg0, arg1);
        }
        if (item == arg0->field_10 && arg0->field_22 == 0x41) {
            highlight = 0;
        }
        item++;
        rowY  = arg0->field_1A + inset;
        rowY += arg0->field_7;
        if (item >= arg0->field_4) {
            item -= arg0->field_4;
        }
    }
    if (highlight == 1 && arg0->field_7 != 0x2E) {
        Ui_DrawListHighlight((UiList*)arg0, (UiPanel*)arg1, highlightY, 0);
    }
    cursorX = arg0->field_18 - 2;
    if (arg0->field_14 != 0) {
        Ui_SetListClip((UiList*)arg0, (UiPanel*)arg1, 0);
    } else if (arg1->state.w == 1) {
        if (arg0->field_22 == 0 && Pad_CheckButtons(arg2, 0, 0xA000) == 0) {
            if (Pad_CheckButtons(arg2, 1, 0x1000) != 0) {
                playSound       = 1;
                arg0->field_B   = -1;
                step            = -1;
                arg0->field_10 -= 1;
            } else if (Pad_CheckButtons(arg2, 1, 0x4000) != 0) {
                playSound       = 1;
                step            = 1;
                arg0->field_10 += 1;
                arg0->field_B   = 1;
            } else if (arg0->field_5 < arg0->field_4 && arg0->field_6 == 0) {
                if (Pad_CheckButtons(0, 1, 4) != 0) {
                    if (arg0->field_10 != 0) {
                        playSound = 1;
                    }
                    arg0->field_B   = -1;
                    step            = -1;
                    arg0->field_10 -= 1;
                    if (arg0->field_9 > 0) {
                        arg0->field_9 -= arg0->field_5;
                        if (arg0->field_9 < 0) {
                            arg0->field_9 = 0;
                        }
                        if (arg0->field_9 + arg0->field_5 - 1 < arg0->field_10) {
                            arg0->field_10 = arg0->field_9 + arg0->field_5 - 1;
                        }
                    } else {
                        arg0->field_10 = 0;
                    }
                } else if (Pad_CheckButtons(0, 1, 8) != 0) {
                    if (arg0->field_10 != arg0->field_4 - 1) {
                        playSound = 1;
                    }
                    arg0->field_B   = 1;
                    step            = 1;
                    arg0->field_10 += 1;
                    if (arg0->field_9 + arg0->field_5 < arg0->field_4) {
                        arg0->field_9 += arg0->field_5;
                        if (arg0->field_9 > arg0->field_4 - arg0->field_5) {
                            arg0->field_9 = arg0->field_4 - arg0->field_5;
                        }
                        if (arg0->field_10 < arg0->field_9) {
                            arg0->field_10 = arg0->field_9;
                        }
                    } else {
                        arg0->field_10 = arg0->field_4 - 1;
                    }
                }
            }
        }
        if (arg0->field_22 == 0x41) {
            arg0->field_22 = 0;
            if (arg0->field_B == 0) {
                arg0->field_B = 1;
            }
            arg0->field_10 += arg0->field_B;
            step            = arg0->field_B;
        }
    }
    if ((arg1->state.w == 1 || arg1->state.h[1] == 1) && arg0->field_7 != 0x2E) {
        _uiListMoveCursor(arg1, cursorX, cursorY);
    }
    if (step == -1) {
        if (arg0->field_10 < 0) {
            if (arg0->field_6 != 0) {
                arg0->field_10 += arg0->field_4;
            } else {
                playSound      = 0;
                arg0->field_22 = 2;
                arg0->field_10 = 0;
                arg0->field_B  = 1;
            }
        }
        if (arg0->field_4 != arg0->field_5) {
            s32 edge = margin - 1;

            if (arg0->field_9 + edge >= arg0->field_10 % arg0->field_4) {
                if (arg0->field_6 != 0) {
                    arg0->field_9 -= 1;
                    if (arg0->field_9 < 0) {
                        arg0->field_9 += arg0->field_4;
                    }
                    arg0->field_16 = -1;
                    arg0->field_14 = arg0->field_7;
                } else {
                    arg0->field_9 -= 1;
                    if (arg0->field_9 < 0) {
                        arg0->field_9 = 0;
                    } else {
                        arg0->field_16 = -1;
                        arg0->field_14 = arg0->field_7;
                    }
                }
            }
        }
    } else if (step == 1) {
        if (arg0->field_10 >= arg0->field_4) {
            if (arg0->field_6 != 0) {
                arg0->field_10 -= arg0->field_4;
            } else {
                playSound      = 0;
                arg0->field_10 = arg0->field_4 - 1;
                arg0->field_22 = 3;
                arg0->field_B  = -1;
            }
        }
        if (arg0->field_4 != arg0->field_5) {
            if (arg0->field_10 % arg0->field_4 >= (arg0->field_9 + arg0->field_5 - margin) % arg0->field_4 && (arg0->field_6 != 0 || arg0->field_9 < arg0->field_4 - arg0->field_5)) {
                arg0->field_16 = 1;
                arg0->field_14 = arg0->field_7;
            }
        }
    }
    if (playSound != 0) {
        sound = 0x15;
        if (!(arg0->field_A & 2)) {
            sound = 2;
        }
        SndEvt_EnqueueType6(sound, 0, 0);
    }
}

void Ui_DrawHBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4* p;
    s32       y;

    if (arg1 < arg2) {
        p     = (POLY_FT4*)gGpuPrimCursor;
        p->x0 = p->x2  = arg0->field_20 + arg1;
        gGpuPrimCursor = (u8*)(p + 1);
        p->x1 = p->x3 = arg0->field_20 + arg2;
        y             = arg0->field_22 + arg3;
        p->y0 = p->y1 = y - 4;
        p->y2 = p->y3 = y + 3;
        setUV4(p, 0x68, 0x50, 0x6F, 0x50, 0x68, 0x57, 0x6F, 0x57);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 2, p);
    }
}

void Ui_DrawVBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4* p;
    s32       x;

    if (arg1 < arg2) {
        p     = (POLY_FT4*)gGpuPrimCursor;
        x     = arg0->field_20 + arg3;
        p->x0 = p->x2 = x - 3;
        p->x1 = p->x3  = x + 5;
        gGpuPrimCursor = (u8*)(p + 1);
        p->y0 = p->y1 = arg0->field_22 + arg1;
        p->y2 = p->y3 = arg0->field_22 + arg2;
        setUV4(p, 0x70, 0x50, 0x77, 0x50, 0x70, 0x57, 0x77, 0x57);
        p->tpage = 0x1E;
        p->clut  = 0x3C03;
        setPolyFT4(p);
        setShadeTex(p, 1);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 2, p);
    }
}

static void Ui_DrawTextUnderline(UiPanel* arg0, s32 x, s32 y, char* arg3, s32 arg4)
{
    TextDrawReq req;
    POLY_F4*    p;
    s16         textX;
    s32         otIdx;

    otIdx          = (s16)arg0->field_14 + 1;
    x             += (s16)arg0->field_20;
    y             += (s16)arg0->field_22;
    req.x          = x + 2;
    req.y          = y + 5;
    req.otIndex    = otIdx;
    req.field_8    = arg4;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    func_8002E53C(&req, (u8*)arg3);

    p     = (POLY_F4*)gGpuPrimCursor;
    p->x0 = p->x2         = x;
    textX                 = req.x;
    gGpuPrimCursor        = (u8*)((POLY_FT4*)p + 1);
    PRIM_COLOR_WORD(p, 0) = PRIM_RGBC(0x02, 0x10, 0x02, 0);
    p->y2 = p->y3 = y + 7;
    setPolyF4(p);
    p->y0 = p->y1 = y;
    p->x3         = textX;
    p->x1         = textX + 3;
    addPrim(gGpuCurrentOt + otIdx, p);

    Ui_DrawHBar(arg0, x - (s16)arg0->field_20, req.x - (s16)arg0->field_20, y + 7 - (s16)arg0->field_22);
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

/// Spawns a UiObject from a descriptor, with a task that runs it and frees it
/// on exit, optionally as a child of `parent`'s task. Returns NULL, leaving
/// no task behind, when either allocation fails.
static inline UiObject* _uiSpawnObject(UiObjectDesc* arg0, s32 arg1, s32 arg2, s32 arg3, UiObject* parent)
{
    TaskDesc  desc;
    Task*     task;
    UiObject* obj;
    s32       field_8;

    obj            = NULL;
    desc.flags     = arg0->field_10;
    desc.priority  = arg0->field_12;
    field_8        = arg0->field_18;
    desc.callback  = Ui_DispatchObjectState;
    desc.arg.value = field_8;
    task           = Task_SpawnFromTable(&desc, (s32)obj, arg1, (s32)obj);
    if (task != NULL) {
        obj = (UiObject*)memCalloc(0x30, (s32)obj);
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
            if (parent != NULL) {
                Task_Reparent(parent->owner, task);
            }
        } else {
            taskKill(task);
        }
    }
    return obj;
}

UiObject* Ui_SpawnTextBlock(TextBlockDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    UiObject*     obj;
    TextLineNode* node;
    s32           count;
    s32           maxWidth;
    s32           width;

    obj = NULL;
    if (arg0->count > 0) {
        obj = _uiSpawnObject(&D_80067678, (s32)arg0, 1, 1, NULL);
        if (obj != NULL) {
            RECT rect;

            count    = arg0->count;
            node     = arg0->lines;
            maxWidth = 0;
            if (arg0->field_8 == 0) {
                obj->field_4 = 3;
            }
            for (; count > 0; count--) {
                width = Text_MeasureWidth(node->text);
                if (maxWidth < width) {
                    maxWidth = width;
                }
                node = node->next;
            }
            Ui_InsetRect2(obj, (RECT*)&obj->field_C, &rect);
            if ((obj->field_4 & 0xF) == 2) {
                rect.y += 9;
                rect.h -= 0xB;
                rect.x += 2;
                rect.w -= 4;
            } else {
                rect.y += 2;
                rect.h -= 4;
                rect.x += 2;
                rect.w -= 4;
            }
            obj->field_1C = -(rect.w >> 1);
            obj->field_1E = obj->field_1C + rect.w;
            obj->field_18 = -(rect.h >> 1);
            obj->field_1A = obj->field_18 + rect.h;
            obj->baseX    = rect.x - obj->field_1C;
            obj->baseY    = rect.y - obj->field_18;

            // Grow the panel so the widest line and every line fit inside.
            maxWidth     -= (s16)obj->field_1E - obj->field_1C;
            obj->field_10 = obj->field_10 + maxWidth + 0xC;
            obj->field_C  = -((s16)obj->field_10 / 2);
            maxWidth      = arg0->count * 0xF;
            maxWidth     -= (s16)obj->field_1A - (s16)obj->field_18;
            obj->field_12 = obj->field_12 + maxWidth;
            obj->field_E  = -((s16)obj->field_12 / 2);
        }
    }
    arg0->field_2 = 0;
    return obj;
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
    return _uiSpawnObject(arg0, arg1, arg2, arg3, arg4);
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
        memFree(arg0->spawnArg2);
    }
    taskKill(arg0);
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
    if (arg0->field_10 >= arg0->field_4) {
        arg0->field_10 = arg0->field_4 - 1;
    }
    if (arg0->field_4 <= (s8)arg0->field_5) {
        arg0->field_9 = 0;
    }
    arg0->field_A  = 0;
    arg0->field_14 = 0;
    arg0->field_16 = 0;
    arg0->field_C  = 0;
    if (Mc_SaveData[0].cursorMode != 0) {
        arg0->field_10 = 0;
        arg0->field_9  = 0;
    }
}

void Ui_ComputeVisibleRows(UiList* arg0, UiPanel* arg1)
{
    RECT sp;
    s32  height;

    sp.x    = arg1->field_20 + arg1->field_1C;
    sp.y    = arg1->field_22 + arg1->field_18;
    sp.w    = arg1->field_1E - arg1->field_1C;
    sp.h    = arg1->field_1A - arg1->field_18;
    height  = sp.h;
    height -= arg0->field_17;
    if (arg0->field_7 == 0) {
        arg0->field_7 = 0xA;
    }
    if (height >= arg0->field_4 * arg0->field_7) {
        arg0->field_5 = arg0->field_4;
    } else {
        arg0->field_5 = height / arg0->field_7;
        if ((s8)arg0->field_5 <= 0) {
            arg0->field_5 = 1;
        }
    }
    if (arg0->field_10 >= arg0->field_4) {
        arg0->field_10 = arg0->field_4 - 1;
    }
    if (arg0->field_4 <= (s8)arg0->field_5) {
        arg0->field_9 = 0;
    }
    arg0->field_A = 0;
}

void Ui_UpdateListNoAnim(void* arg0, void* arg1)
{
    func_80046EEC(arg0, arg1, 0);
}

static void Ui_ComputeVisibleRowsEx(UiList* arg0, UiPanel* arg1, s32 arg2)
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
    count     = gDisplayState.frameTicks;
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

static void Ui_DrawTextAtLayout(UiPanel* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
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

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 0, 1, 0x1E | ((arg1 & 3) << 5));
    addPrim(gGpuCurrentOt + arg0, p);
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
        p                     = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor        = p + 1;
        p->x0                 = arg0->field_20 + arg1 + 1;
        y                     = arg0->field_22;
        p->w                  = arg3 - 1;
        p->h                  = arg4 - 1;
        PRIM_COLOR_WORD(p, 0) = color;
        setlen(p, 3);
        p->y0 = y + arg2 + 1;
        setcode(p, 0x60);
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, p);
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

static void Ui_InsetRect2(void* arg0, RECT* arg1, RECT* arg2)
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

static void Ui_ComputeAnimRect(UiPanel* arg0, RECT* arg1)
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

static void Ui_AnimOpenStep(UiPanel* arg0, void* arg1)
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

static void Ui_DrawAndCallback(UiPanel* arg0, void* arg1)
{
    s32 temp_s2;

    temp_s2       = arg0->field_0;
    arg0->field_0 = temp_s2 << 0x10;
    Ui_LayoutAndClip(arg0);
    arg0->field_24(arg1);
    arg0->field_16 -= gDisplayState.frameTicks;
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

static void Ui_LayoutDrawAndCallback(UiPanel* arg0, void* arg1)
{
    Ui_LayoutAndDraw(arg0);
    arg0->field_24(arg1);
}

static void Ui_TickAnimCounter(UiPanel* arg0, void* arg1)
{
    if (arg0->field_16 >= 0) {
        arg0->field_16 += gDisplayState.frameTicks;
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

static void Ui_AnimCloseStep(UiPanel* arg0, void* arg1)
{
    s32 temp_s1;

    temp_s1 = arg0->field_0;
    if (arg0->field_16 >= 0) {
        arg0->field_16 += gDisplayState.frameTicks;
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

static void Ui_ClipAndCallback(UiPanel* arg0, void* arg1)
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
        temp_v0        = (u16)arg0->field_16 - gDisplayState.frameTicks;
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

static void Ui_DispatchObjectState(Task* arg0)
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
    POLY_F3* p;
    u16      t;
    u32*     ot;

    p              = (POLY_F3*)gGpuPrimCursor;
    t              = arg0->field_20 + arg1;
    p->x2          = t;
    p->x1          = t;
    p->x0          = t;
    gGpuPrimCursor = (POLY_G3*)p + 1;
    t              = arg0->field_22 + arg2;
    p->y2          = t;
    p->y1          = t;
    p->y0          = t;
    if (arg4 == 0) {
        p->x1 = p->x1 - 4;
        t     = p->y0 + 5;
        p->x2 = p->x2 + 5;
        p->y2 = t;
        p->y1 = t;
    } else {
        p->x1 = p->x1 - 3;
        t     = p->y0 - 4;
        p->x2 = p->x2 + 4;
        p->y2 = t;
        p->y1 = t;
    }
    PRIM_COLOR_WORD(p, 0) = arg3 * 2;
    setlen(p, 4);
    setcode(p, 0x20);
    ot = gGpuCurrentOt;
    addPrim(&ot[(s16)arg0->field_14 + 1], p);
}

void Ui_WaitCdThenOverlay(Task* arg0)
{
    UiPanel* temp_s0;

    temp_s0 = arg0->spawnArg2;
    if (CdCmd_IsIdle() != 0) {
        func_801D4B64(arg0);
        return;
    }
    temp_s0->field_16 += gDisplayState.frameTicks;
}

static void Ui_DrawDialogLine(DialogPrompt* arg0, UiObject* arg1)
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

static void Ui_ListTaskCallback(Task* arg0)
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
    func_80046EEC((UiListRender*)menu, (UiPanelRender*)obj, 0);
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
