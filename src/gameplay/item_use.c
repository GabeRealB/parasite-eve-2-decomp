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

/* After Armor/Attachments from itemMenuArmorAttachmentPanelTask so overlay .rodata stays packed. */
/// "Weapon" string drawn by `itemMenuAttachmentUseWeaponPanelTask` (trailing 0x60 byte).
static const char Gp_StrWeapon[];

// Catalogue items handled by the attachment-use panel.
enum {
    INVENTORY_ITEM_RECOVERY1        = 1,
    INVENTORY_ITEM_RECOVERY2        = 2,
    INVENTORY_ITEM_RECOVERY3        = 3,
    INVENTORY_ITEM_STIM             = 4,
    INVENTORY_ITEM_COLA             = 5,
    INVENTORY_ITEM_MP_BOOST1        = 6,
    INVENTORY_ITEM_MP_BOOST2        = 7,
    INVENTORY_ITEM_PENICILLIN       = 8,
    INVENTORY_ITEM_FLARE            = 0x3A,
    INVENTORY_ITEM_PEPPER_SPRAY     = 0x3B,
    INVENTORY_ITEM_PROTEIN_CAPSULE  = 0x3C,
    INVENTORY_ITEM_RINGERS_SOLUTION = 0x3D,
    INVENTORY_ITEM_EAU_DE_TOILETTE  = 0x3E,
    INVENTORY_ITEM_COMBAT_LIGHT     = 0x41
};

// Item-id domains used here; weapon and consumable selectors cover 32 ids each.
enum {
    ITEM_USE_ORDINARY_ITEM_FIRST          = 1,
    ITEM_USE_ORDINARY_ITEM_COUNT          = 0x41,
    ITEM_USE_ARMOR_ITEM_FIRST             = 0x60,
    ITEM_USE_ARMOR_ITEM_COUNT             = 0x20,
    ITEM_USE_WEAPON_ITEM_COUNT            = 0x20,
    ITEM_MENU_ATTACHMENT_PANEL_TOP_PIXELS = 28,
    ITEM_MENU_ATTACHMENT_TEXT_COLOR_RGB   = 0x606060
};

static s32 _itemUseAttachedItem(InventoryItemRow* attachedRow);

static s32 _itemIsAttachedItemUnusable(s32 itemId, const InventoryItemRow* attachedRow);

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

/// Applies an armor attachment's item use and returns 1 when accepted, else 0.
///
/// The writable row must remain in the live carried range, whose rows must fit
/// its table; the player task must be live. Call after the availability check.
/// Consumables require an equipped weapon selector in 1..32. Weapon selection
/// exchanges attachment positions with the last carried row of the old weapon;
/// scripted player mode accepts that request without changing equipment.
/// Consumables queue a signed reload id, clear a changed load and reattach its
/// old stock, trying only armor slots 1..3 before exchanging attachment positions.
/// Medicines change player state now and queue the menu-exit notification;
/// successful medicine uses clear the whole row, independently of quantity.
/// Flare, Pepper Spray and Combat Light retain the row in `Gp_SelItemRec` for
/// their queued attachment action, so its storage must survive that action.
static s32 _itemUseAttachedItem(InventoryItemRow* attachedRow)
{
    // Borrow the last exact-id row. Arguments must be side-effect-free locals;
    // result starts NULL. The range must fit its table. The workspace arguments
    // are writable traversal locals; neither the range nor its rows is changed.
#define ITEM_USE_FIND_LAST_ROW(range, soughtItemId, result, workspaceRow, workspaceRowIndex, workspaceRowCount) \
    {                                                                                                           \
        (workspaceRow)      = inventoryGetRangeTable(range);                                                    \
        (workspaceRowIndex) = 0;                                                                                \
        (workspaceRow)      = &(workspaceRow)[(range)->firstRow];                                               \
        (workspaceRowCount) = (range)->rowCount;                                                                \
        for (; (workspaceRowIndex) < (workspaceRowCount); (workspaceRowIndex)++) {                              \
            if ((workspaceRow)->itemId == (soughtItemId)) {                                                     \
                (result) = (workspaceRow);                                                                      \
            }                                                                                                   \
            (workspaceRow)++;                                                                                   \
        }                                                                                                       \
    }
    enum {
        ATTACHMENT_FLARE_SOUND_FILE_INDEX         = ATTACHMENT_INDEX_FLARE * ATTACHMENT_AREA_LEVEL_COUNT + 1,
        ATTACHMENT_COMBAT_LIGHT_SOUND_FILE_INDEX  = ATTACHMENT_INDEX_COMBAT_LIGHT * ATTACHMENT_AREA_LEVEL_COUNT + 1,
        ITEM_USE_REATTACH_SLOT_COUNT              = 3,
        ITEM_USE_NO_ATTACHMENT_SLOT               = -1,
        ITEM_USE_HP_BONUS_LIMIT                   = 250,
        ITEM_USE_PRIMARY_CONSUMABLE_SELECTOR_BIAS = 0x100 - (INVENTORY_CONSUMABLE_ITEM_FIRST - 1)
    };
    PlayerStatus*             player;
    GameActor*                playerActor;
    const InventoryItemRange* weaponRange;
    const InventoryItemRange* stockRange;
    const InventoryItemRange* previousConsumableRange;
    const InventoryItemRange* attachmentRange;
    const InventoryItemRange* newConsumableRange;
    EquipmentWeaponLoad*      weaponLoad;
    InventoryItemRow*         row;
    InventoryItemRow*         previousWeaponRow;
    InventoryItemRow*         previousConsumableRow;
    s32                       itemId;
    s32                       applied;
    s32                       consumeRow;
    s32                       rowIndex;
    u8                        rowCount;
    s32                       weaponItemId;
    s32                       previousWeaponItemId;
    s32                       previousConsumableItemId;
    s32                       unloadedQuantity;
    s32                       attachmentIndex;
    s32                       slotAvailable;
    s32                       freeAttachmentSlot;
    s32                       transferredAttachmentSlot;
    InventoryItemRow*         matchingRow;

    applied     = 0;
    consumeRow  = 1;
    itemId      = attachedRow->itemId;
    playerActor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    player      = &gPlayerStatus;

    if (itemId != INVENTORY_ITEM_NONE) {
        if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < (u32)ITEM_USE_WEAPON_ITEM_COUNT) {
            // Keep the old weapon on the attachment position vacated by the new one.
            if (playerActor->mode != GAME_ACTOR_MODE_SCRIPTED) {
                previousWeaponRow    = NULL;
                weaponRange          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                previousWeaponItemId = player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1);

                player->weapon = itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);

                ITEM_USE_FIND_LAST_ROW(weaponRange, previousWeaponItemId, previousWeaponRow, row, rowIndex, rowCount);
                if (previousWeaponRow != NULL) {
                    previousWeaponRow->attachSlot = attachedRow->attachSlot;
                    inventoryDetachItem(attachedRow);
                }
                itemSetIdentified(itemId, 1);
            }
            applied = 1;
        } else if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
            // Select primary before secondary; actual loading waits for the reload cue.
            previousConsumableItemId = INVENTORY_ITEM_NONE;
            unloadedQuantity         = 0;
            weaponItemId             = player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
            weaponLoad               = equipmentGetWeaponLoad(weaponItemId);
            if (equipmentLoadCarriedWeaponConsumable(EQUIPMENT_WEAPON_SUPPLY_PRIMARY, weaponItemId, itemId, EQUIPMENT_WEAPON_LOAD_CHECK_ONLY) == 0) {
                Gp_PendingRelatedId      = itemId;
                Gp_RelatedPending        = consumeRow;
                previousConsumableItemId = weaponLoad->primaryItemId;
                if (previousConsumableItemId != itemId) {
                    // Encode the one-based consumable selector modulo the stored byte.
                    player->weaponSlotItem    = itemId + ITEM_USE_PRIMARY_CONSUMABLE_SELECTOR_BIAS;
                    weaponLoad->primaryItemId = itemId;
                    weaponLoad->primaryQty    = 0;
                }
                itemSetIdentified(itemId, 1);
                applied = 1;
            } else if (equipmentLoadCarriedWeaponConsumable(EQUIPMENT_WEAPON_SUPPLY_SECONDARY, weaponItemId, itemId, EQUIPMENT_WEAPON_LOAD_CHECK_ONLY) == 0) {
                Gp_PendingRelatedId      = -itemId;
                Gp_RelatedPending        = consumeRow;
                previousConsumableItemId = weaponLoad->secondaryItemId;
                if (previousConsumableItemId != itemId) {
                    weaponLoad->secondaryItemId = itemId;
                    weaponLoad->secondaryQty    = 0;
                }
                itemSetIdentified(itemId, 1);
                applied = 1;
            }

            if (previousConsumableItemId != INVENTORY_ITEM_NONE && previousConsumableItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
                stockRange        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                unloadedQuantity  = inventoryGetConsumableStackQuantity(stockRange, previousConsumableItemId);
                unloadedQuantity -= equipmentGetLoadedConsumableQuantity(stockRange, previousConsumableItemId);
            }
            if (unloadedQuantity > 0) {
                // Return displaced unloaded stock to an armor attachment position.
                previousConsumableRange = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                matchingRow             = NULL;
                ITEM_USE_FIND_LAST_ROW(previousConsumableRange, previousConsumableItemId, matchingRow, row, rowIndex, rowCount);
                previousConsumableRow = matchingRow;
                if (previousConsumableRow != NULL && previousConsumableRow->attachSlot == INVENTORY_ATTACHMENT_NONE) {
                    freeAttachmentSlot = ITEM_USE_NO_ATTACHMENT_SLOT;
                    attachmentRange    = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                    inventoryGetRangeTable(attachmentRange);
                    for (attachmentIndex = 0; attachmentIndex < ITEM_USE_REATTACH_SLOT_COUNT; attachmentIndex++) {
                        slotAvailable = 1;
                        row           = inventoryGetRangeTable(attachmentRange);
                        rowIndex      = 0;
                        row           = &row[attachmentRange->firstRow];
                        rowCount      = attachmentRange->rowCount;
                        for (; rowIndex < rowCount; rowIndex++) {
                            if (row->itemId != INVENTORY_ITEM_NONE && row->attachSlot == attachmentIndex + 1) {
                                slotAvailable = 0;
                                break;
                            }
                            row++;
                        }
                        if (slotAvailable == 1) {
                            freeAttachmentSlot = attachmentIndex + 1;
                            break;
                        }
                    }

                    if (freeAttachmentSlot == ITEM_USE_NO_ATTACHMENT_SLOT) {
                        newConsumableRange = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                        matchingRow        = NULL;
                        ITEM_USE_FIND_LAST_ROW(newConsumableRange, itemId, matchingRow, row, rowIndex, rowCount);
                        if (matchingRow != NULL) {
                            transferredAttachmentSlot         = matchingRow->attachSlot;
                            matchingRow->attachSlot           = INVENTORY_ATTACHMENT_NONE;
                            previousConsumableRow->attachSlot = transferredAttachmentSlot;
                        }
                    } else {
                        previousConsumableRow->attachSlot = freeAttachmentSlot;
                    }
                }
            }
        } else if ((u32)(itemId - ITEM_USE_ARMOR_ITEM_FIRST) >= (u32)ITEM_USE_ARMOR_ITEM_COUNT) {
            // Apply immediate medicine effects, or lend the row to a queued defense action.
            Gp_UsedItemId = itemId;

            if ((u32)(itemId - ITEM_USE_ORDINARY_ITEM_FIRST) < (u32)ITEM_USE_ORDINARY_ITEM_COUNT) {
                switch (itemId) {
                    case INVENTORY_ITEM_RECOVERY1:
                    case INVENTORY_ITEM_RECOVERY2:
                    case INVENTORY_ITEM_RECOVERY3:
                        if (player->hp < player->hpMax) {
                            if (itemId == INVENTORY_ITEM_RECOVERY1) {
                                player->hp += 0x2D;
                            } else if (itemId == INVENTORY_ITEM_RECOVERY2) {
                                player->hp += 0x5A;
                            } else {
                                player->hp += 0x96;
                            }
                            if (player->hp > player->hpMax) {
                                player->hp = player->hpMax;
                            }
                            Gp_HealPending = 1;
                            applied        = 1;
                        }
                        break;
                    case INVENTORY_ITEM_PROTEIN_CAPSULE:
                        if ((u32)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.hpBonus < (u32)ITEM_USE_HP_BONUS_LIMIT) {
                            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.hpBonus += 5;
                        }
                        equipmentRecalculateMaxHp();
                        Gp_HealPending = 1;
                        applied        = 1;
                        player->hp     = player->hpMax;
                        break;
                    case INVENTORY_ITEM_STIM:
                        playerStateSetStatusEffects(1, (PLAYER_STATUS_SILENCE | PLAYER_STATUS_CONFUSION | PLAYER_STATUS_BERSERKER));
                        applied = Gp_HealPending = Gp_StateC08.mindWard = 1;
                        break;
                    case INVENTORY_ITEM_PENICILLIN:
                        playerStateSetStatusEffects(1, (PLAYER_STATUS_DARKNESS | PLAYER_STATUS_PARALYSIS | PLAYER_STATUS_POISON));
                        applied = Gp_HealPending = Gp_StateC08.bodyWard = 1;
                        break;
                    case INVENTORY_ITEM_COLA:
                        if (player->mp < player->mpMax || player->hp < player->hpMax) {
                            player->mp += 0x50;
                            player->hp += 0x14;
                            if (player->mp > player->mpMax) {
                                player->mp = player->mpMax;
                            }
                            if (player->hp > player->hpMax) {
                                player->hp = player->hpMax;
                            }
                            Gp_HealPending = 1;
                            applied        = 1;
                        }
                        break;
                    case INVENTORY_ITEM_MP_BOOST1:
                    case INVENTORY_ITEM_MP_BOOST2:
                        if (player->mp < player->mpMax) {
                            if (itemId == INVENTORY_ITEM_MP_BOOST1) {
                                player->mp += 0x19;
                            } else {
                                player->mp += 0x64;
                            }
                            if (player->mp > player->mpMax) {
                                player->mp = player->mpMax;
                            }
                            Gp_HealPending = 1;
                            applied        = 1;
                        }
                        break;
                    case INVENTORY_ITEM_FLARE:
                    case INVENTORY_ITEM_PEPPER_SPRAY:
                        attachmentSoundLoadStub((u8)((itemId - INVENTORY_ITEM_FLARE) * ATTACHMENT_AREA_LEVEL_COUNT + ATTACHMENT_FLARE_SOUND_FILE_INDEX));
                        attachmentQueueIndex(itemId - (INVENTORY_ITEM_FLARE - ATTACHMENT_INDEX_FLARE));
                        Gp_SelItemRec = attachedRow;
                        consumeRow    = 0;
                        applied       = 1;
                        break;
                    case INVENTORY_ITEM_COMBAT_LIGHT:
                        attachmentSoundLoadStub(ATTACHMENT_COMBAT_LIGHT_SOUND_FILE_INDEX);
                        attachmentQueueIndex(ATTACHMENT_INDEX_COMBAT_LIGHT);
                        Gp_SelItemRec = attachedRow;
                        consumeRow    = 0;
                        applied       = 1;
                        break;
                    case INVENTORY_ITEM_RINGERS_SOLUTION:
                        if (player->mp < player->mpMax || player->hp < player->hpMax) {
                            player->mp     = player->mpMax;
                            Gp_HealPending = 1;
                            player->hp     = player->hpMax;
                        }
                        Gp_HealPending = 1;
                        applied        = 1;
                        break;
                    case INVENTORY_ITEM_EAU_DE_TOILETTE:
                        Gp_HealPending = 1;
                        applied        = 1;
                        break;
                }
            }

            if (applied == 1 && consumeRow != 0) {
                attachedRow->itemId     = INVENTORY_ITEM_NONE;
                attachedRow->qty        = 0;
                attachedRow->attachSlot = INVENTORY_ATTACHMENT_NONE;
                itemSetIdentified(itemId, 1);
            }
        }
    }
#undef ITEM_USE_FIND_LAST_ROW
    return applied;
}

/// Returns 1 when an attachment's item use is unavailable, otherwise 0.
///
/// Weapons always qualify. Medicines test missing HP/MP, active wards or
/// Berserker resistance; the disposable defense items and Protein Capsule
/// always qualify. Empty rows, armor and unhandled catalogue ids return 1.
/// For consumable ids 0xA0..0xBF, attachedRow must be readable and supply that
/// item's total quantity, including loaded units. Other ids permit NULL.
/// The live carried range must fit its readable table and the weapon selector
/// must be 0..32. Compatibility and positive unloaded stock are checked without
/// changing inventory, weapon loads, identification or player state.
static s32 _itemIsAttachedItemUnusable(s32 itemId, const InventoryItemRow* attachedRow)
{
    const PlayerStatus*       player;
    const InventoryItemRange* range;
    s32                       unusable;
    s32                       unloadedQuantity;

    unusable = 1;
    player   = &gPlayerStatus;
    if (itemId != INVENTORY_ITEM_NONE) {
        if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < (u32)ITEM_USE_WEAPON_ITEM_COUNT) {
            unusable = 0;
        } else if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
            // A loaded round cannot be selected again as free stock in this panel.
            range            = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            unloadedQuantity = attachedRow->qty - equipmentGetLoadedConsumableQuantity(range, itemId);
            if (unloadedQuantity > 0) {
                if (equipmentLoadWeaponConsumable(range, player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1), itemId, EQUIPMENT_WEAPON_LOAD_CHECK_ONLY) == 0) {
                    unusable = 0;
                }
            }
        } else if ((u32)(itemId - ITEM_USE_ORDINARY_ITEM_FIRST) < (u32)ITEM_USE_ORDINARY_ITEM_COUNT) {
            switch (itemId) {
                case INVENTORY_ITEM_RECOVERY1:
                case INVENTORY_ITEM_RECOVERY2:
                case INVENTORY_ITEM_RECOVERY3:
                    if (player->hp < player->hpMax) {
                        unusable = 0;
                    }
                    break;
                case INVENTORY_ITEM_STIM:
                    if (Gp_StateC08.mindWard == 0) {
                        unusable = 0;
                    }
                    break;
                case INVENTORY_ITEM_PENICILLIN:
                    if (Gp_StateC08.bodyWard == 0) {
                        unusable = 0;
                    }
                    break;
                case INVENTORY_ITEM_COLA:
                    if (player->mp < player->mpMax) {
                        unusable = 0;
                    } else if (player->hp < player->hpMax) {
                        unusable = 0;
                    }
                    break;
                case INVENTORY_ITEM_MP_BOOST1:
                case INVENTORY_ITEM_MP_BOOST2:
                    if (player->mp < player->mpMax) {
                        unusable = 0;
                    }
                    break;
                case INVENTORY_ITEM_FLARE:
                case INVENTORY_ITEM_PEPPER_SPRAY:
                case INVENTORY_ITEM_PROTEIN_CAPSULE:
                case INVENTORY_ITEM_COMBAT_LIGHT:
                    unusable = 0;
                    break;
                case INVENTORY_ITEM_RINGERS_SOLUTION:
                    if (player->mp < player->mpMax) {
                        unusable = 0;
                    } else if (player->hp < player->hpMax) {
                        unusable = 0;
                    }
                    break;
                case INVENTORY_ITEM_EAU_DE_TOILETTE:
                    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_BERSERKER) == 0) {
                        unusable = 0;
                    }
                    break;
            }
        }
    }
    return unusable;
}

static const char D_80097440[] = { 'A', 'r', 'm', 'o', 'r', 0, 0, 0 };
static const char D_80097448[] = { 'A', 't', 't', 'a', 'c', 'h', 'm', 'e', 'n', 't', 's', 0 };

void itemMenuArmorAttachmentPanelTask(Task* panelTask)
{
    // Borrow the first row storing attachmentIndex + 1; item id is ignored.
    // The result starts NULL, the workspace row points at the range start,
    // and its index starts at zero. rowLimit counts rows and is re-read each
    // iteration. All arguments must be side-effect-free locals or field views.
#define ITEM_MENU_FIND_ATTACHMENT_ROW(attachmentIndex, result, workspaceRow, workspaceIndex, rowLimit) \
    {                                                                                                  \
        for (; (workspaceIndex) < (rowLimit); (workspaceIndex)++) {                                    \
            if ((workspaceRow)->attachSlot == (attachmentIndex) + 1) {                                 \
                (result) = (workspaceRow);                                                             \
                break;                                                                                 \
            }                                                                                          \
            (workspaceRow)++;                                                                          \
        }                                                                                              \
    }
    enum {
        ITEM_MENU_ATTACHMENT_PANEL_INITIAL   = 0,
        ITEM_MENU_ATTACHMENT_PANEL_SELECTING = 1,
        ITEM_MENU_ATTACHMENT_PANEL_CLOSED    = 2
    };
    TextDrawReq         itemName;
    TextDrawReq         attachmentLabel;
    UiObject*           object;
    InventoryItemRow*   attachedRow;
    InventoryItemRow*   row;
    InventoryItemRange* range;
    InventoryItemRow*   selectedRow;
    InventoryItemRow*   selectedScanRow;
    InventoryItemRow*   useRow;
    InventoryItemRow*   useScanRow;
    InventoryItemRange* selectedRange;
    InventoryItemRange* useRange;
    s32                 selectedRowIndex;
    s32                 selectedRowCount;
    s32                 useRowIndex;
    s32                 useRowCount;
    s32                 useAttachmentIndex;
    s32                 selectedUsable;
    s32                 armorItemId;
    s32                 attachmentX;
    s32                 attachmentY;
    s32                 iconX;
    s32                 itemId;
    s32                 iconFlags;
    s32                 attachmentIndex;
    s32                 rowIndex;
    s32                 selectedAttachmentIndex;
    s32                 armorX;
    s32                 armorY;

    armorItemId                         = gPlayerStatus.armor + (ITEM_USE_ARMOR_ITEM_FIRST - 1);
    object                              = panelTask->spawnArg2.pointer;
    object->result                      = USER_INTERFACE_RESULT_NONE;
    object->panel.bounds.unsignedRect.y = ITEM_MENU_ATTACHMENT_PANEL_TOP_PIXELS - gDisplayState.vramYOffset;
    uiUpdatePanelContentLayout(&object->panel, NULL, NULL, 0);
    uiDrawPanelLabel(&object->panel, D_80097440);
    selectedUsable = 1;
    if (panelTask->state == ITEM_MENU_ATTACHMENT_PANEL_INITIAL) {
        // Reset item-use notifications and open the companion weapon summary once.
        Gp_HealPending = 0;
        Gp_UsedItemId  = INVENTORY_ITEM_NONE;
        if (D_8010F884 >= equipmentGetArmorAttachmentSlotCount(armorItemId)) {
            D_8010F884 = 0;
        }
        uiSpawnObject(&D_8010F8B4, 0, 0, 0, object);
        panelTask->state++;
    }
    attachmentX = object->panel.contentLeft.signedValue + 4;
    attachmentY = object->panel.contentTop.signedValue + 0x2B;
    if (panelTask->state == ITEM_MENU_ATTACHMENT_PANEL_SELECTING) {
        // Enlarge the selected attachment and dim every currently unusable item.
        selectedAttachmentIndex = D_8010F884;
        iconX                   = attachmentX + selectedAttachmentIndex * 13;
        selectedRow             = NULL;
        selectedRange           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        selectedScanRow         = inventoryGetRangeTable(selectedRange);
        selectedRowIndex        = 0;
        selectedScanRow         = &selectedScanRow[selectedRange->firstRow];
        selectedRowCount        = selectedRange->rowCount;
        ITEM_MENU_FIND_ATTACHMENT_ROW(selectedAttachmentIndex, selectedRow, selectedScanRow, selectedRowIndex, selectedRowCount);
        attachedRow = selectedRow;
        if (attachedRow != NULL) {
            itemId              = attachedRow->itemId;
            itemName.x          = object->panel.contentOriginX.unsignedValue + attachmentX;
            itemName.y          = object->panel.contentOriginY.unsignedValue + 10 + attachmentY;
            itemName.otIndex    = object->panel.otIndex.signedValue + 1;
            itemName.colorRgb   = ITEM_MENU_ATTACHMENT_TEXT_COLOR_RGB;
            itemName.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            itemName.alignment  = TEXT_ALIGNMENT_LEFT;
            itemName.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&itemName, itemGetText(itemId, ITEM_TEXT_NAME, 0));
            itemMenuDrawUnloadedConsumableQuantity(object, attachmentX - 15, attachmentY + 16, attachedRow, ITEM_MENU_ATTACHMENT_TEXT_COLOR_RGB, 0);
        } else {
            itemId = INVENTORY_ITEM_NONE;
        }
        iconFlags      = ITEM_MENU_ICON_ENLARGED;
        selectedUsable = 1;
        if (_itemIsAttachedItemUnusable(itemId, attachedRow)) {
            iconFlags      = ITEM_MENU_ICON_ENLARGED | ITEM_MENU_ICON_DIMMED;
            selectedUsable = 0;
        }
        itemMenuDrawItemIcon(object, iconX, attachmentY, itemId, iconFlags);
        iconX = attachmentX;
        for (attachmentIndex = 0; attachmentIndex < equipmentGetArmorAttachmentSlotCount(armorItemId); attachmentIndex++, iconX += 13) {
            if (attachmentIndex != D_8010F884) {
                attachedRow = NULL;
                range       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                row         = inventoryGetRangeTable(range);
                rowIndex    = 0;
                row         = &row[range->firstRow];
                ITEM_MENU_FIND_ATTACHMENT_ROW(attachmentIndex, attachedRow, row, rowIndex, range->rowCount);
                itemId = INVENTORY_ITEM_NONE;
                if (attachedRow != NULL) {
                    itemId = attachedRow->itemId;
                }
                iconFlags = (_itemIsAttachedItemUnusable(itemId, attachedRow) != 0) * ITEM_MENU_ICON_DIMMED;
                itemMenuDrawItemIcon(object, iconX, attachmentY, itemId, iconFlags);
            }
        }
    }
    armorX = object->panel.contentLeft.signedValue + 2;
    armorY = object->panel.contentTop.signedValue;
    itemMenuDrawItemRow(object, armorX, armorY + 15, armorItemId, ITEM_MENU_ATTACHMENT_TEXT_COLOR_RGB, 0);
    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, object->panel.contentTop.signedValue + 17);
    attachmentLabel.x          = object->panel.contentOriginX.unsignedValue + armorX;
    attachmentLabel.y          = object->panel.contentOriginY.unsignedValue + armorY + 24;
    attachmentLabel.otIndex    = object->panel.otIndex.signedValue + 1;
    attachmentLabel.colorRgb   = ITEM_MENU_ATTACHMENT_TEXT_COLOR_RGB;
    attachmentLabel.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    attachmentLabel.alignment  = TEXT_ALIGNMENT_LEFT;
    attachmentLabel.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&attachmentLabel, (const u8*)D_80097448);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        // Re-read the selected position on Confirm before applying its item use.
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm)) {
            if (selectedUsable == 1) {
                useAttachmentIndex = D_8010F884;
                useRow             = NULL;
                useRange           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                useScanRow         = inventoryGetRangeTable(useRange);
                useRowIndex        = 0;
                useScanRow         = &useScanRow[useRange->firstRow];
                useRowCount        = useRange->rowCount;
                ITEM_MENU_FIND_ATTACHMENT_ROW(useAttachmentIndex, useRow, useScanRow, useRowIndex, useRowCount);
                if (_itemUseAttachedItem(useRow)) {
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    object->result   = USER_INTERFACE_RESULT_CANCEL;
                    panelTask->state = ITEM_MENU_ATTACHMENT_PANEL_CLOSED;
                }
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT)) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            D_8010F884--;
            if (D_8010F884 < 0) {
                D_8010F884 += equipmentGetArmorAttachmentSlotCount(armorItemId);
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT)) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            D_8010F884++;
            if (D_8010F884 >= equipmentGetArmorAttachmentSlotCount(armorItemId)) {
                D_8010F884 = 0;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu)) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result   = USER_INTERFACE_RESULT_CANCEL;
            panelTask->state = ITEM_MENU_ATTACHMENT_PANEL_CLOSED;
        }
    }
#undef ITEM_MENU_FIND_ATTACHMENT_ROW
}

/* After Armor/Attachments from itemMenuArmorAttachmentPanelTask so overlay .rodata stays packed. */
/// "Weapon" string drawn by `itemMenuAttachmentUseWeaponPanelTask` (trailing 0x60 byte).
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

s32 equipmentLoadPendingConsumable(s32 weaponItemId, s32 unused)
{
    s32 consumableItemId;

    consumableItemId = Gp_PendingRelatedId;
    if (consumableItemId <= 0) {
        if (consumableItemId >= 0) {
            return EQUIPMENT_WEAPON_LOAD_FAILED;
        }
        consumableItemId = -consumableItemId;
    }
    Gp_PendingRelatedId = INVENTORY_ITEM_NONE;
    return equipmentLoadWeaponConsumable(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, weaponItemId, consumableItemId, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY);
}

/// Borrows the highest-index row in `range` whose item id equals `itemId`.
///
/// Returns `NULL` for no match, including a zero-row range. The comparison
/// does not narrow `itemId`: 0 selects the last free row, and values outside
/// 0..255 never match. Quantity and attachment state are ignored.
/// `range` must be readable, and `firstRow + rowCount` must fit its selected
/// table; both fields count rows, including free rows. The descriptor and rows
/// are left intact.
/// The writable result borrows the table, whose storage must remain available.
/// Sorting, transfers or replacing saved contents can change the item at the
/// returned address.
static inline InventoryItemRow* _inventoryFindLastItemRowInRange(s32 itemId, const InventoryItemRange* range)
{
    InventoryItemRow* row;
    s32               rowIndex;
    s32               rowCount;
    InventoryItemRow* matchingRow;

    matchingRow = NULL;
    row         = inventoryGetRangeTable(range);
    row        += range->firstRow;
    rowCount    = range->rowCount;
    for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        if (row->itemId == itemId) {
            matchingRow = row;
        }
        row++;
    }
    return matchingRow;
}

InventoryItemRow* inventoryFindLastCarriedItemRow(s32 itemId)
{
    return _inventoryFindLastItemRowInRange(itemId, &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems);
}

/// Borrows the first carried row at a zero-based armour attachment position.
///
/// Positions 0..9 correspond to stored slots 1..10; -1 selects unattached rows
/// and -2 selects equipped armour. Item id and quantity are not checked.
/// Returns `NULL` when no row matches. The carried range must fit its selected
/// table; sorting, transfers or replacing the live save can change the item
/// at the returned address.
static InventoryItemRow* _inventoryFindCarriedAttachmentRow(s32 attachmentIndex)
{
    const InventoryItemRange* range;
    InventoryItemRow*         row;
    s32                       rowIndex;
    s32                       rowCount;
    InventoryItemRow*         matchingRow;

    matchingRow = NULL;
    range       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    row         = inventoryGetRangeTable(range);
    row        += range->firstRow;
    rowCount    = range->rowCount;
    for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        if (row->attachSlot == attachmentIndex + 1) {
            matchingRow = row;
            break;
        }
        row++;
    }
    return matchingRow;
}

InventoryItemRow* inventoryFindLastItemRowInRange(s32 itemId, const InventoryItemRange* range)
{
    return _inventoryFindLastItemRowInRange(itemId, range);
}

void itemMenuAttachmentUseWeaponPanelTask(Task* panelTask)
{
    UiObject* object;
    UiPanel*  panel;
    s32       weaponX;
    s32       weaponY;

    object               = panelTask->spawnArg2.pointer;
    panel                = &object->panel;
    panel->bounds.rect.y = ITEM_MENU_ATTACHMENT_PANEL_TOP_PIXELS - gDisplayState.vramYOffset;
    uiUpdatePanelContentLayout(panel, NULL, NULL, 0);
    weaponX = panel->contentLeft.signedValue;
    weaponY = panel->contentTop.signedValue;
    itemMenuDrawWeaponSummary(object, weaponX + 2, weaponY + 0xF, 1);
    uiDrawPanelLabel(panel, Gp_StrWeapon);
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
