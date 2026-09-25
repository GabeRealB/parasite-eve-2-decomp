#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include "decomp/common.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/mist_parking.h"

extern UiObjectDesc  D_800611E4;
extern UiObject*     D_80067634;
extern u8            D_80071072;
extern u8            D_80071086;
extern u8            D_8007216D;
extern u8            D_80072170;
extern u16           D_80072174;
extern s8            D_80072176;
extern GpItemScan    D_80072724;
extern u16           D_80072834;
extern u16           D_80072836;
extern u8            D_80072A93;
extern s32           D_80072A94;
extern s32           D_80072A98;
extern UiObjectDesc  D_8010D80C;
extern RoomShopStock D_8010E138[];
extern UiObjectDesc  D_8010EFA0;
extern s16           D_80114D08;
extern u32           D_80115694;
extern u8            D_801156F9;
extern char          Gp_StrEmpty[];

/// The 0xFFFF-terminated item id lists `func_mist_parking_8017D8F8` chooses
/// from.
extern u16 D_mist_parking_80186058[];
extern u16 D_mist_parking_80186060[];
extern u16 D_mist_parking_80186068[];
extern u16 D_mist_parking_80186070[];
extern u16 D_mist_parking_80186080[];
extern u16 D_mist_parking_80186090[];
extern u16 D_mist_parking_801860A0[];
extern u16 D_mist_parking_801860A8[];
extern u16 D_mist_parking_801860B8[];
extern u16 D_mist_parking_801860C8[];
extern u16 D_mist_parking_801860D8[];
extern u16 D_mist_parking_801860E0[];
extern u16 D_mist_parking_801860F4[];
extern u16 D_mist_parking_8018610C[];
extern u16 D_mist_parking_80186120[];
extern u16 D_mist_parking_80186128[];
extern u16 D_mist_parking_80186138[];
extern u16 D_mist_parking_80186150[];
extern u16 D_mist_parking_80186164[];
extern u16 D_mist_parking_8018616C[];
extern u16 D_mist_parking_80186180[];
extern u16 D_mist_parking_8018619C[];
extern u16 D_mist_parking_801861AC[];
extern u16 D_mist_parking_801861B8[];
extern u16 D_mist_parking_801861D0[];
extern u16 D_mist_parking_801861EC[];
extern u16 D_mist_parking_80186200[];
extern u16 D_mist_parking_80186208[];
extern u16 D_mist_parking_8018621C[];
extern u16 D_mist_parking_8018623C[];
extern u16 D_mist_parking_8018624C[];
extern u16 D_mist_parking_80186258[];
extern u16 D_mist_parking_80186270[];
extern u16 D_mist_parking_80186274[];
extern u16 D_mist_parking_80186278[];
extern u16 D_mist_parking_80186280[];
extern u16 D_mist_parking_80186290[];
extern u16 D_mist_parking_80186298[];
extern u16 D_mist_parking_801862A0[];
extern u16 D_mist_parking_801862A8[];
extern u16 D_mist_parking_801862B4[];
extern u16 D_mist_parking_801862BC[];
extern u16 D_mist_parking_801862C8[];
extern u16 D_mist_parking_801862D0[];
extern u16 D_mist_parking_801862DC[];
extern u16 D_mist_parking_801862E8[];
extern u16 D_mist_parking_801862F0[];
extern u16 D_mist_parking_801862F8[];
extern u16 D_mist_parking_80186304[];
extern u16 D_mist_parking_80186310[];
extern u16 D_mist_parking_80186318[];
extern u16 D_mist_parking_80186324[];
extern u16 D_mist_parking_80186330[];
extern u16 D_mist_parking_8018633C[];
extern u16 D_mist_parking_80186340[];
extern u16 D_mist_parking_8018634C[];
extern u16 D_mist_parking_80186358[];
extern u16 D_mist_parking_80186364[];
extern u16 D_mist_parking_8018636C[];
extern u16 D_mist_parking_80186378[];
extern u16 D_mist_parking_80186384[];
extern u16 D_mist_parking_80186390[];
extern u16 D_mist_parking_80186398[];
extern u16 D_mist_parking_801863A4[];
extern u16 D_mist_parking_80186534[];

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists and the high halfword one of
/// the group's four; `Mc_SaveData.gameMode` 2 and above has groups of its own,
/// and anything unmatched falls back to `D_mist_parking_80186534`.
u16* func_mist_parking_8017D8F8(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186138;
                    case 1:
                        return D_mist_parking_80186150;
                    case 2:
                        return D_mist_parking_80186164;
                    case 3:
                        return D_mist_parking_8018616C;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186180;
                    case 1:
                        return D_mist_parking_8018619C;
                    case 2:
                        return D_mist_parking_801861AC;
                    case 3:
                        return D_mist_parking_801861B8;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801861D0;
                    case 1:
                        return D_mist_parking_801861EC;
                    case 2:
                        return D_mist_parking_80186200;
                    case 3:
                        return D_mist_parking_80186208;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_8018621C;
                    case 1:
                        return D_mist_parking_8018623C;
                    case 2:
                        return D_mist_parking_8018624C;
                    case 3:
                        return D_mist_parking_80186258;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186080;
                    case 1:
                        return D_mist_parking_80186090;
                    case 2:
                        return D_mist_parking_801860A0;
                    case 3:
                        return D_mist_parking_801860A8;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801860F4;
                    case 1:
                        return D_mist_parking_8018610C;
                    case 2:
                        return D_mist_parking_80186120;
                    case 3:
                        return D_mist_parking_80186128;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801860B8;
                    case 1:
                        return D_mist_parking_801860C8;
                    case 2:
                        return D_mist_parking_801860D8;
                    case 3:
                        return D_mist_parking_801860E0;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186058;
                    case 1:
                        return D_mist_parking_80186060;
                    case 2:
                        return D_mist_parking_80186068;
                    case 3:
                        return D_mist_parking_80186070;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186304;
                    case 1:
                        return D_mist_parking_80186310;
                    case 2:
                        return D_mist_parking_80186318;
                    case 3:
                        return D_mist_parking_80186324;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186330;
                    case 1:
                        return D_mist_parking_8018633C;
                    case 2:
                        return D_mist_parking_80186340;
                    case 3:
                        return D_mist_parking_8018634C;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186358;
                    case 1:
                        return D_mist_parking_80186364;
                    case 2:
                        return D_mist_parking_8018636C;
                    case 3:
                        return D_mist_parking_80186378;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186384;
                    case 1:
                        return D_mist_parking_80186390;
                    case 2:
                        return D_mist_parking_80186398;
                    case 3:
                        return D_mist_parking_801863A4;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186290;
                    case 1:
                        return D_mist_parking_80186298;
                    case 2:
                        return D_mist_parking_801862A0;
                    case 3:
                        return D_mist_parking_801862A8;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801862DC;
                    case 1:
                        return D_mist_parking_801862E8;
                    case 2:
                        return D_mist_parking_801862F0;
                    case 3:
                        return D_mist_parking_801862F8;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801862B4;
                    case 1:
                        return D_mist_parking_801862BC;
                    case 2:
                        return D_mist_parking_801862C8;
                    case 3:
                        return D_mist_parking_801862D0;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186270;
                    case 1:
                        return D_mist_parking_80186274;
                    case 2:
                        return D_mist_parking_80186278;
                    case 3:
                        return D_mist_parking_80186280;
                }
                break;
        }
    }
    return D_mist_parking_80186534;
}

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
extern u8           D_mist_parking_80186480[];
extern u8           D_mist_parking_8018646C[];
extern u8           D_mist_parking_8018647C[];
extern UiObjectDesc D_mist_parking_801865E4;
extern UiObjectDesc D_mist_parking_80186638;

/// Draws one row of the shop list and handles its input. Row 0xFFFE is greyed
/// out unless `Gp_HasMappedItem` answers non-zero and opens its own panel;
/// row 0xFFFC is greyed out while the scan holds item 0x8F. Any other row is
/// an item with its price, greyed out when `func_800B7420` refuses it; confirm
/// opens the buy panel and button 0x10 the item's detail panel.
void func_mist_parking_8017DF68(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq   req;
    u8            buf[0x20];
    RoomShopList* shop;
    GpItemScan*   scan;
    s32           y;
    s32           scaled;
    UiObject*     child;
    UiObject*     child2;
    s32           blocked;
    s32           status;
    s32           itemId;
    s32           price;

    shop    = (RoomShopList*)obj->owner->work;
    blocked = 0;
    itemId  = shop->items[prompt->field_8];
    /* &D_80072724 hoisted into a saved register here, as the original does,
       instead of being rematerialised at the Gp_SumScanQty call. */
    scan = &D_80072724;
    if (prompt->field_C == 1) {
        D_mist_parking_8018644C = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->status;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam((s32)D_mist_parking_80186480, 0, 0);
            }
        }
        if (Gp_HasMappedItem() == 0) {
            prompt->field_1C = Ui_LookupTable(obj, 2);
            prompt->field_C  = 0;
        }
        req.x          = obj->baseX + prompt->field_18;
        y              = obj->baseY - 4;
        req.y          = prompt->field_1A + y;
        req.otIndex    = (s16)obj->drawOrder + 1;
        req.field_8    = prompt->field_1C;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, D_mist_parking_8018646C);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_mist_parking_80186638, 0, 1, 1, obj);
            obj->status = 0;
        }
        return;
    }

    if (itemId == 0xFFFC) {
        status = obj->status;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam((s32)Gp_StrEmpty, 0, 0);
            }
        }
        if (Gp_SumScanQty(scan, 0x8F) != 0) {
            blocked          = 1;
            prompt->field_1C = Ui_LookupTable(obj, 2);
        }
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_8018647C, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_mist_parking_801865E4, itemId, 1, 1, obj);
            if (child != NULL) {
                Ui_ClampDialogRect((UiPanel*)child, (UiPanel*)prompt, (UiPanel*)obj);
                obj->status = 0;
            }
        }
        return;
    }

    price = Gp_ItemDescs[itemId].price;
    if (func_800B7420(itemId) != 0) {
        blocked          = 1;
        prompt->field_1C = Ui_LookupTable(obj, 2);
    }
    if (prompt->field_22 != 0x41) {
        status = obj->status;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Gp_SetHolderItemText(itemId);
                Gp_SetPreviewItem(itemId, 0);
            }
        }
    }
    if (prompt->field_C == 1) {
        if (blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            child2 = Ui_SpawnFromDesc(&D_mist_parking_801865E4, itemId, 1, 1, obj);
            if (child2 != NULL) {
                SndEvt_EnqueueType6(0x16, 0, 0);
                Ui_ClampDialogRect((UiPanel*)child2, (UiPanel*)prompt, (UiPanel*)obj);
                obj->status = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, itemId, 1, 1, obj);
            obj->status = 0;
        }
    }
    Gp_DrawItemLabel(obj, prompt->field_18, prompt->field_1A, itemId, prompt->field_1C, 0);
    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: emits the scaled index before the table base so the
           `addu` is index-first, matching the original. */
        scaled = itemId * 4;
        Gp_DrawQty(obj, prompt->field_18, prompt->field_1A, D_8010E138[itemId].perBuy, prompt->field_1C);
    }
    Text_ItoaUnsigned(buf, price);
    Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, buf, prompt->field_1C, 3, 2);
}

/// Adds an item id to the room's shop list, keeping one entry per item kind:
/// ids 0xF..0x32 are three consecutive levels of the same kind, so an entry of
/// the same kind is overwritten only by a higher level.
void func_mist_parking_8017E3F4(RoomShopList* shop, UiObject* obj, s32 item)
{
    Task*         task = obj->owner;
    s32           mode = task->spawnArg1;
    RoomShopList* list = (RoomShopList*)task->work;
    s32           i;

    for (i = 0; i < shop->list.field_4; i++) {
        s32 cur = list->items[i];
        s32 q;

        if (cur == item) {
            return;
        }
        if (((mode & 0xFFFF) == 0x10) &&
            (((u32)(item - 0x9D) < 3U) || (item == 0x8A) || (item == 0x65))) {
            return;
        }
        if (((u32)(item - 0xF) < 0x24U) && ((u16)(cur - 0xF) < 0x24U)) {
            q = (item - 0xF) / 3;
            if ((q == (cur - 0xF) / 3) && (((item - 0xF) % 3 + 1) > ((cur - 0xF) % 3 + 1))) {
                list->items[i] = item;
                return;
            }
        }
    }

    Gp_SetItemSeenBit(item, 1);
    list->items[shop->list.field_4] = item;
    shop->list.field_4++;
}

extern RoomShopTier D_mist_parking_801863B0[13];

/// Fills `shop` with the ids the vending machine currently offers, then sorts
/// them by `Gp_ItemSortKey` and caps the visible row count at 9.
///
/// The upper halfword of the owning task's `spawnArg1` picks the machine's
/// mode, which decides both the fixed id list (`func_mist_parking_8017D8F8`)
/// and which of a price row's items the machine will stock: mode 0 takes tools
/// (0x80-0x9F) plus a handful of key items, mode 1 armour (0xA0-0xBF), mode 2
/// weapon parts (0x60-0x7F) and mode 3 everything up to 0x5F that the other
/// three modes do not carry. Mode 3 additionally offers the twelve two-bit
/// stock levels the save keeps in `Mc_SaveData.shopStock`, whose first slot
/// needs a level of 2 rather than 1.
void func_mist_parking_8017E540(RoomShopList* shop, UiObject* obj)
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
    ids  = func_mist_parking_8017D8F8(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_mist_parking_8017E3F4(shop, obj, *ids);
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
                        item = D_mist_parking_801863B0[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_mist_parking_8017E3F4(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_mist_parking_8017E3F4(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_mist_parking_8017E3F4(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_mist_parking_8017E3F4(shop, obj, item);
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
                    func_mist_parking_8017E3F4(shop, obj, slot * 3 + (id = level + 0xE));
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
    D_mist_parking_8018644C = -1;
}

/// Titles and labels of the shop's panels.
const u8 D_mist_parking_8017D6D0[] = "Select";
const u8 D_mist_parking_8017D6D8[] = "BP";
const u8 D_mist_parking_8017D6DC[] = "List";
const u8 D_mist_parking_8017D6E4[] = "TOTAL";
const u8 D_mist_parking_8017D6EC[] = "Notice";

/// "Charge", with a stray non-zero byte after its terminator that C cannot
/// place, so the string stays assembly.
extern u8 D_mist_parking_8017D6F4[];
INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D6F4);

/// Messages of the shop's panels.
extern u8 D_mist_parking_80186450[];
extern u8 D_mist_parking_80186464[];
extern u8 D_mist_parking_801864BC[];
extern u8 D_mist_parking_801864C4[];
extern u8 D_mist_parking_801864D0[];
extern u8 D_mist_parking_801864D8[];
extern u8 D_mist_parking_801864E0[];
extern u8 D_mist_parking_801864F4[];
extern u8 D_mist_parking_80186504[];
extern u8 D_mist_parking_80186524[];
extern u8 D_mist_parking_80186530[];

/// Row handlers, lists and panel descriptors of the shop's panels.
extern UiListItemFunc D_mist_parking_80186538[];
extern UiList         D_mist_parking_80186540;
extern UiList         D_mist_parking_8018656C;
extern UiObjectDesc   D_mist_parking_801865AC;
extern UiObjectDesc   D_mist_parking_801865C8;
extern UiObjectDesc   D_mist_parking_80186600;
extern UiObjectDesc   D_mist_parking_80186654;
extern UiObjectDesc   D_mist_parking_80186670;

/// Work pair of the charge panel `func_mist_parking_8017F49C`.
extern s32        D_mist_parking_80195310;
extern GpItemMap* D_mist_parking_80195314;

void func_mist_parking_8017E90C(Task* task)
{
    TextDrawReq   req;
    UiObject*     obj;
    RoomShopList* shop;
    Task*         head;
    Task*         child;
    Task*         next;
    UiObject*     childObj;
    void*         mem;
    s32           code;
    s32           x;
    s32           y;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_mist_parking_8017D6D0);
    if (task->state == 0) {
        mem  = memCalloc(sizeof(RoomShopList), 0);
        shop = mem;
        if (mem != NULL) {
            /* Keeps the allocation's own register distinct from the tested one,
               so the null test stays on $v0 and the copy fills its delay slot. */
            SOFT_TOUCH_REG(shop);
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_mist_parking_80186538;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_mist_parking_8017E540(shop, obj);
            Ui_LayoutListPanel(&shop->list, (UiPanel*)obj);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->field_12      += 8;
            shop->list.field_17 = 8;
            Ui_SpawnFromDesc(&D_mist_parking_80186654, 0, 0, 0, obj);
            task->state += 1;
        }
    }
    Ui_UpdateListNoAnim(task->work, obj);
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 6);

    x              = obj->baseX - 2;
    req.x          = obj->field_1E + x;
    y              = obj->baseY + 2;
    req.y          = obj->field_18 + y;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    func_8002E53C(&req, D_mist_parking_8017D6D8);

    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        }
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            code     = childObj->field_2E;
            next     = child->nextSibling;
            if (code != -1) {
                if (code == 6) {
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->status = 1;
                }
            } else {
                obj->field_2E = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

void func_mist_parking_8017EB5C(DialogPrompt* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_80186464, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_mist_parking_801864BC;
    obj->owner->spawnArg1 = (u16)obj->owner->spawnArg1;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_mist_parking_801864C4;
            obj->owner->spawnArg1 |= 0x10000;
            break;
        case 2:
            text                   = D_mist_parking_801864D0;
            obj->owner->spawnArg1 |= 0x20000;
            break;
        case 3:
            text                   = D_mist_parking_801864D8;
            obj->owner->spawnArg1 |= 0x30000;
            break;
    }

    if (*func_mist_parking_8017D8F8(obj->owner->spawnArg1) == 0xFFFF) {
        prompt->field_1C = Ui_LookupTable(obj, 2);
        prompt->field_C  = 0;
    }

    one2 = 1;
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, text, prompt->field_1C, one2, 0);

    status = obj->status;
    if (((status >> 16) == one2) || (status == one2)) {
        if (prompt->field_10 == prompt->field_8) {
            Ui_SetHolderParam((s32)Gp_StrEmpty, 0, 0);
        }
    }

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mist_parking_801865AC, obj->owner->spawnArg1, 1, 1, obj);
        obj->status = 0;
    }
}

void func_mist_parking_8017ED7C(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2;
    list          = &D_mist_parking_80186540;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_mist_parking_8017D6DC);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_mist_parking_801865C8, task->spawnArg1, 0, 1, obj);
        Ui_SpawnFromDesc(&D_8010D80C, 0, 0, 0, obj);
        list->field_4 = 5;
        list->field_5 = 5;
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
        obj->field_2E = -1;
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            code     = childObj->field_2E;
            next     = child->nextSibling;
            if (code != -1) {
                if (code == 6) {
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->status = 1;
                }
            } else {
                Wip_UiHolder  = NULL;
                obj->field_2E = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

void func_mist_parking_8017EF24(Task* task)
{
    s8            digits[0x20];
    s8            total[0x20];
    TextDrawReq   req0;
    TextDrawReq   req1;
    UiObject*     obj;
    PlayerStatus* cfg;
    McItemScan*   scan;
    s8*           p;
    s32           x;
    s32           y;
    s32           y2;
    s32           col;
    s32           capacity;
    s32           count;

    obj = task->spawnArg2;
    cfg = &Player_Status;
    x   = (s16)obj->field_1C + 2;
    col = (s16)obj->field_1E - 2;
    y   = (s16)obj->field_18;

    req0.x          = obj->baseX + x;
    req0.y          = obj->baseY + y + 9;
    req0.otIndex    = (s16)obj->drawOrder + 1;
    req0.field_8    = 0x606060;
    req0.glyphTable = 5;
    req0.centerMode = 0;
    req0.field_E    = 1;
    func_8002E53C(&req0, D_mist_parking_8017D6D8);

    Text_ItoaUnsigned((u8*)digits, cfg->bp);
    Text_DrawPrompt(obj, col, y + 0x19, (u8*)digits, 0x606060, 3, 2);

    y2              = y + 0x28;
    req1.x          = obj->baseX + x;
    req1.y          = obj->baseY + (y2 - 6);
    req1.otIndex    = (s16)obj->drawOrder + 1;
    req1.field_8    = 0x606060;
    req1.glyphTable = 5;
    req1.centerMode = 0;
    req1.field_E    = 1;
    func_8002E53C(&req1, (char*)D_mist_parking_8017D6E4);

    p        = total;
    scan     = &Mc_SaveData.carriedItems;
    count    = Gp_CountScanItems(scan);
    capacity = scan->rowCount;
    Text_ItoaUnsigned((u8*)p, count);
    while (*p != 0) {
        p++;
    }
    *p = '/';
    Text_ItoaUnsigned((u8*)(p + 1), capacity);
    Text_DrawPrompt(obj, col, y2 + 0xA, (u8*)total, 0x606060, 3, 2);
}

void func_mist_parking_8017F108(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq   req;
    UiObject*     child;
    PlayerStatus* cfg;
    GpItemScan*   scan;
    s32           itemId;
    s32           mode;
    s32           price;

    itemId = obj->owner->spawnArg1;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_mist_parking_80186450);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &D_80072724;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_mist_parking_80186600, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_mist_parking_80186600, 1, 1, 1, obj);
                }
                obj->status = 0;
            } else if ((obj->owner->parent->spawnArg1 >> 16) == mode) {
                child = Ui_SpawnFromDesc(&D_mist_parking_80186670, itemId, 1, 1, obj);
                if (child != NULL) {
                    Ui_ClampDialogRect((UiPanel*)child, (UiPanel*)prompt, (UiPanel*)obj);
                    obj->status = 0;
                }
            } else {
                cfg->bp -= price;
                Gp_GiveItem(scan, itemId, -1);
                obj->field_2E = 6;
            }
        } else {
            Ui_SpawnFromDesc(&D_mist_parking_80186600, 0, 1, 1, obj);
            obj->status = 0;
        }
    }
}

void func_mist_parking_8017F31C(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1;
    obj  = task->spawnArg2;
    switch (kind) {
        case 1:
            text = D_mist_parking_801864F4;
            break;
        case 2:
            text = D_mist_parking_80186504;
            break;
        default:
            text = D_mist_parking_801864E0;
            break;
    }

    Ui_DrawText((UiPanel*)obj, (char*)D_mist_parking_8017D6EC);
    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_SizeFromTextPlain((UiPanel*)obj, text);
        task->killCountdown = 0xBC;
        task->state        += 1;
    }
    Text_DrawMultiLine(obj, (s16)obj->field_1C + 2, (s16)obj->field_18 + 0xF, text, 0x606060, 1, 0);
    task->killCountdown -= D_80071072;
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
            return;
        }
        if (task->killCountdown <= 0 || Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            ((UiObject*)task->parent->spawnArg2)->field_2E = 6;
            task->killCountdown                            = 0x7FFF;
        }
    }
}

/// Panel that walks the mapped item slots one at a time: each slot's ammo or
/// attachment quantity is reset to its related quantity, and a bar animates
/// from the old value up to the new one for at most 0xBC frames. Confirm or
/// cancel (or the timer running out) moves to the next slot; when no slot is
/// left the panel reports code 6 to its parent.
void func_mist_parking_8017F49C(Task* task)
{
    UiObject*   obj;
    GpItemMap*  map;
    GpItemSlot* slot;
    s32         slotId;
    s32         itemId;
    s32         curItem;
    s32         relItem;
    s32         qty;
    s32         y;
    s32         h;
    s32         status;
    s16         countdown;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_mist_parking_8017D6F4);

    if (task->state == 0) {
        task->spawnArg1 = 0;
        task->state     = task->state + 1;
    }
    if (task->state == 1) {
        slotId          = Gp_NextMappedSlot(task->spawnArg1);
        task->spawnArg1 = slotId;
        if (slotId < 0) {
            obj->field_2E = 6;
        } else {
            map                     = Gp_GetItemMap(slotId);
            D_mist_parking_80195314 = map;
            itemId                  = map->field_1;
            slot                    = Gp_GetItemSlot(itemId);
            if (D_mist_parking_80195314->field_0 == 0) {
                D_mist_parking_80195310 = slot->ammoQty;
                slot->ammoQty           = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_mist_parking_80195310 = slot->attachQty;
                slot->attachQty         = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown       = 0xBC;
            D_mist_parking_80195310 <<= 8;
            task->state               = task->state + 1;
        }
    }

    curItem = D_mist_parking_80195314->field_1;
    relItem = D_mist_parking_80195314->field_2;
    if (D_mist_parking_80195314->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                    <<= 8;
    D_mist_parking_80195310 += 0x40;
    if (qty < D_mist_parking_80195310) {
        D_mist_parking_80195310 = qty;
    }

    y = (s16)obj->field_18;
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->field_1C + 2, y + 0x23, D_mist_parking_80195310 >> 8, 0x606060);
    h = (s16)obj->field_1A;
    func_800C0E20((UiPanel*)obj, (s16)obj->field_1C + 2, (s16)obj->field_1E - 2, h - 6, qty,
                  D_mist_parking_80195310, 0x1741F);

    if (task->state == 2) {
        countdown           = task->killCountdown - 1;
        task->killCountdown = countdown;
        status              = obj->status;
        if (status == 1 && (countdown <= 0 || Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskConfirm) != 0)) {
            task->state     = status;
            task->spawnArg1 = task->spawnArg1 + 1;
        }
    }
}

/// Draws the preview of the item the shop list's cursor rests on and, for an
/// item id below 0x100, a label with how many of it the player already holds.
/// Stackable items (0xA0..0xBF) ask the scan for their stack quantity;
/// everything else is counted by walking the item table.
///
/// `guard` is the register the loop's entry test reads. The target compares a
/// copy of `count` (`move s4,s3` in the branch delay slot) rather than the
/// loop counter, and everything that keeps GCC 2.8.1 on that shape is
/// codegen-only:
///  - the dead `guard = 0` after the last draw makes `guard`, not `count`,
///    the cse-canonical zero, so the duplicated exit test is rewritten onto it;
///  - the two soft uses give it four references: with two, local-alloc moves a
///    single-use constant init next to its use (into the else block, past the
///    label, where the `move` from `count` can no longer be formed), and with
///    three it colours after `item` (`$s5`) instead of before (`$s4`).
void func_mist_parking_8017F764(Task* task)
{
    u8          buf[0x10];
    TextDrawReq req;
    UiObject*   obj;
    s32         item;
    s32         y;
    s32         ry;
    s32         count;
    s32         guard;
    s32         i;
    s32         n;
    GpItemRec*  rec;

    item         = D_mist_parking_8018644C;
    obj          = task->spawnArg2;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_mist_parking_8018644C == Gp_GetPreviewItem()) {
        func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, 0x20);
    } else {
        func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, 0x120);
    }
    y = (s16)obj->field_18 + 0x50;
    if (item < 0x100) {
        req.x          = obj->field_1C + (obj->baseX + 2);
        ry             = obj->baseY - 6;
        req.y          = ry + y;
        req.otIndex    = (s16)obj->drawOrder + 1;
        req.glyphTable = 5;
        req.field_8    = 0x606060;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, D_mist_parking_80186524);
        count = 0;
        guard = 0;
        if ((u32)(item - 0xA0) < 0x20U) {
            count = Gp_ScanStackQty(&D_80072724, item);
        } else {
            rec = Gp_GetItemTable(&D_80072724) + D_80072724.firstRow;
            n   = D_80072724.rowCount;
            SOFT_USE_REG2(guard, guard);
            for (i = 0; i < n; i++) {
                if (rec[i].itemId == item) {
                    count++;
                }
            }
        }
        Text_DrawPrompt(obj, (s16)obj->field_1E - 2, y + 0xA, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
        guard = 0;
    }
}

void func_mist_parking_8017F938(Task* task)
{
    u8          buf[0x20];
    TextDrawReq req;
    UiObject*   obj;
    UiObject*   parentObj;
    s32         itemId;
    s32         price;
    s32         maxQty;
    s32         scaled;
    s32         afford;
    s32         held;
    /* The stock ceiling stays in $v0, so the scan count the shop just fetched
       has to be copied out of the return register instead of coalescing into
       it. */
    register s32 maxHeld asm("v0");
    s32          count;
    s32          left;
    s32          top;
    s32          x;
    s32          y;
    s32          i;

    itemId = task->spawnArg1;
    obj    = task->spawnArg2;
    maxQty = 1;
    price  = Gp_ItemDescs[itemId].price;

    if (task->state == 0) {
        task->extraState = 1;
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(3) - 3);
        task->state = task->state + 1;
    }

    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: emits the scaled index before the table base so the
           `addu` is index-first, matching the original. */
        scaled = itemId * 4;
        if (D_8010E138[itemId].perBuy != 0) {
            held    = Gp_ScanStackQty(&D_80072724, itemId);
            maxHeld = D_8010E138[itemId].maxHeld;
            maxQty  = maxHeld - held;
            if (maxQty <= 0) {
                maxQty = 1;
            } else {
                maxQty = (maxQty - 1) / D_8010E138[itemId].perBuy;
                maxQty = maxQty + 1;
            }
        }
    } else {
        maxQty = D_80072724.rowCount - Gp_CountScanItems(&D_80072724);
    }

    afford = Player_Status.bp / price;
    if (afford < maxQty) {
        maxQty = afford;
    }

    left = (s16)obj->field_1C;
    x    = left + 2;
    top  = (s16)obj->field_18;
    y    = top + 0xF;
    Gp_DrawItemLabel(obj, x, y, itemId, 0x606060, 0);
    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: same index-first ordering as above. */
        scaled = itemId * 4;
        Gp_DrawQty(obj, x, y, D_8010E138[itemId].perBuy, 0x606060);
    }

    count = task->extraState;
    Text_DrawPrompt(obj, left + 0x98, y, D_mist_parking_80186530, 0x606060, 3, 2);
    Text_DrawPrompt(obj, -x, y, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
    Ui_DrawHBar((UiPanel*)obj, left, -x + 2, top + 0x12);

    req.x          = obj->baseX - x;
    y              = top + 0x1A;
    req.y          = obj->baseY + y;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    func_8002E53C(&req, D_mist_parking_8017D6D8);

    Text_DrawPrompt(obj, -x, top + 0x2B, Text_ItoaSigned(buf, count * price), 0x606060, 3, 2);

    if (obj->status == 1) {
        parentObj = task->parent->spawnArg2;
        if (Pad_CheckButtons(0, 1, 0x3000) != 0) {
            if (task->extraState < maxQty) {
                task->extraState = task->extraState + 1;
                SndEvt_EnqueueType6(0x15, 0, 0);
            }
        } else if (Pad_CheckButtons(0, 1, 0xC000) != 0) {
            if (task->extraState >= 2) {
                task->extraState = task->extraState - 1;
                SndEvt_EnqueueType6(0x15, 0, 0);
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            Player_Status.bp -= price * task->extraState;
            for (i = 0; i < task->extraState; i++) {
                Gp_GiveItem(&D_80072724, itemId, -1);
            }
            SndEvt_EnqueueType6(0x16, 0, 0);
            parentObj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            parentObj->field_2E = 6;
        }
    }
}

void func_mist_parking_8017FDB8(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_mist_parking_80186464);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

void func_mist_parking_8017FE74(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_mist_parking_8018656C;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }

    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        code     = childObj->field_2E;
        if (code != -1) {
            if (code == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->status = 1;
            }
        } else {
            obj->field_2E = -1;
        }
    }
}

/// Descriptor of the panel `func_mist_parking_8017FF9C` opens.
extern UiObjectDesc D_mist_parking_80186590;

/// Opens the panel `D_mist_parking_80186590` with the task's `spawnArg1` as
/// its parameter, setting frame timing 0 and the session's UI flag while it is
/// open; once the panel reports -1 or 6 it is torn down, and ten frames later
/// frame timing 1 and the flag are restored and the task kills itself.
void func_mist_parking_8017FF9C(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_mist_parking_80186590, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        gGameSession->uiOpen = 1;
        task->spawnArg2      = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            gGameSession->uiOpen = 0;
            taskKill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

/// Labels, suffix and holder texts of the play-data summary rows.
extern u8 D_mist_parking_801866C0[];
extern u8 D_mist_parking_801866F0[];
extern u8 D_mist_parking_801866C8[];
extern u8 D_mist_parking_801866CC[];
extern u8 D_mist_parking_801866D4[];
extern u8 D_mist_parking_801866E0[];
extern u8 D_mist_parking_801866F8[];
extern u8 D_mist_parking_80186700[];
extern u8 D_mist_parking_80186708[];
extern u8 D_mist_parking_80186710[];
extern u8 D_mist_parking_8018671C[];
extern u8 D_mist_parking_80186748[];
extern u8 D_mist_parking_8018676C[];
extern u8 D_mist_parking_8018679C[];
extern u8 D_mist_parking_801867D0[];
extern u8 D_mist_parking_80186804[];
extern u8 D_mist_parking_8018683C[];
extern u8 D_mist_parking_80186870[];
extern u8 D_mist_parking_801868A8[];

void func_mist_parking_801800D0(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_mist_parking_8018671C,
                D_mist_parking_80186748,
                D_mist_parking_8018676C,
                D_mist_parking_8018679C,
                D_mist_parking_801867D0,
                D_mist_parking_80186804,
                D_mist_parking_8018683C,
                D_mist_parking_80186870,
                D_mist_parking_801868A8,
            };

            Ui_SetHolderParam((s32)tbl[arg0->field_8], 0, 0);
        }
    }

    switch (arg0->field_8) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_801866C0);
            Text_FormatTime(p, D_80072174);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_801866F0);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_mist_parking_80186710);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_801866C8);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_mist_parking_80186710);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_801866CC);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_mist_parking_80186710);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 4: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_801866D4);
            if (Mc_SaveData.field_6CC == 0) {
                pct = 0;
            } else {
                pct = (Mc_SaveData.field_6CC * 10000) / (Mc_SaveData.field_6CC + Mc_SaveData.field_6CE);
            }
            if (pct < 100) {
                func_8002F44C(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_mist_parking_80186718);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 5: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         total;
            s32         cnt;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            total          = D_80072834;
            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_801866E0);
            cnt   = 326;
            total = total + (GameFlag_GetNibble(0x167) + GameFlag_GetNibble(0x168));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                func_8002F44C(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_mist_parking_80186718);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            Ui_DrawHBar((UiPanel*)arg1, arg1->field_1C, (s16)arg1->field_1E, arg0->field_1A + 3);
            arg0->field_1A = (u16)arg0->field_1A + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_801866F8);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_mist_parking_80186710);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_80186700);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A94), arg0->field_1C, 3, 2);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_mist_parking_80186708);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data menu panel `func_mist_parking_80181920` draws.
const char D_mist_parking_8017D748[] = "Play Data";

/// One row of the "Play Data" item-usage list: the item's name, its share of
/// all recorded uses as `NN.NN%` (or a flat `100.0%` once it is the only item
/// used), and a gauge whose width is the row's `barWidths` fraction of the
/// panel. Confirming the row opens the item's detail panel.

void func_mist_parking_8018089C(DialogPrompt* prompt, UiObject* obj)
{
    u8             buf[0x20];
    u8*            p;
    u8*            q;
    RoomItemUsage* work;
    POLY_G4*       prim;
    s32            itemId;
    s32            pct;
    s32            scale;
    s32            remaining;
    s32            i;
    s32            len;
    s32            n;
    s32            right;
    s32            lo;
    s32            barY;
    s32            ry;
    s32            color;
    s32            barW;
    s32            barX;
    s32            x0;
    s32            x1;
    s32            y0;
    s32            status;
    s32            one;
    s32            px;
    s32            py;
    TextDrawReq    req;
    TextDrawReq*   r;

    p = buf;
    /* The request's address is live across Gp_GetItemText, so the last field is
       written through it while the rest stay sp-relative. */
    r      = &req;
    work   = (RoomItemUsage*)obj->owner->work;
    itemId = work->itemIds[prompt->field_8];
    pct    = work->percents[prompt->field_8];
    px     = prompt->field_18;
    py     = prompt->field_1A;
    color  = prompt->field_1C;

    if (obj->mode != 5) {
        req.x          = obj->baseX + 0x11 + px;
        ry             = obj->baseY - 6;
        req.y          = ry + py;
        req.otIndex    = (s16)obj->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        func_8002E53C(r, (u8*)Gp_GetItemText(itemId, 0, 0));
        func_800CE5D0(obj, px, py, itemId);
    }

    if (pct >= 0x2710) {
        Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, "100.0%", prompt->field_1C, 3, 2);
    } else {
        scale     = 1;
        remaining = 2;
        do {
            scale *= 10;
            remaining--;
        } while (remaining > 0);

        if (pct < scale) {
            func_8002F44C(p, pct, 3);
        } else {
            Text_ItoaUnsigned(p, pct);
        }

        n   = 2;
        q   = p;
        len = 0;
        while (*q != 0) {
            q++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (i = 0; i < n; i++) {
            q[1] = q[0];
            q--;
        }
        q[1] = 0x2E;
        Text_Strcat(p, D_mist_parking_80186718);
        Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, buf, prompt->field_1C, 3, 2);
    }

    lo    = (s16)obj->field_1C + 0x80;
    right = (s16)obj->field_1E - 0x4A;
    barY  = (s16)prompt->field_1A - 0xC;
    barW  = right - lo;
    barW  = (barW * work->barWidths[prompt->field_8]) >> 12;
    barW += 2;
    barX  = right - barW;
    if (barW >= 2) {
        prim     = (POLY_G4*)gGpuPrimCursor;
        x0       = obj->baseX + barX + 1;
        prim->x2 = x0;
        prim->x0 = x0;

        gGpuPrimCursor   = prim + 1;
        y0               = obj->baseY;
        y0               = y0 + barY;
        y0              += 1;
        *(u32*)&prim->r3 = 0x10000;
        *(u32*)&prim->r1 = 0x10000;
        setlen(prim, 8);
        *(u32*)&prim->r0 = 0x100B0;
        setcode(prim, 0x38);
        *(u32*)&prim->r2 = 0x100B0;

        x1 = (u16)prim->x0 + barW;
        x1--;
        prim->y1 = y0;
        prim->y0 = y0;
        y0      += 8;
        prim->y3 = y0;
        prim->y2 = y0;
        prim->x3 = x1;
        prim->x1 = x1;
        addPrim(gGpuCurrentOt + (s16)obj->drawOrder + 1, prim);
    }

    one = 1;
    func_80046B34(obj, barX, (s16)prompt->field_1A - 0xC, barW, 9, 0, one);

    status = obj->status;
    if (((status >> 16) == one) || (status == one)) {
        if (prompt->field_10 == prompt->field_8) {
            Gp_SetPreviewItem(itemId, 0);
            Gp_SetHolderItemText(itemId);
        }
    }

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, 0x10) != 0) {
        SndEvt_EnqueueType6(3, 0, 0);
        Ui_SpawnFromDesc(&D_8010EFA0, itemId, 1, 1, obj);
        obj->status = 0;
    }
}

/// Titles of the two usage lists `func_mist_parking_801812B4` draws (item and
/// PE).
const char D_mist_parking_8017D75C[] = "Weapon Data";
const char D_mist_parking_8017D768[] = "PE Data";

/// "Telephone", the title `func_mist_parking_80181468` draws, with two stray
/// non-zero bytes after its terminator that C cannot place, so the string
/// stays assembly.
extern const char D_mist_parking_8017D770[];
INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D770);

/// Lists of the usage panel and of the play-data menu, and the descriptor of
/// the frame the usage panel spawns.
extern UiList       D_mist_parking_8018690C;
extern UiList       D_mist_parking_80186994;
extern UiObjectDesc D_mist_parking_80186930;

/// Builds the item-usage panel's three parallel arrays from the save's
/// per-item use counters (`Mc_SaveData.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `percents` - its share of all
/// recorded uses in hundredths of a percent, rounded - and `barWidths`, its
/// counter as a 12-bit fraction of the top row's. Both are scaled down by
/// halving until the top counter fits in 17 bits, so the multiply and the
/// shift cannot overflow.
void func_mist_parking_80180C98(UiList* list, UiObject* obj)
{
    RoomItemUsage* work;
    s32            count;
    s32            total;
    s32            i;
    s32            j;
    s32            k;
    s32            id;
    s32            tmp;
    s32            uses;
    s32            scale;
    s32            top;
    s32            shift;
    s16*           p;
    u8             c;

    count = 0;
    total = 0;
    work  = (RoomItemUsage*)obj->owner->work;
    p     = work->itemIds;

    for (i = 0; i < 0x20; i++) {
        id = i + 0x80;
        c  = *Gp_GetItemText(id, 0, 1);
        if ((c != 0) && (c != 0xA) && (Mc_SaveData.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += Mc_SaveData.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (Mc_SaveData.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
                    tmp = work->itemIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->itemIds[k + 1] = work->itemIds[k];
                    }
                    work->itemIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        top   = Mc_SaveData.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->percents[i] =
                (u32)((Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->barWidths[i] =
                (Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->field_4  = count;
    list->field_9  = 0;
    list->field_10 = 0;
}

/// Builds the PE-usage panel's `RoomPeUsage` block from the save's per-slot
/// use counters, in the same way `func_mist_parking_80180C98` builds the
/// item-usage one.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `Mc_SaveData.attachLevels[i]`
/// prints as `i * 3 + 0xF + level - 1` (a slot the player has never levelled
/// keeps the base id). Every slot with a non-zero counter in
/// `Mc_SaveData.attachUseCounts` - which really runs twelve entries wide, past the
/// seven the struct names - is appended and its counter summed. The ids are
/// then insertion-sorted by use count, most-used first, and each row gets
/// `percents`, its share of all recorded uses in hundredths of a percent, and
/// `barWidths`, its counter as a 12-bit fraction of the top row's. Both are
/// scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
void func_mist_parking_80180F94(UiList* list, UiObject* obj)
{
    RoomPeUsage* work;
    s16*         p;
    s32          count;
    s32          total;
    s32          i;
    s32          j;
    s32          k;
    s32          id;
    s32          slot;
    s32          uses;
    s32          scale;
    s32          shift;
    s32          top;
    s32          tmp;
    /* Matching only. The original object frames 24 bytes it never touches and
     * materialises GCC's `/3` magic constant before the first loop for a use
     * that no longer survives, so `scratch` buys the frame size and `magic`
     * plus the (instruction-free) SOFT_USE_REG below buy the allocation. */
    s32 magic;
    s16 scratch[12];

    count = 0;
    total = 0;
    i     = 0;
    work  = (RoomPeUsage*)obj->owner->work;
    p     = work->peIds;
    magic = 0x55555556;

    for (; i < 12; i++) {
        if (Mc_SaveData.attachUseCounts[i] > 0) {
            id = i * 3 + 0xF;
            *p = id;
            if (Mc_SaveData.attachLevels[i] != 0) {
                *p = id + (Mc_SaveData.attachLevels[i] - 1u);
            }
            p++;
            count++;
            total += Mc_SaveData.attachUseCounts[i];
        }
    }
    SOFT_USE_REG(magic);

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->peIds[i] - 0xF) / 3;
            uses = Mc_SaveData.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->peIds[j] - 0xF) / 3;
                if (Mc_SaveData.attachUseCounts[slot] < uses) {
                    tmp = work->peIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->peIds[k + 1] = work->peIds[k];
                    }
                    work->peIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        slot  = (work->peIds[0] - 0xF) / 3;
        top   = Mc_SaveData.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot               = (work->peIds[i] - 0xF) / 3;
            work->percents[i]  = (u32)((Mc_SaveData.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot               = (work->peIds[i] - 0xF) / 3;
            work->barWidths[i] = (Mc_SaveData.attachUseCounts[slot] << shift) / top;
        }
    }

    list->field_4  = count;
    list->field_9  = 0;
    list->field_10 = 0;
}

/// Usage panel task: `spawnArg1` 0 lists items, anything else PE. The first
/// frame allocates the rows' work block and fills it; cancel closes the panel,
/// and a child panel reporting -1 or 6 is torn down.
void func_mist_parking_801812B4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_mist_parking_8018690C;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_mist_parking_8017D75C);
    } else {
        Ui_DrawText((UiPanel*)obj, D_mist_parking_8017D768);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_mist_parking_80186930, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_mist_parking_80180C98(list, obj);
        } else {
            func_mist_parking_80180F94(list, obj);
        }
        Ui_InitList(list, (UiMiniObj*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->status = 1;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

void func_mist_parking_80181468(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s32       ready;
    s32       sel;
    s32       kind;
    s32       mode;
    s32       one;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    ready         = Mc_SaveData.demoScene == 1;
    list          = &D_mist_parking_80186994;
    one           = 1;
    if (Mc_SaveData.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->status   = 0;
            obj->field_4 |= 0x80000000;
            task->state   = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->status          = one;
        gGameSession->uiOpen = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        Ui_DrawText((UiPanel*)obj, D_mist_parking_8017D770);
        Ui_UpdateListNoAnim(list, obj);
    }
    if (obj->field_2E == 6) {
        obj->field_2E = 0;
        Ui_SetState4((Task*)obj, task);
        obj->status = 0;
    }
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
        }
        gGameSession->uiOpen = 0;
        obj->field_2E        = -1;
        obj->field_2C        = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        sel      = childObj->field_2E;
        switch (sel) {
            case 6:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else if (task->state == 3) {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                } else {
                    Ui_TeardownTree(childObj, childObj->owner);
                    SndEvt_EnqueueType6(0x3B, 0, 0);
                    Ui_StartCloseAnim((UiPanel*)obj, task);
                    obj->status = 1;
                }
                break;
            case -1:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                }
                break;
        }
    }
}

void func_mist_parking_80181760(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_mist_parking_80181E50;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_mist_parking_801817BC(u8* str, s32 decimals)
{
    s32 len;

    len = 0;
    if (decimals > 0) {
        if (*str != 0) {
            do {
                str += 1;
                len += 1;
            } while (*str != 0);
        }
        if (len < decimals) {
            decimals = len;
            SOFT_TOUCH_REG(decimals);
            decimals += 1;
        } else {
            decimals += 1;
        }
        len = 0;
        if (decimals > 0) {
            do {
                len   += 1;
                str[1] = str[0];
                str   -= 1;
            } while (len < decimals);
        }
        str[1] = '.';
    }
}

/// Renders `value` into `buf` as a fixed-point number with `decimals` digits
/// after the point, then appends the overlay's "%" suffix. The integer is
/// printed first (zero-padded to `decimals + 1` digits when it is too small to
/// fill them), then the last `decimals` characters are shifted one byte right
/// to open a slot for the '.'.

u8* func_mist_parking_8018182C(u8* buf, s32 value, s32 decimals)
{
    s32 remaining;
    s32 len;
    s32 shifted;
    s32 count;
    s32 scale;
    u8* p;

    scale     = 1;
    remaining = decimals;
    if (decimals > 0) {
        do {
            scale *= 10;
            remaining--;
        } while (remaining > 0);
    }

    if (value < scale) {
        func_8002F44C(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    count = decimals;
    p     = buf;
    len   = 0;
    if (count > 0) {
        if (*buf != 0) {
            do {
                p++;
                len++;
            } while (*p != 0);
        }
        if (len < count) {
            count = len;
        }
        count++;

        shifted = 0;
        if (count > 0) {
            do {
                p[1] = p[0];
                shifted++;
                p--;
            } while (shifted < count);
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_mist_parking_80186718);
    return buf;
}

/// List of the menu panel `func_mist_parking_80181920` draws.
extern UiList D_mist_parking_801868E4;

/// Texts of the four menu rows below, and the panels two of them open.
extern u8           D_mist_parking_80186698[];
extern u8           D_mist_parking_801866A0[];
extern u8           D_mist_parking_801866AC[];
extern u8           D_mist_parking_801866B8[];
extern UiObjectDesc D_mist_parking_8018694C;
extern UiObjectDesc D_mist_parking_80186968;

void func_mist_parking_80181920(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_mist_parking_801868E4;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_mist_parking_8017D748);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_mist_parking_80186930, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->field_12 += 5;
        list->field_A  = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
}

/// Queues a gouraud-shaded rectangle into the current OT one slot past the
/// panel's draw order. Origin is `field_20`/`field_22` plus (`arg1`, `arg2`);
/// `arg3`/`arg4` are width and height. Left vertices take `arg5`, right vertices
/// take `arg6`. A zero color or width < 2 draws nothing.
void func_mist_parking_80181A10(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    register s32 dx asm("v1");
    register s32 w asm("t3");
    POLY_G4*     prim;
    s16          x;
    s16          y;

    dx = arg1;
    w  = arg3;
    if ((arg5 != 0) && (w >= 2)) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        x              = arg0->field_20 + dx + 1;
        prim->x2       = x;
        prim->x0       = x;
        y              = arg0->field_22;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        *(u32*)&prim->r0 = arg5;
        setcode(prim, 0x38);
        *(u32*)&prim->r2 = arg5;
        *(u32*)&prim->r3 = arg6;
        *(u32*)&prim->r1 = arg6;
        y                = y + arg2 + 1;
        x                = (u16)prim->x0 + w - 1;
        prim->y1         = y;
        prim->y0         = y;
        prim->x3         = x;
        prim->x1         = x;
        y                = y + arg4 - 1;
        prim->y3         = y;
        prim->y2         = y;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, prim);
    }
}

void func_mist_parking_80181B14(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_80186698, prompt->field_1C, 1, 0);
    sel = prompt->field_C;
    if (sel == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        D_80071086 = 0xFF;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->status       = 0;
        obj->field_2E     = 6;
        obj->owner->state = sel;
    }
}

void func_mist_parking_80181BF8(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_801866A0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mist_parking_8018694C, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_mist_parking_80181CC0(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_801866AC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mist_parking_80186968, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_mist_parking_80181D88(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_801866B8, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mist_parking_80186968, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Exit callback of a prompt task that registers its UI object as
/// `Wip_UiHolder`: releases the holder if the task still owns it, then frees
/// the UI object and kills the task.
void func_mist_parking_80181E50(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// The area records applied when the scene hands the Dryfield story on.
extern GpAreaApplyRec D_mist_parking_80188888;

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the scene
/// sub-task short, applies the story-flag side effects when the scene ends,
/// and restores everything before killing itself.
void func_mist_parking_80181E8C(Task* task)
{
    RoomCutsceneRec* rec;
    s32              killOut;
    s32              flag;
    s32              cmd;
    s32              fadeA;
    s32              fadeB;

    rec = (RoomCutsceneRec*)task->spawnArg2;
    switch (task->state) {
        case 0:
            D_mist_parking_80195318 = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (rec->field_0 > 0) {
                D_80115694               = Mc_SaveData.at4.loc.view;
                Mc_SaveData.at4.loc.view = rec->field_0;
            } else {
                D_80115694 = -rec->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (rec->field_4 != 0) {
                SndEvt_EnqueueType6(rec->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (rec->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(rec->field_3);
                fadeB = 0;
                fadeA = rec->field_14;
                if (fadeA == 0) {
                    fadeA = 0x3C0;
                } else {
                    fadeB = rec->field_16;
                }
                func_800E6D4C(fadeA, fadeB);
            }
            if (rec->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_mist_parking_80195318 = Task_SpawnFromTable(&D_mist_parking_801869B8, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_mist_parking_80195318);
                task->state++;
            } else if (Task_PollKill(D_mist_parking_80195318, &killOut) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (rec->field_2 == 0) {
                SndEvt_EnqueueType6(rec->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (rec->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(rec->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((GP_LOC_WORD(Mc_SaveData.at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(&D_mist_parking_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData.at4.loc.view = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(rec->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (rec->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

extern s32 D_mist_parking_80186EFC;
extern s32 D_mist_parking_8018F0A4;
extern s32 D_mist_parking_8018F194;

/// The scene record the room hands the cutscene runner.
extern RoomCutsceneRec D_mist_parking_8019533C;

s32 func_mist_parking_801823F8(s32 arg0, s32 arg1, s32 arg2)
{
    GameSession* session;
    u8           temp;

    switch (arg2) {
        case 15:
            temp = gGameSession->at4.loc.place;
            if (temp == 2) {
                if (GameFlag_GetNibble(0xF1) == 1) {
                    Gp_MsgPlayerWeapon(0);
                    func_800E8614((s32)&D_mist_parking_8018F0A4, 1);
                    GameFlag_SetNibble(0xF1, 2);
                } else if (GameFlag_GetNibble(0xF1) == temp) {
                    Gp_MsgPlayerWeapon(0);
                    func_800E8614((s32)&D_mist_parking_8018F194, 1);
                    GameFlag_SetNibble(0xF1, 3);
                } else if (GameFlag_GetNibble(0xF1) == 3) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(&D_mist_parking_8018D75C, 8, 0, 0);
                }
            } else if (GameFlag_GetNibble(0xED) == 1) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_mist_parking_80190824, 4, 0, 0);
            }
            break;
        case 8:
            D_mist_parking_8019533C.field_0  = 9;
            D_mist_parking_8019533C.field_1  = 1;
            D_mist_parking_8019533C.field_3  = 3;
            D_mist_parking_8019533C.field_2  = 0;
            D_mist_parking_8019533C.field_4  = 0x51130003;
            D_mist_parking_8019533C.field_8  = 0x51130004;
            D_mist_parking_8019533C.field_10 = 0x5113000B;
            D_mist_parking_8019533C.field_C  = 0x51130012;
            Task_SpawnFromTable(&D_mist_parking_801869B8, 0, 4, (s32)&D_mist_parking_8019533C);
            session               = gGameSession;
            D_80072170            = 2;
            session->at4.loc.warp = 2;
            break;
        case 18:
            Gp_MsgPlayerWeapon(0);
            if (gGameSession->at4.loc.place == 1) {
                Task_SpawnFromTable(&D_mist_parking_80190824, 3, 0, 0);
            } else {
                Task_SpawnFromTable(&D_mist_parking_8018D75C, 7, 0, 0);
            }
            break;
        case 1:
            func_800E8614((s32)&D_mist_parking_80186EFC, 1);
            break;
    }
    return 0;
}

void func_mist_parking_801827C0(Task* arg0);
void func_mist_parking_80182888(Task* task);

/// State handlers of the task `func_mist_parking_80182898` runs: its set-up,
/// an empty per-frame state and the kill.
const TaskFuncTable3 D_mist_parking_8017D7DC = {
    {
        func_mist_parking_801827C0,
        func_mist_parking_80182888,
        taskKill,
    },
};

/// State handlers of the same shape for a task that attaches a model to a
/// parent's part and then idles; nothing in the room reads this table.
const TaskFuncTable3 D_mist_parking_8017D7E8 = {
    {
        func_mist_parking_8018307C,
        func_mist_parking_801830F8,
        taskKill,
    },
};

/// State handlers of the text-block task `func_mist_parking_801832AC` runs.
const TaskFuncTable3 D_mist_parking_8017D7F4 = {
    {
        func_mist_parking_80183304,
        func_mist_parking_801833F8,
        func_mist_parking_80183434,
    },
};

/// Plays the sound event in `spawnArg2` on its first frame and again at frame
/// 0x50, then asks for the task's own kill at frame 0x78.
void func_mist_parking_80182628(Task* task)
{
    s32 zero;

    switch (task->state) {
        case 0x50:
        case 0x0:
            zero = 0;
            TOUCH_REG(zero);
            SndEvt_EnqueueType6((s32)task->spawnArg2, zero, zero);
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            return;
    }
    task->state += 1;
}

/// Handler that answers 0.
s32 func_mist_parking_801826B8(void)
{
    return 0;
}

/// Message handler that copies the location record it is given onto the
/// reply record and answers 1.
s32 func_mist_parking_801826C0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

extern s32 D_mist_parking_80186BB8;
extern s32 D_mist_parking_80186C5C;
extern s32 D_mist_parking_80186DC4;
extern s32 D_mist_parking_8018DF34;
extern s32 D_mist_parking_8018EDBC;
extern s32 D_mist_parking_8018EFE4;

s32 func_mist_parking_801826E8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        func_800E8614((s32)&D_mist_parking_80186C5C, 1);
    }
    if (arg2->field_2 == 2) {
        func_800E8614((s32)&D_mist_parking_80186DC4, 1);
        GameFlag_SetNibble(0xED, 1);
    }
    return 1;
}

void func_mist_parking_80182750(s32 arg0)
{
    if (GameFlag_GetNibble(0x7A) != 0) {
        arg0 += 2;
    }
    D_8007216D                  = arg0;
    gGameSession->at4.loc.room  = arg0;
    gGameSession->roomObjsDirty = 1;
}

void func_mist_parking_801827A0(s32 arg0)
{
    Gp_SpawnIfCapIdle(arg0, 0);
}

void func_mist_parking_801827C0(Task* arg0)
{
    arg0->msgTable = &D_mist_parking_80186BB8;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 2) && (GameFlag_GetNibble(0xF1) == 0)) {
        if (D_80072170 == 3) {
            func_800E3FAC(0xA2, 0x3C);
            func_mist_parking_801837A4(0);
            func_800E8634((s32)&D_mist_parking_8018DF34, 0, (s32)&D_mist_parking_8018EDBC);
        } else {
            func_mist_parking_8018471C(0);
            func_800E8614((s32)&D_mist_parking_8018EFE4, 1);
        }
    }
    arg0->state = arg0->state + 1;
}

/// The empty per-frame state of `D_mist_parking_8017D7DC`.
void func_mist_parking_80182888(Task* task)
{
    char pad[0x10];
}

extern s32 D_mist_parking_8018D830;
extern s8  D_mist_parking_8018DA28[];

/// Runs the handler for the task's state from a stack copy of
/// `D_mist_parking_8017D7DC`.
void func_mist_parking_80182898(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_parking_8017D7DC;
    sp.funcs[task->state](task);
}

void func_mist_parking_801828F0(Task* task)
{
    GameActor* actor;
    GpWorkObj* work;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)(gameGetPtrSlot(3))->work;
    if (D_801156F9 == 0) {
        idx = actor->field_438[1].nextSet - 0x2F;
        if ((idx > 0) && (idx < D_mist_parking_8018D830)) {
            flag = D_mist_parking_8018DA28[idx];
        } else {
            flag = 0;
        }
        if (task->state == 0) {
            if ((flag != 0) || (task->spawnArg1 != 0)) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
            func_800B0928(gameGetPtrSlot(3), (Task*)work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}
