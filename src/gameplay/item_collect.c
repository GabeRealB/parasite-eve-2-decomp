#include "gameplay/items.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/memory.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "attachments.h"
#include "gameplay/enemy.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_menu.h"
#include "items.h"
#include "player_actor.h"
#include "gameplay/scene_runtime.h"
#include "scene_runtime.h"

#include "gameplay/damage.h"
#include "main/gfx.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// 0xFFFF-terminated item-id list walked by `inventoryGetCollectedItemId`. Each id's
/// low 7 bits index a collected-item bit in `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits`.
extern u16 Gp_CollectedIds[];

/// Unreferenced nonzero halfword after the collected-item terminator.
extern u16 D_80114B32;

/// Catalogue identification flags stored in the live save.
enum {
    ITEM_IDENTIFICATION_ID_LIMIT      = 0x180,
    ITEM_IDENTIFICATION_BITS_PER_WORD = 32
};

/// Whole saved minutes before the Ice Bag becomes a Bag of Water.
enum { INVENTORY_ICE_BAG_MELT_MINUTES = 2 };

static inline s32 _gpGetModLevel(s32 item);

static inline void _gpApplyBit2List(AreaObjectRoom* table, u32* dest);

static inline s32 _gpReadBit2Flag(u32* p, s32 index);

static void func_800BB7B4(Task* arg0);

static void Gp_ApplyBit2List(AreaObjectRoom* table, u32* dest);

static s32 Gp_GetBit2Flag(GameLocationKey* arg0, s32 arg1);

static Enemy* Gp_SpawnAtPlace(AreaObjectSpawn* spawn, AreaObjectPlace* place);

static void func_800BBB54(Task* arg0);

static void Gp_ResetAuxSlots(void);

static s32 Gp_SumItemQty(s32 arg0);

static void Gp_SetPlayerScan(s32 arg0);

static void Gp_InitItemSeenBits(void);

static s16 Gp_PlayTimeDelta(void);

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
static inline s32 _gpReadBit2Flag(u32* p, s32 index)
{
    u32 word;
    s32 shift;

    p     += index >> 4;
    shift  = (index & 0xF) * 2;
    word   = *p;
    word  &= 3 << shift;
    word >>= shift;
    return word;
}

u16 Gp_CollectedIds[41] = {
    297,
    296,
    295,
    293,
    292,
    301,
    291,
    290,
    289,
    288,
    300,
    287,
    286,
    282,
    281,
    280,
    279,
    278,
    275,
    274,
    276,
    273,
    277,
    283,
    272,
    271,
    268,
    267,
    266,
    265,
    264,
    261,
    260,
    259,
    263,
    258,
    257,
    262,
    303,
    304,
    65535
};
/// Unreferenced nonzero halfword after the collected-item terminator.
u16 D_80114B32 = 0x1131;

/* Item names and descriptions shared by the inventory tables. */

/* Item table a scan window lies in. */

s32 inventoryGetCollectedItemId(s32 collectedIndex, s32 unused)
{
    enum {
        INVENTORY_COLLECTED_LIST_END       = 0xFFFF,
        INVENTORY_COLLECTION_ID_MASK       = 0x7F,
        INVENTORY_COLLECTION_BITS_PER_WORD = 32
    };
    const u16* collectedIds;
    s32        collectedItemId;
    const s32* collectionWord;
    s32        bitIndex;
    s32        itemId;
    u32        maskUnit;

    collectedIds    = Gp_CollectedIds;
    collectedItemId = INVENTORY_ITEM_NONE;
    if (*collectedIds != INVENTORY_COLLECTED_LIST_END) {
        do {
            itemId          = *collectedIds;
            collectionWord  = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
            maskUnit        = 1U;
            bitIndex        = itemId & INVENTORY_COLLECTION_ID_MASK;
            collectionWord += bitIndex / INVENTORY_COLLECTION_BITS_PER_WORD;
            bitIndex       %= INVENTORY_COLLECTION_BITS_PER_WORD;
            if (*collectionWord & (maskUnit << bitIndex)) {
                collectedIndex--;
                collectedIds++;
                if (collectedIndex >= 0) {
                    continue;
                }
                collectedItemId = itemId;
                break;
            }
            collectedIds++;
        } while (*collectedIds != INVENTORY_COLLECTED_LIST_END);
    }
    return collectedItemId;
}

/// Borrows the base row table selected by a readable inventory range.
///
/// The range is unchanged; saved and area tables last with their images,
/// while the indirect table's owner determines its lifetime.
static inline InventoryItemRow* _inventoryGetRangeTable(const InventoryItemRange* range)
{
    InventoryItemRow* table;

    switch (range->tableId) {
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

s32 inventoryGetItemQuantity(const InventoryItemRange* range, s32 itemId)
{
    enum { INVENTORY_KEY_ITEM_FIRST = 0x100 };
    InventoryItemRow*       table;
    const InventoryItemRow* row;
    s32                     rowIndex;
    s32                     quantity;
    s32                     rowCount;
    s32                     firstRow;
    s32                     rowLimit;

    if (itemId >= INVENTORY_KEY_ITEM_FIRST) {
        return inventoryHasCollectedBit(itemId);
    }

    quantity = 0;
    table    = _inventoryGetRangeTable(range);
    rowIndex = 0;
    rowCount = range->rowCount;
    firstRow = range->firstRow;
    if (rowCount != 0) {
        rowLimit = rowCount;
        row      = gpItemRowAt(table, firstRow);
        do {
            if (row->itemId == itemId) {
                quantity += row->qty;
            }
            rowIndex++;
            row++;
        } while (rowIndex < rowLimit);
    }
    return quantity;
}

static void func_800BB7B4(Task* arg0)
{
    arg0->extra.tmd->flags = 0;
}

void itemSetIdentified(s32 itemId, s32 identified)
{
    McSaveData* save;
    s32         wordIndex;
    u32         bitMask;

    wordIndex = itemId / ITEM_IDENTIFICATION_BITS_PER_WORD;
    bitMask   = 1U << (itemId % ITEM_IDENTIFICATION_BITS_PER_WORD);
    if ((u32)itemId >= ITEM_IDENTIFICATION_ID_LIMIT) {
        return;
    }
    if (identified == 0) {
        save                                 = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        save->state.itemSeenBits[wordIndex] &= ~bitMask;
        return;
    }
    save                                 = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.itemSeenBits[wordIndex] |= bitMask;
}

static void Gp_ApplyBit2List(AreaObjectRoom* table, u32* dest)
{
    _gpApplyBit2List(table, dest);
}

void Gp_SetBit2Flag(s32 arg0, u8 arg1, s32 arg2)
{
    s32  shift;
    u32  mask;
    u32* p;

    shift = (arg0 & 0xF) * 2;
    mask  = 3 << shift;
    p     = &Gp_Bit2Banks[arg2].objectStates[arg0 >> 4];
    *p   &= ~mask;
    mask  = arg1 << shift;
    *p   |= mask;
}

s32 equipmentGetWeaponLoadCapacity(s32 weaponItemId, s32 loadSelection)
{
    s32 capacity;
    s32 weaponIndex;

    weaponIndex = weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST;
    capacity    = 0;
    if ((u32)weaponIndex < ARRAY_SIZE(Gp_RelatedQty0.rows)) {
        if (loadSelection == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
            capacity = Gp_RelatedQty0.rows[weaponIndex].capacity;
        } else {
            capacity = Gp_RelatedQty1.rows[weaponIndex].capacity;
        }
    }
    return capacity;
}

static s32 Gp_GetBit2Flag(GameLocationKey* arg0, s32 arg1)
{
    return _gpReadBit2Flag(Gp_Bit2Banks[arg0->stage].objectStates, arg1);
}

void Gp_SavePlayerPos(void)
{
    GfxCoord*     coord;
    PlayerPos*    savedPos;
    s32           angle;
    s32           storedX;
    PlayerStatus* cfg;
    McSaveData*   save;

    // Capture the root transform with each coordinate narrowed to 16 bits.
    coord         = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    storedX       = (u16)coord->coord.t[0];
    savedPos      = &gPlayerStatus.pos;
    savedPos->x   = storedX;
    savedPos->y   = coord->coord.t[1];
    savedPos->z   = coord->coord.t[2];
    angle         = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    savedPos->yaw = angle;
    if ((s16)angle >= PLAYER_YAW_HALF_TURN + 1) {
        savedPos->yaw = angle - PLAYER_YAW_FULL_TURN;
    } else if ((s16)angle < -PLAYER_YAW_HALF_TURN) {
        savedPos->yaw = angle + PLAYER_YAW_FULL_TURN;
    }
    cfg                   = &gPlayerStatus;
    save                  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.playerExp = cfg->exp;
    save->state.playerBp  = cfg->bp;
}

static Enemy* Gp_SpawnAtPlace(AreaObjectSpawn* spawn, AreaObjectPlace* place)
{
    Enemy*     enemy;
    Task*      task;
    TmdObject* extra;
    GfxCoord*  coord;

    enemy = enemySpawnFromTable(&spawn->taskDesc, 0, spawn->kind, NULL);
    if (enemy != NULL) {
        task = enemy->task;
        if (task->bodyKind != TASK_BODY_NONE) {
            extra               = task->extra.tmd;
            coord               = extra->coords;
            enemy->placeKey     = place->flagIndex | (place->placeKeyHigh << ENEMY_PLACE_STAGE_SHIFT);
            enemy->workType     = place->kind;
            coord->coord.t[0]   = place->x;
            coord->coord.t[1]   = place->y;
            coord->coord.t[2]   = place->z;
            coord->param.rot.vy = place->yaw;
            if (coord->param.rot.vy != 0) {
                gfxRotMatrixY(&coord->coord, (s16)place->yaw, 1);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    return enemy;
}

static void func_800BBB54(Task* arg0)
{
    TmdObject* extra;

    extra = arg0->extra.tmd;
    if (arg0->state == 0) {
        extra->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_FLAGGED_PASS);
        arg0->state += 1;
    }
    if (arg0->state == 1) {
        AreaObjectStage* banks;
        u32*             p;
        u32*             indexed;
        GameSession*     sess;
        s32              id;
        s32              shift;
        u32              word;

        sess    = gGameSession;
        banks   = Gp_Bit2Banks;
        id      = (u8)((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        p       = banks[sess->location.loc.stage].objectStates;
        indexed = p + (id >> 4);
        shift   = (id & 0xF) * 2;
        word    = *indexed;
        if (((word & (3 << shift)) >> shift) == 2) {
            extra->flags &= (u16)~TMD_OBJECT_FLAGGED_PASS;
            taskCallExit(arg0);
        }
    }
}

void Gp_WaitItemFlag2(Task* arg0)
{
    TmdObject* extra;

    extra = arg0->extra.tmd;
    if (arg0->state == 0) {
        extra->flags = TMD_OBJECT_FLAGGED_PASS;
        arg0->state += 1;
    }
    if (arg0->state == 1) {
        s32  id;
        s32  stage;
        u32* p;
        s32  shift;

        id    = (u8)((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        stage = gGameSession->location.loc.stage;
        p     = &Gp_Bit2Banks[stage].objectStates[id >> 4];
        shift = (id & 0xF) * 2;
        if (((*p & (3 << shift)) >> shift) == 2) {
            extra->flags &= (u16)~TMD_OBJECT_FLAGGED_PASS;
            taskCallExit(arg0);
        }
    }
}

s16 inventoryFindStackQuantity(const InventoryItemRow* table, const InventoryItemRange* range, s32* rowIndex, s32 itemId)
{
    s32 currentRowIndex;
    s32 quantity;
    s32 nextRowIndex;

    currentRowIndex = *rowIndex;
    quantity        = 0;
    while (currentRowIndex < range->firstRow + range->rowCount) {
        if (table[currentRowIndex].itemId == itemId) {
            quantity = table[currentRowIndex].qty;
            break;
        }
        nextRowIndex    = currentRowIndex + 1;
        *rowIndex       = nextRowIndex;
        currentRowIndex = nextRowIndex;
    }
    return quantity;
}

s32 equipmentFindNextCarriedWeaponSupply(s32 firstSupplyIndex)
{
    const InventoryItemRange*    range;
    s32                          supplyIndex;
    const EquipmentWeaponSupply* supply;

    range = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    if ((u32)firstSupplyIndex >= EQUIPMENT_WEAPON_SUPPLY_COUNT) {
        return EQUIPMENT_WEAPON_SUPPLY_NOT_FOUND;
    }
    for (supplyIndex = firstSupplyIndex; supplyIndex < EQUIPMENT_WEAPON_SUPPLY_COUNT; supplyIndex++) {
        supply = &Gp_ItemMaps[supplyIndex];
        if (inventoryGetItemQuantity(range, supply->weaponItemId)) {
            return supplyIndex;
        }
    }
    return EQUIPMENT_WEAPON_SUPPLY_NOT_FOUND;
}

const EquipmentWeaponSupply* equipmentGetWeaponSupply(s32 supplyIndex)
{
    return &Gp_ItemMaps[supplyIndex];
}

s32 Gp_HasMappedItem(void)
{
    s32                    found;
    InventoryItemRange*    scan;
    s32                    i;
    EquipmentWeaponSupply* supply;

    found = 0;
    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    for (i = 0, supply = Gp_ItemMaps; i < EQUIPMENT_WEAPON_SUPPLY_COUNT; i++) {
        if (inventoryGetItemQuantity(scan, supply->weaponItemId)) {
            found = 1;
            break;
        }
        supply++;
    }
    return found;
}

static void Gp_ResetAuxSlots(void)
{
    EquipmentWeaponLoad* p;
    s32                  i;

    p = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems;
    for (i = 0; i < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems); i++) {
        p->primaryItemId   = INVENTORY_ITEM_NONE;
        p->primaryQty      = 0;
        p->secondaryItemId = EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE;
        p->secondaryQty    = 0;
        // The M4A1 grenade launcher has a reloadable secondary slot.
        if (i == 0x9A - EQUIPMENT_WEAPON_ITEM_FIRST) {
            p->secondaryItemId = INVENTORY_ITEM_NONE;
            p->secondaryQty    = 0;
        }
        p->field_4 = 0;
        p++;
    }
    equipmentInitializeWeaponSupplies();
}

static s32 Gp_SumItemQty(s32 arg0)
{
    InventoryItemRange query;

    memset(&query, 0, sizeof(query));
    query.rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS;
    return inventoryGetItemQuantity(&query, arg0);
}

static void Gp_SetPlayerScan(s32 arg0)
{
    McSaveData* p;

    p                              = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    p->state.carriedItems.firstRow = 0;
    p->state.carriedItems.rowCount = arg0;
    p->state.carriedItems.tableId  = INVENTORY_ITEM_TABLE_SAVED;
}

void Gp_SyncHeldRelated(void)
{
    PlayerStatus* p;
    s32           weaponItemId;
    u8            primaryItemId;

    p = &gPlayerStatus;
    if (p->weapon == PLAYER_STATUS_EQUIPMENT_NONE) {
        p->weaponSlotItem = PLAYER_STATUS_EQUIPMENT_NONE;
    } else {
        weaponItemId  = p->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
        primaryItemId = _equipmentGetWeaponLoad(weaponItemId)->primaryItemId;
        if (primaryItemId == INVENTORY_ITEM_NONE) {
            p->weaponSlotItem = PLAYER_STATUS_EQUIPMENT_NONE;
        } else {
            p->weaponSlotItem = primaryItemId + 0x61;
        }
    }
    func_801061F0();
}

static void Gp_InitItemSeenBits(void)
{
    McSaveData*     p;
    const ItemDesc* desc;
    const u8*       str;
    s32             i;
    s32             count;

    p = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    for (i = 0x5F; i >= 0; i--) {
        p->state.itemSeenBits[i] = 0;
    }

    i = 0;
    do {
        count = 3;
        if (i < 0x100) {
            desc = &Gp_ItemDescs[i];
        } else {
            desc = &Gp_KeyItemDescs[(i)-0x100];
        }
        str = desc->textFields;
        while (count > 0) {
            if (*str == '\0' || *str == '\n') {
                count--;
            }
            str++;
        }
        if (*str == '\n') {
            itemSetIdentified(i, 1);
        }
        i++;
    } while (i < 0x180);
}

bool itemIsIdentified(s32 itemId)
{
    const McSaveData* save;
    s32               wordIndex;
    u32               bitMask;
    u32               identifiedBit;

    wordIndex = itemId / ITEM_IDENTIFICATION_BITS_PER_WORD;
    bitMask   = 1U << (itemId % ITEM_IDENTIFICATION_BITS_PER_WORD);
    if ((u32)itemId >= ITEM_IDENTIFICATION_ID_LIMIT) {
        return true;
    }
    save          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    identifiedBit = save->state.itemSeenBits[wordIndex] & bitMask;
    return identifiedBit != 0;
}

void equipmentRecalculateMaxHp(void)
{
    PlayerStatus*              status;
    const McSaveData*          save;
    const PlayerModeBaseStats* modeStats;
    u16                        maximumHp;

    // Preserve 16-bit narrowing before the signed maximum is capped.
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

void equipmentRestoreHpMp(void)
{
    PlayerStatus* status;

    status     = &gPlayerStatus;
    status->hp = status->hpMax;
    status->mp = status->mpMax;
}

s32 inventoryGetRangeCapacity(const InventoryItemRange* range)
{
    return range->rowCount;
}

s32 inventoryGetItemSortKey(s32 itemId)
{
    enum {
        INVENTORY_SORT_ARMOR_ITEM_FIRST = 0x60,
        INVENTORY_SORT_EMPTY_KEY        = 0x1000,
        INVENTORY_SORT_FALLBACK_OFFSET  = 0x100
    };
    s32 key;

    key = 0;
    if (itemId == INVENTORY_ITEM_NONE) {
        key = INVENTORY_SORT_EMPTY_KEY;
    } else if (itemId > 0 && itemId < INVENTORY_SORT_ARMOR_ITEM_FIRST) {
        key = Gp_ItemSortKey0[itemId];
    } else if (itemId >= INVENTORY_SORT_ARMOR_ITEM_FIRST && itemId < EQUIPMENT_WEAPON_ITEM_FIRST) {
        key = Gp_ItemSortKey60[itemId - INVENTORY_SORT_ARMOR_ITEM_FIRST];
    } else if (itemId >= EQUIPMENT_WEAPON_ITEM_FIRST && itemId < INVENTORY_CONSUMABLE_ITEM_FIRST) {
        key = Gp_ItemSortKey80[itemId - EQUIPMENT_WEAPON_ITEM_FIRST];
    } else if (itemId >= INVENTORY_CONSUMABLE_ITEM_FIRST && itemId < (INVENTORY_CONSUMABLE_ITEM_FIRST + INVENTORY_CONSUMABLE_ITEM_COUNT)) {
        key = Gp_ItemSortKeyA0[itemId - INVENTORY_CONSUMABLE_ITEM_FIRST];
    }
    if (key == 0) {
        key = itemId + INVENTORY_SORT_FALLBACK_OFFSET;
    }
    return key;
}

void inventoryResetIceBagTimer(void)
{
    gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime;
}

static s16 Gp_PlayTimeDelta(void)
{
    u16* markMinutes;

    markMinutes = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark;
    return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime - *markMinutes;
}

/// Converts the held Ice Bag when the signed elapsed-minute timer expires.
///
/// Returns whether the collection bits changed; the timer marker is retained.
static inline s32 _inventoryMeltIceBagIfExpired(void)
{
    s32        melted;
    const u16* markMinutes;

    melted = 0;
    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) != 0) {
        markMinutes = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark;
        if ((s16)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime - *markMinutes) >= INVENTORY_ICE_BAG_MELT_MINUTES) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG);
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_BAG_OF_WATER);
            melted = 1;
        }
    }
    return melted;
}

s32 inventoryMeltIceBagIfExpired(void)
{
    return _inventoryMeltIceBagIfExpired();
}

void inventoryUpdateIceBag(void)
{
    _inventoryMeltIceBagIfExpired();
}

s32 equipmentGetArmorAttachmentSlotCount(s32 armorItemId)
{
    return _gpGetModLevel(armorItemId);
}

void Gp_TickBoostPanel(Task* arg0)
{
    UiPanel* panel;

    panel = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        uiSetPanelContentSize(panel, 0xB0, 0x2F);
        panel->bounds.rect.y = -0xC;
        panel->bounds.rect.x = -panel->bounds.rect.w / 2;
        arg0->state++;
    }
    itemMenuDrawPlayerStats(panel, 0);
}

s32 Gp_HasStockedItem(s32 arg0)
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

void Gp_ResetScanDefault(void)
{
    McSaveData* p;

    p                     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    p->state.carriedItems = Gp_DefaultScan;
}

void func_800BC4BC(void)
{
    gPlayerStatus.resourceVariant = 1;
    Gp_InitModeEquip();
}

void func_800BC4E4(void)
{
    gPlayerStatus.resourceVariant = 2;
    Gp_InitModeEquip();
}
