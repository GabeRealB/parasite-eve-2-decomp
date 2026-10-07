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

/// Packing of sixteen two-bit area object states into each saved word.
enum {
    AREA_OBJECT_STATE_BITS       = 2,
    AREA_OBJECT_STATE_INDEX_MASK = 0xF,
    AREA_OBJECT_STATE_WORD_SHIFT = 4
};

static inline s32 _gpGetModLevel(s32 item);

static inline void _gpApplyBit2List(AreaObjectRoom* table, u32* dest);

static inline s32 _areaReadObjectState(const u32* objectStates, s32 objectId);

static inline void _playerCaptureRootPose(void);

static inline void _areaApplyObjectPlacement(Enemy* enemy, const AreaObjectPlace* place);

static void func_800BB7B4(Task* arg0);

static void _areaSeedRoomObjectStates(AreaObjectRoom* rooms, u32* objectStates);

static s32 _areaGetObjectState(const GameLocationKey* location, s32 objectId);

static Enemy* _areaSpawnObjectAtPlace(AreaObjectSpawn* spawn, const AreaObjectPlace* place);

static void func_800BBB54(Task* arg0);

static void _equipmentInitializeWeaponLoads(void);

static s32 _inventoryGetSavedItemQuantity(s32 itemId);

static void _inventorySetCarriedSavedRange(s32 rowCount);

static void _itemInitializeIdentification(void);

static s16 _inventoryGetIceBagElapsedMinutes(void);

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
/// Reads one low-pair-first two-bit state from borrowed, readable saved words.
///
/// `objectId` must be nonnegative and select a pair within the supplied bank.
static inline s32 _areaReadObjectState(const u32* objectStates, s32 objectId)
{
    u32 word;
    s32 stateShift;

    objectStates += objectId >> AREA_OBJECT_STATE_WORD_SHIFT;
    stateShift    = (objectId & AREA_OBJECT_STATE_INDEX_MASK) * AREA_OBJECT_STATE_BITS;
    word          = *objectStates;
    word         &= AREA_OBJECT_PLACE_STATE_MASK << stateShift;
    word        >>= stateShift;
    return word;
}

/// Captures the live player root in the resident pose used by save preparation.
///
/// Requires the player task's model root. XYZ narrow to signed halfwords;
/// yaw uses 4096 units per turn with both signed half-turn endpoints retained.
static inline void _playerCaptureRootPose(void)
{
    const GfxCoord* root;
    PlayerPos*      savedPose;
    s32             yaw;
    s32             storedX;

    root           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    storedX        = (u16)root->coord.t[0];
    savedPose      = &gPlayerStatus.pos;
    savedPose->x   = storedX;
    savedPose->y   = root->coord.t[1];
    savedPose->z   = root->coord.t[2];
    yaw            = ratan2(root->coord.m[0][2], root->coord.m[2][2]);
    savedPose->yaw = yaw;
    if ((s16)yaw >= PLAYER_YAW_HALF_TURN + 1) {
        savedPose->yaw = yaw - PLAYER_YAW_FULL_TURN;
    } else if ((s16)yaw < -PLAYER_YAW_HALF_TURN) {
        savedPose->yaw = yaw + PLAYER_YAW_FULL_TURN;
    }
}

/// Applies a room placement to a live enemy whose task has a model body.
///
/// A zero yaw preserves the model's initial rotation. Coordinates use game
/// units; nonzero yaw replaces the rotation and invalidates its composition.
static inline void _areaApplyObjectPlacement(Enemy* enemy, const AreaObjectPlace* place)
{
    TmdObject* model;
    GfxCoord*  root;

    model              = enemy->task->extra.tmd;
    root               = model->coords;
    enemy->placeKey    = place->flagIndex | (place->placeKeyHigh << ENEMY_PLACE_STAGE_SHIFT);
    enemy->workType    = place->kind;
    root->coord.t[0]   = place->x;
    root->coord.t[1]   = place->y;
    root->coord.t[2]   = place->z;
    root->param.rot.vy = place->yaw;
    if (root->param.rot.vy != 0) {
        gfxRotMatrixY(&root->coord, (s16)place->yaw, GRAPHICS_ROTATION_REPLACE);
    }
    root->composeStamp = GRAPHICS_COORD_DIRTY;
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

/// Seeds a bank from every room's placed-object initial states.
///
/// NULL rooms is a no-op. Room and place lists require their respective end
/// markers. Every flag index must fit the writable bank; other pairs remain.
/// The tables are borrowed unchanged and no pointers are retained.
static void _areaSeedRoomObjectStates(AreaObjectRoom* rooms, u32* objectStates)
{
    _gpApplyBit2List(rooms, objectStates);
}

void areaSetObjectState(s32 objectId, u8 state, s32 stageId)
{
    s32  stateShift;
    u32  stateMask;
    u32* stateWord;

    stateShift  = (objectId & AREA_OBJECT_STATE_INDEX_MASK) * AREA_OBJECT_STATE_BITS;
    stateMask   = AREA_OBJECT_PLACE_STATE_MASK << stateShift;
    stateWord   = &Gp_Bit2Banks[stageId].objectStates[objectId >> AREA_OBJECT_STATE_WORD_SHIFT];
    *stateWord &= ~stateMask;
    stateMask   = state << stateShift;
    *stateWord |= stateMask;
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

/// Returns the saved two-bit object state in a supplied location's stage.
///
/// Requires stage 1..5 and object id 0..63. Area, view and variant are ignored;
/// Night Dryfield shares daytime Dryfield's words. The location is borrowed.
static s32 _areaGetObjectState(const GameLocationKey* location, s32 objectId)
{
    return _areaReadObjectState(Gp_Bit2Banks[location->stage].objectStates, objectId);
}

void playerCaptureSaveState(void)
{
    const PlayerStatus* player;
    McSaveData*         save;

    _playerCaptureRootPose();
    player                = &gPlayerStatus;
    save                  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.playerExp = player->exp;
    save->state.playerBp  = player->bp;
}

/// Spawns one descriptor at a room placement, returning its owned enemy or NULL.
///
/// Borrows both records during the call; no search or kind comparison is made.
/// A bodyless task keeps the spawn defaults, including its place key/work type.
/// Other bodies must provide a model root and receive the placement transform.
static Enemy* _areaSpawnObjectAtPlace(AreaObjectSpawn* spawn, const AreaObjectPlace* place)
{
    Enemy* enemy;
    Task*  task;

    enemy = enemySpawnFromTable(&spawn->taskDesc, 0, spawn->kind, NULL);
    if (enemy != NULL) {
        task = enemy->task;
        if (task->bodyKind != TASK_BODY_NONE) {
            _areaApplyObjectPlacement(enemy, place);
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

s32 equipmentHasCarriedWeaponSupply(void)
{
    s32                          found;
    const InventoryItemRange*    range;
    s32                          supplyIndex;
    const EquipmentWeaponSupply* supply;

    found = 0;
    range = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    for (supplyIndex = 0, supply = Gp_ItemMaps; supplyIndex < EQUIPMENT_WEAPON_SUPPLY_COUNT; supplyIndex++) {
        if (inventoryGetItemQuantity(range, supply->weaponItemId)) {
            found = 1;
            break;
        }
        supply++;
    }
    return found;
}

/// Resets all saved weapon loads, then fills each built-in supply to capacity.
///
/// Removable primaries start empty; only the M4A1 grenade secondary is reloadable.
/// Every other secondary starts unavailable until the supply catalogue is applied.
static void _equipmentInitializeWeaponLoads(void)
{
    enum { EQUIPMENT_ITEM_M4A1_GRENADE = 0x9A };
    EquipmentWeaponLoad* load;
    s32                  weaponIndex;

    load = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems;
    for (weaponIndex = 0; weaponIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems); weaponIndex++) {
        load->primaryItemId   = INVENTORY_ITEM_NONE;
        load->primaryQty      = 0;
        load->secondaryItemId = EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE;
        load->secondaryQty    = 0;
        // The M4A1 grenade launcher has a reloadable secondary slot.
        if (weaponIndex == EQUIPMENT_ITEM_M4A1_GRENADE - EQUIPMENT_WEAPON_ITEM_FIRST) {
            load->secondaryItemId = INVENTORY_ITEM_NONE;
            load->secondaryQty    = 0;
        }
        load->field_4 = 0;
        load++;
    }
    equipmentInitializeWeaponSupplies();
}

/// Sums an item's quantity in saved rows 0..254, or queries a key collection bit.
///
/// The 255-row range is the representable maximum and excludes saved row 255.
/// Ids at least 0x100 instead select the collection bit by their low seven bits.
static s32 _inventoryGetSavedItemQuantity(s32 itemId)
{
    InventoryItemRange query;

    memset(&query, 0, sizeof(query));
    query.rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS;
    return inventoryGetItemQuantity(&query, itemId);
}

/// Selects saved rows starting at zero as the live carried range.
///
/// `rowCount` narrows to an unsigned byte (0..255); rows are not cleared.
/// The range's unproven fourth byte is retained.
static void _inventorySetCarriedSavedRange(s32 rowCount)
{
    McSaveData* save;

    save                              = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.carriedItems.firstRow = 0;
    save->state.carriedItems.rowCount = rowCount;
    save->state.carriedItems.tableId  = INVENTORY_ITEM_TABLE_SAVED;
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
    playerActorUpdateWeaponCollisionKey();
}

/// Clears saved identification storage and identifies items without an unknown name.
///
/// Scans ids 0..383. Text must contain three NUL/newline-terminated identified
/// fields; a newline immediately after those fields marks an absent unknown name.
/// The ordinary-table id mapping beyond its declared extent remains unproven.
static void _itemInitializeIdentification(void)
{
    enum {
        ITEM_IDENTIFICATION_KEY_ITEM_FIRST = 0x100,
        ITEM_IDENTIFIED_TEXT_FIELD_COUNT   = 3
    };
    McSaveData*     save;
    const ItemDesc* descriptor;
    const u8*       text;
    s32             itemId;
    s32             fieldsRemaining;

    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    // Reset the full stored array before reusing the index for catalogue ids.
    for (itemId = ARRAY_SIZE(save->state.itemSeenBits) - 1; itemId >= 0; itemId--) {
        save->state.itemSeenBits[itemId] = 0;
    }

    itemId = 0;
    do {
        fieldsRemaining = ITEM_IDENTIFIED_TEXT_FIELD_COUNT;
        if (itemId < ITEM_IDENTIFICATION_KEY_ITEM_FIRST) {
            descriptor = &Gp_ItemDescs[itemId];
        } else {
            descriptor = &Gp_KeyItemDescs[itemId - ITEM_IDENTIFICATION_KEY_ITEM_FIRST];
        }
        text = descriptor->textFields;
        while (fieldsRemaining > 0) {
            if (*text == '\0' || *text == '\n') {
                fieldsRemaining--;
            }
            text++;
        }
        if (*text == '\n') {
            itemSetIdentified(itemId, 1);
        }
        itemId++;
    } while (itemId < ITEM_IDENTIFICATION_ID_LIMIT);
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

/// Returns saved whole minutes since the Ice Bag marker, narrowed to signed 16 bits.
///
/// This retains the saved clock's wrap/reset behavior; it is not a monotonic timer.
static s16 _inventoryGetIceBagElapsedMinutes(void)
{
    const u16* markMinutes;

    markMinutes = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark;
    return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime - *markMinutes;
}

/// Converts the held Ice Bag when the signed elapsed-minute timer expires.
///
/// Returns 1 only when Ice Bag becomes Bag of Water, otherwise 0. The difference
/// of saved whole minutes narrows to signed 16 bits before the two-minute test;
/// the marker is retained, including across saved-clock wrap or reset.
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

void itemMenuPlayerStatsPanelTask(Task* task)
{
    enum {
        ITEM_MENU_STATS_PANEL_INITIAL        = 0,
        ITEM_MENU_STATS_PANEL_CONTENT_WIDTH  = 176,
        ITEM_MENU_STATS_PANEL_CONTENT_HEIGHT = 47,
        ITEM_MENU_STATS_PANEL_Y              = -12
    };
    UiObject* object;

    object = task->spawnArg2.pointer;
    if (task->state == ITEM_MENU_STATS_PANEL_INITIAL) {
        uiSetPanelContentSize(&object->panel, ITEM_MENU_STATS_PANEL_CONTENT_WIDTH, ITEM_MENU_STATS_PANEL_CONTENT_HEIGHT);
        object->panel.bounds.rect.y = ITEM_MENU_STATS_PANEL_Y;
        object->panel.bounds.rect.x = -object->panel.bounds.rect.w / 2;
        task->state++;
    }
    itemMenuDrawPlayerStats(&object->panel, 0);
}

s32 inventoryHasAttachedItem(s32 itemId)
{
    const InventoryItemRange* range;
    const InventoryItemRow*   row;
    s32                       rowIndex;
    s32                       hasAttachedItem;
    s32                       rowCount;

    range           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    hasAttachedItem = 0;
    row             = _inventoryGetRangeTable(range);
    rowIndex        = 0;
    row            += range->firstRow;
    rowCount        = range->rowCount;
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

void inventoryResetCarriedRange(void)
{
    McSaveData* save;

    save                     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.carriedItems = Gp_DefaultScan;
}

void func_800BC4BC(void)
{
    gPlayerStatus.resourceVariant = 1;
    equipmentEnsureM93rEquipped();
}

void func_800BC4E4(void)
{
    gPlayerStatus.resourceVariant = 2;
    equipmentEnsureM93rEquipped();
}
