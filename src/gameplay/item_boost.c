#include "gameplay/items.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_menu.h"
#include "items.h"
#include "scene_runtime.h"

#include "main/areas.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// Collection-bit packing and the bit that starts the Ice Bag's age timer.
enum {
    INVENTORY_COLLECTION_ID_MASK       = 0x7F,
    INVENTORY_COLLECTION_BITS_PER_WORD = 32,
    INVENTORY_COLLECTED_ICE_BAG_BIT    = INVENTORY_COLLECTION_ID_ICE_BAG & INVENTORY_COLLECTION_ID_MASK
};

/// Number of saved rows selected when resetting the carried range.
enum { INVENTORY_INITIAL_CARRIED_ROWS = 20 };

/// Layout of the packed saved object states addressed by an object id.
enum {
    AREA_OBJECT_STATE_BITS       = 2,
    AREA_OBJECT_STATE_INDEX_MASK = 0xF,
    AREA_OBJECT_STATE_WORD_SHIFT = 4
};

/// Catalogue ids tested for armour attachment effects in this translation unit.
enum {
    INVENTORY_ITEM_LIPSTICK       = 0x0B,
    INVENTORY_ITEM_MD_PLAYER      = 0x0E,
    INVENTORY_ITEM_SKULL_CRYSTAL  = 0x36,
    INVENTORY_ITEM_MEDICINE_WHEEL = 0x37,
    INVENTORY_ITEM_HOLY_WATER     = 0x38,
    INVENTORY_ITEM_OFUDA          = 0x39,
    INVENTORY_ITEM_HUNTER_GOGGLES = 0x3F,
    INVENTORY_ITEM_GPS            = 0x40
};

extern u8 Gp_StrMore[];

extern u8 Gp_StrAttachAvail[];

extern UiObjectDesc Gp_BoostPanelDesc;

static inline InventoryItemRow* _gpScanTable(const InventoryItemRange* scan);

static inline void _gpClearEquipSlot(s32 item);

u8           Gp_StrMore[]        = "More ";
u8           Gp_StrAttachAvail[] = "attachments available.";
UiObjectDesc Gp_BoostPanelDesc   = { USER_INTERFACE_PANEL_TITLE_STYLE, { 10, 20, 30, 40 }, 12, 0, TASK_BODY_NONE, 192, Gp_TickBoostPanel, 0 };

/* Count item `id` in saved rows 0..254 through a cleared range. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, inventoryGetItemQuantity(&(scan), (id)))

/* Item names and descriptions shared by the inventory tables. */

/* Gives `scan` one `weapon` and loads it with `ammo`. */
#define GP_GIVE_LOADED(scan, weapon, ammo)           \
    do {                                             \
        inventoryGiveItem(scan, weapon, 1);          \
        Gp_EquipRelatedItem(scan, weapon, ammo, -1); \
    } while (0)

/* Clears the carried inventory, equips the starting armour, restores HP/MP,
 * and gives the initial supplies and their attachment slots. */
#define _gpInitStartingItems(scan, cfg)                      \
    do {                                                     \
        inventoryClearItems(scan);                           \
        inventoryGiveItem(scan, 0x60, 1);                    \
        Gp_EquipMod(0x60);                                   \
        (cfg)->hp = (cfg)->hpMax;                            \
        (cfg)->mp = (cfg)->mpMax;                            \
        inventoryGiveItem(scan, 0x92, 1);                    \
        inventoryGiveItem(scan, 0x40, 1)->attachSlot    = 1; \
        inventoryGiveItem(scan, 0xA0, 0x64)->attachSlot = 2; \
    } while (0)

/* Item table a scan window lies in. */

static inline InventoryItemRow* _gpScanTable(const InventoryItemRange* scan)
{
    InventoryItemRow* table;

    switch (scan->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            table = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            table = Gp_ItemTable1;
            break;
        default:
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    return table;
}
static inline void _gpClearEquipSlot(s32 item)
{
    EquipmentWeaponLoad* slot;
    s32                  found = 0;
    s32                  i;

    if ((u32)(item - 0x80) >= 0x20) {
        return;
    }

    slot = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[item - EQUIPMENT_WEAPON_ITEM_FIRST];
    for (i = 0; i < EQUIPMENT_WEAPON_SUPPLY_COUNT; i++) {
        if (item == Gp_ItemMaps[i].weaponItemId) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (Gp_ItemMaps[i].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_PRIMARY)) {
        slot->primaryItemId = INVENTORY_ITEM_NONE;
        slot->primaryQty    = 0;
    }

    if ((found == 0) || (Gp_ItemMaps[i].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_SECONDARY)) {
        if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
            slot->secondaryItemId = INVENTORY_ITEM_NONE;
        }
        slot->secondaryQty = 0;
    }
}
/// Marks a writable inventory row empty and resets its quantity and attachment.
///
/// Weapon loads and player equipment selections are left intact.
static inline void _inventoryClearItemRow(InventoryItemRow* row)
{
    row->itemId     = INVENTORY_ITEM_NONE;
    row->qty        = 0;
    row->attachSlot = INVENTORY_ATTACHMENT_NONE;
}

/// Consumes units from the first row with `itemId` in a writable range.
///
/// A negative quantity removes that row's whole quantity; excess consumption
/// clamps to zero. A zero remainder clears the id, quantity and attachment.
/// The range must fit its table; a missing item leaves every row unchanged.
static inline void _inventoryConsumeFirstStack(InventoryItemRange* range, s32 itemId, s32 quantity)
{
    InventoryItemRow* table;
    s32               stackQuantity;
    s32               rowIndex;
    s32               remainingQuantity;

    table         = _gpScanTable(range);
    stackQuantity = 0;
    for (rowIndex = range->firstRow; rowIndex < range->firstRow + range->rowCount; rowIndex++) {
        if (table[rowIndex].itemId == itemId) {
            stackQuantity = table[rowIndex].qty;
            break;
        }
    }
    if (rowIndex != range->firstRow + range->rowCount) {
        if (quantity < 0) {
            quantity = stackQuantity;
        }
        remainingQuantity = stackQuantity - quantity;
        if (remainingQuantity < 0) {
            remainingQuantity = 0;
        }
        if (remainingQuantity == 0) {
            _inventoryClearItemRow(&table[rowIndex]);
        } else {
            table[rowIndex].qty = remainingQuantity;
        }
    }
}
/// Clears a separate item row or consumes its first matching quantity stack.
///
/// Ids below `INVENTORY_CONSUMABLE_ITEM_FIRST` clear `row` regardless of
/// `quantity`, and permit NULL `range`. Other ids require a valid writable
/// range and consume its first matching row, which need not be `row`.
static __inline void _inventoryRemoveItemRow(InventoryItemRange* range, InventoryItemRow* row, s32 quantity)
{
    s32 itemId;

    itemId = row->itemId;
    if (itemId < INVENTORY_CONSUMABLE_ITEM_FIRST) {
        _inventoryClearItemRow(row);
    } else {
        _inventoryConsumeFirstStack(range, itemId, quantity);
    }
}
/// Returns an armour's base plus saved bonus attachment slots, capped at ten.
///
/// Armour item ids 0x60..0x7F index the live save; other ids return zero.
static inline s32 _equipmentGetArmorAttachmentSlotCount(s32 armorItemId)
{
    enum { EQUIPMENT_ARMOR_ITEM_FIRST = 0x60,
           EQUIPMENT_ARMOR_ITEM_COUNT = 0x20 };
    s32               slotCount;
    s32               armorIndex;
    const ArmorStats* armorStats;

    armorIndex = armorItemId - EQUIPMENT_ARMOR_ITEM_FIRST;
    slotCount  = 0;
    if ((u32)armorIndex < EQUIPMENT_ARMOR_ITEM_COUNT) {
        armorStats = &Gp_ModStatAttrs[armorItemId - EQUIPMENT_ARMOR_ITEM_FIRST];
        slotCount  = armorStats->baseAttachmentSlots;
        slotCount += gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus[armorIndex];
        if (slotCount > ARMOR_ATTACHMENT_SLOT_MAX) {
            slotCount = ARMOR_ATTACHMENT_SLOT_MAX;
        }
    }
    return slotCount;
}
/// Draws an encoded prefix followed by an item's name on one panel text line.
///
/// X/Y are content pixels under `textDrawUiLine`'s contract. The name starts
/// four pixels beyond the prefix's measured width and uses a fixed packed RGB
/// colour. `prefixColorRgb` is packed RGB; `drawMode` narrows to a signed byte
/// selecting `TEXT_DRAW_*`. Text and object are borrowed without modification.
static inline void _itemMenuDrawPrefixedItemName(const UiObject* object, s32 x, s32 y, const u8* prefixText, s32 itemId, u32 prefixColorRgb, s32 drawMode)
{
    enum { ITEM_MENU_ITEM_NAME_COLOR_RGB = 0x037A78 };
    s32 itemNameOffsetX;

    textDrawUiLine(object, x, y, prefixText, prefixColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    itemNameOffsetX = textMeasureLineWidth(prefixText) + 4;
    textDrawUiLine(object, x + itemNameOffsetX, y, itemGetText(itemId, ITEM_TEXT_NAME, 0), ITEM_MENU_ITEM_NAME_COLOR_RGB, drawMode, TEXT_ALIGNMENT_LEFT);
}
/// Returns whether a carried row of `itemId` occupies a positive armour slot.
///
/// Searches the carried range's selected table. Quantity is not tested; the
/// equipped-armour marker and unattached rows do not qualify.
static __inline__ s32 _inventoryHasAttachedItem(s32 itemId)
{
    const InventoryItemRange* range;
    const InventoryItemRow*   row;
    s32                       rowIndex;
    s32                       hasAttachedItem;
    s32                       rowCount;

    range           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    hasAttachedItem = 0;
    switch (range->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            row = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            row = Gp_ItemTable1;
            break;
        default:
            row = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    rowIndex = 0;
    row     += range->firstRow;
    rowCount = range->rowCount;
    for (; rowIndex < rowCount; rowIndex++) {
        if (row->attachSlot > INVENTORY_ATTACHMENT_NONE) {
            if (row->itemId == itemId) {
                hasAttachedItem = 1;
                break;
            }
        }
        row++;
    }
    return hasAttachedItem;
}
/// Clears each row's item, attachment and quantity in a writable range.
///
/// The descriptor is unchanged and must select rows within its backing table.
/// Weapon loads and player equipment selections are left intact.
static inline void _inventoryClearItems(const InventoryItemRange* range)
{
    InventoryItemRow* table;
    s32               clearedRowCount;
    s32               tableRowIndex;

    table = _gpScanTable(range);
    for (clearedRowCount = 0, tableRowIndex = range->firstRow; clearedRowCount < range->rowCount; clearedRowCount++, tableRowIndex++) {
        table[tableRowIndex].itemId     = INVENTORY_ITEM_NONE;
        table[tableRowIndex].attachSlot = INVENTORY_ATTACHMENT_NONE;
        table[tableRowIndex].qty        = 0;
    }
}
/// Recomputes maximum HP from the mode, permanent bonus and equipped armour.
///
/// The save mode must index the base-stat table and armour must be 0..32.
/// Each addend narrows through the unsigned 16-bit accumulator before the
/// signed maximum is capped at 250. Current HP is reduced only if above it.
static inline void _equipmentRecalculateMaxHp(void)
{
    PlayerStatus*              status;
    const McSaveData*          save;
    const PlayerModeBaseStats* modeStats;
    u16                        maximumHp;

    status        = &gPlayerStatus;
    modeStats     = Gp_StatRows;
    save          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    maximumHp     = modeStats[save->state.gameMode].baseHp.hp;
    status->hpMax = maximumHp;
    maximumHp    += save->state.hpBonus;
    status->hpMax = maximumHp;
    if (status->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
        maximumHp    += Gp_ModStatAttrs[status->armor - 1].hpBonus;
        status->hpMax = maximumHp;
    }
    if (status->hpMax >= PLAYER_STATUS_STAT_MAX + 1) {
        status->hpMax = PLAYER_STATUS_STAT_MAX;
    }
    if (status->hp > status->hpMax) {
        status->hp = status->hpMax;
    }
}
/// Sets the live carried range to saved rows starting at zero.
///
/// The row count is stored as a byte; callers supply 0..255. The range's
/// unproven fourth byte and the rows themselves are left intact.
static inline void _inventorySetCarriedRange(s32 rowCount)
{
    McSaveData* save;

    save                              = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.carriedItems.firstRow = 0;
    save->state.carriedItems.rowCount = rowCount;
    save->state.carriedItems.tableId  = INVENTORY_ITEM_TABLE_SAVED;
}
/// Seeds listed object slots from the low two bits of each placement's state.
///
/// NULL rooms or place lists are skipped. Non-NULL room and placement tables
/// require their respective end markers. Every listed index must fit the
/// writable state words; slots absent from the lists keep their saved values.
static inline void _areaSeedObjectStates(const AreaObjectRoom* rooms, u32* objectStates)
{
    const AreaObjectPlace* place;
    u32*                   stateWord;
    u32                    stateMask;

    if (rooms == NULL) {
        return;
    }
    place = rooms->places.list;
    if (rooms->places.sentinel == AREA_OBJECT_ROOM_END) {
        return;
    }
    do {
        if (place != NULL) {
            for (; place->flagIndex != AREA_OBJECT_PLACE_END; place++) {
                stateMask   = AREA_OBJECT_PLACE_STATE_MASK << ((place->flagIndex & AREA_OBJECT_STATE_INDEX_MASK) * AREA_OBJECT_STATE_BITS);
                stateWord   = &objectStates[place->flagIndex >> AREA_OBJECT_STATE_WORD_SHIFT];
                *stateWord &= ~stateMask;
                stateMask   = (place->state & AREA_OBJECT_PLACE_STATE_MASK) << ((place->flagIndex & AREA_OBJECT_STATE_INDEX_MASK) * AREA_OBJECT_STATE_BITS);
                *stateWord |= stateMask;
            }
        }
        rooms++;
        place = rooms->places.list;
    } while (rooms->places.sentinel != AREA_OBJECT_ROOM_END);
}
void Gp_UiBoostAttach(UiObject* arg0, Task* arg1)
{
    s32 item;
    s32 width;
    s32 other;
    s32 saved;
    s32 x;
    s32 y;
    s32 color;
    s32 row;

    item = gPlayerStatus.armor + 0x5F;
    if (arg1->state == 0) {
        arg1->status = 0xFF;
        if (_equipmentGetArmorAttachmentSlotCount(item) < ARMOR_ATTACHMENT_SLOT_MAX) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus[item - 0x60]++;
        } else {
            arg1->status = 0x1A;
        }
        if (arg1->status == 0xFF) {
            width = textMeasureLineWidth(itemGetText(item, ITEM_TEXT_NAME, 0)) + textMeasureLineWidth(Gp_StrMore) + 4;
            other = textMeasureLineWidth(Gp_StrAttachAvail);
            if (width < other) {
                width = other;
            }
            uiSetPanelContentSize(&(arg0)->panel, width + 5, uiGetTextRowsHeight(2) + 1);
            (&(arg0)->panel)->bounds.rect.x = (-(&(arg0)->panel)->bounds.rect.w) >> 1;
            (&(arg0)->panel)->bounds.rect.y = ((-(&(arg0)->panel)->bounds.rect.h) >> 1) - 0x14;
            _inventoryRemoveItemRow(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, Gp_SelItemRec, 1);
            arg1->killCountdown = 0xBC;
            arg1->state++;
        }
    }
    if (arg1->status != 0xFF) {
        saved                 = arg1->spawnArg1.value;
        arg1->spawnArg1.value = arg1->status;
        itemMenuNoticeTask(arg1);
        arg1->spawnArg1.value = saved;
        return;
    }

    x = arg0->panel.contentLeft.signedValue + 2;
    y = arg0->panel.contentTop.signedValue;
    uiDrawPanelLabel(&(arg0)->panel, Gp_StrNotice2);
    color = 0x606060;
    row   = y + 0xF;
    _itemMenuDrawPrefixedItemName(arg0, x, row, Gp_StrMore, item, color, TEXT_DRAW_OUTLINED);
    row += 0xF;
    textDrawUiLine(arg0, x, row, Gp_StrAttachAvail, color, 1, TEXT_ALIGNMENT_LEFT);

    if (arg0->panel.control.word == 1) {
        arg1->killCountdown--;
        if ((arg1->killCountdown <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->result        = USER_INTERFACE_RESULT_DISMISS;
            arg1->killCountdown = 0x7FFF;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            arg0->result        = USER_INTERFACE_RESULT_CANCEL;
            arg1->killCountdown = 0x7FFF;
        }
    }
}

void Gp_UiBoostMp(UiObject* arg0, Task* arg1)
{
    PlayerStatus* cfg;
    McSaveData*   save;
    s32           saved;

    if (arg1->state == 0) {
        cfg            = &gPlayerStatus;
        Gp_HpMpWork.hp = cfg->hp;
        save           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        Gp_HpMpWork.mp = cfg->mp;
        if (save->state.mpBonus < 0xFA) {
            save->state.mpBonus = save->state.mpBonus + 1;
        }
        Gp_RecalcMaxMp();
        cfg->mp = cfg->mpMax;
        _inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
        uiSpawnObject(&Gp_BoostPanelDesc, 0, 0, 1, arg0);
    }
    saved                 = arg1->spawnArg1.value;
    arg1->spawnArg1.value = 0x1D;
    itemMenuNoticeTask(arg1);
    arg1->spawnArg1.value = saved;
}

void Gp_UiBoostHp(UiObject* arg0, Task* arg1)
{
    PlayerStatus* cfg;
    McSaveData*   save;
    s32           saved;
    s32           hp;
    u16           val;

    if (arg1->state == 0) {
        cfg            = &gPlayerStatus;
        hp             = cfg->hp;
        Gp_HpMpWork.hp = hp;
        save           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        Gp_HpMpWork.mp = cfg->mp;
        if (save->state.hpBonus < 0xFA) {
            save->state.hpBonus = save->state.hpBonus + 5;
        }
        val        = Gp_StatRows[save->state.gameMode].baseHp.hp;
        cfg->hpMax = val;
        val       += save->state.hpBonus;
        cfg->hpMax = val;
        if (cfg->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
            val       += Gp_ModStatAttrs[cfg->armor - 1].hpBonus;
            cfg->hpMax = val;
        }
        if (cfg->hpMax >= PLAYER_STATUS_STAT_MAX + 1) {
            cfg->hpMax = PLAYER_STATUS_STAT_MAX;
        }
        if (cfg->hpMax < hp) {
            cfg->hp = cfg->hpMax;
        }
        cfg->hp = cfg->hpMax;
        _inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
        uiSpawnObject(&Gp_BoostPanelDesc, 0, 0, 1, arg0);
    }
    saved                 = arg1->spawnArg1.value;
    arg1->spawnArg1.value = 0x1C;
    itemMenuNoticeTask(arg1);
    arg1->spawnArg1.value = saved;
}

s32 equipmentHasEffect(s32 effectSelector)
{
    const PlayerStatus* status;
    const ArmorStats*   armorStats;
    s32                 armorFeatures;
    s32                 effectActive;
    s32                 bodyProtected;
    s32                 mindProtected;

    effectActive  = 0;
    armorFeatures = 0;
    bodyProtected = 0;
    mindProtected = 0;
    status        = &gPlayerStatus;
    if (status->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
        armorStats    = &Gp_ModStatAttrs[status->armor - 1];
        armorFeatures = armorStats->features;
    }
    // Metabolism and the two wards protect different groups of status effects.
    if ((Gp_StateC08.metabolismTicks > 0) || (Gp_StateC08.bodyWard != 0)) {
        bodyProtected = 1;
    }
    if ((Gp_StateC08.metabolismTicks > 0) || (Gp_StateC08.mindWard != 0)) {
        mindProtected = 1;
    }

    switch (effectSelector) {
        case EQUIPMENT_EFFECT_RESIST_DARKNESS:
            if (_inventoryHasAttachedItem(INVENTORY_ITEM_HUNTER_GOGGLES) || bodyProtected) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_RESIST_PARALYSIS:
            if (bodyProtected || (armorFeatures & ARMOR_FEATURE_RESIST_PARALYSIS)) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_RESIST_POISON:
            if (bodyProtected || (armorFeatures & ARMOR_FEATURE_RESIST_POISON)) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_RESIST_SILENCE:
            effectActive = _inventoryHasAttachedItem(INVENTORY_ITEM_LIPSTICK);
            if (mindProtected || (armorFeatures & ARMOR_FEATURE_RESIST_SILENCE)) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_RESIST_TIMED_STATUS_20:
            break;
        case EQUIPMENT_EFFECT_RESIST_CONFUSION:
            effectActive = _inventoryHasAttachedItem(INVENTORY_ITEM_MD_PLAYER);
            if (mindProtected || (armorFeatures & ARMOR_FEATURE_RESIST_CONFUSION)) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_RESIST_BERSERKER:
            if (mindProtected || _inventoryHasAttachedItem(INVENTORY_ITEM_MD_PLAYER)) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_RESIST_IMPACT:
            if (armorFeatures & ARMOR_FEATURE_RESIST_IMPACT) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_ARMOR_MOTION_DETECTOR:
            if (armorFeatures & ARMOR_FEATURE_MOTION_DETECTOR) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_MP_GENERATION:
            if (armorFeatures & ARMOR_FEATURE_MP_GENERATION) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_HP_RECOVERY:
            if (armorFeatures & ARMOR_FEATURE_HP_RECOVERY) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_QUICK_FIRE:
            if (armorFeatures & ARMOR_FEATURE_QUICK_FIRE) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_MEDICAL_INSPECTION:
            if (armorFeatures & ARMOR_FEATURE_MEDICAL_INSPECTION) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_MP_RECOVERY:
            if (armorFeatures & ARMOR_FEATURE_MP_RECOVERY) {
                effectActive = 1;
            }
            break;
        case EQUIPMENT_EFFECT_SKULL_CRYSTAL:
            effectActive = _inventoryHasAttachedItem(INVENTORY_ITEM_SKULL_CRYSTAL);
            break;
        case EQUIPMENT_EFFECT_OFUDA:
            effectActive = _inventoryHasAttachedItem(INVENTORY_ITEM_OFUDA);
            break;
        case EQUIPMENT_EFFECT_HOLY_WATER:
            effectActive = _inventoryHasAttachedItem(INVENTORY_ITEM_HOLY_WATER);
            break;
        case EQUIPMENT_EFFECT_MEDICINE_WHEEL:
            effectActive = _inventoryHasAttachedItem(INVENTORY_ITEM_MEDICINE_WHEEL);
            break;
        case EQUIPMENT_EFFECT_MOTION_DETECTOR:
            if (_inventoryHasAttachedItem(INVENTORY_ITEM_GPS) || (armorFeatures & ARMOR_FEATURE_MOTION_DETECTOR)) {
                effectActive = 1;
            }
            break;
    }
    return effectActive;
}

void Gp_ResetInventory(void)
{
    PlayerStatus* status;
    s32           i;
    s32           j;

    status = &gPlayerStatus;
    if (status->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
        _gpClearEquipSlot(status->weapon + 0x7F);
        status->weapon = PLAYER_STATUS_EQUIPMENT_NONE;
    }

    _inventoryClearItems(&Gp_DefaultScan);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems = Gp_DefaultScan;
    inventoryAddItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, 0x6C, 1);
    Gp_EquipMod(0x6C);

    gPlayerStatus.hp = gPlayerStatus.hpMax;
    gPlayerStatus.mp = gPlayerStatus.mpMax;
    Gp_ApplyItemMap();

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            Gp_DebugAttachLevels[j + i * 3] = 0;
        }
    }
    Gp_DebugAttachLevels[0] = 1;

    Gp_StateC08.activeIndex = 0;
    Gp_StateC08.wheelIndex  = 0;
}

void Gp_ClearInventory(void)
{
    PlayerStatus*       status;
    InventoryItemRange* scan;
    InventoryItemRow*   rec;
    s32                 i;

    status = &gPlayerStatus;
    if (status->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
        _gpClearEquipSlot(status->weapon + 0x7F);
        status->weapon = PLAYER_STATUS_EQUIPMENT_NONE;
    }

    _inventoryClearItems(&Gp_DefaultScan);
    _inventorySetCarriedRange(INVENTORY_INITIAL_CARRIED_ROWS);
    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;

    rec = &_gpScanTable(scan)[scan->firstRow];
    for (i = 0; i < scan->rowCount; i++, rec++) {
        if (rec->attachSlot == INVENTORY_ATTACHMENT_EQUIPPED_ARMOR && (u32)(rec->itemId - 0x60) < 0x20) {
            status->armor = rec->itemId - 0x5F;
            _equipmentRecalculateMaxHp();
            Gp_RecalcMaxMp();
            break;
        }
    }

    Gp_StateC08.wheelIndex  = 0;
    Gp_StateC08.activeIndex = 0;
    gPlayerStatus.hp        = gPlayerStatus.hpMax;
    gPlayerStatus.mp        = gPlayerStatus.mpMax;
    Gp_ApplyItemMap();
}

void Gp_InitModeEquip(void)
{
    PlayerStatus*       cfg;
    InventoryItemRange* scan;
    InventoryItemRow*   tmp;
    InventoryItemRow*   table;
    InventoryItemRow*   row;
    s32                 i;
    s32                 acc;
    s32                 count;
    s32                 start;
    s32                 limit;

    s32 weaponItemId;
    u8  primaryItemId;

    cfg = &gPlayerStatus;
    acc = 0;
    if (cfg->weapon == PLAYER_STATUS_EQUIPMENT_NONE) {
        scan         = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        weaponItemId = 0x81;
        switch (scan->tableId) {
            case INVENTORY_ITEM_TABLE_AREA_GRANTS:
                tmp = Gp_ItemTable2;
                break;
            case INVENTORY_ITEM_TABLE_INDIRECT:
                tmp = Gp_ItemTable1;
                break;
            default:
                tmp = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
                break;
        }
        table = tmp;
        i     = 0;
        count = scan->rowCount;
        start = scan->firstRow;
        if (count != 0) {
            limit = count;

            row = gpItemRowAt(table, start);
            do {
                if (row->itemId == weaponItemId) {
                    acc += row->qty;
                }
                i++;
                row++;
            } while (i < limit);
        }
        if (acc != 0) {
            equipmentEquipCarriedWeapon(0x81);
        }
    }
    if (cfg->weapon == 2) {
        weaponItemId  = 0x81;
        primaryItemId = _equipmentGetWeaponLoad(weaponItemId)->primaryItemId;
        if ((primaryItemId == INVENTORY_ITEM_NONE) || (primaryItemId == 0xA0)) {
            Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, 0x81, 0xA0, -1);
        }
    }
}

void areaSeedStageObjectStates(s32 stageId)
{
    const AreaObjectRoom* rooms;
    u32*                  objectStates;

    rooms        = Gp_Bit2Banks[stageId].rooms;
    objectStates = Gp_Bit2Banks[stageId].objectStates;
    // Night Dryfield shares the daytime words, so its first visit must keep them.
    if (stageId == GAME_STAGE_DRYFIELD_NIGHT) {
        return;
    }
    _areaSeedObjectStates(rooms, objectStates);
}

void areaSetCurrentObjectState(s32 objectId, u8 state)
{
    s32  stateShift;
    u32  stateMask;
    u32* stateWord;
    s32  stageId;

    stateShift  = (objectId & AREA_OBJECT_STATE_INDEX_MASK) * AREA_OBJECT_STATE_BITS;
    stateMask   = AREA_OBJECT_PLACE_STATE_MASK << stateShift;
    stageId     = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage;
    stateWord   = &Gp_Bit2Banks[stageId].objectStates[objectId >> AREA_OBJECT_STATE_WORD_SHIFT];
    *stateWord &= ~stateMask;
    stateMask   = state << stateShift;
    *stateWord |= stateMask;
}

void inventoryClearItems(const InventoryItemRange* range)
{
    _inventoryClearItems(range);
}

InventoryItemRow* inventoryGiveItem(const InventoryItemRange* range, s32 itemId, s32 quantity)
{
    return inventoryAddItem(range, itemId, quantity);
}

s32 inventoryRemoveItemRow(InventoryItemRange* range, InventoryItemRow* row, s32 quantity)
{
    _inventoryRemoveItemRow(range, row, quantity);
    return 0;
}

void inventoryClearCollectedBits(void)
{
    s32  wordIndex;
    s32* collectionWord;

    collectionWord = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    for (wordIndex = ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits) - 1; wordIndex >= 0; wordIndex--) {
        *collectionWord++ = 0;
    }
}

void inventorySetCollectedBit(s32 collectionId)
{
    s32* word;
    s32  bitIndex;

    word      = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    bitIndex  = collectionId & INVENTORY_COLLECTION_ID_MASK;
    word     += bitIndex / INVENTORY_COLLECTION_BITS_PER_WORD;
    bitIndex %= INVENTORY_COLLECTION_BITS_PER_WORD;
    *word    |= 1 << bitIndex;
    // Refresh the age marker even when the Ice Bag bit was already set.
    if ((collectionId & INVENTORY_COLLECTION_ID_MASK) == INVENTORY_COLLECTED_ICE_BAG_BIT) {
        gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime;
    }
}

void inventoryClearCollectedBit(s32 collectionId)
{
    s32* word;

    word          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    collectionId &= INVENTORY_COLLECTION_ID_MASK;
    word         += collectionId / INVENTORY_COLLECTION_BITS_PER_WORD;
    collectionId %= INVENTORY_COLLECTION_BITS_PER_WORD;
    *word        &= ~(1 << collectionId);
}

s32 inventoryCountCollectedBits(void)
{
    s32        collectedCount;
    const s32* collectionWord;
    s32        wordIndex;
    s32        bitIndex;
    s32        packedBits;
    s32        bitMaskBase;

    collectionWord = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    collectedCount = 0;
    bitMaskBase    = 1;
    for (wordIndex = ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits) - 1; wordIndex >= 0; wordIndex--) {
        bitIndex   = 0;
        packedBits = *collectionWord;
        do {
            if (packedBits & (bitMaskBase << bitIndex)) {
                collectedCount++;
            }
            bitIndex++;
        } while (bitIndex < INVENTORY_COLLECTION_BITS_PER_WORD);
        collectionWord++;
    }
    return collectedCount;
}

s32 Gp_CountScanItems(InventoryItemRange* arg0)
{
    InventoryItemRow* tmp;
    InventoryItemRow* table;
    InventoryItemRow* row;
    s32               i;
    s32               ret;
    s32               count;
    s32               start;
    s32               limit;

    switch (arg0->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            tmp = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    table = tmp;
    i     = 0;
    count = arg0->rowCount;
    start = arg0->firstRow;
    ret   = i;
    if (count != 0) {
        limit = count;

        row = gpItemRowAt(table, start);
        do {
            if (row->itemId != INVENTORY_ITEM_NONE) {
                ret++;
            }
            i++;
            row++;
        } while (i < limit);
    }
    return ret;
}

EquipmentWeaponLoad* equipmentGetWeaponLoad(s32 weaponItemId)
{
    return &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
}

s32 Gp_CountEquippedRelated(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow*    table;
    EquipmentWeaponLoad* weaponLoad;
    s32                  count;
    s32                  i;
    s32                  end;
    s32                  weaponItemId;

    table = inventoryGetRangeTable(arg0);
    count = 0;
    if ((u32)(arg1 - 0xA0) < 0x20) {
        i   = arg0->firstRow;
        end = i + arg0->rowCount;
        if (i < end) {
            for (; i < arg0->firstRow + arg0->rowCount; i++) {
                weaponItemId = table[i].itemId;
                if ((u32)(weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems)) {
                    weaponLoad = _equipmentGetWeaponLoad(weaponItemId);
                    if (weaponLoad->primaryItemId == arg1) {
                        count += weaponLoad->primaryQty;
                    }
                    if (weaponLoad->secondaryItemId == arg1) {
                        count += weaponLoad->secondaryQty;
                    }
                }
            }
            return count;
        }
    }
    return count;
}

void equipmentClearRemovableLoads(s32 weaponItemId)
{
    EquipmentWeaponLoad* load;
    s32                  hasBuiltInSupply = 0;
    s32                  supplyIndex;

    if ((u32)(weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST) >= ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems)) {
        return;
    }

    load = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
    for (supplyIndex = 0; supplyIndex < EQUIPMENT_WEAPON_SUPPLY_COUNT; supplyIndex++) {
        if (weaponItemId == Gp_ItemMaps[supplyIndex].weaponItemId) {
            hasBuiltInSupply = 1;
            break;
        }
    }

    // A built-in supply keeps both its selection and its remaining charge.
    if ((hasBuiltInSupply == 0) || (Gp_ItemMaps[supplyIndex].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_PRIMARY)) {
        load->primaryItemId = INVENTORY_ITEM_NONE;
        load->primaryQty    = 0;
    }

    if ((hasBuiltInSupply == 0) || (Gp_ItemMaps[supplyIndex].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_SECONDARY)) {
        if (load->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
            load->secondaryItemId = INVENTORY_ITEM_NONE;
        }
        load->secondaryQty = 0;
    }
}

void equipmentClearSelectedRemovableLoads(s32 weaponItemId, s32 loadSelection)
{
    EquipmentWeaponLoad* load;
    s32                  hasBuiltInSupply = 0;
    s32                  supplyIndex;

    if ((u32)(weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST) >= ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems)) {
        return;
    }

    load = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
    for (supplyIndex = 0; supplyIndex < EQUIPMENT_WEAPON_SUPPLY_COUNT; supplyIndex++) {
        if (weaponItemId == Gp_ItemMaps[supplyIndex].weaponItemId) {
            hasBuiltInSupply = 1;
            break;
        }
    }

    if (loadSelection != EQUIPMENT_CLEAR_LOAD_SECONDARY) {
        if ((hasBuiltInSupply == 0) || (Gp_ItemMaps[supplyIndex].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_PRIMARY)) {
            load->primaryItemId = INVENTORY_ITEM_NONE;
            load->primaryQty    = 0;
        }
    }

    if (loadSelection != EQUIPMENT_CLEAR_LOAD_PRIMARY) {
        if ((hasBuiltInSupply == 0) || (Gp_ItemMaps[supplyIndex].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_SECONDARY)) {
            if (load->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
                load->secondaryItemId = INVENTORY_ITEM_NONE;
            }
            load->secondaryQty = 0;
        }
    }
}

s32 Gp_ScanStackQty(InventoryItemRange* arg0, s32 arg1)
{
    s32               index;
    s32               ret;
    InventoryItemRow* table;

    index = arg0->firstRow;
    table = inventoryGetRangeTable(arg0);
    if ((u32)(arg1 - 0xA0) < 0x20) {
        ret = inventoryFindStackQuantity(table, arg0, &index, arg1);
    } else {
        ret = 0;
    }
    return ret;
}

void inventoryConsumeFirstStack(InventoryItemRange* range, s32 itemId, s32 quantity)
{
    _inventoryConsumeFirstStack(range, itemId, quantity);
}

s32 Gp_FillRelated(s32 arg0, s32 arg1)
{
    EquipmentWeaponLoad* slot;
    const u8*            primaryItemId;
    s32                  ret;

    slot          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[arg0 - EQUIPMENT_WEAPON_ITEM_FIRST];
    primaryItemId = &slot->primaryItemId;
    if (arg1 != 0) {
        ret = Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg0, slot->secondaryItemId, -1);
    } else {
        ret = Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg0, *primaryItemId, -1);
    }
    return ret;
}

s32 Gp_UnequipRelated(s32 arg0, s32 arg1)
{
    EquipmentWeaponLoad*       slot;
    const EquipmentWeaponLoad* secondaryLoad;
    s32                        ret;

    slot          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[arg0 - EQUIPMENT_WEAPON_ITEM_FIRST];
    secondaryLoad = slot;
    if (arg1 == 0) {
        ret = Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg0, slot->primaryItemId, 0);
    } else {
        ret = Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg0, secondaryLoad->secondaryItemId, 0);
    }
    return ret == 0;
}

s32 areaGetCurrentObjectState(s32 objectId)
{
    s32        stageId;
    const u32* stateWord;
    u32        packedStates;
    s32        stateShift;

    stageId       = gGameSession->location.loc.stage;
    stateWord     = &Gp_Bit2Banks[stageId].objectStates[objectId >> 4];
    stateShift    = (objectId & 0xF) * 2;
    packedStates  = *stateWord;
    packedStates &= AREA_OBJECT_PLACE_STATE_MASK << stateShift;
    return packedStates >> stateShift;
}

s32 inventoryHasCollectedBit(s32 collectionId)
{
    const s32* word;
    s32        maskedBit;

    word          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    collectionId &= INVENTORY_COLLECTION_ID_MASK;
    word         += collectionId / INVENTORY_COLLECTION_BITS_PER_WORD;
    collectionId %= INVENTORY_COLLECTION_BITS_PER_WORD;
    maskedBit     = *word & (1 << collectionId);
    return maskedBit != 0;
}

InventoryItemRow* inventoryGetRangeTable(const InventoryItemRange* range)
{
    switch (range->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            return Gp_ItemTable2;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            return Gp_ItemTable1;
        default:
            return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
    }
}

s32 inventoryIndexOfRow(const InventoryItemRange* range, const InventoryItemRow* row)
{
    const InventoryItemRow* rangeRow;
    s32                     rowIndex;
    s32                     foundIndex;

    rangeRow   = _gpScanTable(range);
    foundIndex = INVENTORY_ROW_NOT_FOUND;
    rangeRow  += range->firstRow;
    for (rowIndex = 0; rowIndex < range->rowCount; rowIndex++) {
        if (rangeRow == row) {
            foundIndex = rowIndex;
            break;
        }
        rangeRow++;
    }
    return foundIndex;
}

InventoryItemRow* inventoryGetRow(const InventoryItemRange* range, s32 rowIndex, s32 unused)
{
    InventoryItemRow* table;

    table = _gpScanTable(range);
    return &table[range->firstRow + rowIndex];
}

/// Returns the one-byte item id at a zero-based row position in a range.
///
/// Zero denotes a free row. The descriptor and selected table are borrowed;
/// the position must be within the range, which must fit readable storage.
static s32 _inventoryGetRowItemId(const InventoryItemRange* range, s32 rowIndex)
{
    const InventoryItemRow* table;
    const InventoryItemRow* row;

    table = _gpScanTable(range);
    row   = &table[range->firstRow + rowIndex];
    return row->itemId;
}
