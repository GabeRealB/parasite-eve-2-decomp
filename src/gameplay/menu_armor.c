#include "item_menu.h"

#include "types.h"

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

static void Gp_CountEquippableRows(UiList* arg0, UiObject* arg1);

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

/// Shows `item`'s name in the holder (the empty-slot text for item 0) and
/// makes it the preview in slot 0.
#define GP_SHOW_ITEM_IN_HOLDER(item)                                                    \
    do {                                                                                \
        if ((item) == 0) {                                                              \
            uiSetPromptText(Gp_StrEmpty, 0, 0);                                         \
        } else {                                                                        \
            uiSetPromptText(itemGetText((item), ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0); \
        }                                                                               \
        Gp_SetPreviewItem((item), 0);                                                   \
    } while (0)

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

void Gp_DrawRemoveArmorRow(UiList* prompt, UiObject* obj)
{
    InventoryItemRange* scan;
    InventoryItemRow*   rec;
    s32                 item;

    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    rec  = inventoryFindNthAttachmentCandidate(scan, prompt->currentItemIndex, 0);
    if (rec != NULL) {
        item = rec->itemId;
        {
            u8          buf[0x20];
            TextDrawReq req;
            s32         x;
            s32         y;
            s32         color;
            s32         qty;

            x     = prompt->rowTextX.signedValue;
            y     = prompt->rowTextY.signedValue;
            color = prompt->colorRgb;
            if ((u32)(item - 0xA0) < 0x20U) {
                qty            = rec->qty - Gp_CountEquippedRelated(scan, item);
                req.x          = obj->panel.contentOriginX.unsignedValue + 0x84 + x;
                req.y          = obj->panel.contentOriginY.unsignedValue + (y - 3);
                req.otIndex    = obj->panel.otIndex.signedValue + 1;
                req.colorRgb   = color;
                req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                req.alignment  = TEXT_ALIGNMENT_RIGHT;
                req.drawMode   = TEXT_DRAW_FILL_ONLY;
                textDrawString(&req, textItoaSigned(buf, qty));
                uiDrawRecessedRect(&obj->panel, x + 0x69, y - 8, 0x1B, 7, 0x102010);
            }
        }

        if (rec->attachSlot > INVENTORY_ATTACHMENT_NONE) {
            _gpDrawItemName(prompt, obj, item, 2);
        } else {
            _gpDrawItemName(prompt, obj, item, 1);
        }

        if (((obj->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE || obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && prompt->selectedItemIndex == prompt->currentItemIndex) {
            if (item == 0) {
                uiSetPromptText(Gp_StrEmpty, 0, 0);
            } else {
                uiSetPromptText(itemGetText(item, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
            }
        }

        if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                UiList*           menu;
                InventoryItemRow* table;
                s32               i;
                s32               count;

                menu = &D_8010E8AC;
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                table = inventoryGetRangeTable(scan);
                count = scan->rowCount;
                table = &table[scan->firstRow];
                for (i = 0; i < count; i++) {
                    if (table[i].attachSlot == menu->selectedItemIndex + 1) {
                        inventoryDetachItem(&table[i]);
                        break;
                    }
                }
                rec->attachSlot = menu->selectedItemIndex + 1;
                obj->result     = USER_INTERFACE_RESULT_DISMISS;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                uiSpawnObject(&D_8010EFA0, item | 0x10000, 1, 1, obj);
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
    } else {
        if (((obj->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE || obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && prompt->selectedItemIndex == prompt->currentItemIndex) {
            uiSetPromptText(Gp_StrDetachArmorHelp, 0, 0);
        }
        {
            TextDrawReq req;
            s32         off;

            req.x          = obj->panel.contentOriginX.unsignedValue + prompt->rowTextX.signedValue;
            off            = obj->panel.contentOriginY.unsignedValue - 6;
            req.y          = prompt->rowTextY.signedValue + off;
            req.otIndex    = obj->panel.otIndex.signedValue + 1;
            req.colorRgb   = prompt->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Gp_StrRemoveArmor);
        }
        if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                s32 slot;
                s32 i;
                s32 count;

                slot = D_8010E8AC.selectedItemIndex + 1;
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                rec   = inventoryGetRangeTable(scan);
                count = scan->rowCount;
                rec   = &rec[scan->firstRow];
                for (i = 0; i < count; i++, rec++) {
                    if (rec->attachSlot == slot) {
                        inventoryDetachItem(rec);
                        break;
                    }
                }
                obj->result = USER_INTERFACE_RESULT_DISMISS;
            }
        }
    }
}

static void Gp_CountEquippableRows(UiList* arg0, UiObject* arg1)
{
    InventoryItemRow*   table;
    s32                 i;
    s32                 count;
    InventoryItemRange* scan;

    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    table = inventoryGetRangeTable(scan);
    count = 0;
    table = &table[scan->firstRow];
    for (i = 0; i < scan->rowCount; i++, table++) {
        if ((Gp_ItemDescs[table->itemId].flags & ITEM_FLAG_NO_ATTACHMENT) || (table->itemId == INVENTORY_ITEM_NONE)) {
            continue;
        }
        if ((u8)(table->itemId + 0x80) < 0x20 && _gpIsEquippedItem(table->itemId)) {
            continue;
        }
        count++;
    }
    arg0->itemCount                     = count + 1;
    arg0->visibleRowCount.unsignedValue = 4;
}

void Gp_EquipSelectMenuTask(Task* arg0)
{
    UiObject*         obj;
    UiList*           menu;
    InventoryItemRow* rec;
    s32               val;
    Task*             parent;

    obj         = arg0->spawnArg2.pointer;
    menu        = &D_8010E8D4;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrSelectTitle);
    val = 0;
    if (arg0->state == 0) {
        Gp_CountEquippableRows(menu, obj);
        uiFitPanelToList(menu, &(obj)->panel);
        menu->topInset                           += 0x4C;
        obj->panel.bounds.unsignedRect.h         += 0x4C;
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        parent                                    = arg0->parent;
        uiStartPanelHiding(parent->spawnArg2.pointer, parent);
        uiSpawnObject(&D_8010EC3C, 3, val, 0x10, obj);
        arg0->state = arg0->state + 1;
    }
    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 0x4A);
    uiUpdateList(menu, &obj->panel);
    rec = inventoryFindNthAttachmentCandidate(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, menu->selectedItemIndex, 0);
    if (rec != NULL) {
        val = rec->itemId;
    }
    Gp_ItemRowSelect(menu, obj, val, 2);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_DISMISS;
        }
    }
    func_800CF148(obj, arg0);
    if (arg0->spawnArg1.value == 0) {
        if (obj->result == USER_INTERFACE_RESULT_DISMISS) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_EnqueueItemPreviewCd(s32 arg0, s32 arg1)
{
    s32         flags[3];
    CdCmdEntry  saved[3];
    CdCmdQueue* queue;
    CdCmdEntry* entry;
    s32         type;
    s32         index;
    s32         nibble;
    s32         hi;
    s32         lo;
    s32         i;

    queue = &gCdCmdQueue;
    if (arg0 == 0) {
        return;
    }
    if (gDisplayState.debugMode == -1) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0xC) {
            return;
        }
    }
    if (queue->scenePayloadAvailable == 1) {
        return;
    }

    if (arg0 >= 0x500) {
        type  = 6;
        index = arg0;
    } else if (arg0 >= 0x300) {
        type   = 7;
        nibble = arg0 & 3;
        hi     = (arg0 & 0x30) >> 4;
        lo     = (arg0 & 0xC) >> 2;
        if (nibble == 0) {
            nibble = 1;
        }
        index = (hi * 3 + lo) * 3 + nibble;
    } else if ((u32)arg0 >= 0x180U) {
        return;
    } else if ((u32)(arg0 - 1) < 0x5FU) {
        type  = 3;
        index = arg0;
    } else if ((u32)(arg0 - 0x60) < 0x20U) {
        type  = 5;
        index = arg0 - 0x5F;
    } else if ((u32)(arg0 - 0x80) < 0x20U) {
        type  = 1;
        index = arg0 - 0x7F;
    } else if ((u32)(arg0 - 0xA0) < 0x20U) {
        type  = 4;
        index = arg0 + 0x61;
    } else {
        type  = 2;
        index = arg0;
    }

    if (arg1 & 0xFF) {
        D_80114D88 = 1;
    }

    flags[2] = -1;
    flags[1] = -1;
    flags[0] = -1;
    cdCmdResetEntryIterator();

    // Preserve requests for the other preview destinations before dropping work.
    while ((entry = cdCmdNextQueuedEntry()) != NULL) {
        if (entry->args.file.loadMode == CD_COMMAND_LOAD_RELOCATE_IMAGES && entry->args.file.imageXPageOffset == -8 && entry->args.file.imageYOffset == -3) {
            saved[0] = *entry;
            flags[0] = 0;
        } else if (entry->args.file.loadMode == CD_COMMAND_LOAD_DEFAULT && entry->args.file.imageXPageOffset == 0 && entry->args.file.imageYOffset == -2) {
            saved[1] = *entry;
            flags[1] = 0;
        } else if (entry->args.file.loadMode == CD_COMMAND_LOAD_RELOCATE_IMAGES && entry->args.file.imageXPageOffset == 0 && entry->args.file.imageYOffset == -2) {
            saved[2] = *entry;
            flags[2] = 0;
        }
    }
    flags[arg1 & 0xFF] = -1;
    cdCmdDropQueuedTail();
    for (i = 0; i < ARRAY_SIZE(saved); i++) {
        if (flags[i] != -1) {
            _cdCmdEnqueueEntry(&saved[i]);
        }
    }

    cdCmdEnqueueDisplayResource(type, index & 0xFF, arg1 & 0xFF);
}

/// Sets bit 0x100 in `flags`, which makes `func_800C7AE8` skip drawing the
/// item preview, while the CD queue is still busy loading it.
#define GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags) \
    do {                                     \
        if (cdCmdIsIdle() == 0) {            \
            (flags) |= 0x100;                \
        }                                    \
    } while (0)
