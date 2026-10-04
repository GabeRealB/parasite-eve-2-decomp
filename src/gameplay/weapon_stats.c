#include "item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/inventory.h"
#include "inventory.h"
#include "gameplay/item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "weapon_data.h"

#define D_8010EEF8 D_8010EAB4[39]

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// Comparison values for weapon items 0x7F..0x9F (row zero is unequipped).
extern u16 Gp_WeaponStats[33][4];

static inline u16* gpWeaponStats(s32 itemId)
{
    return Gp_WeaponStats[itemId - 0x7F];
}

/// Primary and secondary consumable comparisons use the low-id combat parameter table.
static inline u16* gpAmmoStats(s32 itemId)
{
    return &Gp_IdParamLo[itemId - 0x9F].amount;
}

/// Draws `item`'s name, its `func_800C22D8` marker in `mode`, the variant
/// marker for items 0x0F-0x32 and its icon at (`x`, `y`) in `obj`. Nothing is
/// drawn while `obj->panel.state` is `USER_INTERFACE_PANEL_HIDDEN`.
static inline void _gpDrawItemNameAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item, s32 mode);

/// Makes `item` the preview in slot `slot` of `Gp_PreviewItems`, setting the
/// other two slots to -1, and queues its load. Nothing happens when the slot
/// already shows `item`.
static inline void _gpSetPreviewItem(s32 item, u8 slot);

/// `_gpSetPreviewItem` written as a walk of a pointer over `Gp_PreviewItems`
/// rather than an indexed store.
static inline void _gpSetPreviewItemWalk(s32 item, u8 slot);

/// Draws `item` as `_gpDrawItemNameAt` does, but without the
/// `func_800C22D8` marker.
static inline void _gpDrawItemNameUnmarkedAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item);

u16 Gp_WeaponStats[33][4] = {
    { 0, 0, 0, 0 },
    { 70, 80, 100, 0 },
    { 50, 110, 117, 0 },
    { 40, 90, 227, 0 },
    { 70, 80, 87, 0 },
    { 120, 90, 92, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 60, 70, 168, 0 },
    { 0, 0, 0, 0 },
    { 350, 1, 260, 0 },
    { 350, 12, 900, 0 },
    { 30, 2, 270, 0 },
    { 40, 24, 420, 0 },
    { 50, 36, 550, 0 },
    { 500, 85, 254, 0 },
    { 400, 100, 685, 0 },
    { 0, 0, 0, 0 },
    { 1, 5, 68, 0 },
    { 500, 85, 274, 0 },
    { 500, 85, 294, 0 },
    { 1000, 7, 881, 0 },
    { 100, 36, 579, 0 },
    { 0, 0, 0, 0 },
    { 500, 85, 339, 0 },
    { 500, 85, 284, 0 },
    { 500, 85, 390, 0 },
    { 500, 85, 437, 0 },
    { 500, 85, 488, 0 },
    { 55, 80, 288, 0 },
    { 55, 80, 306, 0 },
    { 55, 80, 324, 0 }
};

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
        Text_DrawString(&req, Gp_GetItemText(item, 0, 0));
        func_800C22D8(obj, x, y, item, mode);
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }
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

void func_800C7DA8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8                   buf[8];
    u16                  selStats[3];
    u16                  eqStats[3];
    TextDrawReq          nameReq;
    TextDrawReq          valReq;
    s32                  xOff;
    s32                  yBase;
    s32                  y;
    s32                  xCopy;
    s32                  nx;
    s32                  ot;
    u16*                 itemRow;
    u16*                 eqRow;
    u16*                 pItem;
    u16*                 pEq;
    UiList*              list;
    SPRT*                p;
    ArmorStats*          attr;
    EquipmentWeaponLoad* slot;
    PlayerStatus*        cfg;
    s16                  field18;
    s32                  color;
    s32                  swap;
    s32                  i;
    s32                  count;
    s32                  two;
    u32                  selVal;
    u32                  itemVal;
    s32                  val;

    field18 = arg0->panel.contentTop.signedValue;
    xOff    = arg0->panel.contentLeft.signedValue + 0x60;
    yBase   = field18 + 8;
    cfg     = &gPlayerStatus;
    if (arg2 == 0) {
        yBase = field18 + 0x1C;
    }
    if ((u32)(arg1 - 0x80) < 0x20U) {
        list       = &D_8010E9A4;
        itemRow    = gpWeaponStats(arg1);
        count      = 3;
        D_80114D80 = D_8010E984;
        eqRow      = Gp_WeaponStats[cfg->weapon];
    } else if ((u32)(arg1 - 0xA0) < 0x20U) {
        slot    = Gp_GetItemSlot(cfg->weapon + 0x7F);
        list    = &D_8010E9CC;
        itemRow = gpAmmoStats(arg1);
        if (Gp_ReloadMode == 2) {
            if (slot->secondaryItemId == INVENTORY_ITEM_NONE) {
                eqRow = gpAmmoStats(0x9F);
            } else {
                eqRow = gpAmmoStats(slot->secondaryItemId);
            }
        } else {
            if (slot->primaryItemId == INVENTORY_ITEM_NONE) {
                eqRow = gpAmmoStats(0x9F);
            } else {
                eqRow = gpAmmoStats(slot->primaryItemId);
            }
        }
        D_80114D80 = D_8010E990;
        count      = 1;
    } else if ((u32)(arg1 - 0x60) < 0x20U) {

        list        = &D_8010E9F4;
        attr        = &Gp_ModStatAttrs[(arg1)-0x60];
        selStats[0] = attr->hpBonus;
        selStats[1] = attr->mpBonus;
        selStats[2] = Gp_GetModLevel(arg1);
        itemRow     = selStats;
        eqRow       = eqStats;
        attr        = &Gp_ModStatAttrs[(cfg->armor + 0x5F) - 0x60];
        count       = 3;
        eqStats[0]  = attr->hpBonus;
        eqStats[1]  = attr->mpBonus;
        eqStats[2]  = Gp_GetModLevel(cfg->armor + 0x5F);
        D_80114D80  = D_8010E994;
    } else {
        return;
    }

    xCopy = xOff;
    y     = yBase;
    i     = 0;
    if (count != 0) {
        two   = 2;
        pItem = itemRow;
        pEq   = eqRow;
        do {
            nx                 = arg0->panel.contentOriginX.unsignedValue - 0xA;
            nameReq.x          = nx + xCopy;
            nameReq.y          = arg0->panel.contentOriginY.unsignedValue + y;
            ot                 = arg0->panel.otIndex.signedValue + 1;
            nameReq.otIndex    = ot;
            nameReq.colorRgb   = 0x606060;
            nameReq.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            nameReq.alignment  = TEXT_ALIGNMENT_LEFT;
            nameReq.drawMode   = TEXT_DRAW_OUTLINED;
            Text_DrawString(&nameReq, D_80114D80[i]);
            swap = 0;
            if (i == two) {
                swap = list == &D_8010E9A4;
            }
            if (arg2 == 1) {
                selVal  = *pEq;
                itemVal = *pItem;
                if (selVal < itemVal) {
                    color = 0x1741F;
                    if (swap != 0) {
                        color = 0xD287F;
                    }
                } else if (itemVal < selVal) {
                    color = 0xD287F;
                    if (swap != 0) {
                        color = 0x1741F;
                    }
                } else {
                    color = 0x606060;
                }
            } else {
                color = 0x606060;
            }
            if (((u32)(arg1 - 0x60) < 0x20U) && (i < 2)) {
                {
                    s32 vx;
                    vx       = arg0->panel.contentOriginX.unsignedValue - 2;
                    valReq.x = arg0->panel.contentRight.unsignedValue + vx;
                }
                valReq.y          = arg0->panel.contentOriginY.unsignedValue + 0xB + y;
                valReq.otIndex    = arg0->panel.otIndex.signedValue + 1;
                valReq.colorRgb   = color;
                valReq.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                valReq.alignment  = two;
                valReq.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                Text_DrawString(&valReq, Text_ItoaSignedPlus(buf, *pItem));
            } else {
                {
                    s32 vx;
                    vx       = arg0->panel.contentOriginX.unsignedValue - 2;
                    valReq.x = arg0->panel.contentRight.unsignedValue + vx;
                }
                valReq.y          = arg0->panel.contentOriginY.unsignedValue + 0xB + y;
                valReq.otIndex    = arg0->panel.otIndex.signedValue + 1;
                valReq.colorRgb   = color;
                valReq.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                valReq.alignment  = two;
                valReq.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                Text_DrawString(&valReq, Text_ItoaSigned(buf, *pItem));
            }
            y     += 0x18;
            pItem += 1;
            i     += 1;
            pEq   += 1;
        } while (i < count);
    }

    i = 0;
    if (arg2 == 1) {
        y     = yBase;
        xCopy = xOff;
        if (count != 0) {
            s32 two2;
            two2 = 2;

            do {
                swap = 0;
                if (i == two2) {
                    swap = list == &D_8010E9A4;
                }
                p              = gGpuPrimCursor;
                p->x0          = arg0->panel.contentOriginX.unsignedValue + xCopy;
                p->y0          = arg0->panel.contentOriginY.unsignedValue + y + 5;
                val            = 8;
                p->w           = val;
                p->h           = val;
                selVal         = eqRow[i];
                itemVal        = itemRow[i];
                gGpuPrimCursor = p + 1;
                if (selVal < itemVal) {
                    p->u0                          = 0x30;
                    GPU_PRIMITIVE_COLOR_WORD(p, 0) = GPU_PACK_COLOR_WORD(0x1f, 0x74, 0x01, 0);
                    if (swap != 0) {
                        GPU_PRIMITIVE_COLOR_WORD(p, 0) = GPU_PACK_COLOR_WORD(0x7f, 0x28, 0x0d, 0);
                    }
                } else if (itemVal < selVal) {
                    p->u0                          = 0xA0;
                    GPU_PRIMITIVE_COLOR_WORD(p, 0) = GPU_PACK_COLOR_WORD(0x7f, 0x28, 0x0d, 0);
                    p->y0                          = p->y0 - 1;
                    if (swap != 0) {
                        GPU_PRIMITIVE_COLOR_WORD(p, 0) = GPU_PACK_COLOR_WORD(0x1f, 0x74, 0x01, 0);
                    }
                } else {
                    p->u0                          = 0x78;
                    val                            = 0x606060;
                    GPU_PRIMITIVE_COLOR_WORD(p, 0) = val;
                }
                y      += 0x18;
                p->v0   = 0x60;
                p->clut = 0x3C09;
                setlen(p, 4);
                setcode(p, 0x64);
                addPrim(gGpuCurrentOt + arg0->panel.otIndex.signedValue + 1, p);
                i += 1;
            } while (i < count);
        }
        Ui_InsertDrawTPage(arg0->panel.otIndex.signedValue + 1, 0);
    }
}

/// Makes `item` the preview in slot `slot` of `Gp_PreviewItems`, setting the
/// other two slots to -1, and queues its load. Nothing happens when the slot
/// already shows `item`.
static inline void _gpSetPreviewItem(s32 item, u8 slot)
{
    s32 i;

    if (item != Gp_PreviewItems[slot]) {
        for (i = 0; i < 3; i++) {
            if (i == slot) {
                Gp_PreviewItems[i] = item;
            } else {
                Gp_PreviewItems[i] = -1;
            }
        }
        Gp_EnqueueItemPreviewCd(item, slot);
    }
}

/// `_gpSetPreviewItem` written as a walk of a pointer over `Gp_PreviewItems`
/// rather than an indexed store.
static inline void _gpSetPreviewItemWalk(s32 item, u8 slot)
{
    s32  i;
    s32* p;

    p = Gp_PreviewItems;
    if (item != p[slot]) {
        for (i = 0; i < 3; i++) {
            if (i == slot) {
                *p++ = item;
            } else {
                *p++ = -1;
            }
        }
        Gp_EnqueueItemPreviewCd(item, slot);
    }
}

void Gp_EquipSummaryTask(Task* arg0)
{
    PlayerStatus*        cfg;
    UiObject*            obj;
    EquipmentWeaponLoad* slotp;
    s32*                 stored;
    s32                  mode;
    s32                  item;
    s32                  skip;
    s32                  slot;
    s32                  flags;

    item   = 0;
    skip   = 0;
    cfg    = &gPlayerStatus;
    stored = (s32*)arg0->work;
    mode   = arg0->spawnArg1.value;
    obj    = arg0->spawnArg2.pointer;
    slot   = 0;
    if (mode == 0) {
        Ui_DrawText(&(obj)->panel, Gp_StrWeaponTitle);
        item = cfg->weapon + 0x7F;
        if (item < 0x80) {
            item = 0;
        }
    } else if (mode == 1) {
        Ui_DrawText(&(obj)->panel, Gp_StrAmmoCaps);
        slotp = Gp_GetItemSlot(cfg->weapon + 0x7F);
        item  = slotp->primaryItemId;
        if (Gp_ReloadMode == 2) {
            item = slotp->secondaryItemId;
        }
    } else if (mode == 2) {
        Ui_DrawText(&(obj)->panel, Gp_StrArmor);
        item = cfg->armor + 0x5F;
    } else {
        Ui_DrawText(&(obj)->panel, Gp_StrAttachments);
        skip = 1;
        if (Gp_SelItemRec != NULL) {
            item = Gp_SelItemRec->itemId;
        }
    }

    if (arg0->state == 0) {
        stored           = memCalloc(4, 0);
        Gp_ItemCountShow = 1;
        arg0->work       = stored;
        *stored          = item;
        arg0->state      = 2;
    }

    if (*stored != item) {
        _gpSetPreviewItem(item, slot);
        *stored     = item;
        arg0->state = 2;
    }

    if (item != 0) {
        _gpDrawItemNameAt(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Ui_LookupTable(obj, 1), item, 1);
    }

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x11);
    if (skip == 0) {
        func_800C7DA8(obj, item, 0, 0);
    }

    flags = slot + 0x10;
    if ((arg0->state != 1) || (item == 0)) {
        flags |= 0x100;
    }
    func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x16, flags);

    if (arg0->state == 2) {
        if (CdCmd_IsIdle()) {
            arg0->state = 1;
        }
    }

    if (obj->panel.state == USER_INTERFACE_PANEL_CLOSING) {
        Gp_ItemCountShow = 0;
    }
}

/// Draws `item` as `_gpDrawItemNameAt` does, but without the
/// `func_800C22D8` marker.
static inline void _gpDrawItemNameUnmarkedAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item)
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
        Text_DrawString(&req, Gp_GetItemText(item, 0, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }
}

void Gp_DrawAmmoRow(UiList* arg0, UiObject* obj)
{
    register UiList* prompt asm("s5");
    register s32     spawnArg asm("s4");
    s32              item;
    s32              status;
    UiObject*        spawned;

    spawnArg = obj->owner->spawnArg1.value;
    USE_REG(spawnArg);
    prompt = arg0;
    USE_REG(arg0);
    item   = Gp_NthRelatedId(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, prompt->currentItemIndex, spawnArg);
    status = obj->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (prompt->selectedItemIndex == prompt->currentItemIndex) {
            if (item == 0) {
                Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            } else {
                Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
            }
            if (spawnArg != 0) {
                _gpSetPreviewItem(item, 0);
            }
        }
    }

    if (spawnArg == 0) {
        _gpDrawItemNameAt(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, prompt->colorRgb, item, 1);
    } else {
        _gpDrawItemNameUnmarkedAt(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, prompt->colorRgb, item);
    }

    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        obj->resultValue = item;
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            if (obj->owner->spawnArg1.value == 0) {
                sndEvtRequestScriptStart(SOUND_WEAPON_EQUIP, 0, 0);
                Gp_EquipHeld(item);
                Gp_ReloadMode = 0;
                spawned       = Ui_SpawnFromDesc(&D_8010EF14, item | 0x10000, 1, 1, obj);
                if (spawned != NULL) {
                    Ui_ClampDialogRect(&(spawned)->panel, prompt, &(obj)->panel);
                }
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                Ui_SpawnFromDesc(&D_8010EEF8, (item << 8) | spawnArg, 1, 1, obj);
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if (spawnArg != 0) {
                Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, obj);
            } else {
                Ui_SpawnFromDesc(&D_8010EFA0, item | 0x10000, 1, 1, obj);
            }
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else if (obj->panel.control.word != USER_INTERFACE_PANEL_ACTIVE) {
        if (prompt->currentItemIndex == 0) {
            if (obj->resultValue == 0) {
                obj->resultValue = item;
            }
        }
    }
}

void Gp_AmmoListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    s32       spawnArg;
    s32       one;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    obj         = arg0->spawnArg2.pointer;
    spawnArg    = arg0->spawnArg1.value;
    obj->result = USER_INTERFACE_RESULT_NONE;
    menu        = &D_8010E9A4;
    if (arg0->state == 0) {
        Gp_CountAmmoRows(menu, spawnArg);
        Ui_LayoutListPanel(menu, &(obj)->panel);
        if (spawnArg == 0) {
            menu->topInset                   += 0x4C;
            obj->panel.bounds.unsignedRect.h += 0x4C;
        }
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        arg0->state                               = arg0->state + 1;
        if (menu->itemCount == 0) {
            arg0->state         = arg0->state + 1;
            arg0->killCountdown = 0xBC;
            obj->panel.style   |= USER_INTERFACE_PANEL_TITLE_STYLE;
            Ui_SizeFromTextPlain(&(obj)->panel, Gp_StrNoWeaponEq);
            return;
        }
        if ((s16)obj->panel.bounds.unsignedRect.y + (s16)obj->panel.bounds.unsignedRect.h < 0x47) {
            return;
        }
        obj->panel.bounds.unsignedRect.y = 0x46 - obj->panel.bounds.unsignedRect.h;
        return;
    }
    one = 1;
    if (arg0->state == one) {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->panel.control.word == one) {
            if (padCheckButtons(0, one, Pad_MaskMenu) != 0) {
                obj->result = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        }
        child = arg0->firstChild;
        if (child != NULL) {
            one = 6;
            do {
                childObj = child->spawnArg2.pointer;
                flag     = childObj->result;
                next     = child->nextSibling;
                switch (flag) {
                    case USER_INTERFACE_RESULT_DISMISS:
                        obj->result = flag;
                        break;
                    case USER_INTERFACE_RESULT_CANCEL:
                        obj->result = flag;
                        break;
                    case USER_INTERFACE_RESULT_CONFIRM:
                        Ui_TeardownTree(childObj, childObj->owner);
                        obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                        break;
                }
                head  = arg0->firstChild;
                child = next;
                if (child == head) {
                    break;
                }
                if (head == NULL) {
                    break;
                }
            } while (1);
        }
        return;
    }
    Ui_DrawText(&(obj)->panel, Gp_StrAttention);
    Text_DrawMultiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Gp_StrNoWeaponEq, 0x606060, one, TEXT_ALIGNMENT_LEFT);
    arg0->killCountdown--;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
        obj->result = USER_INTERFACE_RESULT_CANCEL;
        return;
    }
    if ((arg0->killCountdown == 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
        obj->result         = USER_INTERFACE_RESULT_DISMISS;
        arg0->killCountdown = 0x7FFF;
    }
}

/// Sets bit 0x100 in `flags`, which makes `func_800C7AE8` skip drawing the
/// item preview, while the CD queue is still busy loading it.
#define GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags) \
    do {                                     \
        if (CdCmd_IsIdle() == 0) {           \
            (flags) |= 0x100;                \
        }                                    \
    } while (0)

void Gp_SelectWeaponMenuTask(Task* arg0)
{
    UiList*       menu;
    UiObject*     obj;
    s32           val;
    PlayerStatus* cfg;
    s32           flags;
    Task*         parent;

    menu = &D_8010E9A4;
    obj  = arg0->spawnArg2.pointer;
    cfg  = &gPlayerStatus;
    Ui_DrawText(&(obj)->panel, Gp_StrSelectWeapon);
    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x4A);
    if (arg0->state == 0) {
        parent     = arg0->parent;
        D_80114DD8 = -1;
        Ui_SetState4(parent->spawnArg2.pointer, parent);
        Ui_SpawnFromDesc(&D_8010EC3C, 0, 0, 0x10, obj);
    }
    val = Gp_NthRelatedId(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, menu->selectedItemIndex, 0);
    if (((obj->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) || (val != cfg->weapon + 0x7F)) {
        flags = 0x12;
        if (val == 0) {
            flags = 0x112;
            goto draw;
        }
        if (((obj->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
            _gpSetPreviewItemWalk(val, 2);
        }
    } else {
        flags = 0x10;
        if (val == 0) {
            flags = 0x110;
            goto draw;
        }
    }
    GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags);
draw:
    func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);
    func_800C7DA8(obj, val, 1, 0);
    Gp_AmmoListTask(arg0);
    obj->resultValue = 0;
    if (obj->result == USER_INTERFACE_RESULT_DISMISS) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

void Gp_DrawRemoveAmmoRow(UiList* prompt, UiObject* obj)
{
    s32                  item;
    s32                  spawnArg;
    s32                  status;
    InventoryItemRow*    rec;
    s32                  qty;
    EquipmentWeaponLoad* load;
    union {
        struct {
            u8          buf[0x20];
            TextDrawReq req;
        } count;
        TextDrawReq req;
    } draw;

    item     = Gp_AttachListIds[prompt->currentItemIndex];
    spawnArg = (u16)obj->owner->spawnArg1.value;
    status   = obj->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (prompt->selectedItemIndex == prompt->currentItemIndex) {
            if (item == 0) {
                Ui_SetHolderParam(Gp_StrRemoveAmmoHelp, 0, 0);
            } else {
                Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
                if (Gp_ReloadMode == 0) {
                    _gpSetPreviewItem(item, 2);
                }
            }
        }
    }

    if (item != 0) {
        rec = Gp_FindItemById(item);
        qty = rec->qty - Gp_CountEquippedRelated(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, item);
        if (Gp_ReloadMode == 0) {
            load = Gp_GetItemSlot(spawnArg);
            if (load->primaryItemId == item) {
                qty += load->primaryQty;
            } else if (load->secondaryItemId == item) {
                qty += load->secondaryQty;
            }
        }
        {
            s32 x;
            s32 y;
            s32 color;

            x                         = prompt->rowTextX.signedValue;
            y                         = prompt->rowTextY.signedValue;
            color                     = prompt->colorRgb;
            draw.count.req.x          = obj->panel.contentOriginX.unsignedValue + 0x84 + x;
            draw.count.req.y          = obj->panel.contentOriginY.unsignedValue + (y - 3);
            draw.count.req.otIndex    = obj->panel.otIndex.signedValue + 1;
            draw.count.req.colorRgb   = color;
            draw.count.req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            draw.count.req.alignment  = TEXT_ALIGNMENT_RIGHT;
            draw.count.req.drawMode   = TEXT_DRAW_FILL_ONLY;
            Text_DrawString(&draw.count.req, Text_ItoaSigned(draw.count.buf, qty));
            Ui_LayoutWithMode0(obj, (x + 0x69), (y - 8), 0x1B, 7,
                               0x102010);
        }
        {
            s32 x;
            s32 y;
            s32 color;
            s32 temp;

            x     = prompt->rowTextX.signedValue;
            y     = prompt->rowTextY.signedValue;
            color = prompt->colorRgb;
            if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                draw.req.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
                draw.req.y          = obj->panel.contentOriginY.unsignedValue + (y - 6);
                draw.req.otIndex    = obj->panel.otIndex.signedValue + 1;
                draw.req.colorRgb   = color;
                draw.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                draw.req.alignment  = TEXT_ALIGNMENT_LEFT;
                draw.req.drawMode   = TEXT_DRAW_OUTLINED;
                Text_DrawString(&draw.req, Gp_GetItemText(item, 0, 0));
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    func_800C2538(obj, x, y, temp % 3 + 1, color);
                }
                Gp_DrawItemIcon(obj, x, y, item, 0);
            }
        }
    } else {
        s32 baseY;

        draw.req.x          = obj->panel.contentOriginX.unsignedValue + prompt->rowTextX.unsignedValue;
        baseY               = obj->panel.contentOriginY.unsignedValue - 6;
        draw.req.y          = baseY + prompt->rowTextY.unsignedValue;
        draw.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        draw.req.colorRgb   = prompt->colorRgb;
        draw.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        draw.req.alignment  = TEXT_ALIGNMENT_LEFT;
        draw.req.drawMode   = TEXT_DRAW_OUTLINED;
        Text_DrawString(&draw.req, Gp_StrRemoveAmmo);
    }

    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            Ui_SpawnFromDesc(&D_8010EEF8, (spawnArg << 8) | item, 1, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        } else if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) && (item != 0)) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            // Both arms open the same prompt; the ammo row's handler, which
            // this one follows, passes a different argument in each.
            if (spawnArg != 0) {
                Ui_SpawnFromDesc(&D_8010EFA0, item | 0x10000, 1, 1, obj);
            } else {
                Ui_SpawnFromDesc(&D_8010EFA0, item | 0x10000, 1, 1, obj);
            }
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void Gp_BuildAttachList(UiList* arg0, s32 arg1)
{
    InventoryItemRange*  scan;
    EquipmentWeaponLoad* slot;
    s32                  mode;
    s32                  count;
    s32                  n;
    s32                  i;
    s32                  item;
    s32                  qty;

    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    mode  = Gp_ReloadMode;
    count = 0;
    slot  = Gp_GetItemSlot(arg1);
    n     = count;
    if (mode != 2) {
        SOFT_TOUCH_REG(n);
        i = n;
        do {
            item = Gp_RelatedQty0.rows[arg1 - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[i];
            if (item != INVENTORY_ITEM_NONE) {
                qty  = Gp_ScanStackQty(scan, item);
                qty -= Gp_CountEquippedRelated(scan, item);
                if (mode == 0 && slot->primaryItemId == item) {
                    qty += slot->primaryQty;
                }
                if (qty > 0) {
                    Gp_AttachListIds[count++] = item;
                    n++;
                }
            }
            i++;
        } while (i < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds));
    }
    if (mode != 1) {
        for (i = 0; i < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds); i++) {
            item = Gp_RelatedQty1.rows[arg1 - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[i];
            if (item != INVENTORY_ITEM_NONE) {
                qty  = Gp_ScanStackQty(scan, item);
                qty -= Gp_CountEquippedRelated(scan, item);
                if (mode == 0 && slot->secondaryItemId == item) {
                    qty += slot->secondaryQty;
                }
                if (qty > 0) {
                    Gp_AttachListIds[count++] = item;
                    n++;
                }
            }
        }
    }
    if (mode != 0 && n > 0) {
        Gp_AttachListIds[count] = 0;
        n++;
    }
    arg0->itemCount                     = n;
    arg0->visibleRowCount.unsignedValue = n;
}
