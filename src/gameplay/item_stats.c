#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "attachments.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/message.h"
#include "weapon_data.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

/// Pixel size of the item preview picture, laid out as a GTE short vector so
/// `gte_gpf12` can scale both dimensions in one pass.
typedef struct {
    u16 width;  // on-screen width in pixels
    u16 height; // on-screen height in pixels
    u16 depth;  // third GTE lane; always 0, so scaling leaves it 0
} _ItemMenuPreviewSize;

#define D_8010EF68 D_8010EAB4[43]

/// Draws `item` as `_gpDrawItemNameUnmarkedAt` does, but fills the caller's
/// `req` instead of a request of its own.
static inline void _gpDrawItemNameUnmarkedInto(UiObject* obj, TextDrawReq* req, s32 x, s32 y, s32 color,
                                               s32 item);

WeaponAttackRow Gp_IdParamLo[47] = {
    { 0, 0, 0, 0, 0 },
    { 10, 0, 0, 1, 0 },
    { 15, 0, 0, 1, 0 },
    { 20, 0, 0, 1, 0 },
    { 9999, 0, 0, 0, 0 },
    { 9999, 0, 0, 0, 0 },
    { 9999, 0, 0, 0, 0 },
    { 40, 0, 0, 1, 0 },
    { 70, 0, 3, 2, 0 },
    { 999, 0, 3, 2, 0 },
    { 270, 0, 4, 3, 10 },
    { 220, 0, 6, 4, 3 },
    { 60, 0, 9, 0, 10 },
    { 40, 0, 6, 4, 0 },
    { 70, 0, 7, 3, 0 },
    { 90, 0, 5, 5, 0 },
    { 22, 0, 0, 1, 0 },
    { 1, 0, 2, 0, 0 },
    { 1, 0, 9, 0, 0 },
    { 9999, 0, 0, 0, 0 },
    { 9999, 0, 0, 0, 0 },
    { 10, 0, 0, 6, 11 },
    { 0, 0, 8, 0, 0 },
    { 10, 0, 0, 6, 8 },
    { 1500, 0, 6, 4, 9 },
    { 2000, 0, 7, 3, 9 },
    { 2000, 0, 1, 7, 10 },
    { 100, 0, 1, 8, 9 },
    { 80, 0, 1, 15, 0 },
    { 90, 0, 0, 8, 10 },
    { 35, 0, 7, 16, 5 },
    { 25, 0, 0, 10, 5 },
    { 2500, 0, 5, 5, 9 },
    { 60, 0, 6, 4, 2 },
    { 80, 0, 6, 4, 2 },
    { 100, 0, 6, 4, 2 },
    { 15, 0, 7, 3, 5 },
    { 30, 0, 1, 0, 1 },
    { 45, 0, 1, 10, 0 },
    { 80, 0, 1, 10, 0 },
    { 45, 0, 2, 7, 0 },
    { 45, 0, 1, 10, 0 },
    { 12, 0, 0, 1, 0 },
    { 40, 0, 1, 1, 1 },
    { 60, 0, 1, 1, 1 },
    { 60, 0, 7, 3, 1 },
    { 100, 0, 7, 3, 1 },
};

/// Shows `item`'s name in the holder (the empty-slot text for item 0) and
/// makes it the preview in slot 0.
#define GP_SHOW_ITEM_IN_HOLDER(item)                               \
    do {                                                           \
        if ((item) == 0) {                                         \
            Ui_SetHolderParam(Gp_StrEmpty, 0, 0);                  \
        } else {                                                   \
            Ui_SetHolderParam(Gp_GetItemText((item), 1, 0), 0, 0); \
        }                                                          \
        Gp_SetPreviewItem((item), 0);                              \
    } while (0)

/// Draws `item` as `_gpDrawItemNameUnmarkedAt` does, but fills the caller's
/// `req` instead of a request of its own.
static inline void _gpDrawItemNameUnmarkedInto(UiObject* obj, TextDrawReq* req, s32 x, s32 y, s32 color,
                                               s32 item)
{
    s32 temp;

    if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req->x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
        req->y          = obj->panel.contentOriginY.unsignedValue + (y - 6);
        req->otIndex    = obj->panel.otIndex.signedValue + 1;
        req->colorRgb   = color;
        req->glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req->alignment  = TEXT_ALIGNMENT_LEFT;
        req->drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(req, Gp_GetItemText(item, 0, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }
}

void func_800C5F70(Task* arg0)
{
    TextDrawReq      req20;
    TextDrawReq      req30;
    u8               buf40[0x20];
    TextDrawReq      req60;
    TextDrawReq      req70;
    TextDrawReq      req80;
    TextDrawReq      req90;
    TextDrawReq      reqA0;
    TextDrawReq      reqB0;
    u8               bufC0[0x20];
    u8               bufE0[0x20];
    TextDrawReq      req100;
    TextDrawReq      req110;
    TextDrawReq      req120;
    TextDrawReq      req130;
    TextDrawReq      req140;
    TextDrawReq      req150;
    s32              ready;
    u32              flags;
    UiList*          menu;
    UiObject*        obj;
    s32              item;
    s32              featCount;
    s32              altColor;
    s32              state;
    s32              lines;
    const u8*        p;
    u8*              payload;
    s32              y;
    s32              i;
    s32              spriteCount;
    s32              h;
    s32              x;
    SPRT*            sprt;
    s32              saved;
    ArmorStats*      attr;
    u8**             names;
    u8*              text;
    WeaponAttackRow* rec;
    WeaponAttackRow* recBase;
    s32              idx;
    s32              temp;
    s32              caliber;
    s32              recIndex;
    s32              spriteW;
    s32              textColor;
    s32              featIndex;
    s32              spriteI;
    s32              baseY;
    s32              spriteMode;
    const ItemDesc*  descBase;
    const ItemDesc*  desc;

    ready = 0;
    flags = ready;
    menu  = &D_8010E910;
    temp  = arg0->spawnArg1.value;
    obj   = arg0->spawnArg2.pointer;
    item  = temp & 0xFFFF;
    if (temp & 0x10000) {
        flags = 2;
    } else if (temp & 0x40000) {
        flags = 1;
    }
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        if ((D_80067634 != NULL) && (D_80067634 != obj)) {
            taskCallExit(D_80067634->owner);
            obj->panel.animationTicks = 0x14;
        }
        D_80067634         = obj;
        arg0->exitCallback = func_800CF330;
        arg0->state        = 2;
        if ((item < 0x100) || (item == 0x10C) || (item == 0x113) || (item == 0x114) ||
            (item == 0x11B) || (item == 0x11F) || (item == 0x125) || (item == 0x127) ||
            (item == 0x128) || (item == 0x129) || (item == 0x107)) {
            Gp_SetItemSeenBit(item, 1);
        }
        if (item == 0x125) {
            gameFlagSetNibble(GAME_FLAG_ITEM_125_EXAMINED, 1);
        }
    } else {
        if (arg0->spawnArg1.value & 0x20000) {
            uiDrawPanelLabel(&(obj)->panel, Gp_StrNextReplay);
        } else {
            uiDrawPanelLabel(&(obj)->panel, Gp_StrSpecs);
        }
        if (obj->panel.state == USER_INTERFACE_PANEL_OPEN) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
        }
        state = arg0->state;
        if (state == 2) {
            goto parse;
        } else if (state < 3) {
            goto hbar_setup;
        } else if (state == 3) {
            goto state3;
        } else if (state == 4) {
            goto state4;
        } else {
            goto hbar_setup;
        }
    parse:
        if (CdCmd_IsIdle() & 0xFFFF) {
            lines = 0;
            p     = textSkipLines(Fs_GetChunkPayload(), 5);
            while (*p != 0) {
                if (*p == '\\') {
                    p++;
                    if (*p == 'Z' || *p == 'z') {
                        break;
                    }
                }
                if (*p == '\n') {
                    lines++;
                }
                p++;
            }
            if (item < 0x100) {
                if (lines < 2) {
                    lines = 2;
                }
            }
            menu->selectedItemIndex                   = 0;
            menu->firstVisibleItemIndex.unsignedValue = 0;
            menu->visibleRowCount.unsignedValue       = lines;
            menu->itemCount                           = lines;
            Ui_InitList(menu, &(obj)->panel);
            if (menu->visibleRowCount.signedValue >= 7) {
                menu->visibleRowCount.unsignedValue = 6;
            }
            menu->flags    = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
            menu->topInset = -(u8)obj->panel.contentTop.unsignedValue + 7;
            arg0->state    = 3;
        }
        goto hbar_setup;
    state3:
        arg0->state = 4;
    state4:
        if (CdCmd_IsIdle() & 0xFFFF) {
            ready = 1;
        }
    hbar_setup:
        if (item >= 0x500) {
            flags |= 0x400;
            uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, 0x25);
        } else {
            uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, 5);
        }
        if (ready == 1) {
            x       = 2;
            payload = Fs_GetChunkPayload();
            y       = obj->panel.contentTop.signedValue + 0x1E;
            for (i = 0; i < 5; i++) {
                req20.x          = obj->panel.contentOriginX.unsignedValue + x;
                req20.y          = obj->panel.contentOriginY.unsignedValue + y;
                y               += 0xF;
                req20.otIndex    = obj->panel.otIndex.signedValue + 1;
                req20.colorRgb   = 0x606060;
                req20.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                req20.alignment  = TEXT_ALIGNMENT_LEFT;
                req20.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&req20, textSkipLines(payload, i));
            }
            if (item >= 0x500) {
                textDrawUiLines(obj, obj->panel.contentLeft.signedValue + 2, 0x34, textSkipLines(payload, 5),
                                0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
            } else {
                saved                   = obj->panel.control.word;
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                Ui_UpdateListNoAnim(menu, obj);
                obj->panel.control.word = saved;
                if ((saved == 1) && (menu->itemCount > menu->visibleRowCount.signedValue)) {
                    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
                        menu->firstVisibleItemIndex.unsignedValue = menu->firstVisibleItemIndex.unsignedValue - 1;
                        if (menu->firstVisibleItemIndex.signedValue < 0) {
                            menu->firstVisibleItemIndex.unsignedValue = 0;
                        } else {
                            menu->scrollDirection       = USER_INTERFACE_LIST_STEP_PREVIOUS;
                            menu->scrollPixelsRemaining = menu->rowHeight;
                        }
                        menu->selectedItemIndex = menu->firstVisibleItemIndex.signedValue;
                    } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
                        menu->selectedItemIndex = menu->firstVisibleItemIndex.signedValue + menu->visibleRowCount.signedValue;
                        if (menu->selectedItemIndex < menu->itemCount) {
                            menu->scrollDirection       = saved;
                            menu->scrollPixelsRemaining = menu->rowHeight;
                        } else {
                            menu->selectedItemIndex = menu->itemCount - 1;
                        }
                    } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L1) != 0) {
                        if (menu->firstVisibleItemIndex.signedValue > 0) {
                            menu->firstVisibleItemIndex.unsignedValue = menu->firstVisibleItemIndex.unsignedValue - menu->visibleRowCount.unsignedValue;
                            if (menu->firstVisibleItemIndex.signedValue < 0) {
                                menu->firstVisibleItemIndex.unsignedValue = 0;
                            }
                            menu->selectedItemIndex = menu->firstVisibleItemIndex.signedValue;
                        }
                    } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_R1) != 0) {
                        if (menu->firstVisibleItemIndex.signedValue < (menu->itemCount - menu->visibleRowCount.signedValue)) {
                            menu->firstVisibleItemIndex.unsignedValue += menu->visibleRowCount.unsignedValue;
                            if (menu->firstVisibleItemIndex.signedValue > (menu->itemCount - menu->visibleRowCount.signedValue)) {
                                menu->firstVisibleItemIndex.unsignedValue = menu->itemCount - menu->visibleRowCount.unsignedValue;
                            }
                            menu->selectedItemIndex = (menu->firstVisibleItemIndex.signedValue + menu->visibleRowCount.signedValue) - 1;
                        }
                    }
                }
            }
            func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);
            if ((u32)(item - 0x80) < 0x20U) {
                spriteCount = 1;
                if ((Gp_GetItemSlot(item)->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) || (item == 0x8F) || (item == 0x93) ||
                    (item == 0x94) || (item == 0x96) || (item == 0x99) || (item == 0x81)) {
                    spriteCount = 2;
                }
                y       = 0x4E;
                h       = 0xF;
                spriteW = h;
                spriteI = 0;
                x       = obj->panel.contentLeft.signedValue + 2;
                if (spriteCount != 0) {
                    do {
                        sprt           = gGpuPrimCursor;
                        gGpuPrimCursor = sprt + 1;
                        sprt->x0       = x;
                        sprt->y0       = y - 8;
                        spriteMode     = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
                        if (spriteMode != 2) {
                            h        = 8;
                            sprt->y0 = y - 4;
                            if (spriteI == 0) {
                                sprt->u0 = 0x90;
                                sprt->v0 = 0x58;
                            } else {
                                sprt->u0 = 0xB0;
                                sprt->v0 = 0x58;
                            }
                        } else if (spriteI == 0) {
                            sprt->u0 = 0x10;
                            sprt->v0 = 0x60;
                        } else {
                            sprt->u0 = 0x20;
                            sprt->v0 = 0x70;
                        }
                        sprt->clut = 0x3C00;
                        setlen(sprt, 4);
                        y      += 0xF;
                        sprt->w = spriteW;
                        sprt->h = h;
                        setcode(sprt, 0x65);
                        addPrim(gGpuCurrentOt + obj->panel.otIndex.signedValue + 1, sprt);
                        spriteI++;
                    } while (spriteI < spriteCount);
                }
                uiQueueTexturePage(obj->panel.otIndex.signedValue + 1, 0);
                x                = obj->panel.contentLeft.signedValue + 2;
                req30.x          = obj->panel.contentOriginX.unsignedValue + x;
                req30.y          = obj->panel.contentOriginY.unsignedValue + 0x40;
                req30.otIndex    = obj->panel.otIndex.signedValue + 1;
                req30.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                req30.colorRgb   = 0x606060;
                req30.alignment  = TEXT_ALIGNMENT_LEFT;
                req30.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&req30, Gp_StrOperation);
            } else if ((u32)(item - 0x60) < 0x20U) {
                attr      = &Gp_ModStatAttrs[(item)-0x60];
                featCount = 0;
                altColor  = 0x808008;
                x         = 2;
                text      = Gp_StrAddHp;
                SOFT_TOUCH_REG_USE(text, attr);
                flags            = (u32)attr->features;
                y                = obj->panel.contentTop.signedValue + 0x1E;
                req30.x          = obj->panel.contentOriginX.unsignedValue + x;
                req30.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
                req30.otIndex    = obj->panel.otIndex.signedValue + 1;
                req30.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                req30.colorRgb   = 0x606060;
                req30.alignment  = TEXT_ALIGNMENT_LEFT;
                req30.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&req30, text);
                if (attr->hpBonus == 0) {
                    req60.x          = obj->panel.contentOriginX.unsignedValue + 0x78;
                    req60.y          = obj->panel.contentOriginY.unsignedValue + y;
                    req60.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req60.colorRgb   = 0x606060;
                    req60.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    req60.alignment  = TEXT_ALIGNMENT_RIGHT;
                    req60.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&req60, D_8009707C);
                } else {
                    req60.x          = obj->panel.contentOriginX.unsignedValue + 0x7A;
                    req60.y          = obj->panel.contentOriginY.unsignedValue + y;
                    req60.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req60.colorRgb   = altColor;
                    req60.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    req60.alignment  = TEXT_ALIGNMENT_RIGHT;
                    req60.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&req60, textItoaSignPrefixed(buf40, attr->hpBonus));
                }
                y += 0xF;

                req60.x          = obj->panel.contentOriginX.unsignedValue + x;
                req60.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
                req60.otIndex    = obj->panel.otIndex.signedValue + 1;
                req60.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                req60.colorRgb   = 0x606060;
                req60.alignment  = TEXT_ALIGNMENT_LEFT;
                req60.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&req60, Gp_StrAddMp);
                if (attr->mpBonus == 0) {
                    req70.x          = obj->panel.contentOriginX.unsignedValue + 0x76 + x;
                    req70.y          = obj->panel.contentOriginY.unsignedValue + y;
                    req70.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req70.alignment  = TEXT_ALIGNMENT_RIGHT;
                    req70.colorRgb   = 0x606060;
                    req70.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    req70.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&req70, D_8009707C);
                } else {
                    req70.x          = obj->panel.contentOriginX.unsignedValue + 0x78 + x;
                    req70.y          = obj->panel.contentOriginY.unsignedValue + y;
                    req70.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req70.alignment  = TEXT_ALIGNMENT_RIGHT;
                    req70.colorRgb   = altColor;
                    req70.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    req70.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&req70, textItoaSignPrefixed(buf40, attr->mpBonus));
                }
                y += 0xF;

                req70.x          = obj->panel.contentOriginX.unsignedValue + x;
                req70.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
                req70.otIndex    = obj->panel.otIndex.signedValue + 1;
                req70.colorRgb   = 0x606060;
                req70.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                req70.alignment  = TEXT_ALIGNMENT_LEFT;
                req70.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&req70, Gp_StrAttachments3);
                featIndex        = 0;
                req80.x          = obj->panel.contentOriginX.unsignedValue + 0x78 + x;
                req80.y          = obj->panel.contentOriginY.unsignedValue + y;
                req80.otIndex    = obj->panel.otIndex.signedValue + 1;
                y               += 0xF;
                req80.alignment  = TEXT_ALIGNMENT_RIGHT;
                req80.colorRgb   = altColor;
                req80.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                req80.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                textDrawString(&req80, textItoaUnsigned(buf40, Gp_GetModLevel(item)));
                req90.x          = obj->panel.contentOriginX.unsignedValue + x;
                req90.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
                req90.otIndex    = obj->panel.otIndex.signedValue + 1;
                y               += 8;
                req90.colorRgb   = 0x606060;
                req90.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                req90.alignment  = TEXT_ALIGNMENT_LEFT;
                req90.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&req90, Gp_StrSpecialFeat);
                names = Gp_FeatNameTbl;
                do {
                    if (flags & 1) {
                        reqA0.x          = obj->panel.contentOriginX.unsignedValue + 8 + x;
                        reqA0.y          = obj->panel.contentOriginY.unsignedValue + y;
                        reqA0.otIndex    = obj->panel.otIndex.signedValue + 1;
                        reqA0.colorRgb   = 0x606060;
                        reqA0.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                        reqA0.alignment  = TEXT_ALIGNMENT_LEFT;
                        reqA0.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                        textDrawString(&reqA0, *names);
                        featCount++;
                        y += 0xB;
                        if (featCount >= 2) {
                            break;
                        }
                    }
                    flags >>= 1;
                    featIndex++;
                    names++;
                } while (featIndex < 0xD);
                x                = obj->panel.contentLeft.signedValue + 2;
                reqB0.x          = obj->panel.contentOriginX.unsignedValue + x;
                reqB0.y          = obj->panel.contentOriginY.unsignedValue + 0x40;
                reqB0.otIndex    = obj->panel.otIndex.signedValue + 1;
                reqB0.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                reqB0.colorRgb   = 0x606060;
                reqB0.alignment  = TEXT_ALIGNMENT_LEFT;
                reqB0.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&reqB0, Gp_StrSpecialFeat);
            } else {
                idx = item - 0xA0;
                if ((u32)idx < 0x20U) {
                    descBase = Gp_ItemDescs;
                    desc     = descBase + item;
                    TOUCH_REG(desc);
                    caliber          = desc->classification & ITEM_SUBTYPE_MASK;
                    baseY            = obj->panel.contentTop.signedValue;
                    reqB0.x          = obj->panel.contentOriginX.unsignedValue + 2;
                    reqB0.y          = obj->panel.contentOriginY.unsignedValue + baseY + 0x1C;
                    reqB0.otIndex    = obj->panel.otIndex.signedValue + 1;
                    reqB0.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                    textColor        = 0x606060;
                    reqB0.colorRgb   = textColor;
                    reqB0.alignment  = TEXT_ALIGNMENT_LEFT;
                    reqB0.drawMode   = TEXT_DRAW_OUTLINED;
                    textDrawString(&reqB0, Gp_CaliberNameTbl[caliber]);
                    recBase  = Gp_IdParamLo;
                    recIndex = item - 0x9F;
                    rec      = recBase + recIndex;
                    textItoaSigned(bufC0, rec->amount);
                    y                 = baseY + 0x2D;
                    req100.x          = obj->panel.contentOriginX.unsignedValue + 2;
                    req100.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
                    req100.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req100.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                    req100.colorRgb   = textColor;
                    req100.alignment  = TEXT_ALIGNMENT_LEFT;
                    req100.drawMode   = TEXT_DRAW_OUTLINED;
                    textDrawString(&req100, Gp_StrPowerCaps);
                    req110.x          = obj->panel.contentOriginX.unsignedValue + 0x4C;
                    req110.y          = obj->panel.contentOriginY.unsignedValue + y;
                    req110.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req110.colorRgb   = textColor;
                    req110.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    req110.alignment  = TEXT_ALIGNMENT_LEFT;
                    req110.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&req110, bufC0);
                    textItoaSigned(bufC0, Gp_ScanStackQty(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, item));
                    textItoaSigned(bufE0, Gp_StackLimits[idx].maxHeld);
                    textAppendString(bufC0, Gp_StrSlash);
                    textAppendString(bufC0, bufE0);
                    y                 = baseY + 0x3C;
                    req120.x          = obj->panel.contentOriginX.unsignedValue + 2;
                    req120.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
                    req120.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req120.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                    req120.colorRgb   = textColor;
                    req120.alignment  = TEXT_ALIGNMENT_LEFT;
                    req120.drawMode   = TEXT_DRAW_OUTLINED;
                    textDrawString(&req120, Gp_StrCapacity);
                    req130.x          = obj->panel.contentOriginX.unsignedValue + 0x4C;
                    req130.y          = obj->panel.contentOriginY.unsignedValue + y;
                    req130.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req130.colorRgb   = textColor;
                    req130.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                    req130.alignment  = TEXT_ALIGNMENT_LEFT;
                    req130.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                    textDrawString(&req130, bufC0);
                    y = baseY + 0x4B;
                    if (rec->hitReaction != 0) {
                        req140.x          = obj->panel.contentOriginX.unsignedValue + 2;
                        req140.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
                        req140.otIndex    = obj->panel.otIndex.signedValue + 1;
                        req140.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                        req140.colorRgb   = textColor;
                        req140.alignment  = TEXT_ALIGNMENT_LEFT;
                        req140.drawMode   = TEXT_DRAW_OUTLINED;
                        textDrawString(&req140, Gp_StrSpecial);
                        req150.x          = obj->panel.contentOriginX.unsignedValue + 0x4C;
                        req150.y          = obj->panel.contentOriginY.unsignedValue + y;
                        req150.otIndex    = obj->panel.otIndex.signedValue + 1;
                        req150.colorRgb   = 0xD287F;
                        req150.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                        req150.alignment  = TEXT_ALIGNMENT_LEFT;
                        req150.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                        textDrawString(&req150, D_8010E7C0[rec->hitReaction]);
                    }
                    x                 = obj->panel.contentLeft.signedValue + 2;
                    req140.x          = obj->panel.contentOriginX.unsignedValue + x;
                    req140.y          = obj->panel.contentOriginY.unsignedValue + 0x40;
                    req140.otIndex    = obj->panel.otIndex.signedValue + 1;
                    req140.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                    req140.colorRgb   = textColor;
                    req140.alignment  = TEXT_ALIGNMENT_LEFT;
                    req140.drawMode   = TEXT_DRAW_OUTLINED;
                    textDrawString(&req140, Gp_StrApplicableWpn);
                }
            }
        } else {
            func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags | 0x100);
        }
        _gpDrawItemNameUnmarkedInto(obj, &req30, 2, obj->panel.contentTop.signedValue + 0xF, 0x606060, item);
        if ((obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (CdCmd_IsIdle() & 0xFFFF)) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskConfirm | PAD_BUTTON_TRIANGLE) != 0) {
                if (!(arg0->spawnArg1.value & 0x20000)) {
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                }
                displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
                obj->result = USER_INTERFACE_RESULT_CANCEL;
            }
        }
    }
}

void Gp_UseKeyItemRow(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     roomTask;
    s32       item;
    s32       ret;
    s32       width;
    s32       other;
    s32       color;
    s32       one;
    u8*       text;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        menu     = &D_8010E960;
        roomTask = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
        item     = Gp_NthCollectedId(menu->selectedItemIndex, 0);
        ret      = taskMessageDispatch(roomTask, 0x13F1, item, 0);
        if (ret == 1) {
            arg0->spawnArg1.value = item;
            width                 = textMeasureLineWidth((const u8*)Gp_GetItemText(item, 0, 0)) + 0xB;
            other                 = textMeasureLineWidth((const u8*)Gp_StrUsed);
            if (width < other) {
                width = other;
            }
            uiSetPanelContentSize(&(obj)->panel, width + 5, uiGetTextRowsHeight(2) + 1);
            (&(obj)->panel)->bounds.rect.x = (-(&(obj)->panel)->bounds.rect.w) >> 1;
            obj->panel.style              &= (s32)~USER_INTERFACE_PANEL_NO_FRAME;
        } else if (ret == 2) {
            uiStartPanelHiding(obj, arg0);
            obj->result               = USER_INTERFACE_RESULT_CANCEL;
            obj->panel.animationTicks = 0x64;
            arg0->state               = arg0->state + 1;
        } else {
            arg0->spawnArg1.value = -1;
            Ui_SizeFromTextPlain(&(obj)->panel, Gp_StrNoUseNow);
            obj->panel.style &= (s32)~USER_INTERFACE_PANEL_NO_FRAME;
        }
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }
    if (arg0->state != 2) {
        uiDrawPanelLabel(&(obj)->panel, Gp_StrNotice);
        if (arg0->spawnArg1.value == -1) {
            color = Ui_LookupTable(obj, 1);
            textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrNoUseNow, color, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        } else {
            color = Ui_LookupTable(obj, 1);
            one   = 1;
            textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrUsed, color, one, TEXT_ALIGNMENT_LEFT);
            text  = Gp_GetItemText(arg0->spawnArg1.value, 0, 0);
            width = textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, text, 0x37A78, one, TEXT_ALIGNMENT_LEFT);
            textDrawUiLine(obj, width, obj->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, 0x606060, one, TEXT_ALIGNMENT_LEFT);
        }
        arg0->killCountdown = arg0->killCountdown - gDisplayState.frameTicks;
        if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                obj->result = USER_INTERFACE_RESULT_CANCEL;
            } else if ((arg0->killCountdown <= 0) ||
                       (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
                if (arg0->spawnArg1.value == -1) {
                    if (gGameSession->cutsceneHold == 1) {
                        obj->result = USER_INTERFACE_RESULT_CONFIRM;
                    } else {
                        obj->result = USER_INTERFACE_RESULT_DISMISS;
                    }
                } else {
                    obj->result = USER_INTERFACE_RESULT_CANCEL;
                }
                arg0->killCountdown = 0x7FFF;
            }
        }
    }
}

void Gp_KeyItemSubMenuTask(Task* arg0)
{
    Task*     childTask;
    UiObject* obj;
    UiList*   menu;
    UiObject* child;
    s32       flag;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    menu        = &D_8010E938;
    if (arg0->state == 0) {
        obj->panel.bounds.unsignedRect.w = 0x60;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
    childTask = arg0->firstChild;
    if (childTask != NULL) {
        child = childTask->spawnArg2.pointer;
        flag  = child->result;
        switch (flag) {
            case USER_INTERFACE_RESULT_CANCEL:
                obj->result = flag;
                break;
            case USER_INTERFACE_RESULT_CONFIRM:
                uiStartTreeClosing(child, child->owner);
                obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                break;
            case USER_INTERFACE_RESULT_DISMISS:
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
                break;
        }
    }
}

void Gp_DrawCollectedRow(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         item;
    s32         x;
    s32         y;
    s32         color;
    s32         temp;
    s32         status;
    s32         one;
    s32         flag;
    s32         i;
    s32         minusOne;
    UiObject*   obj;
    s32         baseY;

    item  = Gp_NthCollectedId(arg0->currentItemIndex, 0);
    x     = arg0->rowTextX.signedValue;
    y     = arg0->rowTextY.signedValue;
    color = arg0->colorRgb;
    if (arg1->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = arg1->panel.contentOriginX.unsignedValue + 0x11 + x;
        baseY          = arg1->panel.contentOriginY.unsignedValue - 6;
        req.y          = baseY + y;
        req.otIndex    = arg1->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, Gp_GetItemText(item, 0, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg1, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(arg1, x, y, item, 0);
    }

    status = arg1->panel.control.word;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            if (item != Gp_PreviewItems[0]) {
                i        = 0;
                minusOne = -1;
                for (; i < 3; i++) {
                    if (i == 0) {
                        Gp_PreviewItems[0] = item;
                    } else {
                        Gp_PreviewItems[i] = minusOne;
                    }
                }
                Gp_EnqueueItemPreviewCd(item, 0);
            }
            if (item == 0) {
                Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            } else {
                Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
            }
        }
    }

    flag = arg0->rowInputEnabled;
    if (flag == 1) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if (gGameSession->cutsceneHold == flag) {
                Ui_SpawnFromDesc(&D_8010EF84, 0, 1, 1, arg1);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                obj = Ui_SpawnFromDesc(&D_8010EF68, item, 1, 1, arg1);
                if (obj != NULL) {
                    uiPositionRowDialog(&(obj)->panel, arg0, &(arg1)->panel);
                    arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void Gp_KeyItemMenuTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    obj         = arg0->spawnArg2.pointer;
    menu        = &D_8010E960;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrKeyItem);
    if (arg0->state == 0) {
        menu->visibleRowCount.unsignedValue = menu->itemCount = Gp_CountCollectedBits();
        if (menu->itemCount < menu->selectedItemIndex) {
            menu->selectedItemIndex = menu->itemCount;
        }
        Ui_InitList(menu, &(obj)->panel);
        menu->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        if (arg0->spawnArg1.value == 0) {
            uiSetPanelContentSize(&(obj)->panel, 0, uiGetTextRowsHeight(0xA) + 1);
            Ui_SpawnFromDesc(&D_8010F868, 0, 0, 1, obj);
        }
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        arg0->state                               = arg0->state + 1;
    } else {
        menu->visibleRowCount.unsignedValue = menu->itemCount = Gp_CountCollectedBits();
        if (menu->itemCount < menu->selectedItemIndex) {
            menu->selectedItemIndex = menu->itemCount;
        }
        Ui_ComputeVisibleRows(menu, &(obj)->panel);
        menu->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        if (menu->selectedItemIndex >= menu->itemCount) {
            menu->selectedItemIndex = menu->itemCount - 1;
        }
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            if (obj->result == USER_INTERFACE_RESULT_NONE) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                    obj->result = USER_INTERFACE_RESULT_CANCEL;
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                    if (gGameSession->cutsceneHold == 1) {
                        sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                        obj->result = USER_INTERFACE_RESULT_CANCEL;
                    } else {
                        sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                        obj->resultValue = 1;
                        obj->result      = USER_INTERFACE_RESULT_CONFIRM;
                    }
                } else {
                    padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_R2);
                }
            }
        } else if (obj->panel.control.word >= USER_INTERFACE_PANEL_REQUEST_MIN) {
            obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
        }
    }
    head = arg0->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->result;
            next     = child->nextSibling;
            switch (flag) {
                case USER_INTERFACE_RESULT_CANCEL:
                    obj->result = flag;
                    break;
                case USER_INTERFACE_RESULT_CONFIRM:
                    uiStartTreeClosing(childObj, childObj->owner);
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    break;
            }
            head  = arg0->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
}

void func_800C7AE8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4*            p;
    _ItemMenuPreviewSize size;
    s32                  w;
    s32                  h;
    s32                  x;
    s32                  y;
    s32                  scale;

    w = 0x80;
    h = 0x60;
    if (arg3 & 0x200) {
        w = 0x50;
        h = 0x3C;
    } else if (arg3 & 0x400) {
        h = 0x7F;
    }
    size.width  = w;
    size.height = h;
    size.depth  = 0;
    // Shrink the preview by a 4.12 factor on the GTE.
    if ((arg3 & 0xF0) == 0x10) {
        scale = 0xA00;
        gte_lddp(scale);
        gte_ldsv(&size);
        gte_gpf12();
        gte_stsv(&size);
    } else if ((arg3 & 0xF0) == 0x20) {
        scale = 0xAA0;
        gte_lddp(scale);
        gte_ldsv(&size);
        gte_gpf12();
        gte_stsv(&size);
    }
    if (!(arg3 & 0x100)) {
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setlen(p, 9);
        setcode(p, 0x2D);
        x     = arg0->panel.contentOriginX.unsignedValue + arg1;
        p->x2 = x;
        p->x0 = x;
        x     = x + size.width;
        p->x3 = x;
        p->x1 = x;
        y     = arg0->panel.contentOriginY.unsignedValue + arg2;
        p->y1 = y;
        p->y0 = y;
        y     = y + size.height;
        p->y3 = y;
        p->y2 = y;
        switch (arg3 & 0xF) {
            case 1:
                p->tpage = 0x8F;
                p->u0    = 0;
                p->v0    = 0;
                p->u1    = w;
                p->v1    = 0;
                p->u2    = 0;
                p->v2    = h;
                p->u3    = w;
                p->v3    = h;
                p->clut  = 0x3F40;
                break;
            case 2:
                p->u0    = 0;
                p->u1    = w;
                p->u2    = 0;
                p->u3    = w;
                p->v0    = 0x80;
                p->v1    = 0x80;
                p->v2    = h - 0x80;
                p->v3    = h - 0x80;
                p->tpage = 0x8F;
                p->clut  = 0x3F80;
                break;
            default:
                p->u0    = 0;
                p->u1    = w;
                p->u2    = 0;
                p->u3    = w;
                p->v0    = 0x80;
                p->v1    = 0x80;
                p->v2    = h - 0x80;
                p->v3    = h - 0x80;
                p->tpage = 0x87;
                p->clut  = 0x3F40;
                break;
        }
        addPrim(gGpuCurrentOt + arg0->panel.otIndex.signedValue + 1, p);
    }
    uiDrawRecessedRect(&arg0->panel, (arg1 - 1), (arg2 - 1), ((s16)size.width + 1),
                       ((s16)size.height + 1), 0x81008);
}

/// Sets bit 0x100 in `flags`, which makes `func_800C7AE8` skip drawing the
/// item preview, while the CD queue is still busy loading it.
#define GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags) \
    do {                                     \
        if (CdCmd_IsIdle() == 0) {           \
            (flags) |= 0x100;                \
        }                                    \
    } while (0)
