#include "attachments.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/item_menu.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"

#define D_8010EEDC D_8010EAB4[38]

#define D_8010EF30 D_8010EAB4[41]

#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern UiListRowCallback Gp_ItemCmdRows[2];

extern const u8 Gp_StrWrongAmmo2[];

static inline s32 _gpIsArmorItem(u8 id);

static inline s32 _gpFindSpareArmor(s32 index);

static inline void _gpApplyChildResults(UiObject* obj, Task* task);

static inline s32 _gpIsArmorItem(u8 id)
{
    return (u32)(id - 0x60) < 0x20U;
}

#define GP_SET_PREVIEW_ITEM(item, slot)          \
    do {                                         \
        s32* _p;                                 \
        s32  _i;                                 \
                                                 \
        _p = Gp_PreviewItems;                    \
        if ((item) != _p[slot]) {                \
            for (_i = 0; _i < 3; _i++, _p++) {   \
                if (_i == (slot)) {              \
                    *_p = (item);                \
                } else {                         \
                    *_p = -1;                    \
                }                                \
            }                                    \
            Gp_EnqueueItemPreviewCd(item, slot); \
        }                                        \
    } while (0)
#define GP_FIND_SPARE_ARMOR(found, index)                                               \
    do {                                                                                \
        PlayerStatus*       _cfg;                                                       \
        InventoryItemRange* _scan;                                                      \
        InventoryItemRow*   _rec;                                                       \
        s32                 _i;                                                         \
        s32                 _n;                                                         \
                                                                                        \
        _scan   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;               \
        _cfg    = &gPlayerStatus;                                                       \
        _n      = (index);                                                              \
        _rec    = Gp_GetItemTable(_scan);                                               \
        (found) = _i = 0;                                                               \
        _rec         = &_rec[_scan->firstRow];                                          \
        for (; _i < _scan->rowCount; _i++) {                                            \
            if (_gpIsArmorItem(_rec->itemId) && (_cfg->armor != _rec->itemId - 0x5F)) { \
                _n--;                                                                   \
                if (_n < 0) {                                                           \
                    (found) = _rec->itemId;                                             \
                    break;                                                              \
                }                                                                       \
            }                                                                           \
            _rec++;                                                                     \
        }                                                                               \
    } while (0)
#define GP_COUNT_SPARE_ARMOR(count)                                                     \
    do {                                                                                \
        PlayerStatus*       _cfg;                                                       \
        InventoryItemRange* _scan;                                                      \
        InventoryItemRow*   _rec;                                                       \
        s32                 _i;                                                         \
                                                                                        \
        (count) = 0;                                                                    \
        _scan   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;               \
        _cfg    = &gPlayerStatus;                                                       \
        _rec    = Gp_GetItemTable(_scan);                                               \
        _i      = 0;                                                                    \
        _rec    = &_rec[_scan->firstRow];                                               \
        for (; _i < _scan->rowCount; _i++) {                                            \
            if (_gpIsArmorItem(_rec->itemId) && (_cfg->armor != _rec->itemId - 0x5F)) { \
                (count)++;                                                              \
            }                                                                           \
            _rec++;                                                                     \
        }                                                                               \
    } while (0)
static inline s32 _gpFindSpareArmor(s32 index)
{
    PlayerStatus*       cfg;
    InventoryItemRange* scan;
    InventoryItemRow*   rec;
    s32                 i;
    s32                 found;

    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    cfg   = &gPlayerStatus;
    rec   = Gp_GetItemTable(scan);
    found = i = 0;
    rec       = &rec[scan->firstRow];
    for (; i < scan->rowCount; i++) {
        if (_gpIsArmorItem(rec->itemId) && (cfg->armor != rec->itemId - 0x5F)) {
            index--;
            if (index < 0) {
                found = rec->itemId;
                break;
            }
        }
        rec++;
    }
    return found;
}
static inline void _gpApplyChildResults(UiObject* obj, Task* task)
{
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    child = task->firstChild;
    if (child != NULL) {
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
            head  = task->firstChild;
            child = next;
            if (child == head) {
                break;
            }
            if (head == NULL) {
                break;
            }
        } while (1);
    }
}

UiObjectDesc D_8010F6FC    = { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, -30, 120, 60 }, 8, 0, TASK_BODY_NONE, 192, Gp_DiscardWarnTask, 0 };
UiObjectDesc D_8010F718[4] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -80, 144, 72 }, 292, 0, TASK_BODY_NONE, 192, func_800D29B0, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -8, 144, 72 }, 288, 0, TASK_BODY_NONE, 192, func_800D29B0, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -80, 144, 72 }, 284, 0, TASK_BODY_NONE, 192, func_800D29B0, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -8, 144, 72 }, 280, 0, TASK_BODY_NONE, 192, func_800D29B0, 0 },
};
UiObjectDesc D_8010F788    = { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, 0, 1, 1 }, 8, 0, TASK_BODY_NONE, 192, Gp_NoticePanelTask, 0 };
UiObjectDesc D_8010F7A4    = { 0, { -128, 64, 256, 32 }, 264, 0, TASK_BODY_NONE, 192, Gp_PeUpgradePanelTask, 0 };
UiObjectDesc D_8010F7C0[2] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -128, -80, 192, 144 }, 12, 0, TASK_BODY_NONE, 192, Gp_DrawNextLevelCmd, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -96, -72, 176, 112 }, 8, 0, TASK_BODY_NONE, 192, func_800D573C, 0 },
};
UiObjectDesc      D_8010F7F8          = { USER_INTERFACE_PANEL_TITLE_STYLE, { -96, -104, 192, 208 }, 8, 0, TASK_BODY_NONE, 192, Gp_DrawSpecsCmd, 0 };
UiListRowCallback Gp_ItemCmdRows[2]   = { Gp_DrawExaminePushCmd, Gp_DrawItemCmd };
UiList            D_8010F81C          = { Gp_ItemCmdRows, 2, { 2 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };
UiObjectDesc      D_8010F840          = { 3, { 0, 0, 70, 64 }, 60, 0, TASK_BODY_NONE, 192, Gp_MapMenuListTask, 0 };
TaskDesc          D_8010F85C          = { { { TASK_BODY_NONE, 192 } }, Gp_MapScreenTask, { NULL } };
UiObjectDesc      D_8010F868          = { 0, { 0, -104, 144, 16 }, 36, 0, TASK_BODY_NONE, 192, func_800D5A48, 0 };
s32               D_8010F884          = 0;
s32               Gp_HealPending      = 0;
s32               Gp_PendingRelatedId = 0;
s32               Gp_RelatedPending   = 0;
s32               Gp_UsedItemId       = 0;
UiObjectDesc      D_8010F898          = { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, 28, 152, 72 }, 56, 0, TASK_BODY_NONE, 192, func_800D6334, 0 };
UiObjectDesc      D_8010F8B4          = { USER_INTERFACE_PANEL_TITLE_STYLE, { -152, 28, 152, 72 }, 60, 0, TASK_BODY_NONE, 192, Gp_DrawWeaponLabel, 0 };

const u8 Gp_StrWrongAmmo2[] = "You do not have the correct ammo.";

void Gp_AttachListTask(Task* task)
{
    UiList*              menu;
    UiObject*            obj;
    s32                  val;
    s32                  one;
    s32                  state;
    EquipmentWeaponLoad* slot;
    u8                   temp;
    Task*                child;
    Task*                next;
    Task*                head;
    UiObject*            childObj;
    s32                  flag;

    obj         = task->spawnArg2.pointer;
    val         = (u16)task->spawnArg1.value;
    menu        = &D_8010E9CC;
    obj->result = USER_INTERFACE_RESULT_NONE;
    state       = task->state;
    if (state == 0) {
        Gp_BuildAttachList(menu, val);
        Ui_LayoutListPanel(menu, &(obj)->panel);
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        task->state                               = task->state + 1;
        if (menu->itemCount == 0) {
            task->state         = 2;
            task->killCountdown = 0xBC;
            obj->panel.style   |= USER_INTERFACE_PANEL_TITLE_STYLE;
            if (task->spawnArg1.value & 0x10000) {
                if (val != 0) {
                    slot = Gp_GetItemSlot(val);
                    if ((val == 0x92) || (val == 0x99) || (val == 0x96)) {
                        task->state = 3;
                    } else if (val == 0x95) {
                        if (slot->primaryQty != 0) {
                            task->state = 3;
                        }
                    } else {
                        temp = slot->secondaryItemId;
                        if ((temp != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) && (temp != INVENTORY_ITEM_NONE)) {
                            if (slot->secondaryQty != 0) {
                                task->state = 3;
                            }
                        }
                    }
                }
            }
            Ui_SizeFromTextPlain(&(obj)->panel, Gp_StrWrongAmmo2);
            if (task->state != 2) {
                Gp_SizeEquippedPanel(&(obj)->panel, val);
            }
            obj->panel.animationTicks = USER_INTERFACE_PANEL_ANIMATION_TICKS;
            return;
        }
        if ((s16)obj->panel.bounds.unsignedRect.y + (s16)obj->panel.bounds.unsignedRect.h < 0x47) {
            return;
        }
        obj->panel.bounds.unsignedRect.y = 0x46 - obj->panel.bounds.unsignedRect.h;
        return;
    }
    one = 1;
    if (state == one) {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->panel.control.word == one) {
            if (padCheckButtons(0, one, Pad_MaskMenu) != 0) {
                obj->result = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        }
        child = task->firstChild;
        if (child != NULL) {
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
                head  = task->firstChild;
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
    if (state == 2) {
        Ui_DrawText(&(obj)->panel, Gp_StrNotice);
        Text_DrawMultiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Gp_StrWrongAmmo2, 0x606060, one, TEXT_ALIGNMENT_LEFT);
    } else {
        Ui_DrawText(&(obj)->panel, Gp_StrEquip);
        func_800CF6E8(obj, val);
    }
    task->killCountdown--;
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
            return;
        }
        if ((task->killCountdown == 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            if ((task->spawnArg1.value & 0x10000) && (task->state == 2)) {
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
            } else {
                obj->result = USER_INTERFACE_RESULT_DISMISS;
            }
            task->killCountdown = 0x7FFF;
        }
    }
}

void Gp_SelectAmmoMenuTask(Task* arg0)
{
    UiList*   menu;
    UiObject* obj;
    s32       savedState;
    s32       state;
    s32       val;
    s32       flags;
    Task*     parent;
    s32*      table;
    s32       i;
    s32       slot;
    s32       minusOne;
    s32*      p;

    menu       = &D_8010E9CC;
    obj        = arg0->spawnArg2.pointer;
    savedState = arg0->state;
    Gp_AttachListTask(arg0);
    if (obj->result == USER_INTERFACE_RESULT_DISMISS) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
    state = arg0->state;
    if (state == 1) {
        Ui_DrawText(&(obj)->panel, Gp_StrSelectAmmo);
        if (savedState == 0) {
            menu->topInset                   += 0x4C;
            obj->panel.bounds.unsignedRect.h += 0x4C;
            parent                            = arg0->parent;
            D_80114DD8                        = -1;
            Ui_SetState4(parent->spawnArg2.pointer, parent);
            Ui_SpawnFromDesc(&D_8010EC3C, 1, 0, 0x10, obj);
        }
        uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x4A);
        val = Gp_AttachListIds[menu->selectedItemIndex];
        if (val != 0) {
            func_800C7DA8(obj, val, 1, 0);
        }
        flags = 0x12;
        if (val == 0) {
            flags = 0x112;
            goto draw;
        }
        if (((obj->panel.control.word >> 16) == state) || (obj->panel.control.word == state)) {
            table = Gp_PreviewItems;
            if (val != table[2]) {
                i = 0;
                do {
                    slot     = 2;
                    minusOne = -1;
                    p        = table;
                } while (0);
                for (; i < 3; i++, p++) {
                    if (i == slot) {
                        *p = val;
                    } else {
                        *p = minusOne;
                    }
                }
                Gp_EnqueueItemPreviewCd(val, 2);
            }
        }
        if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
            flags |= 0x100;
        }
    draw:
        func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);
    }
}

void Gp_DrawArmorSelectRow(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         item;
    s32         x;
    s32         y;
    s32         color;
    s32         one;
    s32         temp;
    s32         baseY;
    s32         status;

    item   = _gpFindSpareArmor(arg0->currentItemIndex);
    status = arg1->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            if (item == 0) {
                Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            } else {
                Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
            }
        }
    }

    x     = arg0->rowTextX.signedValue;
    y     = arg0->rowTextY.signedValue;
    color = arg0->colorRgb;
    one   = 1;
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
        func_800C22D8(arg1, x, y, item, one);
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg1, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(arg1, x, y, item, 0);
    }

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            Ui_SpawnFromDesc(&D_8010EF30, item, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item | 0x10000, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void Gp_SelectArmorMenuTask(Task* arg0)
{
    UiList*   menu;
    UiObject* obj;
    Task*     parent;
    s32       count;
    s32       found;
    s32       item;
    s32       flags;

    menu        = &D_8010E9F4;
    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawText(&(obj)->panel, Gp_StrSelectArmor);

    if (arg0->state == 0) {
        GP_COUNT_SPARE_ARMOR(count);
        menu->itemCount                     = count;
        menu->visibleRowCount.unsignedValue = 4;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        menu->topInset                           += 0x4C;
        obj->panel.bounds.unsignedRect.h         += 0x4C;
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        parent                                    = arg0->parent;
        Ui_SetState4(parent->spawnArg2.pointer, parent);
        Ui_SpawnFromDesc(&D_8010EC3C, 2, 0, 0x10, obj);
        arg0->state++;
    }

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x4A);
    Ui_UpdateListNoAnim(menu, obj);

    GP_FIND_SPARE_ARMOR(found, menu->selectedItemIndex);
    item = found;
    func_800C7DA8(obj, item, 1, 0);

    flags = 0x12;
    if (item == 0) {
        flags = 0x112;
        goto draw;
    }
    if (((obj->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
        GP_SET_PREVIEW_ITEM(item, 2);
    }
    if (CdCmd_IsIdle() == 0) {
        flags |= 0x100;
    }
draw:
    func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);

    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_DISMISS;
        }
    }

    _gpApplyChildResults(obj, arg0);

    if (obj->result == USER_INTERFACE_RESULT_DISMISS) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

void Gp_ReloadPromptTask(Task* arg0)
{
    UiObject*            obj;
    u8*                  text;
    s32                  lo;
    s32                  hi;
    s32                  width;
    s32                  other;
    s32                  rows;
    s32                  color;
    s32                  one;
    EquipmentWeaponLoad* slot;

    obj         = arg0->spawnArg2.pointer;
    lo          = arg0->spawnArg1.value & 0xFF;
    hi          = (arg0->spawnArg1.value >> 8) & 0xFF;
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawText(&(obj)->panel, Gp_StrReload);
    if (arg0->state == 0) {
        if (lo == 0) {
            slot        = Gp_GetItemSlot(hi);
            arg0->state = 0x10;
            if (Gp_ReloadMode == 1) {
                if (slot->primaryItemId != INVENTORY_ITEM_NONE) {
                    arg0->spawnArg1.value |= slot->primaryItemId;
                    text                   = Gp_GetItemText(slot->primaryItemId, 0, 0);
                } else {
                    arg0->state = 0x20;
                    text        = Gp_StrRemovedAmmo;
                }
            } else if (Gp_ReloadMode == 2) {
                if (slot->secondaryItemId != INVENTORY_ITEM_NONE) {
                    arg0->spawnArg1.value |= slot->secondaryItemId;
                    text                   = Gp_GetItemText(slot->secondaryItemId, 0, 0);
                } else {
                    arg0->state = 0x20;
                    text        = Gp_StrRemovedAmmo;
                }
            } else {
                arg0->state = 0x20;
                text        = Gp_StrRemovedAmmo;
            }
            Gp_ClearEquipSlotSel(hi, Gp_ReloadMode);
            other = Text_MeasureWidth(Gp_StrRemoved);
        } else {
            Gp_SetItemSeenBit(lo, 1);
            text = Gp_GetItemText(lo, 0, 0);
            Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, hi, lo, -1);
            other       = Text_MeasureWidth(Gp_StrLoaded);
            arg0->state = 1;
        }
        if (arg0->state != 0x20) {
            width = Text_MeasureWidth(text) + 0xB;
            if (width < other) {
                width = other;
            }
            rows = 2;
        } else {
            width = Text_MeasureWidth(text);
            rows  = 1;
        }
        Ui_UpdateLayoutSize(&(obj)->panel, width + 5, Ui_Scale15(rows) + 1);
        (&(obj)->panel)->bounds.rect.x = (-(&(obj)->panel)->bounds.rect.w) >> 1;
        if (arg0->state < 0x20) {
            if (arg0->state < 0x10) {
                sndEvtRequestScriptStart(SOUND_AMMO_LOAD, 0, 0);
            } else {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            }
        } else {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        }
        arg0->killCountdown = 0xBC;
    } else if (arg0->state < 0x20) {
        text = Gp_GetItemText(lo, 0, 0);
        if (arg0->state < 0x10) {
            Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Gp_StrLoaded, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        } else {
            Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Gp_StrRemoved, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        }
        one   = 1;
        width = Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, text, 0x37A78, one, TEXT_ALIGNMENT_LEFT);
        color = 0x606060;
        Text_DrawPrompt(obj, width, obj->panel.contentTop.signedValue + 0x1E, Gp_StrDot, color, one, TEXT_ALIGNMENT_LEFT);
    } else {
        Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Gp_StrRemovedAmmo, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
    arg0->killCountdown--;
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if ((arg0->killCountdown <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            obj->result         = USER_INTERFACE_RESULT_DISMISS;
            arg0->killCountdown = 0x7FFF;
        }
    }
}

void Gp_AttachPromptTask(Task* arg0)
{
    UiObject* obj;
    u8*       text;
    s32       color;
    s32       one;
    s32       width;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    text        = Gp_GetItemText(arg0->spawnArg1.value, 0, 0);
    if (arg0->state == 0) {
        width = Text_MeasureWidth(text) + 0x40;
        Ui_UpdateLayoutSize(&(obj)->panel, width, Ui_Scale15(2) + 8);
        (&(obj)->panel)->bounds.rect.x = (-(&(obj)->panel)->bounds.rect.w) >> 1;
        arg0->killCountdown            = 0xBC;
        arg0->state                    = arg0->state + 1;
    } else if (arg0->state == 1) {
        if (obj->panel.state == USER_INTERFACE_PANEL_OPEN) {
            sndEvtRequestScriptStart(SOUND_WEAPON_EQUIP, 0, 0);
            arg0->state = arg0->state + 1;
        }
    }
    Ui_DrawText(&(obj)->panel, Gp_StrAttach);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 6, 0, Gp_StrEquipped, color, one, TEXT_ALIGNMENT_LEFT);
    width = Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 6, 0xE, text, 0x37A78, one, TEXT_ALIGNMENT_LEFT);
    Text_DrawPrompt(obj, width, 0xE, Gp_StrDot, color, one, TEXT_ALIGNMENT_LEFT);
    arg0->killCountdown--;
    if (obj->panel.control.word == one) {
        if (padCheckButtons(0, one, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if ((arg0->killCountdown <= 0) || (padCheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            obj->result         = USER_INTERFACE_RESULT_DISMISS;
            arg0->killCountdown = 0x7FFF;
        }
    }
}

void Gp_EquipPromptTask(Task* arg0)
{
    UiObject*         obj;
    u8*               text;
    s32               color;
    s32               one;
    s32               width;
    s32               val;
    s32               other;
    PlayerStatus*     p;
    InventoryItemRow* rec;
    InventoryItemRow* prev;
    u8                field21;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        val = arg0->spawnArg1.value;
        if ((u32)(val - 0x80) < 0x20U) {
            p       = &gPlayerStatus;
            rec     = Gp_FindItemById(val);
            field21 = p->weapon;
            if (field21 != val - 0x7F) {
                if (field21 != 0) {
                    prev = Gp_FindItemById(field21 + 0x7F);
                    if (rec->attachSlot > INVENTORY_ATTACHMENT_NONE) {
                        prev->attachSlot = rec->attachSlot;
                    } else {
                        Gp_ClearEquipSlotSel(prev->itemId, 0);
                    }
                }
                p->weapon = val - 0x7F;
                Gp_RefreshItemRow(rec);
                Gp_SetItemSeenBit(val, 1);
            }
        } else if ((u32)(val - 0x60) < 0x20U) {
            Gp_EquipMod(val);
        }
        width = Text_MeasureWidth(Gp_GetItemText(arg0->spawnArg1.value, 0, 0)) + 0xB;
        other = Text_MeasureWidth(Gp_StrEquipped);
        if (width < other) {
            width = other;
        }
        Ui_UpdateLayoutSize(&(obj)->panel, width + 5, Ui_Scale15(2) + 1);
        (&(obj)->panel)->bounds.rect.x = (-(&(obj)->panel)->bounds.rect.w) >> 1;
        arg0->killCountdown            = 0xBC;
        arg0->state                    = arg0->state + 1;
    } else if (arg0->state == 1) {
        if (obj->panel.state == USER_INTERFACE_PANEL_OPEN) {
            if ((u32)(arg0->spawnArg1.value - 0x80) < 0x20U) {
                sndEvtRequestScriptStart(SOUND_WEAPON_EQUIP, 0, 0);
            } else {
                sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            }
            arg0->state = arg0->state + 1;
        }
    }
    Ui_DrawText(&(obj)->panel, Gp_StrEquip);
    text  = Gp_GetItemText(arg0->spawnArg1.value, 0, 0);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Gp_StrEquipped, color, one, TEXT_ALIGNMENT_LEFT);
    width = Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, text, 0x37A78, one, TEXT_ALIGNMENT_LEFT);
    Text_DrawPrompt(obj, width, obj->panel.contentTop.signedValue + 0x1E, Gp_StrDot, color, one, TEXT_ALIGNMENT_LEFT);
    arg0->killCountdown--;
    if (obj->panel.control.word == one) {
        if (padCheckButtons(0, one, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if ((arg0->killCountdown <= 0) || (padCheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            obj->result         = USER_INTERFACE_RESULT_DISMISS;
            arg0->killCountdown = 0x7FFF;
        }
    }
}

void Gp_DrawLoadCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    UiObject*   obj;
    s32         val;
    s32         one;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrLoad);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            val = Gp_SelItemRec->itemId;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if ((u32)(val - 0x80) < 0x20U) {
                Gp_ReloadMode = 0;
                one           = 1;
                obj           = Ui_SpawnFromDesc(&D_8010EF14, val, one, one, arg1);
            } else if ((u32)(val - 0xA0) < 0x20U) {
                one = 1;
                obj = Ui_SpawnFromDesc(&D_8010EEDC, val, one, one, arg1);
            } else {
                return;
            }
            if (obj != NULL) {
                Ui_ClampDialogRect(&(obj)->panel, arg0, &(arg1)->panel);
            }
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            arg0->actionResult       = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}

void Gp_DrawExchangeCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    UiObject*   obj;
    s32         val;
    s32         one;
    s32         x;
    s32         y;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrExchange);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            val = 0;
            if (Gp_SelItemRec != NULL) {
                val = Gp_SelItemRec->itemId;
            }
            if (((u32)(val - 0xA0) < 0x20U) || (val == 0)) {
                one = 1;
                obj = Ui_SpawnFromDesc(&D_8010ECC8, gPlayerStatus.weapon + 0x7F, one, 0x10, arg1);
            } else if ((u32)(val - 0x80) < 0x20U) {
                one = 1;
                obj = Ui_SpawnFromDesc(&D_8010ECE4, 0, one, 0x10, arg1);
            } else if ((u32)(val - 0x60) < 0x20U) {
                one = 1;
                obj = Ui_SpawnFromDesc(&D_8010ECAC, 0, one, 0x10, arg1);
            } else {
                return;
            }
            if (obj != NULL) {
                y                                = -0x5C;
                obj->panel.bounds.unsignedRect.y = y;
                x                                = -8;
                obj->panel.bounds.unsignedRect.x = x;
            }
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            arg0->actionResult       = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}
