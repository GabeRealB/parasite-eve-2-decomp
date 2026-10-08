#include "items.h"

#include <psyq/sys/types.h>

#include "common.h"

#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "gameplay/items.h"
#include "scene_runtime.h"

#include "main/mc.h"
#include "main/task_types.h"

u16 Gp_PubItemReady;

u32 D_80114DCC;

u16 Gp_PubItemQty;

InventoryItemRow* Gp_SelItemRec;

s32 D_80114DD8;

u16 Gp_PubItemLoc;

u16 D_80114DDE;

s32 D_80114DE0;

s32 D_80114DE4;

s32 D_80114DE8;

u16 Gp_PubItemId;

EquipmentWeaponLoadOptionsTable Gp_RelatedQty0      = { { { 32, { 160, 161, 162 } }, { 20, { 160, 161, 162 } }, { 100, { 160, 161, 162 } }, { 7, { 160, 161, 162 } }, { 12, { 160, 161, 162 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 6, { 166, 167, 168 } }, { 0, { 0, 0, 0 } }, { 1, { 169, 170, 171 } }, { 12, { 169, 170, 171 } }, { 3, { 172, 173, 174 } }, { 7, { 172, 173, 174 } }, { 20, { 172, 173, 174 } }, { 30, { 175, 176, 177 } }, { 200, { 175, 176, 177 } }, { 0, { 0, 0, 0 } }, { 1, { 0, 0, 0 } }, { 60, { 175, 176, 177 } }, { 90, { 175, 176, 177 } }, { 100, { 181, 185, 187 } }, { 12, { 172, 173, 174 } }, { 0, { 0, 0, 0 } }, { 30, { 175, 176, 177 } }, { 30, { 175, 176, 177 } }, { 30, { 175, 176, 177 } }, { 30, { 175, 176, 177 } }, { 30, { 175, 176, 177 } }, { 30, { 160, 161, 162 } }, { 60, { 160, 161, 162 } }, { 90, { 160, 161, 162 } } } };
ArmorStats                      Gp_ModStatAttrs[32] = {
    { ARMOR_FEATURE_RESIST_PARALYSIS, 10, 3, 0, 0 },
    { ARMOR_FEATURE_MOTION_DETECTOR | ARMOR_FEATURE_MEDICAL_INSPECTION, 60, 8, 30, 0 },
    { ARMOR_FEATURE_MOTION_DETECTOR | ARMOR_FEATURE_RESIST_SILENCE, 40, 5, 10, 0 },
    { ARMOR_FEATURE_RESIST_POISON, 0, 5, 10, 0 },
    { ARMOR_FEATURE_RESIST_POISON | ARMOR_FEATURE_HP_RECOVERY, 20, 6, 0, 0 },
    { ARMOR_FEATURE_QUICK_FIRE | ARMOR_FEATURE_HP_RECOVERY, 50, 7, 10, 0 },
    { ARMOR_FEATURE_RESIST_IMPACT | ARMOR_FEATURE_RESIST_PARALYSIS, 100, 5, 0, 0 },
    { ARMOR_FEATURE_MP_RECOVERY | ARMOR_FEATURE_RESIST_PARALYSIS, 5, 3, 20, 0 },
    { ARMOR_FEATURE_RESIST_IMPACT | ARMOR_FEATURE_HP_RECOVERY, 60, 5, 0, 0 },
    { ARMOR_FEATURE_RESIST_POISON | ARMOR_FEATURE_RESIST_PARALYSIS, 20, 6, 20, 0 },
    { ARMOR_FEATURE_MEDICAL_INSPECTION | ARMOR_FEATURE_RESIST_CONFUSION, 0, 4, 50, 0 },
    { ARMOR_FEATURE_QUICK_FIRE | ARMOR_FEATURE_MP_RECOVERY, 30, 7, 50, 0 },
    { ARMOR_FEATURE_QUICK_FIRE, 0, 4, 20, 0 },
    { ARMOR_FEATURE_MP_GENERATION | ARMOR_FEATURE_MP_RECOVERY, 0, 10, 100, 0 },
    // Ids 0x6E–0x7F grant no features. Their base slot count is still 4.
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
};
InventoryConsumableStack Gp_StackLimits[32] = {
    { 50, 0, 500 },
    { 50, 0, 500 },
    { 50, 0, 500 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 50, 0, 500 },
    { 25, 0, 500 },
    { 231, 0, 999 },
    { 4, 0, 100 },
    { 4, 0, 100 },
    { 4, 0, 100 },
    { 10, 0, 200 },
    { 10, 0, 200 },
    { 10, 0, 200 },
    { 80, 0, 800 },
    { 231, 0, 999 },
    { 231, 0, 999 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
};

s32 itemPickupPublishPlacedObject(s32 flagIndex)
{
    enum {
        ITEM_PICKUP_ITEM_BANK_END                = 0x100U,
        ITEM_PICKUP_WEAPON_COUNT                 = ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems),
        ITEM_PICKUP_DUPLICATE_WEAPON_REPLACEMENT = 0x3D,
        ITEM_PICKUP_DUPLICATE_OTHER_REPLACEMENT  = 0x0D,
        ITEM_PICKUP_FULL_STACK_STATE             = 3,
        ITEM_PICKUP_PUBLICATION_READY            = 1
    };
    const AreaObjectRoom*           room;
    const AreaObjectPlace*          place;
    const InventoryConsumableStack* consumableStacks;
    s32                             stageIndex;
    s32                             consumableIndex;
    s32                             roomMatched;
    u16                             objectKind;
    u16                             placeState;
    s32                             placeListEnd;
    s32                             found;

    stageIndex = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage;
    room       = Gp_Bit2Banks[stageIndex].rooms;
    found      = 0;
    // Search the saved stage's room records, preserving the lookup terminator.
    if (room != NULL) {
        if (room->places.sentinel != AREA_OBJECT_ROOM_LOOKUP_END) {
            placeListEnd = AREA_OBJECT_PLACE_END;
            do {
                place       = room->places.list;
                roomMatched = 0;
                if (place != NULL) {
                    if (place->flagIndex != placeListEnd) {
                        consumableStacks = Gp_StackLimits;

                        do {
                            if (place->flagIndex == flagIndex) {
                                objectKind    = place->kind;
                                placeState    = place->state;
                                Gp_PubItemId  = flagIndex;
                                Gp_PubItemLoc = objectKind;
                                D_80114DDE    = placeState;
                                // Publish item quantity only for the item bank.
                                if (objectKind < ITEM_PICKUP_ITEM_BANK_END) {
                                    if (inventoryIsItemLimitReached(place->kind) != 0) {
                                        if ((u32)(place->kind - EQUIPMENT_WEAPON_ITEM_FIRST) < ITEM_PICKUP_WEAPON_COUNT) {
                                            Gp_PubItemLoc = ITEM_PICKUP_DUPLICATE_WEAPON_REPLACEMENT;
                                        } else {
                                            Gp_PubItemLoc = ITEM_PICKUP_DUPLICATE_OTHER_REPLACEMENT;
                                        }
                                        Gp_PubItemQty   = 1;
                                        Gp_PubItemReady = ITEM_PICKUP_PUBLICATION_READY;
                                    } else if ((u32)(place->kind - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
                                        if (areaGetCurrentObjectState(flagIndex) != ITEM_PICKUP_FULL_STACK_STATE) {
                                            consumableIndex = place->kind - INVENTORY_CONSUMABLE_ITEM_FIRST;
                                            Gp_PubItemQty   = consumableStacks[consumableIndex].packQty;
                                        } else {
                                            consumableIndex = place->kind - INVENTORY_CONSUMABLE_ITEM_FIRST;
                                            Gp_PubItemQty   = consumableStacks[consumableIndex].maxHeld;
                                        }
                                        Gp_PubItemReady = ITEM_PICKUP_PUBLICATION_READY;
                                    } else {
                                        Gp_PubItemQty   = 1;
                                        Gp_PubItemReady = ITEM_PICKUP_PUBLICATION_READY;
                                    }
                                }
                                found       = 1;
                                roomMatched = found;
                                break;
                            }
                            place++;

                        } while (place->flagIndex != placeListEnd);
                    }
                }
                if (roomMatched == 1) {
                    break;
                }
                room++;
            } while (room->places.sentinel != AREA_OBJECT_ROOM_LOOKUP_END);
        }
    }
    return found;
}
