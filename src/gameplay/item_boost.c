#include "gameplay/items.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

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

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern u8 Gp_StrMore[];

extern u8 Gp_StrAttachAvail[];

extern UiObjectDesc Gp_BoostPanelDesc;

static inline InventoryItemRow* _gpScanTable(InventoryItemRange* scan);

static inline void _gpClearEquipSlot(s32 item);

static inline void _gpConsumeScanQty(InventoryItemRange* scan, s32 item, s32 n);

static __inline void func_800B996C_RemoveItem(InventoryItemRange* arg0, InventoryItemRow* arg1, s32 arg2);

static inline s32 _gpGetModLevel(s32 item);

static inline void _gpDrawPromptItem(UiObject* obj, s32 x, s32 y, u8* str, s32 item, s32 color, s32 one);

static __inline__ s32 Gp_HasStockedItemInline(s32 arg0);

static inline void _gpClearScanItems(InventoryItemRange* scan);

static inline void _gpRecalcMaxHp(void);

static inline void _gpSetPlayerScan(s32 count);

static inline void _gpApplyBit2List(AreaObjectRoom* table, u32* dest);

static s32 Gp_GetScanItemId(InventoryItemRange* arg0, s32 arg1);

u8           Gp_StrMore[]        = "More ";
u8           Gp_StrAttachAvail[] = "attachments available.";
UiObjectDesc Gp_BoostPanelDesc   = { USER_INTERFACE_PANEL_TITLE_STYLE, { 10, 20, 30, 40 }, 12, 0, TASK_BODY_NONE, 192, Gp_TickBoostPanel, 0 };

/* Count item `id` in saved rows 0..254 through a cleared range. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, Gp_SumScanQty(&(scan), (id)))

/* Item names and descriptions shared by the inventory tables. */

/* Gives `scan` one `weapon` and loads it with `ammo`. */
#define GP_GIVE_LOADED(scan, weapon, ammo)           \
    do {                                             \
        Gp_GiveItem(scan, weapon, 1);                \
        Gp_EquipRelatedItem(scan, weapon, ammo, -1); \
    } while (0)

/* Clears the carried inventory, equips the starting armour, restores HP/MP,
 * and gives the initial supplies and their attachment slots. */
#define _gpInitStartingItems(scan, cfg)                \
    do {                                               \
        Gp_ClearScanItems(scan);                       \
        Gp_GiveItem(scan, 0x60, 1);                    \
        Gp_EquipMod(0x60);                             \
        (cfg)->hp = (cfg)->hpMax;                      \
        (cfg)->mp = (cfg)->mpMax;                      \
        Gp_GiveItem(scan, 0x92, 1);                    \
        Gp_GiveItem(scan, 0x40, 1)->attachSlot    = 1; \
        Gp_GiveItem(scan, 0xA0, 0x64)->attachSlot = 2; \
    } while (0)

/* Item table a scan window lies in. */

static inline InventoryItemRow* _gpScanTable(InventoryItemRange* scan)
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
static inline void _gpConsumeScanQty(InventoryItemRange* scan, s32 item, s32 n)
{
    InventoryItemRow* table;
    s32               qty;
    s32               i;
    s32               left;

    table = _gpScanTable(scan);
    qty   = 0;
    for (i = scan->firstRow; i < scan->firstRow + scan->rowCount; i++) {
        if (table[i].itemId == item) {
            qty = table[i].qty;
            break;
        }
    }
    if (i != scan->firstRow + scan->rowCount) {
        if (n < 0) {
            n = qty;
        }
        left = qty - n;
        if (left < 0) {
            left = 0;
        }
        if (left == 0) {
            table[i].itemId     = INVENTORY_ITEM_NONE;
            table[i].qty        = 0;
            table[i].attachSlot = INVENTORY_ATTACHMENT_NONE;
        } else {
            table[i].qty = left;
        }
    }
}
static __inline void func_800B996C_RemoveItem(InventoryItemRange* arg0, InventoryItemRow* arg1, s32 arg2)
{
    s32 item;

    item = arg1->itemId;
    if (item < 0xA0) {
        arg1->itemId     = INVENTORY_ITEM_NONE;
        arg1->qty        = 0;
        arg1->attachSlot = INVENTORY_ATTACHMENT_NONE;
    } else {
        _gpConsumeScanQty(arg0, item, arg2);
    }
}
static inline s32 _gpGetModLevel(s32 item)
{
    s32         ret;
    s32         idx;
    ArmorStats* stats;

    idx = item - 0x60;
    ret = 0;
    if ((u32)idx < 0x20) {
        stats = &Gp_ModStatAttrs[(item)-0x60];
        ret   = stats->baseAttachmentSlots;
        ret  += gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus[idx];
        if (ret >= ARMOR_ATTACHMENT_SLOT_MAX + 1) {
            ret = ARMOR_ATTACHMENT_SLOT_MAX;
        }
    }
    return ret;
}
static inline void _gpDrawPromptItem(UiObject* obj, s32 x, s32 y, u8* str, s32 item, s32 color, s32 one)
{
    s32 width;

    textDrawUiLine(obj, x, y, str, color, one, TEXT_ALIGNMENT_LEFT);
    width = textMeasureLineWidth(str) + 4;
    textDrawUiLine(obj, x + width, y, (const u8*)Gp_GetItemText(item, 0, 0), 0x37A78, one, TEXT_ALIGNMENT_LEFT);
}
static __inline__ s32 Gp_HasStockedItemInline(s32 arg0)
{
    InventoryItemRange* scan;
    InventoryItemRow*   table;
    s32                 i;
    s32                 ret;
    s32                 count;

    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    ret  = 0;
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
    i      = 0;
    table += scan->firstRow;
    count  = scan->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot > INVENTORY_ATTACHMENT_NONE) {
            if (table->itemId == arg0) {
                ret = 1;
                break;
            }
        }
        table++;
    }
    return ret;
}
static inline void _gpClearScanItems(InventoryItemRange* scan)
{
    InventoryItemRow* table;
    s32               i;
    s32               row;

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
    for (i = 0, row = scan->firstRow; i < scan->rowCount; i++, row++) {
        table[row].itemId     = INVENTORY_ITEM_NONE;
        table[row].attachSlot = INVENTORY_ATTACHMENT_NONE;
        table[row].qty        = 0;
    }
}
static inline void _gpRecalcMaxHp(void)
{
    PlayerStatus*        cfg;
    McSaveData*          save;
    PlayerModeBaseStats* table;
    u16                  val;

    cfg        = &gPlayerStatus;
    table      = Gp_StatRows;
    save       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    val        = table[save->state.gameMode].baseHp.hp;
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
    if (cfg->hp > cfg->hpMax) {
        cfg->hp = cfg->hpMax;
    }
}
static inline void _gpSetPlayerScan(s32 count)
{
    McSaveData* p;

    p                              = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    p->state.carriedItems.firstRow = 0;
    p->state.carriedItems.rowCount = count;
    p->state.carriedItems.tableId  = INVENTORY_ITEM_TABLE_SAVED;
}
static inline void _gpApplyBit2List(AreaObjectRoom* table, u32* dest)
{
    AreaObjectPlace* rec;
    u32*             p;
    u32              mask;

    if (table == NULL) {
        return;
    }
    rec = table->places.list;
    if (table->places.sentinel == AREA_OBJECT_ROOM_END) {
        return;
    }
    do {
        if (rec != NULL) {
            for (; rec->flagIndex != AREA_OBJECT_PLACE_END; rec++) {
                mask = AREA_OBJECT_PLACE_STATE_MASK << ((rec->flagIndex & 0xF) * 2);
                p    = &dest[rec->flagIndex >> 4];
                *p  &= ~mask;
                mask = (rec->state & AREA_OBJECT_PLACE_STATE_MASK) << ((rec->flagIndex & 0xF) * 2);
                *p  |= mask;
            }
        }
        table++;
        rec = table->places.list;
    } while (table->places.sentinel != AREA_OBJECT_ROOM_END);
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
        if (_gpGetModLevel(item) < ARMOR_ATTACHMENT_SLOT_MAX) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus[item - 0x60]++;
        } else {
            arg1->status = 0x1A;
        }
        if (arg1->status == 0xFF) {
            width = textMeasureLineWidth((const u8*)Gp_GetItemText(item, 0, 0)) + textMeasureLineWidth(Gp_StrMore) + 4;
            other = textMeasureLineWidth(Gp_StrAttachAvail);
            if (width < other) {
                width = other;
            }
            uiSetPanelContentSize(&(arg0)->panel, width + 5, uiGetTextRowsHeight(2) + 1);
            (&(arg0)->panel)->bounds.rect.x = (-(&(arg0)->panel)->bounds.rect.w) >> 1;
            (&(arg0)->panel)->bounds.rect.y = ((-(&(arg0)->panel)->bounds.rect.h) >> 1) - 0x14;
            func_800B996C_RemoveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, Gp_SelItemRec, 1);
            arg1->killCountdown = 0xBC;
            arg1->state++;
        }
    }
    if (arg1->status != 0xFF) {
        saved                 = arg1->spawnArg1.value;
        arg1->spawnArg1.value = arg1->status;
        Gp_NoticePanelTask(arg1);
        arg1->spawnArg1.value = saved;
        return;
    }

    x = arg0->panel.contentLeft.signedValue + 2;
    y = arg0->panel.contentTop.signedValue;
    uiDrawPanelLabel(&(arg0)->panel, Gp_StrNotice2);
    color = 0x606060;
    row   = y + 0xF;
    _gpDrawPromptItem(arg0, x, row, Gp_StrMore, item, color, 1);
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
        func_800B996C_RemoveItem(0, Gp_SelItemRec, 1);
        uiSpawnObject(&Gp_BoostPanelDesc, 0, 0, 1, arg0);
    }
    saved                 = arg1->spawnArg1.value;
    arg1->spawnArg1.value = 0x1D;
    Gp_NoticePanelTask(arg1);
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
        func_800B996C_RemoveItem(0, Gp_SelItemRec, 1);
        uiSpawnObject(&Gp_BoostPanelDesc, 0, 0, 1, arg0);
    }
    saved                 = arg1->spawnArg1.value;
    arg1->spawnArg1.value = 0x1C;
    Gp_NoticePanelTask(arg1);
    arg1->spawnArg1.value = saved;
}

s32 func_800B9D80(s32 arg0)
{
    PlayerStatus* cfg;
    ArmorStats*   attr;
    s32           features;
    s32           ret;
    s32           stateA;
    s32           stateB;

    ret      = 0;
    features = 0;
    stateA   = 0;
    stateB   = 0;
    cfg      = &gPlayerStatus;
    if (cfg->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
        attr     = &Gp_ModStatAttrs[(cfg->armor + 0x5F) - 0x60];
        features = attr->features;
    }
    if ((Gp_StateC08.metabolismTicks > 0) || (Gp_StateC08.bodyWard != 0)) {
        stateA = 1;
    }
    if ((Gp_StateC08.metabolismTicks > 0) || (Gp_StateC08.mindWard != 0)) {
        stateB = 1;
    }

    switch (arg0) {
        case 0x101:
            if (Gp_HasStockedItemInline(0x3F) || stateA) {
                ret = 1;
            }
            break;
        case 0x102:
            if (stateA || (features & ARMOR_FEATURE_RESIST_PARALYSIS)) {
                ret = 1;
            }
            break;
        case 0x104:
            if (stateA || (features & ARMOR_FEATURE_RESIST_POISON)) {
                ret = 1;
            }
            break;
        case 0x108:
            ret = Gp_HasStockedItemInline(0xB);
            if (stateB || (features & ARMOR_FEATURE_RESIST_SILENCE)) {
                ret = 1;
            }
            break;
        case 0x110:
            break;
        case 0x120:
            ret = Gp_HasStockedItemInline(0xE);
            if (stateB || (features & ARMOR_FEATURE_RESIST_CONFUSION)) {
                ret = 1;
            }
            break;
        case 0x140:
            if (stateB || Gp_HasStockedItemInline(0xE)) {
                ret = 1;
            }
            break;
        case 0x200:
            if (features & ARMOR_FEATURE_RESIST_IMPACT) {
                ret = 1;
            }
            break;
        case 0x400:
            if (features & ARMOR_FEATURE_MOTION_DETECTOR) {
                ret = 1;
            }
            break;
        case 0x800:
            if (features & ARMOR_FEATURE_MP_GENERATION) {
                ret = 1;
            }
            break;
        case 0x1000:
            if (features & ARMOR_FEATURE_HP_RECOVERY) {
                ret = 1;
            }
            break;
        case 0x2000:
            if (features & ARMOR_FEATURE_QUICK_FIRE) {
                ret = 1;
            }
            break;
        case 0x4000:
            if (features & ARMOR_FEATURE_MEDICAL_INSPECTION) {
                ret = 1;
            }
            break;
        case 0x8000:
            if (features & ARMOR_FEATURE_MP_RECOVERY) {
                ret = 1;
            }
            break;
        case 0x10000:
            ret = Gp_HasStockedItemInline(0x36);
            break;
        case 0x20000:
            ret = Gp_HasStockedItemInline(0x39);
            break;
        case 0x40000:
            ret = Gp_HasStockedItemInline(0x38);
            break;
        case 0x80000:
            ret = Gp_HasStockedItemInline(0x37);
            break;
        case 0x100000:
            if (Gp_HasStockedItemInline(0x40) || (features & ARMOR_FEATURE_MOTION_DETECTOR)) {
                ret = 1;
            }
            break;
    }
    return ret;
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

    _gpClearScanItems(&Gp_DefaultScan);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems = Gp_DefaultScan;
    Gp_AddItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, 0x6C, 1);
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

    _gpClearScanItems(&Gp_DefaultScan);
    _gpSetPlayerScan(0x14);
    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;

    rec = &_gpScanTable(scan)[scan->firstRow];
    for (i = 0; i < scan->rowCount; i++, rec++) {
        if (rec->attachSlot == INVENTORY_ATTACHMENT_EQUIPPED_ARMOR && (u32)(rec->itemId - 0x60) < 0x20) {
            status->armor = rec->itemId - 0x5F;
            _gpRecalcMaxHp();
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
    InventoryItemRow*   rec;
    s32                 i;
    s32                 acc;
    s32                 count;
    s32                 start;
    s32                 limit;

    s32 item;
    u8  slotItem;

    cfg = &gPlayerStatus;
    acc = 0;
    if (cfg->weapon == PLAYER_STATUS_EQUIPMENT_NONE) {
        scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        item = 0x81;
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

            rec = gpItemRowAt(table, start);
            do {
                if (rec->itemId == item) {
                    acc += rec->qty;
                }
                i++;
                rec++;
            } while (i < limit);
        }
        if (acc != 0) {
            Gp_EquipHeld(0x81);
        }
    }
    if (cfg->weapon == 2) {
        item     = 0x81;
        slotItem = gpItemSlot(item)->primaryItemId;
        if ((slotItem == 0) || (slotItem == 0xA0)) {
            Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, 0x81, 0xA0, -1);
        }
    }
}

void Gp_ApplyBit2Bank(s32 arg0)
{
    AreaObjectRoom* table;
    u32*            dest;

    table = Gp_Bit2Banks[arg0].rooms;
    dest  = Gp_Bit2Banks[arg0].objectStates;
    if (arg0 == 3) {
        return;
    }
    _gpApplyBit2List(table, dest);
}

void Gp_SetCurBit2Flag(s32 arg0, u8 arg1)
{
    s32  shift;
    u32  mask;
    u32* p;
    s32  stage;

    shift = (arg0 & 0xF) * 2;
    mask  = 3 << shift;
    stage = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage;
    p     = &Gp_Bit2Banks[stage].objectStates[arg0 >> 4];
    *p   &= ~mask;
    mask  = arg1 << shift;
    *p   |= mask;
}

void Gp_ClearScanItems(InventoryItemRange* scan)
{
    _gpClearScanItems(scan);
}

InventoryItemRow* Gp_GiveItem(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    return Gp_AddItem(arg0, arg1, arg2);
}

s32 Gp_RemoveItem(InventoryItemRange* arg0, InventoryItemRow* arg1, s32 arg2)
{
    InventoryItemRow* table;
    s32               item;
    s32               qty;
    s32               i;

    item = arg1->itemId;
    if (item < 0xA0) {
        arg1->itemId     = INVENTORY_ITEM_NONE;
        arg1->qty        = 0;
        arg1->attachSlot = INVENTORY_ATTACHMENT_NONE;
    } else {
        table = _gpScanTable(arg0);
        qty   = 0;
        for (i = arg0->firstRow; i < arg0->firstRow + arg0->rowCount; i++) {
            if (table[i].itemId == item) {
                qty = table[i].qty;
                break;
            }
        }
        if (i != arg0->firstRow + arg0->rowCount) {
            if (arg2 < 0) {
                arg2 = qty;
            }
            arg2 = qty - arg2;
            if (arg2 < 0) {
                arg2 = 0;
            }
            if (arg2 == 0) {
                table[i].itemId     = INVENTORY_ITEM_NONE;
                table[i].qty        = 0;
                table[i].attachSlot = INVENTORY_ATTACHMENT_NONE;
            } else {
                table[i].qty = arg2;
            }
        }
    }
    return 0;
}

void Gp_ClearCollectedBits(void)
{
    s32  i;
    s32* p;

    p = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    for (i = 3; i >= 0; i--) {
        *p++ = 0;
    }
}

void Gp_SetCollectedBit(s32 arg0)
{
    s32* p;
    s32  bit;

    p    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    bit  = arg0 & 0x7F;
    p   += bit / 32;
    bit %= 32;
    *p  |= 1 << bit;
    if ((arg0 & 0x7F) == 0x19) {
        gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime;
    }
}

void Gp_ClearCollectedBit(s32 arg0)
{
    s32* p;

    p     = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    arg0 &= 0x7F;
    p    += arg0 / 32;
    arg0 %= 32;
    *p   &= ~(1 << arg0);
}

s32 Gp_CountCollectedBits(void)
{
    s32  count;
    s32* p;
    s32  i;
    s32  bit;
    s32  word;
    s32  one;

    p     = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    count = 0;
    one   = 1;
    for (i = 3; i >= 0; i--) {
        bit  = 0;
        word = *p;
        do {
            if (word & (one << bit)) {
                count++;
            }
            bit++;
        } while (bit < 32);
        p++;
    }
    return count;
}

s32 Gp_CountScanItems(InventoryItemRange* arg0)
{
    InventoryItemRow* tmp;
    InventoryItemRow* table;
    InventoryItemRow* rec;
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

        rec = gpItemRowAt(table, start);
        do {
            if (rec->itemId != INVENTORY_ITEM_NONE) {
                ret++;
            }
            i++;
            rec++;
        } while (i < limit);
    }
    return ret;
}

EquipmentWeaponLoad* Gp_GetItemSlot(s32 arg0)
{
    return &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[arg0 - EQUIPMENT_WEAPON_ITEM_FIRST];
}

s32 Gp_CountEquippedRelated(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow*    table;
    EquipmentWeaponLoad* slot;
    s32                  count;
    s32                  i;
    s32                  end;
    s32                  itemId;

    table = Gp_GetItemTable(arg0);
    count = 0;
    if ((u32)(arg1 - 0xA0) < 0x20) {
        i   = arg0->firstRow;
        end = i + arg0->rowCount;
        if (i < end) {
            for (; i < arg0->firstRow + arg0->rowCount; i++) {
                itemId = table[i].itemId;
                if ((u32)(itemId - 0x80) < 0x20) {
                    slot = gpItemSlot(itemId);
                    if (slot->primaryItemId == arg1) {
                        count += slot->primaryQty;
                    }
                    if (slot->secondaryItemId == arg1) {
                        count += slot->secondaryQty;
                    }
                }
            }
            return count;
        }
    }
    return count;
}

void Gp_ClearEquipSlot(s32 arg0)
{
    EquipmentWeaponLoad* slot;
    s32                  found = 0;
    s32                  i;

    if ((u32)(arg0 - 0x80) >= 0x20) {
        return;
    }

    slot = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[arg0 - EQUIPMENT_WEAPON_ITEM_FIRST];
    for (i = 0; i < EQUIPMENT_WEAPON_SUPPLY_COUNT; i++) {
        if (arg0 == Gp_ItemMaps[i].weaponItemId) {
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

void Gp_ClearEquipSlotSel(s32 arg0, s32 arg1)
{
    EquipmentWeaponLoad* slot;
    s32                  found = 0;
    s32                  i;

    if ((u32)(arg0 - 0x80) >= 0x20) {
        return;
    }

    slot = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[arg0 - EQUIPMENT_WEAPON_ITEM_FIRST];
    for (i = 0; i < EQUIPMENT_WEAPON_SUPPLY_COUNT; i++) {
        if (arg0 == Gp_ItemMaps[i].weaponItemId) {
            found = 1;
            break;
        }
    }

    if (arg1 != 2) {
        if ((found == 0) || (Gp_ItemMaps[i].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_PRIMARY)) {
            slot->primaryItemId = INVENTORY_ITEM_NONE;
            slot->primaryQty    = 0;
        }
    }

    if (arg1 != 1) {
        if ((found == 0) || (Gp_ItemMaps[i].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_SECONDARY)) {
            if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
                slot->secondaryItemId = INVENTORY_ITEM_NONE;
            }
            slot->secondaryQty = 0;
        }
    }
}

s32 Gp_ScanStackQty(InventoryItemRange* arg0, s32 arg1)
{
    s32               index;
    s32               ret;
    InventoryItemRow* table;

    index = arg0->firstRow;
    table = Gp_GetItemTable(arg0);
    if ((u32)(arg1 - 0xA0) < 0x20) {
        ret = (s16)Gp_FindScanQty(table, arg0, &index, arg1);
    } else {
        ret = 0;
    }
    return ret;
}

void Gp_ConsumeScanQty(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    InventoryItemRow* table;
    s32               qty;
    s32               i;

    table = _gpScanTable(arg0);
    qty   = 0;
    for (i = arg0->firstRow; i < arg0->firstRow + arg0->rowCount; i++) {
        if (table[i].itemId == arg1) {
            qty = table[i].qty;
            break;
        }
    }
    if (i != arg0->firstRow + arg0->rowCount) {
        if (arg2 < 0) {
            arg2 = qty;
        }
        arg2 = qty - arg2;
        if (arg2 < 0) {
            arg2 = 0;
        }
        if (arg2 == 0) {
            table[i].itemId     = INVENTORY_ITEM_NONE;
            table[i].qty        = 0;
            table[i].attachSlot = INVENTORY_ATTACHMENT_NONE;
        } else {
            table[i].qty = arg2;
        }
    }
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

s32 Gp_GetCurBit2Flag(s32 arg0)
{
    s32  stage;
    u32* p;
    u32  word;
    s32  shift;

    stage = gGameSession->location.loc.stage;
    p     = &Gp_Bit2Banks[stage].objectStates[arg0 >> 4];
    shift = (arg0 & 0xF) * 2;
    word  = *p;
    word &= 3 << shift;
    return word >> shift;
}

s32 Gp_HasCollectedBit(s32 arg0)
{
    s32* p;
    s32  val;

    p     = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
    arg0 &= 0x7F;
    p    += arg0 / 32;
    arg0 %= 32;
    val   = *p & (1 << arg0);
    return val != 0;
}

InventoryItemRow* Gp_GetItemTable(InventoryItemRange* arg0)
{
    switch (arg0->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            return Gp_ItemTable2;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            return Gp_ItemTable1;
        default:
            return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
    }
}

s32 Gp_ScanIndexOf(InventoryItemRange* arg0, InventoryItemRow* arg1)
{
    InventoryItemRow* table;
    s32               i;
    s32               ret;

    table  = _gpScanTable(arg0);
    ret    = -1;
    table += arg0->firstRow;
    for (i = 0; i < arg0->rowCount; i++) {
        if (table == arg1) {
            ret = i;
            break;
        }
        table++;
    }
    return ret;
}

InventoryItemRow* Gp_GetScanSlot(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    InventoryItemRow* table;

    switch (arg0->tableId) {
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
    return &table[arg0->firstRow + arg1];
}

static s32 Gp_GetScanItemId(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow* table;
    InventoryItemRow* rec;

    switch (arg0->tableId) {
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
    rec = &table[arg0->firstRow + arg1];
    return rec->itemId;
}
