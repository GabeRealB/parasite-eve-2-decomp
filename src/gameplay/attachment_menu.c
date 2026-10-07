#include "attachments.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/item_menu.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern UiListRowCallback Gp_ItemCmdRows[2];

extern const u8 Gp_StrWrongAmmo2[];

enum {
    ITEM_MENU_ARMOR_ITEM_FIRST       = 0x60,
    ITEM_MENU_EQUIPMENT_ITEM_COUNT_U = 0x20U,
    ITEM_MENU_NOTICE_TIMEOUT_TICKS   = 188,
    ITEM_MENU_NOTICE_DISMISSED_TICKS = 0x7FFF,
    ITEM_MENU_NOTICE_TEXT_COLOR_RGB  = 0x606060,
    ITEM_MENU_NOTICE_ITEM_COLOR_RGB  = 0x037A78
};

/// Returns 1 for armor catalogue ids 0x60..0x7F, otherwise 0.
static inline s32 _itemIsArmorItem(u8 itemId)
{
    return (u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U;
}

#define GP_SET_PREVIEW_ITEM(item, slot)             \
    do {                                            \
        s32* _p;                                    \
        s32  _i;                                    \
                                                    \
        _p = Gp_PreviewItems;                       \
        if ((item) != _p[slot]) {                   \
            for (_i = 0; _i < 3; _i++, _p++) {      \
                if (_i == (slot)) {                 \
                    *_p = (item);                   \
                } else {                            \
                    *_p = -1;                       \
                }                                   \
            }                                       \
            itemMenuEnqueuePreviewLoad(item, slot); \
        }                                           \
    } while (0)
#define GP_FIND_SPARE_ARMOR(found, index)                                                 \
    do {                                                                                  \
        PlayerStatus*       _cfg;                                                         \
        InventoryItemRange* _scan;                                                        \
        InventoryItemRow*   _rec;                                                         \
        s32                 _i;                                                           \
        s32                 _n;                                                           \
                                                                                          \
        _scan   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;                 \
        _cfg    = &gPlayerStatus;                                                         \
        _n      = (index);                                                                \
        _rec    = inventoryGetRangeTable(_scan);                                          \
        (found) = _i = 0;                                                                 \
        _rec         = &_rec[_scan->firstRow];                                            \
        for (; _i < _scan->rowCount; _i++) {                                              \
            if (_itemIsArmorItem(_rec->itemId) && (_cfg->armor != _rec->itemId - 0x5F)) { \
                _n--;                                                                     \
                if (_n < 0) {                                                             \
                    (found) = _rec->itemId;                                               \
                    break;                                                                \
                }                                                                         \
            }                                                                             \
            _rec++;                                                                       \
        }                                                                                 \
    } while (0)
#define GP_COUNT_SPARE_ARMOR(count)                                                       \
    do {                                                                                  \
        PlayerStatus*       _cfg;                                                         \
        InventoryItemRange* _scan;                                                        \
        InventoryItemRow*   _rec;                                                         \
        s32                 _i;                                                           \
                                                                                          \
        (count) = 0;                                                                      \
        _scan   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;                 \
        _cfg    = &gPlayerStatus;                                                         \
        _rec    = inventoryGetRangeTable(_scan);                                          \
        _i      = 0;                                                                      \
        _rec    = &_rec[_scan->firstRow];                                                 \
        for (; _i < _scan->rowCount; _i++) {                                              \
            if (_itemIsArmorItem(_rec->itemId) && (_cfg->armor != _rec->itemId - 0x5F)) { \
                (count)++;                                                                \
            }                                                                             \
            _rec++;                                                                       \
        }                                                                                 \
    } while (0)
/// Returns the armor item id at a zero-based position in the unequipped list, or 0.
///
/// Scans the live carried range in row order, skipping every row of the equipped
/// armor id. The range must fit its readable table. Negative list positions
/// select the first eligible row; positions past the list return INVENTORY_ITEM_NONE.
/// Duplicate item ids remain separate rows; quantity and attachment markers do
/// not affect eligibility. No saved state is changed or pointer retained.
static inline s32 _itemMenuGetUnequippedArmorItem(s32 listIndex)
{
    const PlayerStatus*       player;
    const InventoryItemRange* carriedRange;
    const InventoryItemRow*   row;
    s32                       rowIndex;
    s32                       armorItemId;

    carriedRange = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    player       = &gPlayerStatus;
    row          = inventoryGetRangeTable(carriedRange);
    armorItemId  = INVENTORY_ITEM_NONE;
    rowIndex     = 0;
    row          = &row[carriedRange->firstRow];
    for (; rowIndex < carriedRange->rowCount; rowIndex++) {
        if (_itemIsArmorItem(row->itemId) && (player->armor != row->itemId - (ITEM_MENU_ARMOR_ITEM_FIRST - 1))) {
            listIndex--;
            if (listIndex < 0) {
                armorItemId = row->itemId;
                break;
            }
        }
        row++;
    }
    return armorItemId;
}

/// Propagates child dismissal/cancellation and closes accepted child dialogs.
///
/// The live task's circular child ring must contain task-owned UiObjects in
/// spawnArg2. CONFIRM detaches and starts closing that child's subtree, then
/// reactivates the parent panel; DISMISS and CANCEL copy to the parent's result.
/// Other outcomes leave it intact. Closing defers release to later task updates.
/// Reaching the current ring head ends the pass, including after closing its
/// previous head while siblings remain.
static inline void _itemMenuApplyChildDialogResults(UiObject* object, Task* task)
{
    Task*     child;
    Task*     nextSibling;
    Task*     childHead;
    UiObject* childObject;
    s32       childResult;

    child = task->firstChild;
    if (child != NULL) {
        do {
            childObject = child->spawnArg2.pointer;
            childResult = childObject->result;
            // Closing detaches the child, so save its link before handling the result.
            nextSibling = child->nextSibling;
            switch (childResult) {
                case USER_INTERFACE_RESULT_DISMISS:
                    object->result = childResult;
                    break;
                case USER_INTERFACE_RESULT_CANCEL:
                    object->result = childResult;
                    break;
                case USER_INTERFACE_RESULT_CONFIRM:
                    uiStartTreeClosing(childObject, childObject->owner);
                    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    break;
            }
            childHead = task->firstChild;
            child     = nextSibling;
            if (child == childHead) {
                break;
            }
            if (childHead == NULL) {
                break;
            }
        } while (1);
    }
}

/// Switches to a carried weapon, transferring its armor slot to the previous weapon.
///
/// Both weapon rows must exist when their selectors are nonzero. If the new
/// weapon has no positive attachment slot, the previous weapon's removable
/// loads are cleared. A changed weapon is detached from armor and identified.
static inline void _equipmentEquipCarriedWeapon(s32 weaponItemId)
{
    PlayerStatus*     player;
    InventoryItemRow* weaponRow;
    InventoryItemRow* previousWeaponRow;
    u8                previousWeapon;

    player         = &gPlayerStatus;
    weaponRow      = inventoryFindLastCarriedItemRow(weaponItemId);
    previousWeapon = player->weapon;
    if (previousWeapon != weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
        if (previousWeapon != PLAYER_STATUS_EQUIPMENT_NONE) {
            previousWeaponRow = inventoryFindLastCarriedItemRow(previousWeapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1));
            if (weaponRow->attachSlot > INVENTORY_ATTACHMENT_NONE) {
                previousWeaponRow->attachSlot = weaponRow->attachSlot;
            } else {
                equipmentClearSelectedRemovableLoads(previousWeaponRow->itemId, EQUIPMENT_CLEAR_LOAD_BOTH);
            }
        }
        player->weapon = weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
        inventoryDetachItem(weaponRow);
        itemSetIdentified(weaponItemId, 1);
    }
}

/// Advances a notice's tick counter and publishes dismissal or menu cancellation.
///
/// The live task owns the object. Opening and inactive panels still count down;
/// only active panels act on expiration or pressed buttons. Dismissal resets the
/// signed-halfword counter to defer another timeout during the closing animation.
static inline void _itemMenuUpdateNoticeResult(UiObject* object, Task* task)
{
    task->killCountdown--;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if ((task->killCountdown <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            object->result      = USER_INTERFACE_RESULT_DISMISS;
            task->killCountdown = ITEM_MENU_NOTICE_DISMISSED_TICKS;
        }
    }
}

UiObjectDesc D_8010F6FC    = { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, -30, 120, 60 }, 8, 0, TASK_BODY_NONE, 192, itemMenuDiscardTask, 0 };
UiObjectDesc D_8010F718[4] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -80, 144, 72 }, 292, 0, TASK_BODY_NONE, 192, itemMenuPeElementTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -8, 144, 72 }, 288, 0, TASK_BODY_NONE, 192, itemMenuPeElementTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -80, 144, 72 }, 284, 0, TASK_BODY_NONE, 192, itemMenuPeElementTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -8, 144, 72 }, 280, 0, TASK_BODY_NONE, 192, itemMenuPeElementTask, 0 },
};
UiObjectDesc D_8010F788    = { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, 0, 1, 1 }, 8, 0, TASK_BODY_NONE, 192, itemMenuNoticeTask, 0 };
UiObjectDesc D_8010F7A4    = { 0, { -128, 64, 256, 32 }, 264, 0, TASK_BODY_NONE, 192, itemMenuPeUpgradeTask, 0 };
UiObjectDesc D_8010F7C0[2] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -128, -80, 192, 144 }, 12, 0, TASK_BODY_NONE, 192, itemMenuPeNextLevelTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -96, -72, 176, 112 }, 8, 0, TASK_BODY_NONE, 192, itemMenuHealingTask, 0 },
};
UiObjectDesc      D_8010F7F8          = { USER_INTERFACE_PANEL_TITLE_STYLE, { -96, -104, 192, 208 }, 8, 0, TASK_BODY_NONE, 192, itemMenuPeSpecificationsTask, 0 };
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
        uiFitPanelToList(menu, &(obj)->panel);
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
                    slot = equipmentGetWeaponLoad(val);
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
            uiSizePanelForTextDefault(&(obj)->panel, Gp_StrWrongAmmo2);
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
        uiUpdateList(menu, &obj->panel);
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
                        uiStartTreeClosing(childObj, childObj->owner);
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
        s32 drawMode = one;

        uiDrawPanelLabel(&(obj)->panel, Gp_StrNotice);
        textDrawUiLines(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Gp_StrWrongAmmo2, 0x606060, drawMode, TEXT_ALIGNMENT_LEFT);
    } else {
        uiDrawPanelLabel(&(obj)->panel, Gp_StrEquip);
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

    menu       = &D_8010E9CC;
    obj        = arg0->spawnArg2.pointer;
    savedState = arg0->state;
    Gp_AttachListTask(arg0);
    if (obj->result == USER_INTERFACE_RESULT_DISMISS) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
    state = arg0->state;
    if (state == 1) {
        uiDrawPanelLabel(&(obj)->panel, Gp_StrSelectAmmo);
        if (savedState == 0) {
            menu->topInset                   += 0x4C;
            obj->panel.bounds.unsignedRect.h += 0x4C;
            parent                            = arg0->parent;
            D_80114DD8                        = -1;
            uiStartPanelHiding(parent->spawnArg2.pointer, parent);
            uiSpawnObject(&D_8010EAB4[14], 1, 0, 0x10, obj);
        }
        uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x4A);
        val = Gp_AttachListIds[menu->selectedItemIndex];
        if (val != 0) {
            func_800C7DA8(obj, val, 1, 0);
        }
        flags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
        if (val == 0) {
            flags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT | ITEM_MENU_PREVIEW_HIDDEN;
        } else {
            if (((obj->panel.control.word >> 16) == state) || (obj->panel.control.word == state)) {
                GP_SET_PREVIEW_ITEM(val, 2);
            }
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                flags |= ITEM_MENU_PREVIEW_HIDDEN;
            }
        }
        itemMenuDrawPreview(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);
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

    item   = _itemMenuGetUnequippedArmorItem(arg0->currentItemIndex);
    status = arg1->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            if (item == 0) {
                uiSetPromptText(Gp_StrEmpty, 0, 0);
            } else {
                uiSetPromptText(itemGetText(item, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
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
        textDrawString(&req, itemGetText(item, ITEM_TEXT_NAME, 0));
        itemMenuDrawEquipmentMarker(arg1, x, y, item, one);
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            itemMenuDrawParasiteEnergyLevel(arg1, x, y, temp % 3 + 1, color);
        }
        itemMenuDrawItemIcon(arg1, x, y, item, ITEM_MENU_ICON_DEFAULT);
    }

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            uiSpawnObject(&D_8010EAB4[41], item, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EAB4[45], item | 0x10000, 1, 1, arg1);
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
    uiDrawPanelLabel(&(obj)->panel, Gp_StrSelectArmor);

    if (arg0->state == 0) {
        GP_COUNT_SPARE_ARMOR(count);
        menu->itemCount                     = count;
        menu->visibleRowCount.unsignedValue = 4;
        uiFitPanelToList(menu, &(obj)->panel);
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        menu->topInset                           += 0x4C;
        obj->panel.bounds.unsignedRect.h         += 0x4C;
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        parent                                    = arg0->parent;
        uiStartPanelHiding(parent->spawnArg2.pointer, parent);
        uiSpawnObject(&D_8010EAB4[14], 2, 0, 0x10, obj);
        arg0->state++;
    }

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x4A);
    uiUpdateList(menu, &obj->panel);

    GP_FIND_SPARE_ARMOR(found, menu->selectedItemIndex);
    item = found;
    func_800C7DA8(obj, item, 1, 0);

    flags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
    if (item == 0) {
        flags = ITEM_MENU_PREVIEW_TEXTURE_RELOCATED | ITEM_MENU_PREVIEW_SCALE_EQUIPMENT | ITEM_MENU_PREVIEW_HIDDEN;
    } else {
        if (((obj->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
            GP_SET_PREVIEW_ITEM(item, 2);
        }
        if (cdCmdIsIdle() == 0) {
            flags |= ITEM_MENU_PREVIEW_HIDDEN;
        }
    }
    itemMenuDrawPreview(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);

    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_DISMISS;
        }
    }

    _itemMenuApplyChildDialogResults(obj, arg0);

    if (obj->result == USER_INTERFACE_RESULT_DISMISS) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

void itemMenuReloadNoticeTask(Task* task)
{
    enum {
        ITEM_MENU_RELOAD_NOTICE_START          = 0,
        ITEM_MENU_RELOAD_NOTICE_LOADED         = 1,
        ITEM_MENU_RELOAD_NOTICE_REMOVED_ITEM   = 0x10,
        ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS  = 0x20,
        ITEM_MENU_RELOAD_ARGUMENT_ITEM_MASK    = 0xFF,
        ITEM_MENU_RELOAD_ARGUMENT_WEAPON_SHIFT = 8
    };
    UiObject*                  obj;
    const u8*                  itemText;
    s32                        consumableItemId;
    s32                        weaponItemId;
    s32                        contentWidth;
    s32                        verbWidth;
    s32                        textRows;
    s32                        textColorRgb;
    s32                        drawMode;
    s32                        textEndX;
    const EquipmentWeaponLoad* weaponLoad;

    obj              = task->spawnArg2.pointer;
    consumableItemId = task->spawnArg1.value & ITEM_MENU_RELOAD_ARGUMENT_ITEM_MASK;
    weaponItemId     = (task->spawnArg1.value >> ITEM_MENU_RELOAD_ARGUMENT_WEAPON_SHIFT) & ITEM_MENU_RELOAD_ARGUMENT_ITEM_MASK;
    obj->result      = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&obj->panel, Gp_StrReload);
    if (task->state == ITEM_MENU_RELOAD_NOTICE_START) {
        if (consumableItemId == INVENTORY_ITEM_NONE) {
            // Preserve the selected item's name before clearing its load record.
            weaponLoad  = equipmentGetWeaponLoad(weaponItemId);
            task->state = ITEM_MENU_RELOAD_NOTICE_REMOVED_ITEM;
            if (Gp_ReloadMode == EQUIPMENT_CLEAR_LOAD_PRIMARY) {
                if (weaponLoad->primaryItemId != INVENTORY_ITEM_NONE) {
                    task->spawnArg1.value |= weaponLoad->primaryItemId;
                    itemText               = itemGetText(weaponLoad->primaryItemId, ITEM_TEXT_NAME, 0);
                } else {
                    task->state = ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS;
                    itemText    = (const u8*)Gp_StrRemovedAmmo;
                }
            } else if (Gp_ReloadMode == EQUIPMENT_CLEAR_LOAD_SECONDARY) {
                if (weaponLoad->secondaryItemId != INVENTORY_ITEM_NONE) {
                    task->spawnArg1.value |= weaponLoad->secondaryItemId;
                    itemText               = itemGetText(weaponLoad->secondaryItemId, ITEM_TEXT_NAME, 0);
                } else {
                    task->state = ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS;
                    itemText    = (const u8*)Gp_StrRemovedAmmo;
                }
            } else {
                task->state = ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS;
                itemText    = (const u8*)Gp_StrRemovedAmmo;
            }
            equipmentClearSelectedRemovableLoads(weaponItemId, Gp_ReloadMode);
            verbWidth = textMeasureLineWidth((const u8*)Gp_StrRemoved);
        } else {
            itemSetIdentified(consumableItemId, 1);
            itemText = itemGetText(consumableItemId, ITEM_TEXT_NAME, 0);
            equipmentLoadWeaponConsumable(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, weaponItemId, consumableItemId, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY);
            verbWidth   = textMeasureLineWidth((const u8*)Gp_StrLoaded);
            task->state = ITEM_MENU_RELOAD_NOTICE_LOADED;
        }
        if (task->state != ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS) {
            contentWidth = textMeasureLineWidth(itemText) + 0xB;
            if (contentWidth < verbWidth) {
                contentWidth = verbWidth;
            }
            textRows = 2;
        } else {
            contentWidth = textMeasureLineWidth(itemText);
            textRows     = 1;
        }
        uiSetPanelContentSize(&obj->panel, contentWidth + 5, uiGetTextRowsHeight(textRows) + 1);
        obj->panel.bounds.rect.x = (-obj->panel.bounds.rect.w) >> 1;
        if (task->state < ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS) {
            if (task->state < ITEM_MENU_RELOAD_NOTICE_REMOVED_ITEM) {
                sndEvtRequestScriptStart(SOUND_AMMO_LOAD, 0, 0);
            } else {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            }
        } else {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        }
        task->killCountdown = ITEM_MENU_NOTICE_TIMEOUT_TICKS;
    } else if (task->state < ITEM_MENU_RELOAD_NOTICE_REMOVED_LOADS) {
        itemText = itemGetText(consumableItemId, ITEM_TEXT_NAME, 0);
        if (task->state < ITEM_MENU_RELOAD_NOTICE_REMOVED_ITEM) {
            textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrLoaded, ITEM_MENU_NOTICE_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        } else {
            textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrRemoved, ITEM_MENU_NOTICE_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        }
        drawMode     = TEXT_DRAW_OUTLINED;
        textEndX     = textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, itemText, ITEM_MENU_NOTICE_ITEM_COLOR_RGB, drawMode, TEXT_ALIGNMENT_LEFT);
        textColorRgb = ITEM_MENU_NOTICE_TEXT_COLOR_RGB;
        textDrawUiLine(obj, textEndX, obj->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    } else {
        textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrRemovedAmmo, ITEM_MENU_NOTICE_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
    _itemMenuUpdateNoticeResult(obj, task);
}

void itemMenuAttachNoticeTask(Task* task)
{
    enum {
        ITEM_MENU_ATTACH_NOTICE_START         = 0,
        ITEM_MENU_ATTACH_NOTICE_WAIT_FOR_OPEN = 1
    };
    UiObject* obj;
    const u8* itemText;
    s32       textColorRgb;
    s32       contentWidth;
    s32       textEndX;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    itemText    = itemGetText(task->spawnArg1.value, ITEM_TEXT_NAME, 0);
    if (task->state == ITEM_MENU_ATTACH_NOTICE_START) {
        contentWidth = textMeasureLineWidth(itemText) + 0x40;
        uiSetPanelContentSize(&obj->panel, contentWidth, uiGetTextRowsHeight(2) + 8);
        obj->panel.bounds.rect.x = (-obj->panel.bounds.rect.w) >> 1;
        task->killCountdown      = ITEM_MENU_NOTICE_TIMEOUT_TICKS;
        task->state              = task->state + 1;
    } else if (task->state == ITEM_MENU_ATTACH_NOTICE_WAIT_FOR_OPEN) {
        if (obj->panel.state == USER_INTERFACE_PANEL_OPEN) {
            sndEvtRequestScriptStart(SOUND_WEAPON_EQUIP, 0, 0);
            task->state = task->state + 1;
        }
    }
    uiDrawPanelLabel(&obj->panel, Gp_StrAttach);
    textColorRgb = ITEM_MENU_NOTICE_TEXT_COLOR_RGB;
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 6, 0, (const u8*)Gp_StrEquipped, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textEndX = textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 6, 0xE, itemText, ITEM_MENU_NOTICE_ITEM_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, textEndX, 0xE, (const u8*)Gp_StrDot, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    _itemMenuUpdateNoticeResult(obj, task);
}

void itemMenuEquipNoticeTask(Task* task)
{
    enum {
        ITEM_MENU_EQUIP_NOTICE_START         = 0,
        ITEM_MENU_EQUIP_NOTICE_WAIT_FOR_OPEN = 1
    };
    UiObject* obj;
    const u8* itemText;
    s32       textColorRgb;
    s32       contentWidth;
    s32       itemId;
    s32       verbWidth;
    s32       textEndX;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_MENU_EQUIP_NOTICE_START) {
        // Apply the selection before sizing and displaying its confirmation.
        itemId = task->spawnArg1.value;
        if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
            _equipmentEquipCarriedWeapon(itemId);
        } else if ((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
            equipmentEquipCarriedArmor(itemId);
        }
        contentWidth = textMeasureLineWidth(itemGetText(task->spawnArg1.value, ITEM_TEXT_NAME, 0)) + 0xB;
        verbWidth    = textMeasureLineWidth((const u8*)Gp_StrEquipped);
        if (contentWidth < verbWidth) {
            contentWidth = verbWidth;
        }
        uiSetPanelContentSize(&obj->panel, contentWidth + 5, uiGetTextRowsHeight(2) + 1);
        obj->panel.bounds.rect.x = (-obj->panel.bounds.rect.w) >> 1;
        task->killCountdown      = ITEM_MENU_NOTICE_TIMEOUT_TICKS;
        task->state              = task->state + 1;
    } else if (task->state == ITEM_MENU_EQUIP_NOTICE_WAIT_FOR_OPEN) {
        if (obj->panel.state == USER_INTERFACE_PANEL_OPEN) {
            if ((u32)(task->spawnArg1.value - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
                sndEvtRequestScriptStart(SOUND_WEAPON_EQUIP, 0, 0);
            } else {
                sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            }
            task->state = task->state + 1;
        }
    }
    uiDrawPanelLabel(&obj->panel, Gp_StrEquip);
    itemText     = itemGetText(task->spawnArg1.value, ITEM_TEXT_NAME, 0);
    textColorRgb = ITEM_MENU_NOTICE_TEXT_COLOR_RGB;
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrEquipped, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textEndX = textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, itemText, ITEM_MENU_NOTICE_ITEM_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, textEndX, obj->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    _itemMenuUpdateNoticeResult(obj, task);
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
                obj           = uiSpawnObject(&D_8010EAB4[40], val, one, one, arg1);
            } else if ((u32)(val - 0xA0) < 0x20U) {
                one = 1;
                obj = uiSpawnObject(&D_8010EAB4[38], val, one, one, arg1);
            } else {
                return;
            }
            if (obj != NULL) {
                uiPositionRowDialog(&(obj)->panel, arg0, &(arg1)->panel);
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
                obj = uiSpawnObject(&D_8010EAB4[19], gPlayerStatus.weapon + 0x7F, one, 0x10, arg1);
            } else if ((u32)(val - 0x80) < 0x20U) {
                one = 1;
                obj = uiSpawnObject(&D_8010EAB4[20], 0, one, 0x10, arg1);
            } else if ((u32)(val - 0x60) < 0x20U) {
                one = 1;
                obj = uiSpawnObject(&D_8010EAB4[18], 0, one, 0x10, arg1);
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
