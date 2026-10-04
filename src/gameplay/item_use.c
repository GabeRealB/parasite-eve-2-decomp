#include "item_use.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/battle_reward.h"
#include "collision.h"
#include "hud_sprites.h"
#include "gameplay/item_menu.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/player_state.h"
#include "gameplay/room.h"

#include "main/display.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

MATRIX Gp_DefaultMtx;

MATRIX Gp_DefaultMtx2;

MATRIX D_80114ED8;

MATRIX D_80114EF8;

u8 Gp_OverrideVecFlag;

SVECTOR Gp_OverrideVec;

static const char D_80097440[];

static const char D_80097448[];

/* After Armor/Attachments from func_800D6334 so overlay .rodata stays packed. */
/// "Weapon" string drawn by `Gp_DrawWeaponLabel` (trailing 0x60 byte).
static const char Gp_StrWeapon[];

static s32 Gp_ApplyItemUse(InventoryItemRow* arg0);

/// Returns 1 if item `arg0` cannot be used, 0 if it can.
/// `arg1` supplies the total quantity for ammo ids 0xA0–0xBF, including loaded rounds.
static s32 Gp_ItemIsUnusable(s32 arg0, InventoryItemRow* arg1);

static InventoryItemRow* Gp_FindItemByKind(s32 arg0);

WorldCoordRoomAmbientEntry Gp_RoomBoundDefault = { .color = { 16, 16, 16, 16 } };

s32 D_8010F9EC = -0x10000;

s32 D_8010F9F0 = -0x10000;

InventoryBattleReward* D_8010F9F4[6] = {
    NULL,
    D_map_akropolis_8017C0DC,
    D_map_dryfield_8017BCE4,
    D_map_dryfield_full_8017D0B8,
    D_map_shelter_8017BB58,
    D_map_neo_ark_8017C9B0,
};

InventoryBattleReward* D_8010FA0C[6] = {
    NULL,
    D_map_akropolis_8017C16C,
    D_map_dryfield_8017BD80,
    D_map_dryfield_full_8017D1CC,
    D_map_shelter_8017BD8C,
    D_map_neo_ark_8017CB0C,
};

WorldCollisionFaceEdge Gp_FaceEdgePairs[5] = {
    { 2, 1 },
    { 1, 0 },
    { 0, 2 },
    { 2, 3 },
    { 3, 1 },
};

static s32 Gp_ApplyItemUse(InventoryItemRow* arg0)
{
    PlayerStatus*        cfg;
    GameActor*           actor;
    InventoryItemRange*  scanEquip;
    InventoryItemRange*  scanQty;
    InventoryItemRange*  scanRel;
    InventoryItemRange*  scanFree;
    InventoryItemRange*  scanId;
    EquipmentWeaponLoad* slot;
    InventoryItemRow*    table;
    InventoryItemRow*    rec;
    InventoryItemRow*    found;
    s32                  id;
    s32                  ret;
    s32                  flag;
    s32                  i;
    u8                   count;
    s32                  held;
    s32                  prevId;
    s32                  relId;
    s32                  qty;
    s32                  k;
    s32                  avail;
    s32                  slotNum;
    s32                  attachmentSlot;
    InventoryItemRow*    hit;

    ret   = 0;
    flag  = 1;
    id    = arg0->itemId;
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    cfg   = &gPlayerStatus;

    if (id != 0) {
        if ((u32)(id - 0x80) < 0x20U) {
            if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
                rec       = NULL;
                scanEquip = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                prevId    = cfg->weapon + 0x7F;

                cfg->weapon = id - 0x7F;

                table = Gp_GetItemTable(scanEquip);
                table = &table[scanEquip->firstRow];
                count = scanEquip->rowCount;
                for (i = 0; i < count; i++) {
                    if (table->itemId == prevId) {
                        rec = table;
                    }
                    table++;
                }
                if (rec != NULL) {
                    rec->attachSlot = arg0->attachSlot;
                    Gp_RefreshItemRow(arg0);
                }
                Gp_SetItemSeenBit(id, 1);
            }
            ret = 1;
        } else if ((u32)(id - 0xA0) < 0x20U) {
            relId = 0;
            qty   = 0;
            held  = cfg->weapon + 0x7F;
            slot  = Gp_GetItemSlot(held);
            if (Gp_EquipRelatedBank(0, held, id, 0) == 0) {
                Gp_PendingRelatedId = id;
                Gp_RelatedPending   = flag;
                relId               = slot->primaryItemId;
                if (relId != id) {
                    cfg->weaponSlotItem = id + 0x61;
                    slot->primaryItemId = id;
                    slot->primaryQty    = 0;
                }
                Gp_SetItemSeenBit(id, 1);
                ret = 1;
            } else if (Gp_EquipRelatedBank(1, held, id, 0) == 0) {
                Gp_PendingRelatedId = -id;
                Gp_RelatedPending   = flag;
                relId               = slot->secondaryItemId;
                if (relId != id) {
                    slot->secondaryItemId = id;
                    slot->secondaryQty    = 0;
                }
                Gp_SetItemSeenBit(id, 1);
                ret = 1;
            }

            if (relId != INVENTORY_ITEM_NONE && relId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
                scanQty = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                qty     = Gp_ScanStackQty(scanQty, relId);
                qty    -= Gp_CountEquippedRelated(scanQty, relId);
            }
            if (qty > 0) {
                scanRel = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                hit     = NULL;
                table   = Gp_GetItemTable(scanRel);
                i       = 0;
                table   = &table[scanRel->firstRow];
                count   = scanRel->rowCount;
                for (; i < count; i++) {
                    if (table->itemId == relId) {
                        hit = table;
                    }
                    table++;
                }
                found = hit;
                if (found != NULL && found->attachSlot == INVENTORY_ATTACHMENT_NONE) {
                    slotNum  = -1;
                    scanFree = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                    Gp_GetItemTable(scanFree);
                    for (k = 0; k < 3; k++) {
                        avail = 1;
                        table = Gp_GetItemTable(scanFree);
                        i     = 0;
                        table = &table[scanFree->firstRow];
                        count = scanFree->rowCount;
                        for (; i < count; i++) {
                            if (table->itemId != INVENTORY_ITEM_NONE && table->attachSlot == k + 1) {
                                avail = 0;
                                break;
                            }
                            table++;
                        }
                        if (avail == 1) {
                            slotNum = k + 1;
                            break;
                        }
                    }

                    if (slotNum == -1) {
                        scanId = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                        hit    = NULL;
                        table  = Gp_GetItemTable(scanId);
                        i      = 0;
                        table  = &table[scanId->firstRow];
                        count  = scanId->rowCount;
                        for (; i < count; i++) {
                            if (table->itemId == id) {
                                hit = table;
                            }
                            table++;
                        }
                        if (hit != NULL) {
                            attachmentSlot    = hit->attachSlot;
                            hit->attachSlot   = INVENTORY_ATTACHMENT_NONE;
                            found->attachSlot = attachmentSlot;
                        }
                    } else {
                        found->attachSlot = slotNum;
                    }
                }
            }
        } else if ((u32)(id - 0x60) >= 0x20U) {
            Gp_UsedItemId = id;

            if ((u32)(id - 1) < 0x41U) {
                switch (id) {
                    case 1:
                    case 2:
                    case 3:
                        if (cfg->hp < cfg->hpMax) {
                            if (id == 1) {
                                cfg->hp += 0x2D;
                            } else if (id == 2) {
                                cfg->hp += 0x5A;
                            } else {
                                cfg->hp += 0x96;
                            }
                            if (cfg->hp > cfg->hpMax) {
                                cfg->hp = cfg->hpMax;
                            }
                            Gp_HealPending = 1;
                            ret            = 1;
                        }
                        break;
                    case 0x3C:
                        if ((u32)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.hpBonus < 0xFAU) {
                            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.hpBonus += 5;
                        }
                        Gp_RecalcMaxHp();
                        Gp_HealPending = 1;
                        ret            = 1;
                        cfg->hp        = cfg->hpMax;
                        break;
                    case 4:
                        Gp_TriggerPeState(1, (PLAYER_STATUS_SILENCE | PLAYER_STATUS_CONFUSION | PLAYER_STATUS_BERSERKER));
                        ret = Gp_HealPending = Gp_StateC08.mindWard = 1;
                        break;
                    case 8:
                        Gp_TriggerPeState(1, (PLAYER_STATUS_DARKNESS | PLAYER_STATUS_PARALYSIS | PLAYER_STATUS_POISON));
                        ret = Gp_HealPending = Gp_StateC08.bodyWard = 1;
                        break;
                    case 5:
                        if (cfg->mp < cfg->mpMax || cfg->hp < cfg->hpMax) {
                            cfg->mp += 0x50;
                            cfg->hp += 0x14;
                            if (cfg->mp > cfg->mpMax) {
                                cfg->mp = cfg->mpMax;
                            }
                            if (cfg->hp > cfg->hpMax) {
                                cfg->hp = cfg->hpMax;
                            }
                            Gp_HealPending = 1;
                            ret            = 1;
                        }
                        break;
                    case 6:
                    case 7:
                        if (cfg->mp < cfg->mpMax) {
                            if (id == 6) {
                                cfg->mp += 0x19;
                            } else {
                                cfg->mp += 0x64;
                            }
                            if (cfg->mp > cfg->mpMax) {
                                cfg->mp = cfg->mpMax;
                            }
                            Gp_HealPending = 1;
                            ret            = 1;
                        }
                        break;
                    case 0x3A:
                    case 0x3B:
                        func_800A7CB0((u8)((id - 0x3A) * 3 + 0x2E));
                        func_800A7DB8(id - 0x2B);
                        Gp_SelItemRec = arg0;
                        flag          = 0;
                        ret           = 1;
                        break;
                    case 0x41:
                        func_800A7CB0(0x34);
                        func_800A7DB8(0x11);
                        Gp_SelItemRec = arg0;
                        flag          = 0;
                        ret           = 1;
                        break;
                    case 0x3D:
                        if (cfg->mp < cfg->mpMax || cfg->hp < cfg->hpMax) {
                            cfg->mp        = cfg->mpMax;
                            Gp_HealPending = 1;
                            cfg->hp        = cfg->hpMax;
                        }
                        Gp_HealPending = 1;
                        ret            = 1;
                        break;
                    case 0x3E:
                        Gp_HealPending = 1;
                        ret            = 1;
                        break;
                }
            }

            if (ret == 1 && flag != 0) {
                arg0->itemId     = INVENTORY_ITEM_NONE;
                arg0->qty        = 0;
                arg0->attachSlot = INVENTORY_ATTACHMENT_NONE;
                Gp_SetItemSeenBit(id, 1);
            }
        }
    }
    return ret;
}

/// Returns 1 if item `arg0` cannot be used, 0 if it can.
/// `arg1` supplies the total quantity for ammo ids 0xA0–0xBF, including loaded rounds.
static s32 Gp_ItemIsUnusable(s32 arg0, InventoryItemRow* arg1)
{
    PlayerStatus*       cfg;
    InventoryItemRange* scan;
    s32                 ret;
    s32                 val;

    ret = 1;
    cfg = &gPlayerStatus;
    if (arg0 != 0) {
        if ((u32)(arg0 - 0x80) < 0x20U) {
            ret = 0;
        } else if ((u32)(arg0 - 0xA0) < 0x20U) {
            scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            val  = arg1->qty - Gp_CountEquippedRelated(scan, arg0);
            if (val > 0) {
                if (Gp_EquipRelatedItem(scan, cfg->weapon + 0x7F, arg0, 0) == 0) {
                    ret = 0;
                }
            }
        } else if ((u32)(arg0 - 1) < 0x41U) {
            switch (arg0) {
                case 1:
                case 2:
                case 3:
                    if (cfg->hp < cfg->hpMax) {
                        ret = 0;
                    }
                    break;
                case 4:
                    if (Gp_StateC08.mindWard == 0) {
                        ret = 0;
                    }
                    break;
                case 8:
                    if ((s8)Gp_StateC08.bodyWard == 0) {
                        ret = 0;
                    }
                    break;
                case 5:
                    if (cfg->mp < cfg->mpMax) {
                        ret = 0;
                    } else if (cfg->hp < cfg->hpMax) {
                        ret = 0;
                    }
                    break;
                case 6:
                case 7:
                    if (cfg->mp < cfg->mpMax) {
                        ret = 0;
                    }
                    break;
                case 0x3A:
                case 0x3B:
                case 0x3C:
                case 0x41:
                    ret = 0;
                    break;
                case 0x3D:
                    if (cfg->mp < cfg->mpMax) {
                        ret = 0;
                    } else if (cfg->hp < cfg->hpMax) {
                        ret = 0;
                    }
                    break;
                case 0x3E:
                    if (func_800B9D80(0x140) == 0) {
                        ret = 0;
                    }
                    break;
            }
        }
    }
    return ret;
}

static const char D_80097440[] = { 'A', 'r', 'm', 'o', 'r', 0, 0, 0 };
static const char D_80097448[] = { 'A', 't', 't', 'a', 'c', 'h', 'm', 'e', 'n', 't', 's', 0 };

void func_800D6334(Task* task)
{
    TextDrawReq         name;
    TextDrawReq         label;
    UiObject*           panel;
    InventoryItemRow*   selected;
    InventoryItemRow*   table;
    InventoryItemRange* scan;
    InventoryItemRow*   firstRec;
    InventoryItemRow*   firstTable;
    InventoryItemRow*   useRec;
    InventoryItemRow*   useTable;
    InventoryItemRange* firstScan;
    InventoryItemRange* useScan;
    s32                 firstI;
    s32                 firstCount;
    s32                 useI;
    s32                 useCount;
    s32                 useSlot;
    s32                 usable;
    s32                 armor;
    s32                 x;
    s32                 y;
    s32                 selectedX;
    s32                 item;
    s32                 flags;
    s32                 slot;
    s32                 i;
    s32                 selectedSlot;
    s32                 labelX;
    s32                 labelY;

    scan                               = NULL;
    armor                              = gPlayerStatus.armor + 0x5F;
    panel                              = task->spawnArg2.pointer;
    panel->result                      = USER_INTERFACE_RESULT_NONE;
    panel->panel.bounds.unsignedRect.y = 0x1C - gDisplayState.vramYOffset;
    Ui_InsetLayout(&(panel)->panel, 0, 0, 0);
    Ui_DrawText(&(panel)->panel, (char*)D_80097440);
    usable = 1;
    if (task->state == 0) {
        Gp_HealPending = 0;
        Gp_UsedItemId  = 0;
        if (D_8010F884 >= Gp_GetModLevel(armor)) {
            D_8010F884 = 0;
        }
        Ui_SpawnFromDesc(&D_8010F8B4, 0, 0, 0, panel);
        task->state++;
    }
    x = panel->panel.contentLeft.signedValue + 4;
    y = panel->panel.contentTop.signedValue + 0x2B;
    if (task->state == 1) {
        selectedSlot = D_8010F884;
        selectedX    = x + selectedSlot * 13;
        firstRec     = NULL;
        firstScan    = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        firstTable   = Gp_GetItemTable(firstScan);
        firstI       = 0;
        firstTable   = &firstTable[firstScan->firstRow];
        firstCount   = firstScan->rowCount;
        for (; firstI < firstCount; firstI++) {
            if (firstTable->attachSlot == selectedSlot + 1) {
                firstRec = firstTable;
                break;
            }
            firstTable++;
        }
        selected = firstRec;
        if (selected != NULL) {
            item            = selected->itemId;
            name.x          = panel->panel.contentOriginX.unsignedValue + x;
            name.y          = panel->panel.contentOriginY.unsignedValue + 10 + y;
            name.otIndex    = panel->panel.otIndex.signedValue + 1;
            name.colorRgb   = 0x606060;
            name.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            name.alignment  = TEXT_ALIGNMENT_LEFT;
            name.drawMode   = TEXT_DRAW_OUTLINED;
            Text_DrawString(&name, (u8*)Gp_GetItemText(item, 0, 0));
            Gp_DrawStackLeft(panel, x - 15, y + 16, selected, 0x606060, 0);
        } else {
            item = 0;
        }
        flags  = 2;
        usable = 1;
        if (Gp_ItemIsUnusable(item, selected)) {
            flags  = 6;
            usable = 0;
        }
        Gp_DrawItemIcon(panel, selectedX, y, item, flags);
        selectedX = x;
        for (slot = 0; slot < Gp_GetModLevel(armor); slot++, selectedX += 13) {
            if (slot != D_8010F884) {
                selected = NULL;
                scan     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                table    = Gp_GetItemTable(scan);
                i        = 0;
                table    = &table[scan->firstRow];
                for (; i < scan->rowCount; i++) {
                    if (table->attachSlot == slot + 1) {
                        selected = table;
                        break;
                    }
                    table++;
                }
                item = 0;
                if (selected != NULL) {
                    item = selected->itemId;
                }
                flags = (Gp_ItemIsUnusable(item, selected) != 0) * 4;
                Gp_DrawItemIcon(panel, selectedX, y, item, flags);
            }
        }
    }
    labelX = panel->panel.contentLeft.signedValue + 2;
    labelY = panel->panel.contentTop.signedValue;
    Gp_DrawItemLabel(panel, labelX, labelY + 15, armor, 0x606060, 0);
    uiDrawHorizontalSeparator(&(panel)->panel, panel->panel.contentLeft.signedValue, panel->panel.contentRight.signedValue, panel->panel.contentTop.signedValue + 17);
    label.x          = panel->panel.contentOriginX.unsignedValue + labelX;
    label.y          = panel->panel.contentOriginY.unsignedValue + labelY + 24;
    label.otIndex    = panel->panel.otIndex.signedValue + 1;
    label.colorRgb   = 0x606060;
    label.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    label.alignment  = TEXT_ALIGNMENT_LEFT;
    label.drawMode   = TEXT_DRAW_OUTLINED;
    Text_DrawString(&label, (u8*)D_80097448);
    if (panel->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm)) {
            if (usable == 1) {
                useSlot  = D_8010F884;
                useRec   = NULL;
                useScan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                useTable = Gp_GetItemTable(useScan);
                useI     = 0;
                useTable = &useTable[useScan->firstRow];
                useCount = useScan->rowCount;
                for (; useI < useCount; useI++) {
                    if (useTable->attachSlot == useSlot + 1) {
                        useRec = useTable;
                        break;
                    }
                    useTable++;
                }
                if (Gp_ApplyItemUse(useRec)) {
                    SndEvt_EnqueueType6(SOUND_MENU_CONFIRM, 0, 0);
                    panel->result = USER_INTERFACE_RESULT_CANCEL;
                    task->state   = 2;
                }
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT)) {
            SndEvt_EnqueueType6(SOUND_MENU_CURSOR, 0, 0);
            D_8010F884--;
            if (D_8010F884 < 0) {
                D_8010F884 += Gp_GetModLevel(armor);
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT)) {
            SndEvt_EnqueueType6(SOUND_MENU_CURSOR, 0, 0);
            D_8010F884++;
            if (D_8010F884 >= Gp_GetModLevel(armor)) {
                D_8010F884 = 0;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu)) {
            SndEvt_EnqueueType6(SOUND_MENU_CANCEL, 0, 0);
            panel->result = USER_INTERFACE_RESULT_CANCEL;
            task->state   = 2;
        }
    }
}

/* After Armor/Attachments from func_800D6334 so overlay .rodata stays packed. */
/// "Weapon" string drawn by `Gp_DrawWeaponLabel` (trailing 0x60 byte).
static const char Gp_StrWeapon[] = {
    'W',
    'e',
    'a',
    'p',
    'o',
    'n',
    '\0',
    0x60,
};

s32 Gp_FlushPendingRelated(s32 arg0, s32 arg1)
{
    s32 val;

    val = Gp_PendingRelatedId;
    if (val <= 0) {
        if (val >= 0) {
            return -1;
        }
        val = -val;
    }
    Gp_PendingRelatedId = 0;
    return Gp_EquipRelatedItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg0, val, -1);
}

InventoryItemRow* Gp_FindItemById(s32 arg0)
{
    InventoryItemRange* scan;
    InventoryItemRow*   table;
    s32                 i;
    s32                 count;
    InventoryItemRow*   rec;

    rec   = NULL;
    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->firstRow];
    count = scan->rowCount;
    for (; i < count; i++) {
        if (table->itemId == arg0) {
            rec = table;
        }
        table++;
    }
    return rec;
}

static InventoryItemRow* Gp_FindItemByKind(s32 arg0)
{
    InventoryItemRange* scan;
    InventoryItemRow*   table;
    s32                 i;
    s32                 count;
    InventoryItemRow*   rec;

    rec   = NULL;
    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->firstRow];
    count = scan->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot == arg0 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    return rec;
}

InventoryItemRow* Gp_FindItemInScan(s32 arg0, InventoryItemRange* arg1)
{
    InventoryItemRow* table;
    s32               i;
    s32               count;
    InventoryItemRow* rec;

    rec   = NULL;
    table = Gp_GetItemTable(arg1);
    i     = 0;
    table = &table[arg1->firstRow];
    count = arg1->rowCount;
    for (; i < count; i++) {
        if (table->itemId == arg0) {
            rec = table;
        }
        table++;
    }
    return rec;
}

void Gp_DrawWeaponLabel(Task* arg0)
{
    UiPanel* panel;
    s32      x;
    s32      y;

    panel                = arg0->spawnArg2.pointer;
    panel->bounds.rect.y = 0x1C - gDisplayState.vramYOffset;
    Ui_InsetLayout(panel, NULL, NULL, 0);
    x = panel->contentLeft.signedValue;
    y = panel->contentTop.signedValue;
    Gp_DrawEquipSummary(panel, x + 2, y + 0xF, 1);
    Ui_DrawText(panel, Gp_StrWeapon);
}

const char D_8009745C[] = {
    '?',
    '\0',
    0x00,
    0x42,
};
const char Gp_StrGetLockPosNull[] = {
    '#',
    '#',
    '#',
    '#',
    '#',
    '#',
    '#',
    'g',
    'e',
    't',
    '_',
    'l',
    'o',
    'c',
    'k',
    '_',
    'p',
    'o',
    's',
    ' ',
    '-',
    '-',
    '-',
    '>',
    ' ',
    'N',
    'U',
    'L',
    'L',
    '!',
    '!',
    '!',
    '\n',
    '\0',
    0x8C,
    0x16,
};
