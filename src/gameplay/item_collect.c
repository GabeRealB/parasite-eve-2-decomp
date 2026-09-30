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
/// low 7 bits index a collected-item bit in `Mc_SaveData[0].state.collectedBits`.
extern u16 Gp_CollectedIds[];

/// Unreferenced nonzero halfword after the collected-item terminator.
extern u16 D_80114B32;

static inline s32 _gpGetModLevel(s32 item);

static inline void _gpApplyBit2List(GpBit2List* table, u32* dest);

static inline s32 _gpReadBit2Flag(u32* p, s32 index);

static void func_800BB7B4(Task* arg0);

static void Gp_ApplyBit2List(GpBit2List* table, u32* dest);

static s32 Gp_GetBit2Flag(GameLocationKey* arg0, s32 arg1);

static GpEnemy* Gp_SpawnAtPlace(GpEnemyDesc* arg0, GpBit2Rec* arg1);

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
    GpItemAttr* p;

    idx = item - 0x60;
    ret = 0;
    if ((u32)idx < 0x20) {
        p    = &Gp_ModStatAttrs[(item)-0x60];
        ret  = p->field_5;
        ret += Mc_SaveData[0].state.itemLevelBonus[idx];
        if (ret >= 0xB) {
            ret = 0xA;
        }
    }
    return ret;
}
static inline void _gpApplyBit2List(GpBit2List* table, u32* dest)
{
    GpBit2Rec* rec;
    u32*       p;
    u32        mask;

    if (table == NULL) {
        return;
    }
    rec = table->field_0.records;
    if (table->field_0.sentinel == -1) {
        return;
    }
    do {
        if (rec != NULL) {
            for (; rec->field_0 != 0xFFFF; rec++) {
                mask = 3 << ((rec->field_0 & 0xF) * 2);
                p    = &dest[rec->field_0 >> 4];
                *p  &= ~mask;
                mask = (rec->field_6 & 3) << ((rec->field_0 & 0xF) * 2);
                *p  |= mask;
            }
        }
        table++;
        rec = table->field_0.records;
    } while (table->field_0.sentinel != -1);
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
            bits  = Mc_SaveData[0].state.collectedBits;
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
    InventoryItemRow* rec;
    s32               i;
    s32               acc;
    s32               count;
    s32               start;
    s32               limit;

    if (arg1 >= 0x100) {
        return Gp_HasCollectedBit(arg1);
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
            tmp = Mc_SaveData[0].state.itemRows;
            break;
    }
    table = tmp;
    i     = 0;
    count = arg0->rowCount;
    start = arg0->firstRow;
    if (count != 0) {
        limit = count;

        rec = gpItemRowAt(table, start);
        do {
            if (rec->itemId == arg1) {
                acc += rec->qty;
            }
            i++;
            rec++;
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
        p                            = &Mc_SaveData[0];
        p->state.itemSeenBits[word] &= ~bit;
        return;
    }
    p                            = &Mc_SaveData[0];
    p->state.itemSeenBits[word] |= bit;
}

static void Gp_ApplyBit2List(GpBit2List* table, u32* dest)
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
    p     = &Gp_Bit2Banks[arg2].field_4[arg0 >> 4];
    *p   &= ~mask;
    mask  = arg1 << shift;
    *p   |= mask;
}

s32 Gp_GetRelatedQty(s32 arg0, s32 arg1)
{
    s32 ret;

    arg0 -= 0x80;
    ret   = 0;
    if ((u32)arg0 < 0x20) {
        if (arg1 == 0) {
            ret = Gp_RelatedQty0.rows[arg0].field_0;
        } else {
            ret = Gp_RelatedQty1.rows[arg0].field_0;
        }
    }
    return ret;
}

static s32 Gp_GetBit2Flag(GameLocationKey* arg0, s32 arg1)
{
    return _gpReadBit2Flag(Gp_Bit2Banks[arg0->stage].field_4, arg1);
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
    coord         = (gameGetPtrSlot(3))->extra.tmd->coords;
    storedX       = (u16)coord->coord.t[0];
    savedPos      = &Player_Status.pos;
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
    cfg                   = &Player_Status;
    save                  = &Mc_SaveData[0];
    save->state.playerExp = cfg->exp;
    save->state.playerBp  = cfg->bp;
}

static GpEnemy* Gp_SpawnAtPlace(GpEnemyDesc* arg0, GpBit2Rec* arg1)
{
    GpEnemy*   enemy;
    Task*      task;
    TmdObject* extra;
    GfxCoord*  coord;

    enemy = Gp_SpawnEnemyFromTable(&arg0->field_4, 0, arg0->field_0, NULL);
    if (enemy != NULL) {
        task = enemy->task;
        if (task->spawnType != 0) {
            extra               = task->extra.tmd;
            coord               = extra->coords;
            enemy->placeKey     = arg1->field_0 | (arg1->field_4 << 8);
            enemy->workType     = arg1->field_2;
            coord->coord.t[0]   = arg1->field_8;
            coord->coord.t[1]   = arg1->field_A;
            coord->coord.t[2]   = arg1->field_C;
            coord->param.rot.vy = arg1->field_E;
            if (coord->param.rot.vy != 0) {
                Gfx_RotMatrixY(&coord->coord, (s16)arg1->field_E, 1);
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
        extra->flags = (TMD_OBJECT_HIDDEN | TMD_OBJECT_FLAGGED_PASS);
        arg0->state += 1;
    }
    if (arg0->state == 1) {
        GpBit2Bank*  banks;
        u32*         p;
        u32*         indexed;
        GameSession* sess;
        s32          id;
        s32          shift;
        u32          word;

        sess    = gGameSession;
        banks   = Gp_Bit2Banks;
        id      = ((GpItemObj8*)arg0->spawnArg2.pointer)->field_8;
        p       = banks[sess->at4.loc.stage].field_4;
        indexed = p + (id >> 4);
        shift   = (id & 0xF) * 2;
        word    = *indexed;
        if (((word & (3 << shift)) >> shift) == 2) {
            extra->flags &= (u16)~TMD_OBJECT_FLAGGED_PASS;
            Task_CallExit(arg0);
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

        id    = ((GpItemObj8*)arg0->spawnArg2.pointer)->field_8;
        stage = gGameSession->at4.loc.stage;
        p     = &Gp_Bit2Banks[stage].field_4[id >> 4];
        shift = (id & 0xF) * 2;
        if (((*p & (3 << shift)) >> shift) == 2) {
            extra->flags &= (u16)~TMD_OBJECT_FLAGGED_PASS;
            Task_CallExit(arg0);
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
    if (i < arg1->firstRow + arg1->rowCount) {
    loop:
        if (arg0[i].itemId == arg3) {
            ret = arg0[i].qty;
        } else {
            next  = i + 1;
            *arg2 = next;
            i     = next;
            if (i < arg1->firstRow + arg1->rowCount) {
                goto loop;
            }
        }
    }
    return (s16)ret;
}

s32 Gp_NextMappedSlot(s32 arg0)
{
    InventoryItemRange* scan;
    s32                 i;
    GpItemMap*          p;

    scan = &Mc_SaveData[0].state.carriedItems;
    if ((u32)arg0 >= 8) {
        return -1;
    }
    for (i = arg0; i < 8; i++) {
        p = &Gp_ItemMaps[i];
        if (Gp_SumScanQty(scan, p->field_1)) {
            return i;
        }
    }
    return -1;
}

GpItemMap* Gp_GetItemMap(s32 arg0)
{
    return &Gp_ItemMaps[arg0];
}

s32 Gp_HasMappedItem(void)
{
    s32                 found;
    InventoryItemRange* scan;
    s32                 i;
    GpItemMap*          p;

    found = 0;
    scan  = &Mc_SaveData[0].state.carriedItems;
    for (i = 0, p = Gp_ItemMaps; i < 8; i++) {
        if (Gp_SumScanQty(scan, p->field_1)) {
            found = 1;
            break;
        }
        p++;
    }
    return found;
}

static void Gp_ResetAuxSlots(void)
{
    EquipmentWeaponLoad* p;
    s32                  i;

    p = Mc_SaveData[0].state.weaponItems;
    for (i = 0; i < (s32)ARRAY_SIZE(Mc_SaveData[0].state.weaponItems); i++) {
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

    p                              = &Mc_SaveData[0];
    p->state.carriedItems.firstRow = 0;
    p->state.carriedItems.rowCount = arg0;
    p->state.carriedItems.tableId  = INVENTORY_ITEM_TABLE_SAVED;
}

void Gp_SyncHeldRelated(void)
{
    PlayerStatus* p;
    s32           idx;
    u8            item;

    p = &Player_Status;
    if (p->weapon == 0) {
        p->weaponSlotItem = 0;
    } else {
        idx  = p->weapon + 0x7F;
        item = gpItemSlot(idx)->primaryItemId;
        if (item == 0) {
            p->weaponSlotItem = 0;
        } else {
            p->weaponSlotItem = item + 0x61;
        }
    }
    func_801061F0();
}

static void Gp_InitItemSeenBits(void)
{
    McSaveData* p;
    GpItemDesc* desc;
    u8*         str;
    s32         i;
    s32         count;

    p = &Mc_SaveData[0];
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
        str = desc->field_4;
        while (count > 0) {
            if (*str == 0 || *str == 0xA) {
                count--;
            }
            str++;
        }
        if (*str == 0xA) {
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
    p   = &Mc_SaveData[0];
    val = p->state.itemSeenBits[word] & bit;
    return val != 0;
}

void Gp_RecalcMaxHp(void)
{
    PlayerStatus* cfg;
    McSaveData*   save;
    GpStatRow*    table;
    u16           val;

    cfg        = &Player_Status;
    table      = Gp_StatRows;
    save       = &Mc_SaveData[0];
    val        = table[save->state.gameMode].base.half;
    cfg->hpMax = val;
    val       += save->state.hpBonus;
    cfg->hpMax = val;
    if (cfg->armor != 0) {
        val       += Gp_ModStatAttrs[cfg->armor - 1].field_4;
        cfg->hpMax = val;
    }
    if (cfg->hpMax >= 0xFB) {
        cfg->hpMax = 0xFA;
    }
    if (cfg->hp > cfg->hpMax) {
        cfg->hp = cfg->hpMax;
    }
}

void Gp_FillHpMp(void)
{
    PlayerStatus* p;

    p     = &Player_Status;
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
    GameFlag_NibbleBanks[0].payload.state.playTimeMark = Mc_SaveData[0].state.playTime;
}

static s16 Gp_PlayTimeDelta(void)
{
    u16* markMinutes;

    markMinutes = &GameFlag_NibbleBanks[0].payload.state.playTimeMark;
    return Mc_SaveData[0].state.playTime - *markMinutes;
}

s32 Gp_AgeFlag119(void)
{
    s32  ret;
    u16* markMinutes;

    ret = 0;
    if (Gp_HasCollectedBit(0x119) != 0) {
        markMinutes = &GameFlag_NibbleBanks[0].payload.state.playTimeMark;
        if ((s16)(Mc_SaveData[0].state.playTime - *markMinutes) >= 2) {
            Gp_ClearCollectedBit(0x119);
            Gp_SetCollectedBit(0x11A);
            ret = 1;
        }
    }
    return ret;
}

void Gp_AgeFlag119Void(void)
{
    u16* markMinutes;

    if (Gp_HasCollectedBit(0x119) != 0) {
        markMinutes = &GameFlag_NibbleBanks[0].payload.state.playTimeMark;
        if ((s16)(Mc_SaveData[0].state.playTime - *markMinutes) >= 2) {
            Gp_ClearCollectedBit(0x119);
            Gp_SetCollectedBit(0x11A);
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
        Ui_UpdateLayoutSize(panel, 0xB0, 0x2F);
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

    scan = &Mc_SaveData[0].state.carriedItems;
    ret  = 0;
    switch (scan->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            table = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].state.itemRows;
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

    p                     = &Mc_SaveData[0];
    p->state.carriedItems = Gp_DefaultScan;
}

void func_800BC4BC(void)
{
    Player_Status.field_26 = 1;
    Gp_InitModeEquip();
}

void func_800BC4E4(void)
{
    Player_Status.field_26 = 2;
    Gp_InitModeEquip();
}
