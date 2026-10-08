#include "item_menu.h"

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "cdcmd.h"
#include "items.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern const char Gp_StrSelectTitle[];

extern char Gp_StrDetachArmorHelp[];

/// Draws `item`'s name, its equipment status marker in `mode`, the variant
/// marker for items 0x0F-0x32 and its icon at (`x`, `y`) in `obj`. Nothing is
/// drawn while `obj->panel.state` is `USER_INTERFACE_PANEL_HIDDEN`.
static inline void _gpDrawItemNameAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item, s32 mode);

/// Draws `item` as `_gpDrawItemNameAt` does, at the prompt row's position and
/// in its colour.
static inline void _gpDrawItemName(UiList* prompt, UiObject* obj, s32 item, s32 mode);

/// Whether item `id` is the equipped weapon, the equipped armour, or a
/// consumable selected in either firing mode of the equipped weapon.
static inline s32 _gpIsEquippedItem(s32 id);

static void _itemMenuSizeAttachmentPicker(UiList* list, UiObject* unused);

static inline s32 _itemMenuDecodeEnergyPreviewIndex(s32 packedAbilityId);

const char Gp_StrPEnergy[] = "P.Energy";

const char Gp_StrOption[] = "Option";

const char Gp_StrExit[] = "Exit";

const char Gp_StrSlash[] = "/";

const char Gp_StrHp[] = "HP";

const char Gp_StrMp[] = "MP";

const char Gp_StrExp[] = "EXP";

const char Gp_StrBp[] = "BP";

const char Gp_StrArmor[] = "Armor";

const char Gp_StrAttachments[] = "Attachments";

const char Gp_StrWeaponTitle[] = "Weapon";

const char Gp_StrE[] = "E";

const char Gp_StrItemHdr[] = "Item";

const char Gp_StrAttachments2[] = "ATTACHMENTs";

const char Gp_StrSelectTitle[] = "Select";

const char Gp_StrNextReplay[] = "NEXT REPLAY SUPPLY";

const char Gp_StrSpecs[] = "Specifications";

const char Gp_StrOperation[] = "OPERATION";

const char Gp_StrAddHp[] = "ADD HP";

const char D_8009707C[] = "-";

const char Gp_StrAddMp[] = "ADD MP";

const char Gp_StrAttachments3[] = "ATTACHMENTS";

const char Gp_StrSpecialFeat[] = "SPECIAL FEATURES";

const char Gp_StrPowerCaps[] = "POWER";

const char Gp_StrCapacity[] = "CAPACITY";

const char Gp_StrSpecial[] = "SPECIAL";

const char Gp_StrApplicableWpn[] = "APPLICABLE WEAPONS";

const char Gp_StrNotice[] = "Notice";

const char Gp_StrKeyItem[] = "Key Item";

const char gGpStrWeight[] = "Weight";

const char gGpStrRate[] = "Rate";

const char gGpStrRange[] = "Range";

const char gGpStrPower[] = "Power";

const char gGpStrAttachDot[] = "Attach.";

const char Gp_StrAttention[] = "Attention";

const char Gp_StrSelectWeapon[] = "Select Weapon";

const char Gp_StrEquip[] = "Equip";

const char Gp_StrSelectAmmo[] = "Select AMMO";

const char Gp_StrSelectArmor[] = "Select Armor";

const char Gp_StrReload[] = "Reload";

const char Gp_StrAttach[] = "Attach";

char Gp_StrCustomizeHelp[]     = "Customize game settings.";
char Gp_StrRemoveAmmoHelp[]    = "Remove loaded ammunition.";
char Gp_StrDetachArmorHelp[]   = "Detach items from armor.";
char Gp_StrChangeOrderHelp[36] = "Change the order of items carried.\000\335";

/// Draws `item`'s name, its equipment status marker in `mode`, the variant
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
        textDrawString(&req, itemGetText(item, ITEM_TEXT_NAME, 0));
        itemMenuDrawEquipmentMarker(obj, x, y, item, mode);
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            itemMenuDrawParasiteEnergyLevel(obj, x, y, temp % 3 + 1, color);
        }
        itemMenuDrawItemIcon(obj, x, y, item, ITEM_MENU_ICON_DEFAULT);
    }
}

/// Draws `item` as `_gpDrawItemNameAt` does, at the prompt row's position and
/// in its colour.
static inline void _gpDrawItemName(UiList* prompt, UiObject* obj, s32 item, s32 mode)
{
    _gpDrawItemNameAt(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, prompt->colorRgb, item, mode);
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
         ((equipmentGetWeaponLoad(p->weapon + 0x7F)->primaryItemId == id) ||
          (equipmentGetWeaponLoad(p->weapon + 0x7F)->secondaryItemId == id)))) {
        ret = 1;
    }
    return ret;
}

/// Draws an attachment candidate's consumable stock remaining outside weapon loads.
///
/// Requires live list/object, font textures and available UI primitives.
/// `candidateRow` must be non-NULL, belong to `carriedItems` and contain `itemId`.
/// That range must fit its backing table; its weapon loads come from the live
/// save. Subtracts both loads of every weapon row without clamping and formats
/// signed decimal stock; non-consumables draw nothing. Borrows all inputs and
/// consumes the 32-byte text buffer synchronously. Row coordinates are pixels
/// relative to the panel's content origin.
static inline void _itemMenuDrawAvailableAttachmentQuantity(const UiList* list, const UiObject* object,
                                                            const InventoryItemRow*   candidateRow,
                                                            const InventoryItemRange* carriedItems, s32 itemId)
{
    enum { ITEM_MENU_ATTACHMENT_QUANTITY_RECESSED_COLOR_RGB = 0x102010 };
    u8          quantityText[32];
    TextDrawReq quantityRequest;
    s32         rowX;
    s32         rowY;
    s32         textColorRgb;
    s32         unloadedQuantity;

    rowX         = list->rowTextX.signedValue;
    rowY         = list->rowTextY.signedValue;
    textColorRgb = list->colorRgb;
    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
        unloadedQuantity           = candidateRow->qty - equipmentGetLoadedConsumableQuantity(carriedItems, itemId);
        quantityRequest.x          = object->panel.contentOriginX.unsignedValue + 132 + rowX;
        quantityRequest.y          = object->panel.contentOriginY.unsignedValue + (rowY - 3);
        quantityRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        quantityRequest.colorRgb   = textColorRgb;
        quantityRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        quantityRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
        quantityRequest.drawMode   = TEXT_DRAW_FILL_ONLY;
        textDrawString(&quantityRequest, textItoaSigned(quantityText, unloadedQuantity));
        uiDrawRecessedRect(&object->panel, rowX + 105, rowY - 8, 27, 7, ITEM_MENU_ATTACHMENT_QUANTITY_RECESSED_COLOR_RGB);
    }
}

void itemMenuDrawAttachmentCandidateRow(UiList* list, UiObject* object)
{
    enum {
        ITEM_MENU_ATTACHMENT_MARK_EQUIPPED_ONLY    = 1,
        ITEM_MENU_ATTACHMENT_MARK_INCLUDE_ATTACHED = 2,
        ITEM_MENU_ATTACHMENT_INFO_DESCRIPTOR       = 45,
        ITEM_MENU_ATTACHMENT_INFO_OPEN_DELAY_TICKS = 1
    };

    const InventoryItemRange* carriedItems;
    InventoryItemRow*         carriedRow;
    s32                       itemId;

    carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    carriedRow   = inventoryFindNthAttachmentCandidate(carriedItems, list->currentItemIndex, 0);
    if (carriedRow != NULL) {
        itemId = carriedRow->itemId;
        _itemMenuDrawAvailableAttachmentQuantity(list, object, carriedRow, carriedItems, itemId);

        if (carriedRow->attachSlot > INVENTORY_ATTACHMENT_NONE) {
            _gpDrawItemName(list, object, itemId, ITEM_MENU_ATTACHMENT_MARK_INCLUDE_ATTACHED);
        } else {
            _gpDrawItemName(list, object, itemId, ITEM_MENU_ATTACHMENT_MARK_EQUIPPED_ONLY);
        }

        if (((object->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE || object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && list->selectedItemIndex == list->currentItemIndex) {
            if (itemId == INVENTORY_ITEM_NONE) {
                uiSetPromptText(Gp_StrEmpty, 0, 0);
            } else {
                uiSetPromptText(itemGetText(itemId, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
            }
        }

        if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                UiList*           armorSlots;
                InventoryItemRow* rows;
                s32               rowIndex;
                s32               rowCount;

                // Replace the occupant of the selected one-based armor slot.
                armorSlots = &D_8010E8AC;
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                rows     = inventoryGetRangeTable(carriedItems);
                rowCount = carriedItems->rowCount;
                rows     = &rows[carriedItems->firstRow];
                for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                    if (rows[rowIndex].attachSlot == armorSlots->selectedItemIndex + 1) {
                        inventoryDetachItem(&rows[rowIndex]);
                        break;
                    }
                }
                carriedRow->attachSlot = armorSlots->selectedItemIndex + 1;
                object->result         = USER_INTERFACE_RESULT_DISMISS;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                uiSpawnObject(&D_8010EAB4[ITEM_MENU_ATTACHMENT_INFO_DESCRIPTOR], itemId | ITEM_MENU_INFO_RELOCATED_PREVIEW, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ATTACHMENT_INFO_OPEN_DELAY_TICKS, object);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
    } else {
        if (((object->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE || object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && list->selectedItemIndex == list->currentItemIndex) {
            uiSetPromptText(Gp_StrDetachArmorHelp, 0, 0);
        }
        {
            TextDrawReq textRequest;
            s32         textOriginY;

            textRequest.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.signedValue;
            textOriginY            = object->panel.contentOriginY.unsignedValue - 6;
            textRequest.y          = list->rowTextY.signedValue + textOriginY;
            textRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            textRequest.colorRgb   = list->colorRgb;
            textRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            textRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            textRequest.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&textRequest, Gp_StrRemoveArmor);
        }
        if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                s32 attachmentSlot;
                s32 rowIndex;
                s32 rowCount;

                attachmentSlot = D_8010E8AC.selectedItemIndex + 1;
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                carriedRow = inventoryGetRangeTable(carriedItems);
                rowCount   = carriedItems->rowCount;
                carriedRow = &carriedRow[carriedItems->firstRow];
                for (rowIndex = 0; rowIndex < rowCount; rowIndex++, carriedRow++) {
                    if (carriedRow->attachSlot == attachmentSlot) {
                        inventoryDetachItem(carriedRow);
                        break;
                    }
                }
                object->result = USER_INTERFACE_RESULT_DISMISS;
            }
        }
    }
}

/// Sizes the armor attachment picker for carried candidates and its detach row.
///
/// Counts rows, including items already attached elsewhere, rather than quantities.
/// Excludes empty rows, forbidden attachments and the equipped weapon; shows four
/// rows at a time. `list` must be writable and the live carried range in bounds.
/// Each row's itemId must index `Gp_ItemDescs`.
/// `unused` is ignored.
static void _itemMenuSizeAttachmentPicker(UiList* list, UiObject* unused)
{
    enum { ITEM_MENU_ATTACHMENT_PICKER_VISIBLE_ROWS = 4 };

    InventoryItemRow*         row;
    s32                       rowIndex;
    s32                       candidateCount;
    const InventoryItemRange* carriedItems;

    carriedItems   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    row            = inventoryGetRangeTable(carriedItems);
    candidateCount = 0;
    row            = &row[carriedItems->firstRow];
    for (rowIndex = 0; rowIndex < carriedItems->rowCount; rowIndex++, row++) {
        if ((Gp_ItemDescs[row->itemId].flags & ITEM_FLAG_NO_ATTACHMENT) || (row->itemId == INVENTORY_ITEM_NONE)) {
            continue;
        }
        if ((u8)(row->itemId + EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems) && _gpIsEquippedItem(row->itemId)) {
            continue;
        }
        candidateCount++;
    }
    // The trailing command detaches the item from the selected armor slot.
    list->itemCount                     = candidateCount + 1;
    list->visibleRowCount.unsignedValue = ITEM_MENU_ATTACHMENT_PICKER_VISIBLE_ROWS;
}

void itemMenuAttachmentItemPickerTask(Task* task)
{
    enum {
        ITEM_MENU_ATTACHMENT_PICKER_INITIALIZE         = 0,
        ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_HEIGHT     = 76,
        ITEM_MENU_ATTACHMENT_PICKER_SEPARATOR_Y        = 74,
        ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_PANEL      = 14,
        ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_KIND       = 3,
        ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_DELAY      = 16,
        ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_PROFILE    = 2,
        ITEM_MENU_ATTACHMENT_PICKER_CONFIRM_ON_DISMISS = 0
    };
    UiObject*         object;
    UiList*           candidateList;
    InventoryItemRow* candidate;
    s32               previewItemId;
    Task*             parentTask;

    object         = task->spawnArg2.pointer;
    candidateList  = &D_8010E8D4;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, Gp_StrSelectTitle);
    previewItemId = INVENTORY_ITEM_NONE;
    // Reserve the preview header and hide the armor-slot parent while choosing an item.
    if (task->state == ITEM_MENU_ATTACHMENT_PICKER_INITIALIZE) {
        _itemMenuSizeAttachmentPicker(candidateList, object);
        uiFitPanelToList(candidateList, &object->panel);
        candidateList->topInset                           += ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_HEIGHT;
        object->panel.bounds.unsignedRect.h               += ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_HEIGHT;
        candidateList->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        candidateList->selectedItemIndex                   = 0;
        candidateList->firstVisibleItemIndex.unsignedValue = 0;
        parentTask                                         = task->parent;
        uiStartPanelHiding(parentTask->spawnArg2.pointer, parentTask);
        uiSpawnObject(&D_8010EAB4[ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_PANEL], ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_KIND, previewItemId, ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_DELAY, object);
        task->state = task->state + 1;
    }
    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, object->panel.contentTop.signedValue + ITEM_MENU_ATTACHMENT_PICKER_SEPARATOR_Y);
    uiUpdateList(candidateList, &object->panel);
    candidate = inventoryFindNthAttachmentCandidate(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, candidateList->selectedItemIndex, 0);
    if (candidate != NULL) {
        previewItemId = candidate->itemId;
    }
    itemMenuUpdateSelectionPreview(candidateList, object, previewItemId, ITEM_MENU_ATTACHMENT_PICKER_PREVIEW_PROFILE);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_DISMISS;
        }
    }
    itemMenuApplyChildDialogResults(object, task);
    if (task->spawnArg1.value == ITEM_MENU_ATTACHMENT_PICKER_CONFIRM_ON_DISMISS) {
        if (object->result == USER_INTERFACE_RESULT_DISMISS) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

/// Returns the one-based P.E. preview file index for a packed catalogue id.
///
/// The caller supplies ids 0x300..0x4FF. Bits 4..5 select the element,
/// bits 2..3 the energy slot, and bits 0..1 the learned level. Higher bits
/// are ignored; level zero uses the level-one image. Energy slots 0..2
/// produce indices 1..36. Slot 3 is unchecked: it overlaps the next element's
/// indices, or produces 37..39 for the last element.
static inline s32 _itemMenuDecodeEnergyPreviewIndex(s32 packedAbilityId)
{
    enum {
        ITEM_MENU_PREVIEW_ELEMENT_MASK         = 0x30,
        ITEM_MENU_PREVIEW_ELEMENT_SHIFT        = 4,
        ITEM_MENU_PREVIEW_ENERGY_MASK          = 0x0C,
        ITEM_MENU_PREVIEW_ENERGY_SHIFT         = 2,
        ITEM_MENU_PREVIEW_LEVEL_MASK           = 0x03,
        ITEM_MENU_PREVIEW_ENERGIES_PER_ELEMENT = 3,
        ITEM_MENU_PREVIEW_LEVEL_UNLEARNED      = 0,
        ITEM_MENU_PREVIEW_LEVEL_FIRST          = 1
    };
    s32 level;
    s32 element;
    s32 energyIndex;

    level       = packedAbilityId & ITEM_MENU_PREVIEW_LEVEL_MASK;
    element     = (packedAbilityId & ITEM_MENU_PREVIEW_ELEMENT_MASK) >> ITEM_MENU_PREVIEW_ELEMENT_SHIFT;
    energyIndex = (packedAbilityId & ITEM_MENU_PREVIEW_ENERGY_MASK) >> ITEM_MENU_PREVIEW_ENERGY_SHIFT;
    if (level == ITEM_MENU_PREVIEW_LEVEL_UNLEARNED) {
        level = ITEM_MENU_PREVIEW_LEVEL_FIRST;
    }
    return (element * ITEM_MENU_PREVIEW_ENERGIES_PER_ELEMENT + energyIndex) * ATTACHMENT_AREA_LEVEL_COUNT + level;
}

void itemMenuEnqueuePreviewLoad(s32 itemId, s32 loadProfile)
{
    // Hundreds components of stage-zero category-2 display-resource file IDs.
    enum {
        ITEM_MENU_PREVIEW_FILE_WEAPON          = 1,
        ITEM_MENU_PREVIEW_FILE_OTHER           = 2,
        ITEM_MENU_PREVIEW_FILE_ITEM            = 3,
        ITEM_MENU_PREVIEW_FILE_CONSUMABLE      = 4,
        ITEM_MENU_PREVIEW_FILE_ARMOR           = 5,
        ITEM_MENU_PREVIEW_FILE_HIGH_ID         = 6,
        ITEM_MENU_PREVIEW_FILE_ENERGY          = 7,
        ITEM_MENU_PREVIEW_ARMOR_ITEM_FIRST     = 0x60,
        ITEM_MENU_PREVIEW_EQUIPMENT_ITEM_COUNT = 0x20,
        ITEM_MENU_PREVIEW_ITEM_ID_END          = 0x180,
        ITEM_MENU_PREVIEW_PROFILE_COUNT        = 3,
        ITEM_MENU_PREVIEW_REQUEST_ABSENT       = -1,
        ITEM_MENU_PREVIEW_REQUEST_SAVED        = 0,
        ITEM_MENU_PREVIEW_MENU_X_PAGE_OFFSET   = -8,
        ITEM_MENU_PREVIEW_MENU_Y_OFFSET        = -3,
        ITEM_MENU_PREVIEW_IMAGE_Y_OFFSET       = -2,
        ITEM_MENU_PREVIEW_DEBUG_DISABLED_MODE  = -1,
        ITEM_MENU_PREVIEW_DEBUG_ALLOWED_DEMO   = 0xC
    };

    s32               savedRequestState[ITEM_MENU_PREVIEW_PROFILE_COUNT];
    CdCmdEntry        savedRequests[ITEM_MENU_PREVIEW_PROFILE_COUNT];
    const CdCmdQueue* queue;
    const CdCmdEntry* entry;
    s32               fileIdHundreds;
    s32               fileIndex;
    s32               profileIndex;

    queue = &gCdCmdQueue;
    if (itemId == INVENTORY_ITEM_NONE) {
        return;
    }
    if (gDisplayState.debugMode == ITEM_MENU_PREVIEW_DEBUG_DISABLED_MODE) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ITEM_MENU_PREVIEW_DEBUG_ALLOWED_DEMO) {
            return;
        }
    }
    if (queue->scenePayloadAvailable == 1) {
        return;
    }

    if (itemId >= ITEM_TEXT_ENEMY_ID_FIRST) {
        fileIdHundreds = ITEM_MENU_PREVIEW_FILE_HIGH_ID;
        fileIndex      = itemId;
    } else if (itemId >= ITEM_TEXT_PACKED_ID_FIRST) {
        fileIdHundreds = ITEM_MENU_PREVIEW_FILE_ENERGY;
        fileIndex      = _itemMenuDecodeEnergyPreviewIndex(itemId);
    } else if ((u32)itemId >= ITEM_MENU_PREVIEW_ITEM_ID_END) {
        return;
    } else if ((u32)(itemId - 1) < (ITEM_MENU_PREVIEW_ARMOR_ITEM_FIRST - 1)) {
        fileIdHundreds = ITEM_MENU_PREVIEW_FILE_ITEM;
        fileIndex      = itemId;
    } else if ((u32)(itemId - ITEM_MENU_PREVIEW_ARMOR_ITEM_FIRST) < ITEM_MENU_PREVIEW_EQUIPMENT_ITEM_COUNT) {
        fileIdHundreds = ITEM_MENU_PREVIEW_FILE_ARMOR;
        fileIndex      = itemId - (ITEM_MENU_PREVIEW_ARMOR_ITEM_FIRST - 1);
    } else if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_MENU_PREVIEW_EQUIPMENT_ITEM_COUNT) {
        fileIdHundreds = ITEM_MENU_PREVIEW_FILE_WEAPON;
        fileIndex      = itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
    } else if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
        fileIdHundreds = ITEM_MENU_PREVIEW_FILE_CONSUMABLE;
        fileIndex      = itemId + (0x100 - INVENTORY_CONSUMABLE_ITEM_FIRST + 1);
    } else {
        fileIdHundreds = ITEM_MENU_PREVIEW_FILE_OTHER;
        fileIndex      = itemId;
    }

    if (loadProfile & 0xFF) {
        D_80114D88 = 1;
    }

    savedRequestState[CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW] = ITEM_MENU_PREVIEW_REQUEST_ABSENT;
    savedRequestState[CD_COMMAND_DISPLAY_LOAD_PREVIEW]           = ITEM_MENU_PREVIEW_REQUEST_ABSENT;
    savedRequestState[CD_COMMAND_DISPLAY_LOAD_MENU]              = ITEM_MENU_PREVIEW_REQUEST_ABSENT;
    cdCmdResetEntryIterator();

    // Keep the latest request with each profile's load policy and image offsets.
    // The iterator includes the head; these tests deliberately do not check opcodes.
    while ((entry = cdCmdNextQueuedEntry()) != NULL) {
        if (entry->args.file.loadMode == CD_COMMAND_LOAD_RELOCATE_IMAGES && entry->args.file.imageXPageOffset == ITEM_MENU_PREVIEW_MENU_X_PAGE_OFFSET && entry->args.file.imageYOffset == ITEM_MENU_PREVIEW_MENU_Y_OFFSET) {
            savedRequests[CD_COMMAND_DISPLAY_LOAD_MENU]     = *entry;
            savedRequestState[CD_COMMAND_DISPLAY_LOAD_MENU] = ITEM_MENU_PREVIEW_REQUEST_SAVED;
        } else if (entry->args.file.loadMode == CD_COMMAND_LOAD_DEFAULT && entry->args.file.imageXPageOffset == 0 && entry->args.file.imageYOffset == ITEM_MENU_PREVIEW_IMAGE_Y_OFFSET) {
            savedRequests[CD_COMMAND_DISPLAY_LOAD_PREVIEW]     = *entry;
            savedRequestState[CD_COMMAND_DISPLAY_LOAD_PREVIEW] = ITEM_MENU_PREVIEW_REQUEST_SAVED;
        } else if (entry->args.file.loadMode == CD_COMMAND_LOAD_RELOCATE_IMAGES && entry->args.file.imageXPageOffset == 0 && entry->args.file.imageYOffset == ITEM_MENU_PREVIEW_IMAGE_Y_OFFSET) {
            savedRequests[CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW]     = *entry;
            savedRequestState[CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW] = ITEM_MENU_PREVIEW_REQUEST_SAVED;
        }
    }
    // Replace this profile, discard the tail, then restore other profiles in order.
    savedRequestState[loadProfile & 0xFF] = ITEM_MENU_PREVIEW_REQUEST_ABSENT;
    cdCmdDropQueuedTail();
    for (profileIndex = 0; profileIndex < ARRAY_SIZE(savedRequests); profileIndex++) {
        if (savedRequestState[profileIndex] != ITEM_MENU_PREVIEW_REQUEST_ABSENT) {
            _cdCmdEnqueueEntry(&savedRequests[profileIndex]);
        }
    }

    cdCmdEnqueueDisplayResource(fileIdHundreds, fileIndex & 0xFF, loadProfile & 0xFF);
}
