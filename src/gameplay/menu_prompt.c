#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "attachments.h"
#include "hud_sprites.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "menu.h"

#define D_8010EB08 D_8010EAB4[3]

#define D_8010EB24 D_8010EAB4[4]

#define D_8010EB40 D_8010EAB4[5]

#define D_8010EFD8 D_8010EAB4[47]

#include "main/display.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

static void func_800C0B98(UiList* arg0, UiObject* arg1, u32 arg2);

/// Draws `item`'s name, its `func_800C22D8` marker in `mode`, the variant
/// marker for items 0x0F-0x32 and its icon at (`x`, `y`) in `obj`. Nothing is
/// drawn while `obj->panel.state` is `USER_INTERFACE_PANEL_HIDDEN`.
static inline void _gpDrawItemNameAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item, s32 mode);

/// Draws `item` as `_gpDrawItemNameAt` does, at the prompt row's position and
/// in its colour.
static inline void _gpDrawItemName(UiList* prompt, UiObject* obj, s32 item, s32 mode);

/// Returns the `index`-th row (from 0) of `scan` that is free to reorder -
/// not attached to armour and not the equipped armour or weapon - or NULL
/// when there are fewer. The out-of-line copy is `func_800CECC0`.
static inline InventoryItemRow* _gpNthLooseRec(InventoryItemRange* scan, s32 index);

static __inline__ void countItemRows(UiList* menu);

static void Gp_ItemListTask(Task* arg0);

/// Replaces the layout position a spawned child copied from its descriptor
/// with `x`, `y`.
static inline void _gpSetSpawnOffset(UiObject* obj, s32 x, s32 y);

/// Sets the weapon menu's row count from the current weapon's item: one row
/// for weapon index 0 and for item 0x92, otherwise three when the item's
/// load supports a secondary consumable slot (`secondaryItemId` !=
/// `EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE`) and two when it does not.
static inline void _gpWeaponMenuSetRows(UiList* menu);

/// Keeps the armor attachment selection within the visible rows and item count.
static inline void _gpClampArmorRow(UiList* menu, s32 end);

/// Whether item `id` is the equipped weapon, the equipped armour, or a
/// consumable selected in either firing mode of the equipped weapon.
static inline s32 _gpIsEquippedItem(s32 id);

char Gp_StrEmpty[]     = "";
char Gp_StrReleasePe[] = "Release Parasite Energy.";

void Gp_UiPromptDispatch(UiObject* arg0, Task* arg1)
{
    const u8*     text;
    s32           color;
    s32           one;
    TaskSpawnArg  val;
    s32           flag;
    s32           scale;
    s32           height;
    s32           width;
    UiObjectDesc* desc;

    val = arg1->spawnArg1;
    if (val.value != 0) {
        if (val.unsignedValue > 0xFFFF) {
            color = Ui_LookupTable(arg0, 1);
            one   = 1;
            textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0xF, val.pointer, color, one, TEXT_ALIGNMENT_LEFT);
            text = textSkipLines(val.pointer, one);
            textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0x1E, text, color, one, TEXT_ALIGNMENT_LEFT);
        } else if ((u32)(val.value - 0x300) < 0x100U) {
            Gp_DrawCastCostLines(arg0, val.value);
        }
    }

    arg1->killCountdown--;
    if (arg1->killCountdown > 0) {
        return;
    }

    switch (arg0->resultValue) {
        case 5:
            desc = D_8010EAB4;
            Ui_SpawnFromDesc(desc + 5, 0, 1, 8, arg0);
            Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            uiStartPanelOpening(&(arg0)->panel, arg0->owner);
            break;
        case 0x14:
        case 0x19:
            Ui_SpawnFromDesc(D_8010EAB4 + arg0->resultValue, 0, 1, 8, arg0);
            break;
        case 0x100:
            D_80114D88 = 1;
            Ui_SpawnFromDesc(&D_8010F140, 0, 1, 8, arg0);
            break;
        case 0x101:
            Display_SetDrawMode(1);
            Ui_SpawnFromDesc(&D_8010EAD0, 0, 1, 8, arg0);
            Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            uiStartPanelOpening(&(arg0)->panel, arg0->owner);
            break;
        case 6:
        case 0xC:
            Display_SetDrawMode(1);
            Ui_SpawnFromDesc(D_8010EAB4 + arg0->resultValue, 0, 0, 8, arg0);
            Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            uiStartPanelOpening(&(arg0)->panel, arg0->owner);
            break;
        case 0x24:
        default:
            Display_SetDrawMode(1);
            Ui_SpawnFromDesc(D_8010EAB4 + arg0->resultValue, 0, 1, 8, arg0);
            Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            uiStartPanelOpening(&(arg0)->panel, arg0->owner);
            break;
    }

    arg1->state--;

    flag = arg0->resultValue;
    if ((flag == 1) || (flag == 0x101)) {
        scale = 1;
    } else if (flag != 0xC) {
        scale = 2;
    } else {
        uiSetPanelContentSize(&(arg0)->panel, 0, uiGetTextRowsHeight(2) + 1);
        width  = arg0->panel.bounds.unsignedRect.h;
        height = 0x4C;
        goto store;
    }
    uiSetPanelContentSize(&(arg0)->panel, 0, uiGetTextRowsHeight(scale) + 1);
    width  = arg0->panel.bounds.unsignedRect.h;
    height = 0x68;
store:
    arg0->panel.bounds.unsignedRect.y = height - width;
}

void Gp_DrawItemIcon(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    POLY_FT4* p;
    TILE*     q;
    s32       icon;
    s32       kind;
    s32       tu;
    s32       tv;
    s32       clut;
    s32       flag0;
    s32       flag1;
    s32       flag2;
    s32       flag3;
    s32       idx;
    s32       rel;
    s32       tmp;

    clut  = 0;
    tv    = 0;
    tu    = 0;
    flag0 = arg4 & 1;
    flag1 = (arg4 >> 1) & 1;
    flag2 = (arg4 >> 2) & 1;
    flag3 = (arg4 >> 3) & 1;

    if (arg3 >= 0x300) {
        icon = -1;
        kind = -1;
        if (arg3 < 0x340) {
            tu   = ((arg3 & 0xC) << 2) + 0xB0;
            tv   = (arg3 & 0xF0) + 0x80;
            clut = getClut((arg3 & 0xF0) + 0x40, 0xF0);
        } else {
            tu   = 0xE0;
            tv   = 0x40;
            clut = 0x3C8B;
        }
    } else if (arg3 == 0) {
        icon = 6;
        kind = 8;
    } else if (flag0 == 0 && Gp_HasItemSeenBit(arg3) == 0) {
        icon = 2;
        kind = 2;
    } else if (arg3 < 0x60) {
        if ((u32)(arg3 - 0xF) < 0x24) {
            idx  = (arg3 - 0xF) / 3;
            icon = -1;
            kind = -1;
            tu   = (idx % 3) * 16 + 0xB0;
            tv   = (idx / 3) * 16 + 0x80;
            clut = getClut((idx / 3) * 16 + 0x40, 0xF0);
        } else if ((Gp_ItemDescs[arg3].classification & ITEM_SUBTYPE_MASK) == ITEM_SUBTYPE_MEDICINE) {
            icon = 1;
            kind = 4;
        } else {
            if (arg3 < 0x47) {
                if (arg3 < 0x42) {
                    switch (arg3) {
                        case 9:
                        case 0xC:
                            icon = 3;
                            break;
                        case 0xA:
                            icon = 6;
                            break;
                        default:
                            icon = 9;
                            break;
                    }
                } else {
                    icon = 6;
                }
            } else {
                icon = 9;
            }
            kind = 6;
        }
    } else if (arg3 < 0x80) {
        icon = 9;
        kind = 3;
    } else if (arg3 < 0xA0) {
        rel = Gp_RelatedQty0.rows[arg3 - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[0];
        if (rel == INVENTORY_ITEM_NONE) {
            icon = 2;
        } else {
            switch (Gp_ItemDescs[rel].classification & ITEM_SUBTYPE_MASK) {
                case ITEM_AMMO_9MM:
                    icon = 3;
                    break;
                case ITEM_AMMO_44_MAGNUM:
                    icon = 4;
                    break;
                case ITEM_AMMO_40MM:
                    icon = 7;
                    break;
                case ITEM_AMMO_12_GAUGE:
                    icon = 5;
                    break;
                case ITEM_AMMO_556MM:
                    icon = 6;
                    break;
                case ITEM_AMMO_BATTERY:
                    icon = 8;
                    break;
                default:
                    icon = 2;
                    break;
            }
        }
        kind = 0;
    } else if (arg3 < 0xC0) {
        switch (Gp_ItemDescs[arg3].classification & ITEM_SUBTYPE_MASK) {
            case ITEM_AMMO_9MM:
                icon = 3;
                break;
            case ITEM_AMMO_44_MAGNUM:
                icon = 4;
                break;
            case ITEM_AMMO_40MM:
                icon = 7;
                break;
            case ITEM_AMMO_12_GAUGE:
                icon = 5;
                break;
            case ITEM_AMMO_556MM:
                icon = 6;
                break;
            default:
                icon = 2;
                break;
        }
        kind = 1;
    } else {
        icon = 1;
        kind = 7;
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    p->x2 = p->x0 = arg0->panel.contentOriginX.unsignedValue + arg1;
    p->x1 = p->x3 = p->x0 + 0xE;
    p->y1 = p->y0 = arg0->panel.contentOriginY.unsignedValue + arg2 - 0xE;
    p->y2 = p->y3 = p->y0 + 0xE;
    if (flag1 != 0) {
        p->x2 = p->x0 = p->x0 - 2;
        p->x3 = p->x1 = p->x1 + 2;
        p->y1 = p->y0 = p->y0 - 2;
        p->y3 = p->y2 = p->y2 + 2;
    }
    if (kind >= 0) {
        tmp = (kind + 1) * 16;
        setUV4(p, -tmp, 0xF0, 0xE - tmp, 0xF0, -tmp, 0xFE, 0xE - tmp, 0xFE);
    } else {
        setUVWH(p, tu + 1, tv + 1, 0xE, 0xE);
    }
    if (icon >= 0) {
        p->clut = D_80096F88[icon];
    } else {
        p->clut = clut;
    }
    p->tpage = 0x1E;
    if (flag2 == 0) {
        setlen(p, 9);
        setcode(p, 0x2D);
    } else {
        GPU_PRIMITIVE_COLOR_WORD(p, 0) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0x40, 0);
        setlen(p, 9);
        setcode(p, 0x2C);
    }
    addPrim(gGpuCurrentOt + arg0->panel.otIndex.signedValue + 1, p);
    if (flag3 != 0) {
        q                              = gGpuPrimCursor;
        q->x0                          = p->x0 - 1;
        q->y0                          = p->y0 - 1;
        q->w                           = p->x1 - p->x0 + 2;
        q->h                           = p->y2 - p->y0 + 2;
        gGpuPrimCursor                 = q + 1;
        GPU_PRIMITIVE_COLOR_WORD(q, 0) = GPU_PACK_COLOR_WORD(0xc0, 0xc0, 0xc0, 0);
        setlen(q, 3);
        setcode(q, 0x60);
        addPrim(gGpuCurrentOt + arg0->panel.otIndex.signedValue + 1, q);
    }
}

static void func_800C0B98(UiList* arg0, UiObject* arg1, u32 arg2)
{
    SPRT* p;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    p->x0          = arg0->rowTextX.unsignedValue + arg1->panel.contentOriginX.unsignedValue;
    p->y0          = arg0->rowTextY.unsignedValue + arg1->panel.contentOriginY.unsignedValue - 0xF;
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        p->clut = 0x3C09;
    } else {
        p->clut = 0x3C01;
    }
    p->v0 = arg2 >> 8;
    p->w  = (arg2 >> 16) & 0xFF;
    p->h  = arg2 >> 24;
    setlen(p, 4);
    p->u0 = arg2;
    setcode(p, 0x65);
    addPrim(gGpuCurrentOt + arg1->panel.otIndex.signedValue + 1, p);
    uiQueueTexturePage(arg1->panel.otIndex.signedValue + 1, 0);
}

void Gp_StatusPanelTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    menu = &D_8010E820;
    obj  = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        Ui_SpawnFromDesc(&D_8010EB08, 0, 0, 4, obj);
        Ui_LayoutListPanel(menu, &(obj)->panel);
        arg0->state = arg0->state + 1;
    } else {
        obj->result = USER_INTERFACE_RESULT_NONE;
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            if (obj->result == USER_INTERFACE_RESULT_NONE) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                    obj->result = USER_INTERFACE_RESULT_CANCEL;
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                    obj->result = USER_INTERFACE_RESULT_CANCEL;
                }
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
                    case USER_INTERFACE_RESULT_DISMISS:
                        obj->resultValue = childObj->resultValue;
                        obj->result      = USER_INTERFACE_RESULT_CONFIRM;
                        break;
                    case USER_INTERFACE_RESULT_CONFIRM:
                        obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                        uiStartTreeClosing(childObj, childObj->owner);
                        break;
                    case USER_INTERFACE_RESULT_CANCEL:
                        obj->result = flag;
                        break;
                }
                child = next;
            } while (child != arg0->firstChild);
        }
    }
}

void func_800C0E20(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, u32 arg6)
{
    TILE*     tile;
    SPRT*     sp;
    POLY_FT4* poly;
    s32       span;
    s32       max;
    s32       bar;
    s32       right;
    s32       clut;

    if (arg1 < arg2) {
        span = arg2 - arg1;
        max  = span - 2;
        bar  = (max * arg5) / arg4;
        arg1 = arg1 + arg0->contentOriginX.signedValue;
        arg3 = arg3 + arg0->contentOriginY.signedValue;
        if (max < bar) {
            bar = max;
        }
        if (bar > 0) {
            tile                              = gGpuPrimCursor;
            gGpuPrimCursor                    = tile + 1;
            tile->x0                          = arg1 + 1;
            tile->y0                          = arg3 - 1;
            tile->w                           = bar;
            tile->h                           = 2;
            GPU_PRIMITIVE_COLOR_WORD(tile, 0) = arg6;
            setlen(tile, 3);
            setcode(tile, 0x60);
            addPrim(gGpuCurrentOt + arg0->otIndex.signedValue + 1, tile);
        }
        arg3 = arg3 - 4;
        clut = 0x3C0B;

        sp             = gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        sp->x0         = arg1;
        sp->y0         = arg3;
        sp->u0         = 0x98;
        sp->v0         = 0x68;
        sp->clut       = clut;
        setlen(sp, 3);
        setcode(sp, 0x75);
        addPrim(gGpuCurrentOt + arg0->otIndex.signedValue + 1, sp);

        sp             = gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        right          = (arg1 + span) - 8;
        sp->x0         = right;
        sp->y0         = arg3;
        sp->u0         = 0xA8;
        sp->v0         = 0x68;
        sp->clut       = clut;
        setlen(sp, 3);
        setcode(sp, 0x75);
        addPrim(gGpuCurrentOt + arg0->otIndex.signedValue + 1, sp);

        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        poly->x2       = arg1 + 8;
        poly->x0       = arg1 + 8;
        poly->y3       = arg3 + 8;
        poly->y2       = arg3 + 8;
        poly->u0       = 0xA0;
        poly->u2       = 0xA0;
        poly->v2       = 0x70;
        poly->v3       = 0x70;
        poly->tpage    = 0x3E;
        poly->x3       = right;
        poly->x1       = right;
        poly->y1       = arg3;
        poly->y0       = arg3;
        poly->v0       = 0x68;
        poly->u1       = 0xA8;
        poly->v1       = 0x68;
        poly->u3       = 0xA8;
        poly->clut     = clut;
        setlen(poly, 9);
        setcode(poly, 0x2D);
        addPrim(gGpuCurrentOt + arg0->otIndex.signedValue + 1, poly);
    }
}

void Gp_DrawHpMpStats(UiPanel* arg0, s32 arg1)
{
    u8            buf[0x20];
    TextDrawReq   req1;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    TextDrawReq   req5;
    TextDrawReq   req6;
    TextDrawReq   req7;
    TextDrawReq   req8;
    TextDrawReq   req9;
    TextDrawReq   req10;
    TextDrawReq   req11;
    PlayerStatus* cfg;
    s32           xOff;
    s32           x;
    s32           y;
    register s32  y2 asm("s2");
    s32           barX;
    s32           color;
    s32           max;

    cfg  = &gPlayerStatus;
    xOff = arg0->contentLeft.signedValue;
    arg1 = arg1 + 8;
    x    = xOff + 6;
    y    = arg0->contentTop.signedValue + arg1;
    if (Gp_HpMpWork.hp < cfg->hp) {
        Gp_HpMpWork.hp = Gp_HpMpWork.hp + 1;
    }
    if (Gp_HpMpWork.mp < cfg->mp) {
        Gp_HpMpWork.mp = Gp_HpMpWork.mp + 1;
    }
    color = 0x606060;

    req1.x          = arg0->contentOriginX.unsignedValue + 0x17 + x;
    req1.y          = arg0->contentOriginY.unsignedValue + y;
    req1.otIndex    = arg0->otIndex.signedValue + 1;
    req1.colorRgb   = color;
    req1.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req1.alignment  = TEXT_ALIGNMENT_LEFT;
    req1.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req1, textItoaUnsigned(buf, Gp_HpMpWork.hp));

    req2.x          = arg0->contentOriginX.unsignedValue + 0x32 + x;
    req2.y          = arg0->contentOriginY.unsignedValue + y;
    req2.otIndex    = arg0->otIndex.signedValue + 1;
    req2.colorRgb   = color;
    req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req2.alignment  = TEXT_ALIGNMENT_CENTER;
    req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req2, Gp_StrSlash);

    req3.x          = arg0->contentOriginX.unsignedValue + 0x37 + x;
    req3.y          = arg0->contentOriginY.unsignedValue + y;
    req3.otIndex    = arg0->otIndex.signedValue + 1;
    req3.colorRgb   = color;
    req3.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req3.alignment  = TEXT_ALIGNMENT_LEFT;
    req3.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req3, textItoaUnsigned(buf, cfg->hpMax));

    max  = cfg->hpMax;
    barX = xOff + 7;
    func_800C0E20(arg0, x, barX + ((max - 1) * 0x25) / 64, y + 5, max, Gp_HpMpWork.hp, 0x1741F);

    y2              = y + 0x12;
    req4.x          = arg0->contentOriginX.unsignedValue + 0x17 + x;
    req4.y          = arg0->contentOriginY.unsignedValue + y2;
    req4.otIndex    = arg0->otIndex.signedValue + 1;
    req4.colorRgb   = color;
    req4.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req4.alignment  = TEXT_ALIGNMENT_LEFT;
    req4.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req4, textItoaUnsigned(buf, Gp_HpMpWork.mp));

    req5.x          = arg0->contentOriginX.unsignedValue + 0x32 + x;
    req5.y          = arg0->contentOriginY.unsignedValue + y2;
    req5.otIndex    = arg0->otIndex.signedValue + 1;
    req5.colorRgb   = color;
    req5.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req5.alignment  = TEXT_ALIGNMENT_CENTER;
    req5.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req5, Gp_StrSlash);

    req6.x          = arg0->contentOriginX.unsignedValue + 0x37 + x;
    req6.y          = arg0->contentOriginY.unsignedValue + y2;
    req6.otIndex    = arg0->otIndex.signedValue + 1;
    req6.colorRgb   = color;
    req6.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req6.alignment  = TEXT_ALIGNMENT_LEFT;
    req6.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req6, textItoaUnsigned(buf, cfg->mpMax));

    max = cfg->mpMax;
    func_800C0E20(arg0, x, barX + ((max - 1) * 0x25) / 64, y + 0x17, max, Gp_HpMpWork.mp, 0x1741F);

    y2              = y + 0x24;
    req7.x          = arg0->contentOriginX.unsignedValue + 0x17 + x;
    req7.y          = arg0->contentOriginY.unsignedValue + y2;
    req7.otIndex    = arg0->otIndex.signedValue + 1;
    req7.colorRgb   = color;
    req7.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req7.alignment  = TEXT_ALIGNMENT_LEFT;
    req7.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req7, textItoaUnsigned(buf, cfg->exp));

    req8.x          = arg0->contentOriginX.unsignedValue + xOff + 0x72;
    req8.y          = arg0->contentOriginY.unsignedValue + y2;
    req8.otIndex    = arg0->otIndex.signedValue + 1;
    req8.colorRgb   = color;
    req8.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req8.alignment  = TEXT_ALIGNMENT_LEFT;
    req8.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req8, textItoaUnsigned(buf, cfg->bp));

    req8.x          = arg0->contentLeft.unsignedValue + (arg0->contentOriginX.unsignedValue + 2);
    req8.y          = arg0->contentOriginY.unsignedValue + (y - 2);
    req8.otIndex    = arg0->otIndex.signedValue + 1;
    req8.colorRgb   = color;
    req8.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req8.alignment  = TEXT_ALIGNMENT_LEFT;
    req8.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req8, Gp_StrHp);

    req9.x          = arg0->contentLeft.unsignedValue + (arg0->contentOriginX.unsignedValue + 2);
    req9.y          = arg0->contentOriginY.unsignedValue + 0x10 + y;
    req9.otIndex    = arg0->otIndex.signedValue + 1;
    req9.colorRgb   = color;
    req9.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req9.alignment  = TEXT_ALIGNMENT_LEFT;
    req9.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req9, Gp_StrMp);

    req10.x          = arg0->contentLeft.unsignedValue + (arg0->contentOriginX.unsignedValue + 2);
    req10.y          = arg0->contentOriginY.unsignedValue + 0x22 + y;
    req10.otIndex    = arg0->otIndex.signedValue + 1;
    req10.colorRgb   = color;
    req10.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req10.alignment  = TEXT_ALIGNMENT_LEFT;
    req10.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req10, Gp_StrExp);

    req11.x          = arg0->contentLeft.unsignedValue + (arg0->contentOriginX.unsignedValue + 0x57);
    req11.y          = arg0->contentOriginY.unsignedValue + 0x22 + y;
    req11.otIndex    = arg0->otIndex.signedValue + 1;
    req11.colorRgb   = color;
    req11.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req11.alignment  = TEXT_ALIGNMENT_LEFT;
    req11.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req11, Gp_StrBp);
}

void Gp_HpMpBarTask(Task* arg0)
{
    UiObject*     obj;
    PlayerStatus* cfg;
    SPRT*         p;
    POLY_FT4*     poly;
    s32           color;
    s32           x;
    s32           right;
    s32           y;

    obj = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        Ui_SpawnFromDesc(&D_8010EB24, 0, 0, 0, obj);
        cfg            = &gPlayerStatus;
        Gp_HpMpWork.hp = cfg->hp;
        Gp_HpMpWork.mp = cfg->mp;
        arg0->state    = arg0->state + 1;
    }
    color          = 0x606060;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    p->x0          = obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue - 0x72;
    {
        s32 y;
        y       = obj->panel.bounds.unsignedRect.y;
        p->u0   = 0x38;
        p->v0   = 0x60;
        p->w    = 0x40;
        p->h    = 8;
        p->clut = 0x3C02;
        setlen(p, 4);
        GPU_PRIMITIVE_COLOR_WORD(p, 0) = color;
        setcode(p, 0x64);
        p->y0 = y + 3;
        addPrim(gGpuCurrentOt + obj->panel.otIndex.signedValue + 1, p);
    }
    uiQueueTexturePage(obj->panel.otIndex.signedValue + 1, 0);

    poly           = gGpuPrimCursor;
    x              = obj->panel.bounds.unsignedRect.x + obj->panel.bounds.unsignedRect.w;
    right          = x - 1;
    x              = x - 0x32;
    gGpuPrimCursor = poly + 1;
    poly->x0 = poly->x2 = x;
    poly->x1 = poly->x3 = right;
    y                   = obj->panel.bounds.unsignedRect.y;
    poly->y0 = poly->y1 = y + 2;
    poly->y2 = poly->y3 = y + 0x40;
    setUVWH(poly, 0, 0x80, 0x31, 0x3E);
    poly->clut  = 0x3C40;
    poly->tpage = 0x9E;
    setlen(poly, 9);
    setcode(poly, 0x2D);
    addPrim(gGpuCurrentOt + obj->panel.otIndex.signedValue + 1, poly);
    uiDrawVerticalSeparator(&(obj)->panel, obj->panel.contentTop.signedValue - 3, obj->panel.contentBottom.signedValue + 2, obj->panel.contentRight.signedValue - 0x32);
    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue - 2, obj->panel.contentRight.signedValue - 0x32, obj->panel.contentTop.signedValue + 8);
    Gp_DrawHpMpStats(&(obj)->panel, 0xB);
}

void Gp_ArmorStatsPanelTask(Task* arg0)
{
    u8            buf[0x20];
    TextDrawReq   req1;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    TextDrawReq   req5;
    s32           savedX;
    s32           mid;
    UiObject*     obj;
    PlayerStatus* cfg;
    s32           item;
    s32           color;
    s32           x;
    s32           y;
    s32           base;
    s32           i;
    ArmorStats*   attr;

    obj         = arg0->spawnArg2.pointer;
    cfg         = &gPlayerStatus;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x11);

    savedX = obj->panel.contentLeft.signedValue;
    x      = savedX + 2;
    item   = cfg->armor;
    base   = obj->panel.contentTop.signedValue;
    y      = base + 0xF;
    mid    = (obj->panel.contentRight.signedValue - x) / 2;
    uiDrawTitle(&(obj)->panel, Gp_StrArmor);

    if (item > 0) {
        item += 0x5F;
        color = 0x606060;
        attr  = &Gp_ModStatAttrs[(item)-0x60];
        Gp_DrawItemLabel(obj, x, y, item, color, 0);

        y               = base + 0x1D;
        req1.x          = obj->panel.contentOriginX.unsignedValue + 0x20 + x;
        req1.y          = obj->panel.contentOriginY.unsignedValue + y;
        req1.otIndex    = obj->panel.otIndex.signedValue + 1;
        req1.colorRgb   = color;
        req1.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req1.alignment  = TEXT_ALIGNMENT_LEFT;
        req1.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req1, textItoaSignPrefixed(buf, attr->hpBonus));

        req2.x          = obj->panel.contentOriginX.unsignedValue + (x + (mid + 0x1E));
        req2.y          = obj->panel.contentOriginY.unsignedValue + y;
        req2.otIndex    = obj->panel.otIndex.signedValue + 1;
        req2.colorRgb   = color;
        req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req2.alignment  = TEXT_ALIGNMENT_LEFT;
        req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req2, textItoaSignPrefixed(buf, attr->mpBonus));

        req3.x          = obj->panel.contentOriginX.unsignedValue + 2 + x;
        req3.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
        req3.otIndex    = obj->panel.otIndex.signedValue + 1;
        req3.colorRgb   = color;
        req3.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req3.alignment  = TEXT_ALIGNMENT_LEFT;
        req3.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req3, Gp_StrHp);

        req4.x          = obj->panel.contentOriginX.unsignedValue + (x + mid);
        req4.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
        x               = savedX + 4;
        req4.otIndex    = obj->panel.otIndex.signedValue + 1;
        req4.colorRgb   = color;
        req4.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req4.alignment  = TEXT_ALIGNMENT_LEFT;
        req4.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req4, Gp_StrMp);

        y               = base + 0x3D;
        req5.x          = obj->panel.contentOriginX.unsignedValue + x;
        req5.y          = obj->panel.contentOriginY.unsignedValue + base + 0x2C;
        req5.otIndex    = obj->panel.otIndex.signedValue + 1;
        req5.colorRgb   = color;
        req5.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req5.alignment  = TEXT_ALIGNMENT_LEFT;
        req5.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req5, Gp_StrAttachments);

        for (i = 0; i < Gp_GetModLevel(item); i++) {
            InventoryItemRange* scan;
            InventoryItemRow*   rec;
            InventoryItemRow*   found;
            s32                 col;
            s32                 row;
            s32                 j;
            s32                 id;

            col   = i % 5;
            row   = i / 5;
            scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            rec   = Gp_GetItemTable(scan);
            found = NULL;
            rec   = &rec[scan->firstRow];
            for (j = 0; j < scan->rowCount; j++, rec++) {
                if (rec->attachSlot == i + 1) {
                    found = rec;
                    break;
                }
            }
            id = 0;
            if (found != NULL) {
                id = found->itemId;
            }
            if (id != 0) {
                Gp_DrawItemIcon(obj, x + col * 16, y + row * 16, id, 0);
            }
            uiDrawRecessedRect(&obj->panel, x + col * 16, y + row * 16 - 0xE, 0xE, 0xE, 0x102010);
        }
    }
}

void Gp_PeGridPanelTask(Task* arg0)
{
    u8          buf[8];
    TextDrawReq req;
    UiObject*   obj;
    SPRT*       p;
    u8*         levels;
    s32         startX;
    s32         colStep;
    s32         row;
    s32         col;
    s32         slot;
    s32         x;
    s32         y;
    s32         rowOff;
    s32         show;
    s32         panelY;
    s32         level;
    s32         three;
    s32         capY;
    u8*         colLevels;
    s32         iconCol;
    s32         iconSlot;
    s32         baseSlot;
    s32         markOff;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    startX      = obj->panel.contentLeft.signedValue + 3;
    colStep     = (obj->panel.contentRight.signedValue - obj->panel.contentLeft.signedValue) / 4;
    uiDrawTitle(&(obj)->panel, Gp_StrPEnergy);

    row    = 0;
    three  = 3;
    rowOff = 2;
    panelY = obj->panel.contentBottom.signedValue;
    for (; row < 3; row++) {
        for (col = 0, y = panelY - rowOff, x = startX, slot = 2; col < 4; col++) {
            levels = Gp_GetAttachLevels() + (slot - row);
            show   = 0;
            if (row == 0) {
                level = levels[0];
                if ((level > 0) || ((levels[-1] == three) && (levels[-2] == three))) {
                    show = 1;
                }
            }
            if ((row != 0) || (show != 0)) {
                req.x          = obj->panel.contentOriginX.unsignedValue + 0xC + x;
                req.y          = obj->panel.contentOriginY.unsignedValue + y;
                req.otIndex    = obj->panel.otIndex.signedValue + 1;
                req.colorRgb   = 0x606060;
                req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                req.alignment  = TEXT_ALIGNMENT_LEFT;
                req.drawMode   = three;
                textDrawString(&req, textItoaSigned(buf, levels[0]));
            }
            x    += colStep;
            slot += 3;
        }
        rowOff += 9;
    }

    for (iconCol = 0; iconCol < 4; iconCol++) {
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setlen(p, 4);
        GPU_PRIMITIVE_COLOR_WORD(p, 0) = GPU_PACK_COLOR_WORD(0x60, 0x60, 0x60, 0);
        setcode(p, 0x64);
        addPrim(gGpuCurrentOt + obj->panel.otIndex.signedValue + 1, p);
        p->x0   = D_8010E844[iconCol].xOffset + (obj->panel.contentOriginX.unsignedValue + startX + iconCol * colStep);
        p->y0   = obj->panel.contentOriginY.unsignedValue + panelY - 0x23;
        p->w    = (iconCol == 0) ? 0x18 : 0x20;
        p->h    = 8;
        p->u0   = D_8010E844[iconCol].u;
        p->v0   = D_8010E844[iconCol].v;
        p->clut = ((iconCol == 0) || (iconCol == 3)) ? 0x3C85 : 0x3C86;

        iconSlot = (iconCol + 1) * 3 - 1;
        for (row = 0, baseSlot = iconSlot, markOff = 6; row < 3; row++) {
            colLevels = Gp_GetAttachLevels();
            show      = 0;
            if (row == 0) {
                colLevels += baseSlot;
                if ((colLevels[0] != 0) ||
                    ((colLevels[-1] == 3) && (colLevels[-2] == 3))) {
                    show = 1;
                }
            }
            if ((row != 0) || (show != 0)) {
                p              = gGpuPrimCursor;
                gGpuPrimCursor = p + 1;
                p->x0          = obj->panel.contentOriginX.unsignedValue + startX + iconCol * colStep;
                capY           = obj->panel.contentOriginY.unsignedValue + panelY - markOff;
                p->w           = 8;
                p->h           = 8;
                p->u0          = 0xA8;
                p->v0          = 0x88;
                p->clut        = 0x3C02;
                setlen(p, 4);
                GPU_PRIMITIVE_COLOR_WORD(p, 0) = GPU_PACK_COLOR_WORD(0x60, 0x60, 0x60, 0);
                setcode(p, 0x64);
                p->y0 = capY;
                addPrim(gGpuCurrentOt + obj->panel.otIndex.signedValue + 1, p);
            }
            markOff += 9;
        }
    }
    uiQueueTexturePage(obj->panel.otIndex.signedValue + 1, 0);
}

void Gp_DrawEquipSummary(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TextDrawReq          req;
    EquipmentWeaponLoad* slot;
    s32                  item;
    s32                  color;
    s32                  loadedItemId;
    s32                  count;

    item = gPlayerStatus.weapon;
    if (item > 0) {
        item += 0x7F;
    }
    color = 0x606060;
    Gp_DrawItemLabel(PARENT_OF(arg0, UiObject, panel), arg1, arg2, item, color, 0);
    uiDrawHorizontalSeparator(arg0, arg0->contentLeft.signedValue, arg0->contentRight.signedValue, arg0->contentTop.signedValue + 0x11);
    arg2          += 7;
    req.x          = arg0->contentOriginX.unsignedValue + arg1;
    req.y          = arg0->contentOriginY.unsignedValue + 2 + arg2;
    req.otIndex    = arg0->otIndex.signedValue + 1;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrAmmoCaps);
    arg2 += 0x13;
    if (item > 0) {
        if (item != 0x92) {
            slot         = Gp_GetItemSlot(item);
            loadedItemId = slot->primaryItemId;
            count        = slot->primaryQty;
            if (loadedItemId != 0) {
                Gp_DrawQty(PARENT_OF(arg0, UiObject, panel), arg1, arg2, count, color);
            }
            Gp_DrawItemNameRow(PARENT_OF(arg0, UiObject, panel), arg1, arg2, loadedItemId, color, 0);
            if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
                loadedItemId = slot->secondaryItemId;
                count        = slot->secondaryQty;
                arg2        += 0x10;
                if (loadedItemId != 0) {
                    Gp_DrawQty(PARENT_OF(arg0, UiObject, panel), arg1, arg2, count, color);
                }
                Gp_DrawItemNameRow(PARENT_OF(arg0, UiObject, panel), arg1, arg2, loadedItemId, color, 0);
            }
        }
    }
}

void func_800C22D8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    u8                   buf[2];
    TextDrawReq          req;
    s32                  equipped;
    s32                  hasMod;
    PlayerStatus*        cfg;
    EquipmentWeaponLoad* slot;
    s32                  color;
    s32                  x;
    s32                  y;

    equipped = 0;
    cfg      = &gPlayerStatus;
    buf[0]   = 0;
    buf[1]   = 0;
    if ((((u32)(arg3 - 0x80) < 0x20U) && (cfg->weapon == (arg3 - 0x7F))) ||
        (((u32)(arg3 - 0x60) < 0x20U) && (cfg->armor == (arg3 - 0x5F))) ||
        (((u32)(arg3 - 0xA0) < 0x20U) && (cfg->weapon != PLAYER_STATUS_EQUIPMENT_NONE) &&
         ((Gp_GetItemSlot(cfg->weapon + 0x7F)->primaryItemId == arg3) ||
          (Gp_GetItemSlot(cfg->weapon + 0x7F)->secondaryItemId == arg3)))) {
        equipped = 1;
    }
    if (equipped != 0) {
        buf[0]         = 0x45;
        color          = 0x606060;
        x              = arg0->panel.contentOriginX.unsignedValue - 1;
        req.x          = x + arg1;
        y              = arg0->panel.contentOriginY.unsignedValue - 2;
        req.y          = y + arg2;
        req.otIndex    = arg0->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED_SINGLE_ENTRY;
        textDrawString(&req, Gp_StrE);
    } else {
        hasMod = 0;
        if ((u32)(arg3 - 0x80) < 0x20U) {
            slot = Gp_GetItemSlot(arg3);
            if (((slot->primaryQty != 0) && (Gp_FindItemById(slot->primaryItemId) != NULL)) ||
                ((slot->secondaryQty != 0) && (Gp_FindItemById(slot->secondaryItemId) != NULL))) {
                hasMod = 1;
            }
        }
        if (hasMod != 0) {
            buf[0] = 0x4C;
        } else if (arg4 == 2) {
            buf[0] = 0x41;
        }
    }
    if (buf[0] != 0) {
        color          = 0x606060;
        x              = arg0->panel.contentOriginX.unsignedValue - 1;
        req.x          = x + arg1;
        y              = arg0->panel.contentOriginY.unsignedValue - 2;
        req.y          = y + arg2;
        req.otIndex    = arg0->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED_SINGLE_ENTRY;
        textDrawString(&req, buf);
    }
}

void func_800C2538(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    u8          buf[0x20];
    TextDrawReq req;
    SPRT*       p;
    s32         y;
    s32         textY;
    s32         color;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    p->x0          = arg0->panel.contentOriginX.unsignedValue + arg1 + 0x6C;
    y              = arg0->panel.contentOriginY.unsignedValue;
    color          = arg4;
    p->w           = 8;
    p->h           = 8;
    p->u0          = 0xA8;
    p->v0          = 0x88;
    p->clut        = 0x3C02;
    setlen(p, 4);
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = color;
    setcode(p, 0x64);
    p->y0 = y + arg2 - 7;
    addPrim(gGpuCurrentOt + arg0->panel.otIndex.signedValue + 1, p);
    uiQueueTexturePage(arg0->panel.otIndex.signedValue + 1, 0);

    req.x          = arg0->panel.contentOriginX.unsignedValue + arg1 + 0x7C;
    textY          = arg0->panel.contentOriginY.unsignedValue - 3;
    req.y          = textY + arg2;
    req.otIndex    = arg0->panel.otIndex.signedValue + 1;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req, textItoaSigned(buf, arg3));
}

/// Draws `item`'s name, its `func_800C22D8` marker in `mode`, the variant
/// marker for items 0x0F-0x32 and its icon at (`x`, `y`) in `obj`. Nothing is
/// drawn while `obj->panel.state` is `USER_INTERFACE_PANEL_HIDDEN`.
static inline void _gpDrawItemNameAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item, s32 mode)
{
    TextDrawReq req;
    s32         temp;

    if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
        req.y          = obj->panel.contentOriginY.unsignedValue + (y - 6);
        req.otIndex    = obj->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, Gp_GetItemText(item, 0, 0));
        func_800C22D8(obj, x, y, item, mode);
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }
}

/// Draws `item` as `_gpDrawItemNameAt` does, at the prompt row's position and
/// in its colour.
static inline void _gpDrawItemName(UiList* prompt, UiObject* obj, s32 item, s32 mode)
{
    _gpDrawItemNameAt(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, prompt->colorRgb, item, mode);
}

/// Returns the `index`-th row (from 0) of `scan` that is free to reorder -
/// not attached to armour and not the equipped armour or weapon - or NULL
/// when there are fewer. The out-of-line copy is `func_800CECC0`.
static inline InventoryItemRow* _gpNthLooseRec(InventoryItemRange* scan, s32 index)
{
    PlayerStatus*     p;
    InventoryItemRow* table;
    InventoryItemRow* found;
    s32               i;
    s32               ok;
    s32               id;
    s32               one;
    s32               count;
    s32               n;

    table = Gp_GetItemTable(scan);
    found = NULL;
    i     = 0;
    count = scan->rowCount;
    table = &table[scan->firstRow];
    if (count != 0) {
        p   = &gPlayerStatus;
        one = 1;
        n   = count;
        do {
            id = table->itemId;
            ok = 1;
            if ((table->attachSlot != INVENTORY_ATTACHMENT_NONE) ||
                (((u32)(id - 0x60) < 0x20U) && (p->armor == id - 0x5F)) ||
                (((u32)(id - 0x80) < 0x20U) && (p->weapon == id - 0x7F))) {
                ok = 0;
            }
            if (ok == one) {
                index--;
            }
            if (index < 0) {
                found = table;
                break;
            }
            i++;
            table++;
        } while (i < n);
    }
    return found;
}

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

void Gp_DrawItemOrderRow(UiList* arg0, UiObject* arg1)
{
    InventoryItemRow* sel;
    s32               item;
    s32               status;
    s32               idx1;
    s32               idx2;
    UiObject*         obj;

    sel = _gpNthLooseRec(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg0->currentItemIndex);
    if (sel == NULL) {
        Gp_DrawSortCmd(arg0, arg1);
        return;
    }

    item   = sel->itemId;
    status = arg1->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            if (Gp_ItemOrderMode == 0) {
                GP_SHOW_ITEM_IN_HOLDER(item);
            } else {
                Ui_SetHolderParam(Gp_StrSelectDest, 0, 0);
            }
        }
    }

    if (Gp_ItemOrderMode == 1) {
        if (arg0->rowInputEnabled != USER_INTERFACE_LIST_ROW_ACTIVE) {
            if (sel == Gp_SelItemRec) {
                arg0->colorRgb = 0x37A78;
            }
        }
    }

    {
        s32         x;
        s32         y;
        s32         color;
        u8          buf[0x20];
        TextDrawReq req;
        s32         qty;
        s32         baseY;

        x     = arg0->rowTextX.signedValue;
        y     = arg0->rowTextY.signedValue;
        color = arg0->colorRgb;
        if (sel != NULL) {
            if ((u32)(sel->itemId - 0xA0) < 0x20U) {
                qty            = sel->qty - Gp_CountEquippedRelated(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, sel->itemId);
                req.x          = arg1->panel.contentOriginX.unsignedValue + 0x84 + x;
                baseY          = arg1->panel.contentOriginY.unsignedValue - 3;
                req.y          = baseY + y;
                req.otIndex    = arg1->panel.otIndex.signedValue + 1;
                req.colorRgb   = color;
                req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                req.alignment  = TEXT_ALIGNMENT_RIGHT;
                req.drawMode   = TEXT_DRAW_FILL_ONLY;
                textDrawString(&req, textItoaSigned(buf, qty));
                uiDrawRecessedRect(&arg1->panel, (x + 0x69), (y - 8), 0x1B, 7,
                                   0x102010);
            }
        }
    }

    _gpDrawItemName(arg0, arg1, item, 1);

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (Gp_ItemOrderMode == 0) {
            Gp_SelItemRec = sel;
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                obj = Ui_SpawnFromDesc(&D_8010EE6C, 0, 1, 1, arg1);
                if (obj != NULL) {
                    uiPositionRowDialog(&(obj)->panel, arg0, &(arg1)->panel);
                    arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else {
                Gp_CheckItemInfoButton(arg1);
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            InventoryItemRange* scan2;
            scan2 = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            idx1  = Gp_ScanIndexOf(scan2, Gp_SelItemRec);
            idx2  = Gp_ScanIndexOf(scan2, sel);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if (idx1 >= 0) {
                if (idx2 >= 0) {
                    Gp_MoveItemSlot(scan2, idx1, idx2);
                }
            }
            Gp_ItemOrderMode = 0;
        }
    }
}

void Gp_CountAmmoRows(UiList* arg0, s32 arg1)
{
    register s32               count asm("t0");
    s32                        i;
    register InventoryItemRow* rec asm("a3");
    register s32               item asm("v1");
    s32                        j;
    s32                        off;
    s32                        temp;
    s32                        limit;
    u8*                        rowBytes;
    InventoryItemRow*          rec2;
    InventoryItemRange*        scan;
    PlayerStatus*              cfg;
    const u8*                  table0;
    const u8*                  table1;

    count = 0;
    // Byte offsets select whole rows; accesses resume through the row type.
    rowBytes = (u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
    scan     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    {
        register s32 hi asm("v1");
        asm volatile("lui %1, %%hi(gPlayerStatus)\n\t"
                     "addiu %0, %1, %%lo(gPlayerStatus)"
                     : "=r"(cfg), "=r"(hi));
    }
    limit = scan->rowCount;
    item  = scan->firstRow;
    i     = count;
    if (count < limit) {
        // Search the primary and secondary load choices as packed row bytes.
        table0 = (const u8*)Gp_RelatedQty0.rows;
        table1 = (const u8*)Gp_RelatedQty1.rows;
        temp   = (u8)item * sizeof(InventoryItemRow);
        rec    = (InventoryItemRow*)&rowBytes[temp];
        do {
            if ((u8)(rec->itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(Gp_RelatedQty0.rows)) {
                rec2 = rec;
                if (arg1 == 0) {
                    goto increment;
                }
                j = 0;
                USE_REG(j);
                item = rec->itemId;
                off  = (item - EQUIPMENT_WEAPON_ITEM_FIRST) * sizeof(EquipmentWeaponLoadOptions);
                item = item - 0x7F;
                do {
                    temp = j + off;
                    if (table0[temp + OFFSET_OF(EquipmentWeaponLoadOptions, acceptedItemIds)] == arg1) {
                        if (rec2->attachSlot > INVENTORY_ATTACHMENT_NONE) {
                            count++;
                        } else if (cfg->weapon == item) {
                            count++;
                        }
                        break;
                    }
                    j++;
                } while (j < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds));

                j    = 0;
                item = rec->itemId;
                rec2 = rec;
                off  = (item - EQUIPMENT_WEAPON_ITEM_FIRST) * sizeof(EquipmentWeaponLoadOptions);
                item = item - 0x7F;
                do {
                    temp = j + off;
                    if (table1[temp + OFFSET_OF(EquipmentWeaponLoadOptions, acceptedItemIds)] == arg1) {
                        if (rec2->attachSlot > INVENTORY_ATTACHMENT_NONE) {
                            goto increment;
                        }
                        if (cfg->weapon != item) {
                            goto next;
                        }
                    increment:
                        count++;
                        goto next;
                    }
                    j++;
                } while (j < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds));
            }
        next:
            rec++;
            i++;
        } while (i < scan->rowCount);
    }

    arg0->itemCount                     = count;
    arg0->visibleRowCount.unsignedValue = count;
    if (arg1 == 0) {
        arg0->rowHeight                     = 0xF;
        arg0->visibleRowCount.unsignedValue = 4;
    } else {
        arg0->rowHeight = 0xF;
    }
}

static __inline__ void countItemRows(UiList* menu)
{
    InventoryItemRange* scan;
    InventoryItemRow*   table;
    s32                 i;
    u16                 count;
    s32                 ok;
    s32                 id;

    scan            = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    table           = Gp_GetItemTable(scan);
    table           = &table[scan->firstRow];
    menu->itemCount = scan->rowCount;
    count           = scan->rowCount;
    for (i = 0; i < count; i++, table++) {
        id = table->itemId;
        ok = 1;
        if ((table->attachSlot != INVENTORY_ATTACHMENT_NONE) ||
            (((u32)(id - 0x60) < 0x20U) && (gPlayerStatus.armor == id - 0x5F)) ||
            (((u32)(id - 0x80) < 0x20U) && (gPlayerStatus.weapon == id - 0x7F))) {
            ok = 0;
        }
        if (ok == 0) {
            menu->itemCount--;
        }
    }
    menu->itemCount                     = menu->itemCount + 1;
    menu->visibleRowCount.unsignedValue = menu->itemCount;
    if (menu->visibleRowCount.signedValue >= 0xA) {
        menu->visibleRowCount.unsignedValue = 9;
    }
}

static void Gp_ItemListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    UiObject* child;
    void*     workAllocation;
    s32       status;
    Task*     owner;
    Task*     head;
    Task*     node;
    Task*     next;
    UiObject* childObj;
    s32       flag;
    s32       one;
    s32       mask;

    obj         = arg0->spawnArg2.pointer;
    menu        = &D_8010E854;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        Gp_ItemOrderMode = 0;
        workAllocation   = memCalloc(4, 0);
        if (workAllocation == NULL) {
            uiStartTreeClosing(obj, arg0);
            return;
        }
        arg0->work           = workAllocation;
        menu->wrapNavigation = 0;
        countItemRows(menu);
        menu->visibleRowCount.unsignedValue = 9;
        menu->itemCount                     = 9;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        countItemRows(menu);
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        child                                     = Ui_SpawnFromDesc(&D_8010EB40, 0, 0, 1, obj);
        if (child != NULL) {
            child->panel.bounds.unsignedRect.y = obj->panel.bounds.unsignedRect.y + obj->panel.bounds.unsignedRect.h;
        }
        arg0->state = arg0->state + 1;
    }
    uiDrawPanelLabel(&(obj)->panel, Gp_StrItemHdr);
    countItemRows(menu);
    Ui_ComputeVisibleRows(menu, &(obj)->panel);
    menu->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
    if (menu->selectedItemIndex >= menu->itemCount) {
        menu->selectedItemIndex = menu->itemCount - 1;
    }
    if ((menu->itemCount - menu->visibleRowCount.signedValue) < menu->firstVisibleItemIndex.signedValue) {
        menu->firstVisibleItemIndex.unsignedValue = menu->itemCount - menu->visibleRowCount.unsignedValue;
    }
    Ui_UpdateListNoAnim(menu, obj);
    status = obj->panel.control.word;
    if (status == 1) {
        if (obj->result == USER_INTERFACE_RESULT_NONE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                obj->result = USER_INTERFACE_RESULT_CANCEL;
            } else if (Gp_ItemOrderMode == 0) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                    obj->resultValue = 1;
                    obj->result      = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_R2);
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                Gp_ItemOrderMode = 0;
            }
        }
    } else if (status >= 2) {
        obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }
    owner = obj->owner;
    head  = owner->firstChild;
    if (head != NULL) {
        node = head;
        one  = 1;
        mask = (u32)~USER_INTERFACE_PANEL_DIMMED;
        do {
            childObj = node->spawnArg2.pointer;
            flag     = childObj->result;
            next     = node->nextSibling;
            switch (flag) {
                case USER_INTERFACE_RESULT_CANCEL:
                    obj->result = flag;
                    break;
                case USER_INTERFACE_RESULT_CONFIRM:
                    uiStartTreeClosing(childObj, childObj->owner);
                    obj->panel.control.word = one;
                    obj->panel.style       &= mask;
                    break;
                case 0x23:
                    uiStartTreeClosing(childObj, childObj->owner);
                    obj->panel.control.word = one;
                    Gp_ItemOrderMode        = one;
                    obj->panel.style       &= mask;
                    break;
            }
            head = owner->firstChild;
            node = next;
            if (node == head) {
                break;
            }
        } while (head != NULL);
    }
}

void Gp_ItemDestCursorTask(Task* arg0)
{
    UiObject*        obj;
    UiObject*        child;
    UiObjectDesc*    desc;
    s32              one;
    s16              y;
    UiCursorPosition cursor;

    obj = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        desc             = &D_8010EFD8;
        one              = 1;
        Gp_ItemCountShow = 0;
        D_80114D98[0]    = Ui_SpawnFromDesc(desc, one, one, one, obj);
        D_80114D98[1]    = Ui_SpawnFromDesc(desc + 1, one, 0, one, obj);
    }
    Gp_ItemListTask(arg0);
    if ((Gp_ItemCountShow == 1) && (uiIsPanelHidingOrHidden(obj) == 0)) {
        uiStartPanelHiding(obj, obj->owner);
    } else if ((Gp_ItemCountShow == 0) && (uiIsPanelHidingOrHidden(obj) == 1)) {
        uiLimitHiddenDelayOrOpen(&(obj)->panel, obj->owner, 0x10);
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            child          = D_80114D98[1];
            *(s32*)&cursor = uiGetCursorPositionWord();
            if (cursor.y.signedValue < child->panel.bounds.rect.y) {
                child = D_80114D98[0];
            }
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            obj->panel.control.word   = USER_INTERFACE_PANEL_INACTIVE;
            y                         = cursor.y.signedValue;
            child->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
            child->resultValue        = y;
        }
    }
}

/// Replaces the layout position a spawned child copied from its descriptor
/// with `x`, `y`.
static inline void _gpSetSpawnOffset(UiObject* obj, s32 x, s32 y)
{
    obj->panel.bounds.unsignedRect.y = y;
    obj->panel.bounds.unsignedRect.x = x;
}

void Gp_DrawWeaponSlotRow(UiList* prompt, UiObject* obj)
{
    TextDrawReq   req;
    PlayerStatus* player;
    s32           item;
    s32           status;
    s32           mode;
    s32           x;
    s32           y;
    s32           color;
    s32           temp;

    player = &gPlayerStatus;
    item   = player->weapon + 0x7F;
    if (item < 0x80) {
        item = 0;
    }

    status = obj->panel.control.word;
    if (((status >> 16) == 1 || status == 1) && prompt->selectedItemIndex == prompt->currentItemIndex) {
        if (Gp_ItemOrderMode == 0) {
            if (item == 0) {
                Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            } else {
                Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
            }
            Gp_SetPreviewItem(item, 0);
        } else {
            Ui_SetHolderParam(Gp_StrSelectDest, 0, 0);
        }
    }

    status = prompt->rowInputEnabled;
    if (status == 1) {
        mode = Gp_ItemOrderMode;
        if (mode == 0) {
            InventoryItemRange* scan;
            InventoryItemRow*   table;
            s32                 i;
            s32                 count;

            scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            table = Gp_GetItemTable(scan);
            if (item != 0) {
                table = &table[scan->firstRow];
                count = scan->rowCount;
                for (i = 0; i < count; i++, table++) {
                    if (table->itemId == item) {
                        break;
                    }
                }
                Gp_SelItemRec = table;
            } else {
                Gp_SelItemRec = NULL;
            }
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                Gp_CountAmmoRows(&D_8010E9A4, 0);
                if (D_8010E9A4.itemCount >= 2U || (D_8010E9A4.itemCount == 1 && player->weapon == PLAYER_STATUS_EQUIPMENT_NONE)) {
                    UiObject* spawned;
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    spawned = Ui_SpawnFromDesc(&D_8010ECE4, 0, 1, 0x10, obj);
                    if (spawned != NULL) {
                        _gpSetSpawnOffset(spawned, -8, -0x5C);
                    }
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                } else {
                    Gp_SpawnItemPrompt(obj, 0x14, 0, 1);
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else {
                Gp_CheckItemInfoButton(obj);
            }
        } else if (mode == status) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                if ((u8)(Gp_SelItemRec->itemId + 0x80) < 0x20) {
                    PlayerStatus*     p;
                    InventoryItemRow* rec;

                    p = &gPlayerStatus;
                    Gp_ClearEquipSlotSel(item, 0);
                    rec       = Gp_SelItemRec;
                    p->weapon = rec->itemId - 0x7F;
                    Gp_SetItemSeenBit(rec->itemId, 1);
                    Gp_ItemOrderMode = 0;
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                } else {
                    Task* parent;
                    parent = obj->owner->parent;
                    if (parent != NULL) {
                        Gp_ItemOrderMode                                           = 0;
                        ((UiObject*)parent->spawnArg2.pointer)->panel.control.word = mode;
                        obj->panel.control.word                                    = USER_INTERFACE_PANEL_INACTIVE;
                        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    }
                }
            }
        }
    }

    x     = prompt->rowTextX.signedValue;
    y     = prompt->rowTextY.signedValue;
    color = prompt->colorRgb;
    if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
        req.y          = obj->panel.contentOriginY.unsignedValue + (y - 6);
        req.otIndex    = obj->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, Gp_GetItemText(item, 0, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x11);

    req.x          = obj->panel.contentOriginX.unsignedValue + prompt->rowTextX.signedValue;
    req.y          = prompt->rowTextY.signedValue + (obj->panel.contentOriginY.unsignedValue + 9);
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = 0x606060;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrAmmoCaps);
    prompt->rowTextY.signedValue += 0xA;
}

void Gp_DrawWeaponSlotRow2(UiList* prompt, UiObject* obj)
{
    union {
        struct {
            u8          buf[0x20];
            TextDrawReq req;
        } qty;
        TextDrawReq name;
    } draw;
    s32                  item;
    s32                  count;
    s32                  weapon;
    s32                  status;
    s32                  rowState;
    s32                  mode;
    EquipmentWeaponLoad* slot;
    InventoryItemRow*    rec;
    UiObject*            child;
    Task*                parent;
    UiObject*            parentObj;

    item   = 0;
    count  = 0;
    weapon = gPlayerStatus.weapon + 0x7F;
    if (weapon >= 0x80) {
        slot = Gp_GetItemSlot(weapon);
        if (prompt->currentItemIndex == 1) {
            item  = slot->primaryItemId;
            count = slot->primaryQty;
        } else {
            item  = slot->secondaryItemId;
            count = slot->secondaryQty;
        }
    }
    status = obj->panel.control.word;
    if (((status >> 16) == 1 || status == 1) && prompt->selectedItemIndex == prompt->currentItemIndex) {
        if (Gp_ItemOrderMode == 0) {
            if (item != 0) {
                Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
                Gp_SetPreviewItem(item, 0);
            } else {
                Ui_SetHolderParam(Gp_StrAmmoNone, 0, 0);
            }
        } else {
            Ui_SetHolderParam(Gp_StrSelectDest, 0, 0);
        }
    }
    if (item != 0) {
        s32 x;
        s32 y;
        s32 color;
        s32 off;
        x                       = prompt->rowTextX.signedValue;
        y                       = prompt->rowTextY.signedValue;
        color                   = prompt->colorRgb;
        draw.qty.req.x          = obj->panel.contentOriginX.unsignedValue + 0x84 + x;
        off                     = obj->panel.contentOriginY.unsignedValue - 3;
        draw.qty.req.y          = off + y;
        draw.qty.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        draw.qty.req.colorRgb   = color;
        draw.qty.req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        draw.qty.req.alignment  = TEXT_ALIGNMENT_RIGHT;
        draw.qty.req.drawMode   = TEXT_DRAW_FILL_ONLY;
        textDrawString(&draw.qty.req, textItoaSigned(draw.qty.buf, count));
        uiDrawRecessedRect(&obj->panel, x + 0x69, y - 8, 0x1B, 7, 0x102010);
    }
    {
        s32 x;
        s32 y;
        s32 color;
        s32 off;
        s32 temp;
        x     = prompt->rowTextX.signedValue;
        y     = prompt->rowTextY.signedValue;
        color = prompt->colorRgb;
        if (item == 0) {
            uiDrawRecessedRect(&obj->panel, x, y - 0xE, 0xE, 0xE, 0x102010);
        } else {
            if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                draw.name.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
                off                  = obj->panel.contentOriginY.unsignedValue - 6;
                draw.name.y          = off + y;
                draw.name.otIndex    = obj->panel.otIndex.signedValue + 1;
                draw.name.colorRgb   = color;
                draw.name.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                draw.name.alignment  = TEXT_ALIGNMENT_LEFT;
                draw.name.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&draw.name, Gp_GetItemText(item, 0, 0));
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    func_800C2538(obj, x, y, temp % 3 + 1, color);
                }
                Gp_DrawItemIcon(obj, x, y, item, 0);
            }
            uiDrawRecessedRect(&obj->panel, x, y - 0xE, 0xE, 0xE, 0);
        }
    }
    rowState = prompt->rowInputEnabled;
    if (rowState == 1) {
        Gp_ReloadMode = prompt->currentItemIndex;
        mode          = Gp_ItemOrderMode;
        if (mode == 0) {
            rec = NULL;
            if (item != 0) {
                rec = Gp_FindItemById(item);
            }
            Gp_SelItemRec = rec;
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                s32 currentWeapon;
                s32 yOffset;
                s32 xOffset;
                currentWeapon = gPlayerStatus.weapon + 0x7F;
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                child = Ui_SpawnFromDesc(&D_8010ECC8, currentWeapon, 1, 0x10, obj);
                if (child != NULL) {
                    yOffset                            = -0x5C;
                    child->panel.bounds.unsignedRect.y = yOffset;
                    xOffset                            = -8;
                    child->panel.bounds.unsignedRect.x = xOffset;
                }
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                Gp_CheckItemInfoButton(obj);
            }
        } else if (mode == rowState) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                if (Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, weapon, Gp_SelItemRec->itemId, -1) >= 0) {
                    Gp_SetItemSeenBit(Gp_SelItemRec->itemId, 1);
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    Gp_ItemOrderMode = 0;
                } else {
                    parent = obj->owner->parent;
                    if (parent != NULL) {
                        parentObj = parent->spawnArg2.pointer;
                        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                        Gp_ItemOrderMode              = 0;
                        parentObj->panel.control.word = mode;
                        obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                    }
                }
            }
        }
    }
}

/// Sets the weapon menu's row count from the current weapon's item: one row
/// for weapon index 0 and for item 0x92, otherwise three when the item's
/// load supports a secondary consumable slot (`secondaryItemId` !=
/// `EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE`) and two when it does not.
static inline void _gpWeaponMenuSetRows(UiList* menu)
{
    s32                  id;
    EquipmentWeaponLoad* slot;

    id   = gPlayerStatus.weapon + 0x7F;
    slot = Gp_GetItemSlot(id);
    if (id < 0x80 || id == 0x92) {
        menu->itemCount = 1;
    } else if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
        menu->itemCount = 3;
    } else {
        menu->itemCount = 2;
    }
}

void Gp_WeaponMenuTask(Task* arg0)
{
    UiObject*        obj;
    UiList*          menu;
    s32              status;
    Task*            owner;
    Task*            child;
    Task*            next;
    Task*            head;
    UiObject*        childObj;
    s32              one;
    s32              mask;
    s32              flag;
    UiCursorPosition cursor;

    obj         = arg0->spawnArg2.pointer;
    menu        = &D_8010E884;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrWeaponTitle);
    if (arg0->state == 0) {
        _gpWeaponMenuSetRows(menu);
        menu->selectedItemIndex = 0;
        Ui_InitList(menu, &(obj)->panel);
        arg0->state = arg0->state + 1;
    }
    _gpWeaponMenuSetRows(menu);
    Ui_ComputeVisibleRows(menu, &(obj)->panel);
    Ui_UpdateListNoAnim(menu, obj);
    if ((Gp_ItemCountShow == 1) && (uiIsPanelHidingOrHidden(obj) == 0)) {
        uiStartPanelHiding(obj, obj->owner);
    } else if ((Gp_ItemCountShow == 0) && (uiIsPanelHidingOrHidden(obj) == 1)) {
        uiLimitHiddenDelayOrOpen(&(obj)->panel, obj->owner, 0x10);
    }
    status = obj->panel.control.word;
    if (status == 1) {
        if (menu->actionResult == USER_INTERFACE_LIST_ACTION_AT_END) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            D_80114D98[1]->resultValue        = -0xA0;
            D_80114D98[1]->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
            obj->panel.control.word           = USER_INTERFACE_PANEL_INACTIVE;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            Task*     parent;
            UiObject* parentObj;

            parent = arg0->parent;
            if (parent != 0) {
                UiList* other;
                s16     row;
                s32     sel;
                s32     row9;
                s32     vis;

                parentObj = parent->spawnArg2.pointer;
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                *(s32*)&cursor           = uiGetCursorPositionWord();
                other                    = &D_8010E854;
                row                      = cursor.y.unsignedValue - (parentObj->panel.contentOriginY.unsignedValue + parentObj->panel.contentTop.unsignedValue);
                row                      = row / other->rowHeight;
                vis                      = other->visibleRowCount.signedValue;
                row9                     = other->firstVisibleItemIndex.signedValue;
                sel                      = row + row9;
                other->selectedItemIndex = sel;
                if (sel >= row9 + vis) {
                    other->selectedItemIndex = row9 + vis - 1;
                }
                if (other->selectedItemIndex >= other->itemCount) {
                    other->selectedItemIndex = other->itemCount - 1;
                }
                parentObj->panel.control.word = status;
                obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            Task*     parent;
            UiObject* parentObj;

            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            parent = arg0->parent;
            if (parent != 0) {
                parentObj = parent->spawnArg2.pointer;
                if (Gp_ItemOrderMode == 0) {
                    parentObj->resultValue = status;
                    parentObj->result      = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    parentObj->panel.control.word = status;
                    obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                    Gp_ItemOrderMode              = 0;
                }
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
    owner = obj->owner;
    head  = owner->firstChild;
    if (head != NULL) {
        child = head;
        one   = 1;
        mask  = (u32)~USER_INTERFACE_PANEL_DIMMED;
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
                    obj->panel.control.word = one;
                    obj->panel.style       &= mask;
                    break;
                case 0x23:
                    uiStartTreeClosing(childObj, childObj->owner);
                    obj->panel.control.word = one;
                    Gp_ItemOrderMode        = one;
                    obj->panel.style       &= mask;
                    break;
            }
            head  = owner->firstChild;
            child = next;
            if (child == head) {
                break;
            }
        } while (head != NULL);
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_FOCUS_TRANSFER) {
        s32 t;

        t  = obj->panel.contentOriginY.signedValue;
        t += obj->panel.contentTop.signedValue;
        t  = obj->resultValue - t;
        if (t < menu->rowHeight) {
            menu->selectedItemIndex = 0;
        } else if ((menu->rowHeight * 2 + 0xA) >= t) {
            menu->selectedItemIndex = 1;
        } else if (menu->itemCount < 2) {
            menu->selectedItemIndex = 1;
        } else {
            menu->selectedItemIndex = 2;
        }
        obj->resultValue        = 0;
        obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }
}

void func_800C41A4(UiList* prompt, UiObject* obj)
{
    union {
        struct {
            u8          buf[0x20];
            TextDrawReq req;
        } qty;
        TextDrawReq name;
    } draw;
    s32               item;
    s32               status;
    s32               rowState;
    s32               mode;
    InventoryItemRow* rec;
    UiObject*         child;
    UiObject*         dialog;
    Task*             parent;
    UiObject*         parentObj;
    {
        InventoryItemRange* scan;
        InventoryItemRow*   table;
        InventoryItemRow*   found;
        s32                 row;
        s32                 i;
        row   = prompt->currentItemIndex;
        scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        table = Gp_GetItemTable(scan);
        found = NULL;
        i     = 0;
        item  = 0;
        table = &table[scan->firstRow];
        for (; i < scan->rowCount; i++, table++) {
            if (table->attachSlot == row + 1) {
                found = table;
                break;
            }
        }
        rec = found;
    }
    if (rec != NULL) {
        item = rec->itemId;
    }
    status = obj->panel.control.word;
    if (((status >> 16) == 1 || status == 1) && prompt->selectedItemIndex == prompt->currentItemIndex) {
        if (Gp_ItemOrderMode == 0) {
            if (item != 0) {
                Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
                Gp_SetPreviewItem(item, 0);
            } else {
                Ui_SetHolderParam(Gp_StrAttachNone, 0, 0);
            }
        } else {
            Ui_SetHolderParam(Gp_StrSelectDest, 0, 0);
        }
    }
    if (rec != NULL) {
        s32 x;
        s32 y;
        s32 color;
        s32 off;
        s32 id;
        s32 count;
        id    = rec->itemId;
        x     = prompt->rowTextX.signedValue;
        y     = prompt->rowTextY.signedValue;
        color = prompt->colorRgb;
        if ((u32)(id - 0xA0) < 0x20U) {
            count                   = rec->qty - Gp_CountEquippedRelated(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, id);
            draw.qty.req.x          = obj->panel.contentOriginX.unsignedValue + 0x84 + x;
            off                     = obj->panel.contentOriginY.unsignedValue - 3;
            draw.qty.req.y          = off + y;
            draw.qty.req.otIndex    = obj->panel.otIndex.signedValue + 1;
            draw.qty.req.colorRgb   = color;
            draw.qty.req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            draw.qty.req.alignment  = TEXT_ALIGNMENT_RIGHT;
            draw.qty.req.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&draw.qty.req, textItoaSigned(draw.qty.buf, count));
            uiDrawRecessedRect(&obj->panel, x + 0x69, y - 8, 0x1B, 7, 0x102010);
        }
    }
    {
        s32 x;
        s32 y;
        s32 color;
        s32 off;
        s32 temp;
        s32 one;
        one   = 1;
        x     = prompt->rowTextX.signedValue;
        y     = prompt->rowTextY.signedValue;
        color = prompt->colorRgb;
        if (item == 0) {
            uiDrawRecessedRect(&obj->panel, x, y - 0xE, 0xE, 0xE, 0x102010);
        } else {
            if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                draw.name.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
                off                  = obj->panel.contentOriginY.unsignedValue - 6;
                draw.name.y          = off + y;
                draw.name.otIndex    = obj->panel.otIndex.signedValue + 1;
                draw.name.colorRgb   = color;
                draw.name.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                draw.name.alignment  = TEXT_ALIGNMENT_LEFT;
                draw.name.drawMode   = one;
                textDrawString(&draw.name, Gp_GetItemText(item, 0, 0));
                func_800C22D8(obj, x, y, item, one);
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    func_800C2538(obj, x, y, temp % 3 + 1, color);
                }
                Gp_DrawItemIcon(obj, x, y, item, 0);
            }
            uiDrawRecessedRect(&obj->panel, x, y - 0xE, 0xE, 0xE, 0);
        }
    }
    rowState = prompt->rowInputEnabled;
    if (rowState == 1) {
        mode = Gp_ItemOrderMode;
        if (mode == rowState) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                const ItemDesc* desc;
                desc = &Gp_ItemDescs[Gp_SelItemRec->itemId];
                if (!(desc->flags & ITEM_FLAG_NO_ATTACHMENT)) {
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    if (Gp_SelItemRec->itemId != INVENTORY_ITEM_NONE) {
                        Gp_SelItemRec->attachSlot = prompt->currentItemIndex + 1;
                    }
                    if (item != 0) {
                        Gp_RefreshItemRow(rec);
                    }
                    Gp_ItemOrderMode = 0;
                } else {
                    parent = obj->owner->parent;
                    if (parent != NULL) {
                        parentObj                     = parent->spawnArg2.pointer;
                        Gp_ItemOrderMode              = 0;
                        parentObj->panel.control.word = mode;
                        obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                    }
                }
            }
        } else {
            if (rec == NULL) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    child = Ui_SpawnFromDesc(&D_8010ED00, 0, 1, 0x10, obj);
                    if (child != NULL) {
                        s32 yOffset;
                        s32 xOffset;
                        yOffset                            = -0x5C;
                        child->panel.bounds.unsignedRect.y = yOffset;
                        xOffset                            = -8;
                        child->panel.bounds.unsignedRect.x = xOffset;
                    }
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
                Gp_SelItemRec = rec;
            } else {
                Gp_SelItemRec = rec;
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    dialog = Ui_SpawnFromDesc(&D_8010EE6C, 4, 1, 1, obj);
                    if (dialog != NULL) {
                        uiPositionRowDialog(&(dialog)->panel, prompt, &(obj)->panel);
                        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    }
                } else {
                    Gp_CheckItemInfoButton(obj);
                }
            }
        }
    }
}

/// Keeps the armor attachment selection within the visible rows and item count.
static inline void _gpClampArmorRow(UiList* menu, s32 end)
{
    if (menu->selectedItemIndex >= end) {
        menu->selectedItemIndex = end - 1;
    }
    if (menu->selectedItemIndex >= menu->itemCount) {
        menu->selectedItemIndex = menu->itemCount - 1;
    }
}

void Gp_ArmorMenuTask(Task* arg0)
{
    UiObject*     obj;
    UiList*       menu;
    s32           item;
    s32           color;
    PlayerStatus* cfg;
    s32           status;
    s32           x;
    s32           y;
    struct {
        TextDrawReq      req;
        UiCursorPosition cursor;
        s32              pad0;
        s16              x;
        s16              y;
        s32              pad[2];
    } locals;

    menu        = &D_8010E8AC;
    obj         = arg0->spawnArg2.pointer;
    cfg         = &gPlayerStatus;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrArmor);

    if (arg0->state == 0) {
        s32 id;
        s32 temp;

        id = cfg->armor + 0x5F;
        if (id != 0) {
            menu->itemCount = Gp_GetModLevel(id);
        }
        menu->visibleRowCount.unsignedValue = menu->itemCount;
        if ((s8)menu->itemCount >= 4) {
            menu->visibleRowCount.unsignedValue = 3;
        }
        if ((menu->itemCount - menu->visibleRowCount.signedValue) < menu->firstVisibleItemIndex.signedValue) {
            menu->firstVisibleItemIndex.unsignedValue = 0;
        }
        if (menu->scrollPixelsRemaining == 0) {
            temp = menu->firstVisibleItemIndex.signedValue + menu->visibleRowCount.signedValue;
            if (menu->selectedItemIndex >= temp) {
                menu->selectedItemIndex = temp - 1;
            }
        }
        Ui_InitList(menu, &(obj)->panel);
        menu->topInset = 0x1A;
        menu->flags    = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        arg0->state    = arg0->state + 2;
    }

    item = cfg->armor;
    if (item > 0) {
        item += 0x5F;
    }
    {
        s32 id;
        s32 temp;

        id = gPlayerStatus.armor + 0x5F;
        if (id != 0) {
            menu->itemCount = Gp_GetModLevel(id);
        }
        {
            u8 n;
            n                                   = menu->itemCount;
            menu->visibleRowCount.unsignedValue = n;
            if ((s8)n >= 4) {
                menu->visibleRowCount.unsignedValue = 3;
            }
        }
        if ((menu->itemCount - menu->visibleRowCount.signedValue) < menu->firstVisibleItemIndex.signedValue) {
            menu->firstVisibleItemIndex.unsignedValue = 0;
        }
        if (menu->scrollPixelsRemaining == 0) {
            temp = menu->firstVisibleItemIndex.signedValue + menu->visibleRowCount.signedValue;
            if (menu->selectedItemIndex >= temp) {
                menu->selectedItemIndex = temp - 1;
            }
        }
        Ui_ComputeVisibleRows(menu, &(obj)->panel);
        menu->topInset = 0x1A;
        menu->flags    = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
    }

    if ((Gp_ItemCountShow == 1) && (uiIsPanelHidingOrHidden(obj) == 0)) {
        uiStartPanelHiding(obj, obj->owner);
    } else if ((Gp_ItemCountShow == 0) && (uiIsPanelHidingOrHidden(obj) == 1)) {
        uiLimitHiddenDelayOrOpen(&(obj)->panel, obj->owner, 0x10);
    }

    color = Ui_LookupTable(obj, 1);

    if (arg0->state == 2) {
        {
            s32 t;
            s32 one;
            t   = obj->panel.control.word;
            one = 1;
            if (((t >> 16) == one) || (t == one)) {
                if (Gp_ItemOrderMode == 0) {
                    if (item == 0) {
                        Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
                    } else {
                        Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
                    }
                    Gp_SetPreviewItem(item, 0);
                } else {
                    Ui_SetHolderParam(Gp_StrSelectDest, 0, 0);
                }
            }
        }
        status                  = obj->panel.control.word;
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        Ui_UpdateListNoAnim(menu, obj);
        obj->panel.control.word = status;
        if (status == 1) {
            uiEaseAndDrawCursor(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentTop.signedValue + 7);
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                arg0->state             = status;
                menu->selectedItemIndex = menu->firstVisibleItemIndex.signedValue;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                D_80114D98[0]->panel.control.word = status;
                D_8010E884.selectedItemIndex      = D_8010E884.itemCount - 1;
                obj->panel.control.word           = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                s32 flag;
                flag = Gp_ItemOrderMode;
                if (flag == 0) {
                    InventoryItemRange* scan;
                    InventoryItemRow*   table;
                    s32                 i;

                    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                    table = Gp_GetItemTable(scan);
                    table = &table[scan->firstRow];
                    for (i = 0; i < scan->rowCount; i++, table++) {
                        if (table->itemId == item) {
                            locals.x      = obj->panel.contentLeft.signedValue + 2;
                            Gp_SelItemRec = table;
                            locals.y      = obj->panel.contentTop.unsignedValue + 0xF;
                            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                                func_800CF090(&D_8010E9F4, obj);
                                if (D_8010E9F4.itemCount != 0) {
                                    UiObject* spawned;
                                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                                    spawned = Ui_SpawnFromDesc(&D_8010ECAC, 0, 1, 0x10, obj);
                                    if (spawned != NULL) {
                                        s32 yOffset;
                                        s32 xOffset;
                                        yOffset                              = -0x5C;
                                        spawned->panel.bounds.unsignedRect.y = yOffset;
                                        xOffset                              = -8;
                                        spawned->panel.bounds.unsignedRect.x = xOffset;
                                    }
                                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                                } else {
                                    Gp_SpawnItemPrompt(obj, 0x15, 0, 1);
                                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                                }
                            } else {
                                Gp_CheckItemInfoButton(obj);
                            }
                            break;
                        }
                    }
                } else if ((flag == status) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0)) {
                    if ((u32)(Gp_SelItemRec->itemId - 0x60) < 0x20U) {
                        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                        Gp_EquipMod(Gp_SelItemRec->itemId);
                        Gp_SetItemSeenBit(Gp_SelItemRec->itemId, 1);
                        Gp_ItemOrderMode = 0;
                    } else {
                        Task* parent;
                        parent = arg0->parent;
                        if (parent != 0) {
                            UiObject* po;
                            po                      = parent->spawnArg2.pointer;
                            Gp_ItemOrderMode        = 0;
                            po->panel.control.word  = flag;
                            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                        }
                    }
                }
            }
        }
    } else {
        s32 val;
        Ui_UpdateListNoAnim(menu, obj);
        val = menu->actionResult;
        if (val == USER_INTERFACE_LIST_ACTION_AT_START) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            arg0->state = val;
        }
    }

    x = obj->panel.contentLeft.signedValue + 2;
    y = obj->panel.contentTop.signedValue + 0xF;
    if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        s32 off;
        s32 temp;
        locals.req.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
        off                   = obj->panel.contentOriginY.unsignedValue - 6;
        locals.req.y          = off + y;
        locals.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        locals.req.colorRgb   = color;
        locals.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        locals.req.alignment  = TEXT_ALIGNMENT_LEFT;
        locals.req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&locals.req, Gp_GetItemText(item, 0, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }

    if ((obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (arg0->state == 2)) {
        obj->panel.otIndex.signedValue += 1;
        uiFillRectInterior(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentTop.signedValue,
                           (obj->panel.contentRight.signedValue - obj->panel.contentLeft.signedValue) - 1, 0x10, 0x1741FU);
        obj->panel.otIndex.signedValue -= 1;
    }

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x11);

    {
        s32 grey;
        grey                  = 0x606060;
        locals.req.x          = obj->panel.contentOriginX.unsignedValue + 2 + obj->panel.contentLeft.signedValue;
        locals.req.y          = obj->panel.contentOriginY.unsignedValue + 0x18 + obj->panel.contentTop.unsignedValue;
        locals.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        locals.req.colorRgb   = grey;
        locals.req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        locals.req.alignment  = TEXT_ALIGNMENT_LEFT;
        locals.req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&locals.req, Gp_StrAttachments2);
    }

    {
        s32 st;
        st = obj->panel.control.word;
        if (st == 1) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                obj->result = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                Task*     parent;
                UiObject* parentObj;
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                parent = arg0->parent;
                if (parent != 0) {
                    parentObj = parent->spawnArg2.pointer;
                    if (Gp_ItemOrderMode == 0) {
                        parentObj->resultValue = st;
                        parentObj->result      = USER_INTERFACE_RESULT_CONFIRM;
                    } else {
                        parentObj->panel.control.word = st;
                        obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                        Gp_ItemOrderMode              = 0;
                    }
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                Task*     parent;
                UiObject* parentObj;
                parent = arg0->parent;
                if (parent != 0) {
                    UiList* other;
                    s16     row;
                    s32     sel;
                    s32     row9;
                    s32     vis;

                    parentObj             = parent->spawnArg2.pointer;
                    *(s32*)&locals.cursor = uiGetCursorPositionWord();
                    sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                    other                    = &D_8010E854;
                    row                      = locals.cursor.y.unsignedValue - (parentObj->panel.contentOriginY.unsignedValue + parentObj->panel.contentTop.unsignedValue);
                    row                      = row / other->rowHeight;
                    vis                      = other->visibleRowCount.signedValue;
                    row9                     = other->firstVisibleItemIndex.signedValue;
                    sel                      = row + row9;
                    other->selectedItemIndex = sel;
                    if (sel >= row9 + vis) {
                        other->selectedItemIndex = row9 + vis - 1;
                    }
                    if (other->selectedItemIndex >= other->itemCount) {
                        other->selectedItemIndex = other->itemCount - 1;
                    }
                    parentObj->panel.control.word = st;
                    obj->panel.control.word       = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        }
    }

    if (obj->panel.control.word == USER_INTERFACE_PANEL_FOCUS_TRANSFER) {
        s32 t;

        t = obj->resultValue - (obj->panel.contentOriginY.signedValue + obj->panel.contentTop.signedValue);
        if (t < 0xF) {
            arg0->state = 2;
        } else {
            s32 h;

            arg0->state = 1;
            h           = menu->rowHeight;
            t          -= h * 2 + 0xA;
            if (t < 0) {
                menu->selectedItemIndex = menu->firstVisibleItemIndex.signedValue;
            } else {
                s32          row9;
                s32          f5;
                register s32 vis asm("a0");
                t    = t / h;
                row9 = menu->firstVisibleItemIndex.signedValue;
                f5   = menu->visibleRowCount.signedValue;
                vis  = row9;
                TOUCH_REG(vis);
                vis                     = vis + f5;
                t                       = t + 1;
                menu->selectedItemIndex = t + row9;
                _gpClampArmorRow(menu, vis);
            }
        }
        obj->resultValue        = 0;
        obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }

    {
        Task*     owner;
        Task*     head;
        Task*     child;
        Task*     next;
        UiObject* childObj;
        s32       one;
        s32       mask;
        s32       flag;

        owner = obj->owner;
        head  = owner->firstChild;
        if (head != NULL) {
            child = head;
            one   = 1;
            mask  = (u32)~USER_INTERFACE_PANEL_DIMMED;
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
                        obj->panel.control.word = one;
                        obj->panel.style       &= mask;
                        break;
                    case 0x23:
                        uiStartTreeClosing(childObj, childObj->owner);
                        obj->panel.control.word = one;
                        Gp_ItemOrderMode        = one;
                        obj->panel.style       &= mask;
                        break;
                }
                head  = owner->firstChild;
                child = next;
                if (child == head) {
                    break;
                }
            } while (head != NULL);
        }
    }
}

/// Whether item `id` is the equipped weapon, the equipped armour, or a
/// consumable selected in either firing mode of the equipped weapon.
static inline s32 _gpIsEquippedItem(s32 id)
{
    s32           ret;
    PlayerStatus* p;

    ret = 0;
    p   = &gPlayerStatus;
    if ((((u32)(id - 0x80) < 0x20U) && (p->weapon == id - 0x7F)) ||
        (((u32)(id - 0x60) < 0x20U) && (p->armor == id - 0x5F)) ||
        (((u32)(id - 0xA0) < 0x20U) && (p->weapon != PLAYER_STATUS_EQUIPMENT_NONE) &&
         ((Gp_GetItemSlot(p->weapon + 0x7F)->primaryItemId == id) ||
          (Gp_GetItemSlot(p->weapon + 0x7F)->secondaryItemId == id)))) {
        ret = 1;
    }
    return ret;
}

InventoryItemRow* Gp_NthEquippableRec(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    InventoryItemRow* table;
    s32               i;
    InventoryItemRow* rec;

    table = Gp_GetItemTable(arg0);
    rec   = NULL;
    table = &table[arg0->firstRow];
    for (i = 0; i < arg0->rowCount; i++, table++) {
        if ((Gp_ItemDescs[table->itemId].flags & ITEM_FLAG_NO_ATTACHMENT) || (table->itemId == INVENTORY_ITEM_NONE)) {
            continue;
        }
        if ((u8)(table->itemId + 0x80) < 0x20 && _gpIsEquippedItem(table->itemId)) {
            continue;
        }
        arg1--;
        if (arg1 < 0) {
            rec = table;
            break;
        }
    }
    return rec;
}

/// Sets bit 0x100 in `flags`, which makes `func_800C7AE8` skip drawing the
/// item preview, while the CD queue is still busy loading it.
#define GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags) \
    do {                                     \
        if (CdCmd_IsIdle() == 0) {           \
            (flags) |= 0x100;                \
        }                                    \
    } while (0)
