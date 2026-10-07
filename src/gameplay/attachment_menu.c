#include "attachments.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/inventory.h"
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

/// Opening delay for equipment command and confirmation panels, in callback ticks.
enum { ITEM_MENU_COMMAND_OPEN_DELAY_TICKS = 1 };

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

/// Selects a carried weapon while preserving equipment in its former armor slot.
///
/// weaponItemId must be 0x80..0x9F; a changed selection requires its carried row
/// and the previous weapon's row when the old selector is nonzero. The live
/// carried range must fit its table. Selecting the current weapon changes nothing.
/// The previous weapon inherits a positive armor slot from the new row; without
/// one, its removable load selections and quantities are cleared, preserving
/// built-in supplies. The new one-based selector is stored before detaching the
/// row so its loads survive detachment, then the item is marked identified.
/// Row pointers are borrowed only for this call; no inventory quantity changes.
static inline void _equipmentEquipCarriedWeapon(s32 weaponItemId)
{
    PlayerStatus*     player;
    InventoryItemRow* weaponRow;
    InventoryItemRow* previousWeaponRow;
    u8                previousWeaponSelector;

    player                 = &gPlayerStatus;
    weaponRow              = inventoryFindLastCarriedItemRow(weaponItemId);
    previousWeaponSelector = player->weapon;
    if (previousWeaponSelector == weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
        return;
    }
    // Leave the previous weapon in the incoming weapon's armor position, or unload it.
    if (previousWeaponSelector != PLAYER_STATUS_EQUIPMENT_NONE) {
        previousWeaponRow = inventoryFindLastCarriedItemRow(previousWeaponSelector + (EQUIPMENT_WEAPON_ITEM_FIRST - 1));
        if (weaponRow->attachSlot > INVENTORY_ATTACHMENT_NONE) {
            previousWeaponRow->attachSlot = weaponRow->attachSlot;
        } else {
            equipmentClearSelectedRemovableLoads(previousWeaponRow->itemId, EQUIPMENT_CLEAR_LOAD_BOTH);
        }
    }
    // Detachment keeps loads belonging to the weapon selected here.
    player->weapon = weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
    inventoryDetachItem(weaponRow);
    itemSetIdentified(weaponItemId, 1);
}

/// Advances a notice's tick counter and publishes dismissal or menu cancellation.
///
/// task and its owned object must remain live, with killCountdown initialized by
/// the notice. Decrements once per callback, narrowing to s16 even while inactive.
/// Only a fully active control word accepts input or expiry. Menu takes priority
/// and publishes CANCEL without resetting the counter. Otherwise expiry or
/// Confirm/Cancel publishes DISMISS and resets the counter to 32767, deferring
/// repeated expiry during teardown. Leaves any existing result intact otherwise;
/// the caller clears it each update and handles closing. No object is released.
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
UiListRowCallback Gp_ItemCmdRows[2]   = { itemMenuDrawHotspotActionRow, Gp_DrawItemCmd };
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

/// Draws a visible armor choice's name, equipment status and item icon.
///
/// x/y are signed panel-relative row pixels. Origin-Y subtraction is promoted
/// to s32 before adding y. The retained P.E. level path handles ordinary ability
/// item ids; the armor-choice scan itself returns only armor or an empty id.
/// attachmentState follows `itemMenuDrawEquipmentMarker`; object is borrowed
/// for this draw, with menu/text textures and writable GPU storage required.
static inline void _itemMenuDrawArmorChoiceContents(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState)
{
    enum {
        ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST = 15,
        ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT = ATTACHMENT_SPELL_COUNT * ATTACHMENT_AREA_LEVEL_COUNT
    };
    TextDrawReq nameRequest;
    s32         abilityItemOffset;
    s32         textBaseY;

    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        nameRequest.x          = object->panel.contentOriginX.unsignedValue + 0x11 + x;
        textBaseY              = object->panel.contentOriginY.unsignedValue - 6;
        nameRequest.y          = textBaseY + y;
        nameRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        nameRequest.colorRgb   = colorRgb;
        nameRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        nameRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        nameRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&nameRequest, itemGetText(itemId, ITEM_TEXT_NAME, 0));
        itemMenuDrawEquipmentMarker(object, x, y, itemId, attachmentState);
        abilityItemOffset = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        if ((u32)abilityItemOffset < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
            itemMenuDrawParasiteEnergyLevel(object, x, y, abilityItemOffset % ATTACHMENT_AREA_LEVEL_COUNT + 1, colorRgb);
        }
        itemMenuDrawItemIcon(object, x, y, itemId, ITEM_MENU_ICON_DEFAULT);
    }
}

void itemMenuDrawArmorChoiceRow(UiList* list, UiObject* object)
{
    enum {
        ITEM_MENU_ARMOR_EQUIP_NOTICE_PANEL = 41,
        ITEM_MENU_ARMOR_INFO_PANEL         = 45
    };
    s32 armorItemId;
    s32 panelControl;
    s32 rowX;
    s32 rowY;
    s32 textColorRgb;
    s32 attachmentState;

    armorItemId  = _itemMenuGetUnequippedArmorItem(list->currentItemIndex);
    panelControl = object->panel.control.word;
    // Keep the selected description while the active panel is suspended by a child.
    if (((panelControl >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            if (armorItemId == INVENTORY_ITEM_NONE) {
                uiSetPromptText((const u8*)Gp_StrEmpty, 0, 0);
            } else {
                uiSetPromptText(itemGetText(armorItemId, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
            }
        }
    }

    rowX            = list->rowTextX.signedValue;
    rowY            = list->rowTextY.signedValue;
    textColorRgb    = list->colorRgb;
    attachmentState = ITEM_MENU_ATTACHMENT_MARK_UNATTACHED;
    _itemMenuDrawArmorChoiceContents(object, rowX, rowY, armorItemId, textColorRgb, attachmentState);

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_ARMOR_EQUIP_NOTICE_PANEL], armorItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_ARMOR_INFO_PANEL], armorItemId | ITEM_MENU_INFO_RELOCATED_PREVIEW, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
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

/// Draws an equipment command label at the current list row's pixel position.
///
/// Coordinates retain the unsigned halfword views used by command lists.
/// The encoded label is borrowed for this draw; panel visibility does not gate it.
static inline void _itemMenuDrawEquipmentCommandLabel(const UiList* list, const UiObject* object, const u8* label)
{
    TextDrawReq labelRequest;

    labelRequest.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    labelRequest.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    labelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    labelRequest.colorRgb   = list->colorRgb;
    labelRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    labelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    labelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&labelRequest, label);
}

void itemMenuDrawLoadRow(UiList* list, UiObject* object)
{
    enum {
        ITEM_MENU_LOAD_WEAPON_CHOICE_PANEL = 38,
        ITEM_MENU_LOAD_SLOT_CHOICE_PANEL   = 40
    };
    UiObject* loadDialog;
    s32       selectedItemId;

    _itemMenuDrawEquipmentCommandLabel(list, object, (const u8*)Gp_StrLoad);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            // Capture the selected row before the sound request; it remains the dialog payload.
            selectedItemId = Gp_SelItemRec->itemId;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if ((u32)(selectedItemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
                Gp_ReloadMode = EQUIPMENT_CLEAR_LOAD_BOTH;
                loadDialog    = uiSpawnObject(&D_8010EAB4[ITEM_MENU_LOAD_SLOT_CHOICE_PANEL], selectedItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            } else if ((u32)(selectedItemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
                loadDialog = uiSpawnObject(&D_8010EAB4[ITEM_MENU_LOAD_WEAPON_CHOICE_PANEL], selectedItemId, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_COMMAND_OPEN_DELAY_TICKS, object);
            } else {
                return;
            }
            if (loadDialog != NULL) {
                uiPositionRowDialog(&loadDialog->panel, list, &object->panel);
            }
            // A recognized command consumes input even if its child allocation failed.
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            list->actionResult         = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}

void itemMenuDrawExchangeRow(UiList* list, UiObject* object)
{
    enum {
        ITEM_MENU_EXCHANGE_ARMOR_CHOICE_PANEL  = 18,
        ITEM_MENU_EXCHANGE_AMMO_CHOICE_PANEL   = 19,
        ITEM_MENU_EXCHANGE_WEAPON_CHOICE_PANEL = 20,
        ITEM_MENU_EXCHANGE_OPEN_DELAY_TICKS    = 16
    };
    UiObject* exchangeDialog;
    s32       selectedItemId;

    _itemMenuDrawEquipmentCommandLabel(list, object, (const u8*)Gp_StrExchange);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            selectedItemId = INVENTORY_ITEM_NONE;
            if (Gp_SelItemRec != NULL) {
                selectedItemId = Gp_SelItemRec->itemId;
            }
            // An empty or consumable slot chooses ammunition for the equipped weapon.
            if (((u32)(selectedItemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) || (selectedItemId == INVENTORY_ITEM_NONE)) {
                exchangeDialog = uiSpawnObject(&D_8010EAB4[ITEM_MENU_EXCHANGE_AMMO_CHOICE_PANEL], gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1), USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_EXCHANGE_OPEN_DELAY_TICKS, object);
            } else if ((u32)(selectedItemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
                exchangeDialog = uiSpawnObject(&D_8010EAB4[ITEM_MENU_EXCHANGE_WEAPON_CHOICE_PANEL], INVENTORY_ITEM_NONE, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_EXCHANGE_OPEN_DELAY_TICKS, object);
            } else if ((u32)(selectedItemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ITEM_MENU_EQUIPMENT_ITEM_COUNT_U) {
                exchangeDialog = uiSpawnObject(&D_8010EAB4[ITEM_MENU_EXCHANGE_ARMOR_CHOICE_PANEL], INVENTORY_ITEM_NONE, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_EXCHANGE_OPEN_DELAY_TICKS, object);
            } else {
                return;
            }
            if (exchangeDialog != NULL) {
                exchangeDialog->panel.bounds.rect.y = -0x5C;
                exchangeDialog->panel.bounds.rect.x = -8;
            }
            // A recognized command consumes input even if its child allocation failed.
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            list->actionResult         = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}
