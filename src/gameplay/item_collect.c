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

/// 0xFFFF-terminated item-id list walked by `Gp_NthCollectedId`. Each id's
/// low 7 bits index a collected-item bit in `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits`.
extern u16 Gp_CollectedIds[];

/// Unreferenced nonzero halfword after the collected-item terminator.
extern u16 D_80114B32;

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

/* Count item `id` in saved rows 0..254 through a cleared range. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, Gp_SumScanQty(&(scan), (id)))

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

s32 Gp_NthCollectedId(s32 arg0, s32 arg1)
{
    u16* p;
    s32  ret;
    s32* bits;
    s32  bit;
    s32  item;
    s32  one;

    p   = Gp_CollectedIds;
    ret = 0;
    if (*p != 0xFFFF) {
        do {
            item  = *p;
            bits  = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.collectedBits;
            one   = 1;
            bit   = item & 0x7F;
            bits += bit / 32;
            bit  %= 32;
            if (*bits & (one << bit)) {
                arg0--;
                p++;
                if (arg0 >= 0) {
                    continue;
                }
                ret = item;
                break;
            }
            p++;
        } while (*p != 0xFFFF);
    }
    return ret;
}

s32 Gp_SumScanQty(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow* tmp;
    InventoryItemRow* table;
    InventoryItemRow* row;
    s32               i;
    s32               acc;
    s32               count;
    s32               start;
    s32               limit;

    if (arg1 >= 0x100) {
        return inventoryHasCollectedBit(arg1);
    }

    acc = 0;
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
    if (count != 0) {
        limit = count;

        row = gpItemRowAt(table, start);
        do {
            if (row->itemId == arg1) {
                acc += row->qty;
            }
            i++;
            row++;
        } while (i < limit);
    }
    return acc;
}

static void func_800BB7B4(Task* arg0)
{
    arg0->extra.tmd->flags = 0;
}

void Gp_SetItemSeenBit(s32 arg0, s32 arg1)
{
    McSaveData* p;
    s32         word;
    s32         bit;

    word = arg0 / 32;
    bit  = 1 << (arg0 % 32);
    if ((u32)arg0 >= 0x180) {
        return;
    }
    if (arg1 == 0) {
        p                            = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        p->state.itemSeenBits[word] &= ~bit;
        return;
    }
    p                            = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    p->state.itemSeenBits[word] |= bit;
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

s32 Gp_GetRelatedQty(s32 arg0, s32 arg1)
{
    s32 ret;

    arg0 -= EQUIPMENT_WEAPON_ITEM_FIRST;
    ret   = 0;
    if ((u32)arg0 < ARRAY_SIZE(Gp_RelatedQty0.rows)) {
        if (arg1 == 0) {
            ret = Gp_RelatedQty0.rows[arg0].capacity;
        } else {
            ret = Gp_RelatedQty1.rows[arg0].capacity;
        }
    }
    return ret;
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

    enemy = Gp_SpawnEnemyFromTable(&spawn->taskDesc, 0, spawn->kind, NULL);
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

s32 Gp_FindScanQty(InventoryItemRow* arg0, InventoryItemRange* arg1, s32* arg2, s32 arg3)
{
    s32 i;
    s32 ret;
    s32 next;

    i   = *arg2;
    ret = 0;
    while (i < arg1->firstRow + arg1->rowCount) {
        if (arg0[i].itemId == arg3) {
            ret = arg0[i].qty;
            break;
        }
        next  = i + 1;
        *arg2 = next;
        i     = next;
    }
    return (s16)ret;
}

s32 Gp_NextMappedSlot(s32 arg0)
{
    InventoryItemRange*    scan;
    s32                    i;
    EquipmentWeaponSupply* supply;

    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    if ((u32)arg0 >= EQUIPMENT_WEAPON_SUPPLY_COUNT) {
        return -1;
    }
    for (i = arg0; i < EQUIPMENT_WEAPON_SUPPLY_COUNT; i++) {
        supply = &Gp_ItemMaps[i];
        if (Gp_SumScanQty(scan, supply->weaponItemId)) {
            return i;
        }
    }
    return -1;
}

EquipmentWeaponSupply* Gp_GetItemMap(s32 arg0)
{
    return &Gp_ItemMaps[arg0];
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
        if (Gp_SumScanQty(scan, supply->weaponItemId)) {
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
    Gp_ApplyItemMap();
}

static s32 Gp_SumItemQty(s32 arg0)
{
    InventoryItemRange query;

    memset(&query, 0, sizeof(query));
    query.rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS;
    return Gp_SumScanQty(&query, arg0);
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
            Gp_SetItemSeenBit(i, 1);
        }
        i++;
    } while (i < 0x180);
}

s32 Gp_HasItemSeenBit(s32 arg0)
{
    McSaveData* p;
    s32         word;
    s32         bit;
    s32         val;

    word = arg0 / 32;
    bit  = 1 << (arg0 % 32);
    if ((u32)arg0 >= 0x180) {
        return 1;
    }
    p   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    val = p->state.itemSeenBits[word] & bit;
    return val != 0;
}

void Gp_RecalcMaxHp(void)
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

void Gp_FillHpMp(void)
{
    PlayerStatus* p;

    p     = &gPlayerStatus;
    p->hp = p->hpMax;
    p->mp = p->mpMax;
}

s32 Gp_GetScanCount(InventoryItemRange* scan)
{
    return scan->rowCount;
}

s32 Gp_ItemSortKey(s32 id)
{
    s32 key;

    key = 0;
    if (id == 0) {
        key = 0x1000;
    } else if (id > 0 && id < 0x60) {
        key = Gp_ItemSortKey0[id];
    } else if (id >= 0x60 && id < 0x80) {
        key = Gp_ItemSortKey60[id - 0x60];
    } else if (id >= 0x80 && id < 0xA0) {
        key = Gp_ItemSortKey80[id - 0x80];
    } else if (id >= 0xA0 && id < 0xC0) {
        key = Gp_ItemSortKeyA0[id - 0xA0];
    }
    if (key == 0) {
        key = id + 0x100;
    }
    return key;
}

void Gp_MarkPlayTime(void)
{
    gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime;
}

static s16 Gp_PlayTimeDelta(void)
{
    u16* markMinutes;

    markMinutes = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark;
    return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime - *markMinutes;
}

s32 Gp_AgeFlag119(void)
{
    s32  ret;
    u16* markMinutes;

    ret = 0;
    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) != 0) {
        markMinutes = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark;
        if ((s16)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime - *markMinutes) >= 2) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG);
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_BAG_OF_WATER);
            ret = 1;
        }
    }
    return ret;
}

void Gp_AgeFlag119Void(void)
{
    u16* markMinutes;

    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) != 0) {
        markMinutes = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.state.playTimeMark;
        if ((s16)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime - *markMinutes) >= 2) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG);
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_BAG_OF_WATER);
        }
    }
}

s32 Gp_GetModLevel(s32 arg0)
{
    return _gpGetModLevel(arg0);
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
    Gp_DrawHpMpStats(panel, 0);
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
