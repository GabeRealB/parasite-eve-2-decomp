#include "common.h"

#include "gameplay/268.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/ui.h"
#include "rooms/dryfield_night_trailer_coach.h"
#include "rooms/room_common.h"

/// One row of the shop's price ladder (`D_dryfield_night_trailer_coach_801843F4`,
/// thirteen rows): a spending threshold and the three item ids the row offers
/// once `Mc_SaveData.shopTiers` has its bit set. Only `items` is read here.
typedef struct DryfieldNightTrailerCoachShopTier {
    /* 0x0 */ s32  spendThreshold;
    /* 0x4 */ s16  items[3];
    /* 0xA */ byte pad_A[2];
} DryfieldNightTrailerCoachShopTier;
STATIC_ASSERT_SIZEOF(DryfieldNightTrailerCoachShopTier, 0xC);

extern DryfieldNightTrailerCoachShopTier D_dryfield_night_trailer_coach_801843F4[13];

/// Fills `shop` with the ids the shop currently offers, then sorts them by
/// `Gp_ItemSortKey`, caps the visible row count at 9 and clears the cursor
/// item.
///
/// The upper halfword of the owning task's `spawnArg1` is the mode, which picks
/// the fixed id list (`func_dryfield_night_trailer_coach_8017D81C`) and, in game mode
/// 0, which items of each unlocked price row are added: mode 0 ids 0x80-0x9F
/// and 9, 0xA, 0xC, 0x42-0x46; mode 1 ids 0xA0-0xBF; mode 2 ids 0x60-0x7F and
/// 0xD; mode 3 ids 1-0x5F other than those. Mode 3 also adds, for each of the
/// twelve two-bit levels in `Mc_SaveData.shopStock`, the id of that level
/// (the first slot needs level 2). With `Mc_SaveData.demoScene` 1 every row
/// and level is unlocked first.
void func_dryfield_night_trailer_coach_8017E464(RoomShopList* shop, UiObject* obj)
{
    RoomShopList* list;
    u16*          ids;
    s32           mode;
    s32           tier;
    s32           slot;
    s32           level;
    s32           id;
    s32           item;
    s32           unlocked;
    s32           i;
    s32           j;
    s32           k;
    s32           key;
    s32           otherKey;
    u16           tmp;
    u8            count;

    mode = obj->owner->spawnArg1;
    ids  = func_dryfield_night_trailer_coach_8017D81C(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_dryfield_night_trailer_coach_8017E318(shop, obj, *ids);
        ids++;
    }

    if (Mc_SaveData.demoScene == 1) {
        Mc_SaveData.shopTiers = 0x1FFF;
        Mc_SaveData.shopStock = -1;
    }

    if (Mc_SaveData.gameMode == 0) {
        if (Mc_SaveData.shopTiers != 0) {
            for (tier = 0; tier < 13; tier++) {
                unlocked = Mc_SaveData.shopTiers & (1 << tier);
                if (unlocked != 0) {
                    for (j = 0; j < 3; j++) {
                        item = D_dryfield_night_trailer_coach_801843F4[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_dryfield_night_trailer_coach_8017E318(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_dryfield_night_trailer_coach_8017E318(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_dryfield_night_trailer_coach_8017E318(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_dryfield_night_trailer_coach_8017E318(shop, obj, item);
                                }
                                break;
                        }
                    }
                }
            }
        }

        if ((mode >> 16) == 3) {
            for (slot = 0; slot < 0xC; slot++) {
                level = (Mc_SaveData.shopStock >> (slot * 2)) & 3;
                if (slot == 0 ? level >= 2 : level > 0) {
                    /* The assignment keeps `+ 0xE` on the level instead of
                       letting GCC reassociate it onto the row base. */
                    func_dryfield_night_trailer_coach_8017E318(shop, obj, slot * 3 + (id = level + 0xE));
                }
            }
        }
    }

    list = (RoomShopList*)obj->owner->work;
    for (i = 0; i < shop->list.field_4 - 1; i++) {
        key = Gp_ItemSortKey(list->items[i]);
        for (k = i + 1; k < shop->list.field_4; k++) {
            otherKey = Gp_ItemSortKey(list->items[k]);
            if (otherKey < key) {
                tmp            = list->items[i];
                key            = otherKey;
                list->items[i] = list->items[k];
                list->items[k] = tmp;
            }
        }
    }

    count              = shop->list.field_4;
    shop->list.field_5 = count;
    if ((s8)count >= 0xA) {
        shop->list.field_5 = 9;
    }
    D_dryfield_night_trailer_coach_80184490 = -1;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach", D_dryfield_night_trailer_coach_8017D748);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach", D_dryfield_night_trailer_coach_8017D754);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach", D_dryfield_night_trailer_coach_8017D75C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach", D_dryfield_night_trailer_coach_8017D768);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach", D_dryfield_night_trailer_coach_8017D770);
