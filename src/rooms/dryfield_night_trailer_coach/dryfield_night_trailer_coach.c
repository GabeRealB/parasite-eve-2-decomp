#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "decomp/common.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
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
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_80181228.h"

/// The "%" suffix the room's percentage formatters append.
extern u8 D_dryfield_night_trailer_coach_80184D44[];

/// The item id the shop list's cursor last rested on.
extern s32 D_dryfield_night_trailer_coach_80184490;

/// Task descriptor table the room's cutscene tasks spawn from: the room spawns
/// entry 0 with a cutscene record as its argument, and
/// `func_dryfield_night_trailer_coach_80181DB0` spawns entry 1 for the scene.
extern TaskDesc D_dryfield_night_trailer_coach_80184FE4;

void func_dryfield_night_trailer_coach_80181D74(Task* task);

/// The 0xFFFF-terminated item id lists `func_dryfield_night_trailer_coach_8017D81C`
/// chooses from.
extern u16 D_dryfield_night_trailer_coach_8018409C[];
extern u16 D_dryfield_night_trailer_coach_801840A4[];
extern u16 D_dryfield_night_trailer_coach_801840AC[];
extern u16 D_dryfield_night_trailer_coach_801840B4[];
extern u16 D_dryfield_night_trailer_coach_801840C4[];
extern u16 D_dryfield_night_trailer_coach_801840D4[];
extern u16 D_dryfield_night_trailer_coach_801840E4[];
extern u16 D_dryfield_night_trailer_coach_801840EC[];
extern u16 D_dryfield_night_trailer_coach_801840FC[];
extern u16 D_dryfield_night_trailer_coach_8018410C[];
extern u16 D_dryfield_night_trailer_coach_8018411C[];
extern u16 D_dryfield_night_trailer_coach_80184124[];
extern u16 D_dryfield_night_trailer_coach_80184138[];
extern u16 D_dryfield_night_trailer_coach_80184150[];
extern u16 D_dryfield_night_trailer_coach_80184164[];
extern u16 D_dryfield_night_trailer_coach_8018416C[];
extern u16 D_dryfield_night_trailer_coach_8018417C[];
extern u16 D_dryfield_night_trailer_coach_80184194[];
extern u16 D_dryfield_night_trailer_coach_801841A8[];
extern u16 D_dryfield_night_trailer_coach_801841B0[];
extern u16 D_dryfield_night_trailer_coach_801841C4[];
extern u16 D_dryfield_night_trailer_coach_801841E0[];
extern u16 D_dryfield_night_trailer_coach_801841F0[];
extern u16 D_dryfield_night_trailer_coach_801841FC[];
extern u16 D_dryfield_night_trailer_coach_80184214[];
extern u16 D_dryfield_night_trailer_coach_80184230[];
extern u16 D_dryfield_night_trailer_coach_80184244[];
extern u16 D_dryfield_night_trailer_coach_8018424C[];
extern u16 D_dryfield_night_trailer_coach_80184260[];
extern u16 D_dryfield_night_trailer_coach_80184280[];
extern u16 D_dryfield_night_trailer_coach_80184290[];
extern u16 D_dryfield_night_trailer_coach_8018429C[];
extern u16 D_dryfield_night_trailer_coach_801842B4[];
extern u16 D_dryfield_night_trailer_coach_801842B8[];
extern u16 D_dryfield_night_trailer_coach_801842BC[];
extern u16 D_dryfield_night_trailer_coach_801842C4[];
extern u16 D_dryfield_night_trailer_coach_801842D4[];
extern u16 D_dryfield_night_trailer_coach_801842DC[];
extern u16 D_dryfield_night_trailer_coach_801842E4[];
extern u16 D_dryfield_night_trailer_coach_801842EC[];
extern u16 D_dryfield_night_trailer_coach_801842F8[];
extern u16 D_dryfield_night_trailer_coach_80184300[];
extern u16 D_dryfield_night_trailer_coach_8018430C[];
extern u16 D_dryfield_night_trailer_coach_80184314[];
extern u16 D_dryfield_night_trailer_coach_80184320[];
extern u16 D_dryfield_night_trailer_coach_8018432C[];
extern u16 D_dryfield_night_trailer_coach_80184334[];
extern u16 D_dryfield_night_trailer_coach_8018433C[];
extern u16 D_dryfield_night_trailer_coach_80184348[];
extern u16 D_dryfield_night_trailer_coach_80184354[];
extern u16 D_dryfield_night_trailer_coach_8018435C[];
extern u16 D_dryfield_night_trailer_coach_80184368[];
extern u16 D_dryfield_night_trailer_coach_80184374[];
extern u16 D_dryfield_night_trailer_coach_80184380[];
extern u16 D_dryfield_night_trailer_coach_80184384[];
extern u16 D_dryfield_night_trailer_coach_80184390[];
extern u16 D_dryfield_night_trailer_coach_8018439C[];
extern u16 D_dryfield_night_trailer_coach_801843A8[];
extern u16 D_dryfield_night_trailer_coach_801843B0[];
extern u16 D_dryfield_night_trailer_coach_801843BC[];
extern u16 D_dryfield_night_trailer_coach_801843C8[];
extern u16 D_dryfield_night_trailer_coach_801843D4[];
extern u16 D_dryfield_night_trailer_coach_801843DC[];
extern u16 D_dryfield_night_trailer_coach_801843E8[];

/// The list returned when no case matches.
extern u16 D_dryfield_night_trailer_coach_80184578[];

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_dryfield_night_trailer_coach_80184578`.
u16* func_dryfield_night_trailer_coach_8017D81C(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_8018417C;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184194;
                    case 2:
                        return D_dryfield_night_trailer_coach_801841A8;
                    case 3:
                        return D_dryfield_night_trailer_coach_801841B0;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801841C4;
                    case 1:
                        return D_dryfield_night_trailer_coach_801841E0;
                    case 2:
                        return D_dryfield_night_trailer_coach_801841F0;
                    case 3:
                        return D_dryfield_night_trailer_coach_801841FC;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184214;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184230;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184244;
                    case 3:
                        return D_dryfield_night_trailer_coach_8018424C;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184260;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184280;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184290;
                    case 3:
                        return D_dryfield_night_trailer_coach_8018429C;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801840C4;
                    case 1:
                        return D_dryfield_night_trailer_coach_801840D4;
                    case 2:
                        return D_dryfield_night_trailer_coach_801840E4;
                    case 3:
                        return D_dryfield_night_trailer_coach_801840EC;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184138;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184150;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184164;
                    case 3:
                        return D_dryfield_night_trailer_coach_8018416C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801840FC;
                    case 1:
                        return D_dryfield_night_trailer_coach_8018410C;
                    case 2:
                        return D_dryfield_night_trailer_coach_8018411C;
                    case 3:
                        return D_dryfield_night_trailer_coach_80184124;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_8018409C;
                    case 1:
                        return D_dryfield_night_trailer_coach_801840A4;
                    case 2:
                        return D_dryfield_night_trailer_coach_801840AC;
                    case 3:
                        return D_dryfield_night_trailer_coach_801840B4;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184348;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184354;
                    case 2:
                        return D_dryfield_night_trailer_coach_8018435C;
                    case 3:
                        return D_dryfield_night_trailer_coach_80184368;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184374;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184380;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184384;
                    case 3:
                        return D_dryfield_night_trailer_coach_80184390;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_8018439C;
                    case 1:
                        return D_dryfield_night_trailer_coach_801843A8;
                    case 2:
                        return D_dryfield_night_trailer_coach_801843B0;
                    case 3:
                        return D_dryfield_night_trailer_coach_801843BC;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801843C8;
                    case 1:
                        return D_dryfield_night_trailer_coach_801843D4;
                    case 2:
                        return D_dryfield_night_trailer_coach_801843DC;
                    case 3:
                        return D_dryfield_night_trailer_coach_801843E8;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801842D4;
                    case 1:
                        return D_dryfield_night_trailer_coach_801842DC;
                    case 2:
                        return D_dryfield_night_trailer_coach_801842E4;
                    case 3:
                        return D_dryfield_night_trailer_coach_801842EC;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184320;
                    case 1:
                        return D_dryfield_night_trailer_coach_8018432C;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184334;
                    case 3:
                        return D_dryfield_night_trailer_coach_8018433C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801842F8;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184300;
                    case 2:
                        return D_dryfield_night_trailer_coach_8018430C;
                    case 3:
                        return D_dryfield_night_trailer_coach_80184314;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801842B4;
                    case 1:
                        return D_dryfield_night_trailer_coach_801842B8;
                    case 2:
                        return D_dryfield_night_trailer_coach_801842BC;
                    case 3:
                        return D_dryfield_night_trailer_coach_801842C4;
                }
                break;
        }
    }
    return D_dryfield_night_trailer_coach_80184578;
}

extern GpItemScan    D_80072724;
extern RoomShopStock D_8010E138[];
extern UiObjectDesc  D_8010EFA0;
extern char          Gp_StrEmpty[];

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
extern u8           D_dryfield_night_trailer_coach_801844C4[];
extern u8           D_dryfield_night_trailer_coach_801844B0[];
extern UiObjectDesc D_dryfield_night_trailer_coach_8018467C;
extern u8           D_dryfield_night_trailer_coach_801844C0[];
extern UiObjectDesc D_dryfield_night_trailer_coach_80184628;

/// Draws one row of the shop list and handles its input, recording the row's
/// id as the cursor item while the row is selected. Row 0xFFFE is greyed out
/// and unselectable unless `Gp_HasMappedItem` answers non-zero, and opens its
/// own panel; row 0xFFFC is greyed out while the scan holds item 0x8F. Any
/// other row is an item with its price, greyed out when `func_800B7420`
/// refuses it; confirm opens the buy panel and button 0x10 the item's detail
/// panel.
void func_dryfield_night_trailer_coach_8017DE8C(DialogPrompt* prompt, UiObject* obj)
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
        D_dryfield_night_trailer_coach_80184490 = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->status;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam((s32)D_dryfield_night_trailer_coach_801844C4, 0, 0);
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
        func_8002E53C(&req, D_dryfield_night_trailer_coach_801844B0);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_8018467C, 0, 1, 1, obj);
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
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_trailer_coach_801844C0, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184628, itemId, 1, 1, obj);
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
            child2 = Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184628, itemId, 1, 1, obj);
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
/// the same kind is overwritten only by a higher level. In mode 0x10 the ids
/// 0x9D..0x9F, 0x8A and 0x65 are never added.
void func_dryfield_night_trailer_coach_8017E318(RoomShopList* shop, UiObject* obj, s32 item)
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

extern UiObject*    D_80067634;
extern UiObjectDesc D_8010D80C;

extern u8 D_80071072;

/// Titles and captions of the shop's panels.
const u8 D_dryfield_night_trailer_coach_8017D6D0[] = "Select";
const u8 D_dryfield_night_trailer_coach_8017D6D8[] = "BP";
const u8 D_dryfield_night_trailer_coach_8017D6DC[] = "List";
const u8 D_dryfield_night_trailer_coach_8017D6E4[] = "TOTAL";
const u8 D_dryfield_night_trailer_coach_8017D6EC[] = "Notice";

/// "Charge", with a stray non-zero byte after its terminator that C cannot
/// place, so the string stays assembly.
extern u8 D_dryfield_night_trailer_coach_8017D6F4[];
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach", D_dryfield_night_trailer_coach_8017D6F4);

/// Messages and labels of the shop's panels.
extern u8 D_dryfield_night_trailer_coach_80184494[];
extern u8 D_dryfield_night_trailer_coach_801844A8[];
extern u8 D_dryfield_night_trailer_coach_80184500[];
extern u8 D_dryfield_night_trailer_coach_80184508[];
extern u8 D_dryfield_night_trailer_coach_80184514[];
extern u8 D_dryfield_night_trailer_coach_8018451C[];
extern u8 D_dryfield_night_trailer_coach_80184524[];
extern u8 D_dryfield_night_trailer_coach_80184538[];
extern u8 D_dryfield_night_trailer_coach_80184548[];
extern u8 D_dryfield_night_trailer_coach_80184568[];
extern u8 D_dryfield_night_trailer_coach_80184574[];

/// Row handlers, lists and panel descriptors of the shop's panels.
extern UiListItemFunc D_dryfield_night_trailer_coach_8018457C[];
extern UiList         D_dryfield_night_trailer_coach_80184584;
extern UiList         D_dryfield_night_trailer_coach_801845B0;
extern UiObjectDesc   D_dryfield_night_trailer_coach_801845F0;
extern UiObjectDesc   D_dryfield_night_trailer_coach_8018460C;
extern UiObjectDesc   D_dryfield_night_trailer_coach_80184644;
extern UiObjectDesc   D_dryfield_night_trailer_coach_80184698;
extern UiObjectDesc   D_dryfield_night_trailer_coach_801846B4;

/// Work pair of the charge panel `func_dryfield_night_trailer_coach_8017F3C0`: the
/// animated quantity in 24.8 fixed point, and the item map of the slot being
/// charged.
extern s32        D_dryfield_night_trailer_coach_8018C210;
extern GpItemMap* D_dryfield_night_trailer_coach_8018C214;

/// The shop's "Select" panel. On its first frame it allocates the
/// `RoomShopList` work block, fills it through
/// `func_dryfield_night_trailer_coach_8017E464` and opens the panel
/// `D_dryfield_night_trailer_coach_80184698` beside it. Every frame it draws the list and the
/// "BP" caption; menu reports -1 and cancel 6 to the parent. A child that
/// reports 6 is torn down and the list takes input again; one that reports -1
/// passes it up.
void func_dryfield_night_trailer_coach_8017E830(Task* task)
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
    Ui_DrawText((UiPanel*)obj, (char*)D_dryfield_night_trailer_coach_8017D6D0);
    if (task->state == 0) {
        mem  = memCalloc(sizeof(RoomShopList), 0);
        shop = mem;
        if (mem != NULL) {
            /* Keeps the allocation's own register distinct from the tested one,
               so the null test stays on $v0 and the copy fills its delay slot. */
            SOFT_TOUCH_REG(shop);
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_dryfield_night_trailer_coach_8018457C;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_dryfield_night_trailer_coach_8017E464(shop, obj);
            Ui_LayoutListPanel(&shop->list, (UiPanel*)obj);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->field_12      += 8;
            shop->list.field_17 = 8;
            Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184698, 0, 0, 0, obj);
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
    func_8002E53C(&req, D_dryfield_night_trailer_coach_8017D6D8);

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

/// Row handler of the shop's mode menu. The last row is the exit, which
/// reports 6 on confirm. Any other row stores its index as the owning task's
/// mode (the upper halfword of `spawnArg1`) and draws that mode's label; the
/// row is greyed out and unselectable when the mode's item-id list is empty,
/// and confirm opens the shop list panel with the mode.
void func_dryfield_night_trailer_coach_8017EA80(DialogPrompt* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_trailer_coach_801844A8, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_dryfield_night_trailer_coach_80184500;
    obj->owner->spawnArg1 = (u16)obj->owner->spawnArg1;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_dryfield_night_trailer_coach_80184508;
            obj->owner->spawnArg1 |= 0x10000;
            break;
        case 2:
            text                   = D_dryfield_night_trailer_coach_80184514;
            obj->owner->spawnArg1 |= 0x20000;
            break;
        case 3:
            text                   = D_dryfield_night_trailer_coach_8018451C;
            obj->owner->spawnArg1 |= 0x30000;
            break;
    }

    if (*func_dryfield_night_trailer_coach_8017D81C(obj->owner->spawnArg1) == 0xFFFF) {
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
        Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_801845F0, obj->owner->spawnArg1, 1, 1, obj);
        obj->status = 0;
    }
}

/// The shop's "List" panel, whose rows are the modes. Its first frame clears the item previews,
/// opens the list-row panel and the preview panel, and lays out its five-row
/// list. Cancel or menu reports -1. A child reporting 6 is torn down; one
/// reporting -1 releases `Wip_UiHolder` and passes the code up.
void func_dryfield_night_trailer_coach_8017ECA0(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2;
    list          = &D_dryfield_night_trailer_coach_80184584;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_dryfield_night_trailer_coach_8017D6DC);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_8018460C, task->spawnArg1, 0, 1, obj);
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

/// Balance panel: the "BP" caption with the player's BP, and the "TOTAL"
/// caption with the carried item count over the inventory's row capacity.
void func_dryfield_night_trailer_coach_8017EE48(Task* task)
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
    func_8002E53C(&req0, D_dryfield_night_trailer_coach_8017D6D8);

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
    func_8002E53C(&req1, (char*)D_dryfield_night_trailer_coach_8017D6E4);

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

/// Row handler of the buy prompt. On confirm it checks the price against the
/// player's BP (notice 0 when short) and the inventory (notice 2 for a
/// stackable item already held, 1 otherwise when it cannot be added). When the
/// owning task's parent runs in mode 1 it opens the quantity picker; otherwise
/// it takes the price, gives one of the item and reports 6.
void func_dryfield_night_trailer_coach_8017F02C(DialogPrompt* prompt, UiObject* obj)
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
    func_8002E53C(&req, D_dryfield_night_trailer_coach_80184494);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &D_80072724;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184644, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184644, 1, 1, 1, obj);
                }
                obj->status = 0;
            } else if (((RoomShopTask*)obj->owner->parent)->mode == mode) {
                child = Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_801846B4, itemId, 1, 1, obj);
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
            Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184644, 0, 1, 1, obj);
            obj->status = 0;
        }
    }
}

/// Notice panel: shows one of three messages picked by `spawnArg1`, sized to
/// the text. Menu reports -1; confirm, cancel or 0xBC frames elapsing tell the
/// parent panel to close with 6.
void func_dryfield_night_trailer_coach_8017F240(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1;
    obj  = task->spawnArg2;
    switch (kind) {
        case 1:
            text = D_dryfield_night_trailer_coach_80184538;
            break;
        case 2:
            text = D_dryfield_night_trailer_coach_80184548;
            break;
        default:
            text = D_dryfield_night_trailer_coach_80184524;
            break;
    }

    Ui_DrawText((UiPanel*)obj, (char*)D_dryfield_night_trailer_coach_8017D6EC);
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

/// The charge panel: steps through the mapped item slots, refilling each
/// slot's ammo or attachment quantity to its related quantity and animating a
/// bar from the old value up to the new one for at most 0xBC frames. Confirm
/// or cancel (or the timer running out) moves to the next slot; running out of
/// slots reports 6 to the parent.
void func_dryfield_night_trailer_coach_8017F3C0(Task* task)
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
    Ui_DrawText((UiPanel*)obj, (char*)D_dryfield_night_trailer_coach_8017D6F4);

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
            map                                     = Gp_GetItemMap(slotId);
            D_dryfield_night_trailer_coach_8018C214 = map;
            itemId                                  = map->field_1;
            slot                                    = Gp_GetItemSlot(itemId);
            if (D_dryfield_night_trailer_coach_8018C214->field_0 == 0) {
                D_dryfield_night_trailer_coach_8018C210 = slot->ammoQty;
                slot->ammoQty                           = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_dryfield_night_trailer_coach_8018C210 = slot->attachQty;
                slot->attachQty                         = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown                       = 0xBC;
            D_dryfield_night_trailer_coach_8018C210 <<= 8;
            task->state                               = task->state + 1;
        }
    }

    curItem = D_dryfield_night_trailer_coach_8018C214->field_1;
    relItem = D_dryfield_night_trailer_coach_8018C214->field_2;
    if (D_dryfield_night_trailer_coach_8018C214->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                                    <<= 8;
    D_dryfield_night_trailer_coach_8018C210 += 0x40;
    if (qty < D_dryfield_night_trailer_coach_8018C210) {
        D_dryfield_night_trailer_coach_8018C210 = qty;
    }

    y = (s16)obj->field_18;
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->field_1C + 2, y + 0x23, D_dryfield_night_trailer_coach_8018C210 >> 8, 0x606060);
    h = (s16)obj->field_1A;
    func_800C0E20((UiPanel*)obj, (s16)obj->field_1C + 2, (s16)obj->field_1E - 2, h - 6, qty,
                  D_dryfield_night_trailer_coach_8018C210, 0x1741F);

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
/// item id below 0x100, the "Amount" caption with how many of it the player
/// already holds. Stackable items (0xA0..0xBF) ask the scan for their stack
/// quantity; everything else is counted by walking the item table.
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
void func_dryfield_night_trailer_coach_8017F688(Task* task)
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

    item         = D_dryfield_night_trailer_coach_80184490;
    obj          = task->spawnArg2;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_dryfield_night_trailer_coach_80184490 == Gp_GetPreviewItem()) {
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
        func_8002E53C(&req, D_dryfield_night_trailer_coach_80184568);
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

/// Quantity picker of the buy prompt. Up and down step the count between 1 and
/// the most the player can take: for a stackable item, what its stock ceiling
/// still allows in steps of its per-buy amount; otherwise the free inventory
/// rows; in both cases no more than the BP affords. It shows the unit and
/// total price. Confirm takes the total and gives the items; confirm or
/// cancel tells the parent panel to close with 6.
void func_dryfield_night_trailer_coach_8017F85C(Task* task)
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
    Text_DrawPrompt(obj, left + 0x98, y, D_dryfield_night_trailer_coach_80184574, 0x606060, 3, 2);
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
    func_8002E53C(&req, D_dryfield_night_trailer_coach_8017D6D8);

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

/// Row handler that draws a single message and reports 6 on confirm.
void func_dryfield_night_trailer_coach_8017FCDC(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_dryfield_night_trailer_coach_801844A8);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

/// A list panel over `D_dryfield_night_trailer_coach_801845B0`. Cancel reports 6 and
/// menu -1; a child reporting 6 is torn down, one reporting -1 passes it up.
void func_dryfield_night_trailer_coach_8017FD98(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_dryfield_night_trailer_coach_801845B0;
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

/// Descriptor of the panel `func_dryfield_night_trailer_coach_8017FEC0` opens.
extern UiObjectDesc D_dryfield_night_trailer_coach_801845D4;

/// Opens the panel `D_dryfield_night_trailer_coach_801845D4` with the task's
/// `spawnArg1` as its parameter, setting frame timing 0 and the session's UI
/// flag while it is open; once the panel reports -1 or 6 it is torn down, and
/// ten frames later frame timing 1 and the flag are restored and the task
/// kills itself.
void func_dryfield_night_trailer_coach_8017FEC0(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_801845D4, task->spawnArg1, 1, 1, NULL);
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

extern u16 D_80072174;
extern s8  D_80072176;
extern u16 D_80072834;
extern u16 D_80072836;
extern u8  D_80072A93;
extern s32 D_80072A94;
extern s32 D_80072A98;

extern u8 D_dryfield_night_trailer_coach_80184CEC[];
extern u8 D_dryfield_night_trailer_coach_80184CF4[];
extern u8 D_dryfield_night_trailer_coach_80184CF8[];
extern u8 D_dryfield_night_trailer_coach_80184D00[];
extern u8 D_dryfield_night_trailer_coach_80184D0C[];
extern u8 D_dryfield_night_trailer_coach_80184D1C[];
extern u8 D_dryfield_night_trailer_coach_80184D24[];
extern u8 D_dryfield_night_trailer_coach_80184D2C[];
extern u8 D_dryfield_night_trailer_coach_80184D34[];
extern u8 D_dryfield_night_trailer_coach_80184D3C[];
extern u8 D_dryfield_night_trailer_coach_80184D48[];
extern u8 D_dryfield_night_trailer_coach_80184D74[];
extern u8 D_dryfield_night_trailer_coach_80184D98[];
extern u8 D_dryfield_night_trailer_coach_80184DC8[];
extern u8 D_dryfield_night_trailer_coach_80184DFC[];
extern u8 D_dryfield_night_trailer_coach_80184E30[];
extern u8 D_dryfield_night_trailer_coach_80184E68[];
extern u8 D_dryfield_night_trailer_coach_80184E9C[];
extern u8 D_dryfield_night_trailer_coach_80184ED4[];

void func_dryfield_night_trailer_coach_8017FFF4(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_dryfield_night_trailer_coach_80184D48,
                D_dryfield_night_trailer_coach_80184D74,
                D_dryfield_night_trailer_coach_80184D98,
                D_dryfield_night_trailer_coach_80184DC8,
                D_dryfield_night_trailer_coach_80184DFC,
                D_dryfield_night_trailer_coach_80184E30,
                D_dryfield_night_trailer_coach_80184E68,
                D_dryfield_night_trailer_coach_80184E9C,
                D_dryfield_night_trailer_coach_80184ED4,
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184CEC);
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184D1C);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_dryfield_night_trailer_coach_80184D3C);
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184CF4);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_dryfield_night_trailer_coach_80184D3C);
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184CF8);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_dryfield_night_trailer_coach_80184D3C);
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184D00);
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
            Text_Strcat(p, D_dryfield_night_trailer_coach_80184D44);
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184D0C);
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
            Text_Strcat(p, D_dryfield_night_trailer_coach_80184D44);
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184D24);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_dryfield_night_trailer_coach_80184D3C);
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184D2C);
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
            func_8002E53C(&req, D_dryfield_night_trailer_coach_80184D34);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data menu `func_dryfield_night_trailer_coach_80181844`.
const char D_dryfield_night_trailer_coach_8017D748[] = "Play Data";

/// The completion figure `func_dryfield_night_trailer_coach_801807C0` draws.
const u8 D_dryfield_night_trailer_coach_8017D754[] = "100.0%";

/// Titles of the play-data panels: the two usage lists
/// `func_dryfield_night_trailer_coach_801811D8` draws (item and PE), and the menu
/// `func_dryfield_night_trailer_coach_8018138C` draws. The last has two stray
/// non-zero bytes after its terminator that C cannot place, so it stays
/// assembly.
const char        D_dryfield_night_trailer_coach_8017D75C[] = "Weapon Data";
const char        D_dryfield_night_trailer_coach_8017D768[] = "PE Data";
extern const char D_dryfield_night_trailer_coach_8017D770[];
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach", D_dryfield_night_trailer_coach_8017D770);

void func_dryfield_night_trailer_coach_801807C0(DialogPrompt* arg0, UiObject* arg1)
{
    u8             buf[0x20];
    TextDrawReq    req;
    TextDrawReq*   r;
    RoomItemUsage* work;
    POLY_G4*       prim;
    u8*            p;
    u8*            q;
    s32            item;
    s32            value;
    s32            x;
    s32            y;
    s32            color;
    s32            textY;
    s32            limit;
    s32            n;
    s32            len;
    s32            i;
    s32            avail;
    s32            base;
    s32            barW;
    s32            barX;
    s32            rowY;
    s32            one;
    s32            tx;
    s32            ty;

    p     = buf;
    r     = &req;
    x     = arg0->field_18;
    y     = arg0->field_1A;
    work  = (RoomItemUsage*)arg1->owner->work;
    item  = work->itemIds[arg0->field_8];
    value = work->percents[arg0->field_8];
    color = arg0->field_1C;
    if (arg1->mode != 5) {
        req.x          = arg1->baseX + 0x11 + x;
        textY          = arg1->baseY - 6;
        req.y          = textY + y;
        req.otIndex    = (s16)arg1->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        func_8002E53C(r, (u8*)Gp_GetItemText(item, 0, 0));
        func_800CE5D0(arg1, x, y, item);
    }
    limit = 1;
    if (value >= 10000) {
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_dryfield_night_trailer_coach_8017D754, arg0->field_1C, 3, 2);
    } else {
        for (i = 2; i > 0; i--) {
            limit *= 10;
        }
        if (value < limit) {
            func_8002F44C(p, value, 3);
        } else {
            Text_ItoaUnsigned(p, value);
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
        for (len = 0; len < n; len++) {
            q[1] = q[0];
            q--;
        }
        q[1] = '.';
        Text_Strcat(p, D_dryfield_night_trailer_coach_80184D44);
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
    }

    base  = (s16)arg1->field_1C + 0x80;
    avail = (s16)arg1->field_1E - 0x4A;
    barW  = avail - base;
    barW  = (barW * work->barWidths[arg0->field_8]) >> 12;
    rowY  = arg0->field_1A - 0xC;
    barW  = barW + 2;
    barX  = avail - barW;
    if (barW >= 2) {
        prim             = (POLY_G4*)gGpuPrimCursor;
        tx               = arg1->baseX + barX + 1;
        prim->x2         = tx;
        prim->x0         = tx;
        ty               = arg1->baseY;
        gGpuPrimCursor   = prim + 1;
        ty               = ty + rowY;
        ty              += 1;
        *(s32*)&prim->r3 = 0x10000;
        *(s32*)&prim->r1 = 0x10000;
        setlen(prim, 8);
        *(s32*)&prim->r0 = 0x100B0;
        setcode(prim, 0x38);
        *(s32*)&prim->r2 = 0x100B0;
        tx               = (u16)prim->x0 + barW - 1;
        prim->y1         = ty;
        prim->y0         = ty;
        ty              += 8;
        prim->y3         = ty;
        prim->y2         = ty;
        prim->x3         = tx;
        prim->x1         = tx;
        addPrim(gGpuCurrentOt + (s16)arg1->drawOrder + 1, prim);
    }
    one = 1;
    func_80046B34((UiPanel*)arg1, barX, arg0->field_1A - 0xC, barW, 9, 0, one);
    if (((arg1->status >> 16) == one) || (arg1->status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

extern UiObjectDesc D_800611E4;

/// Lists of the usage panel and of the play-data menu, and the descriptor of
/// the frame the usage panel spawns.
extern UiList       D_dryfield_night_trailer_coach_80184F38;
extern UiList       D_dryfield_night_trailer_coach_80184FC0;
extern UiObjectDesc D_dryfield_night_trailer_coach_80184F5C;

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
void func_dryfield_night_trailer_coach_80180BBC(UiList* list, UiObject* obj)
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
/// use counters, in the same way `func_dryfield_night_trailer_coach_80180BBC` builds the
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
void func_dryfield_night_trailer_coach_80180EB8(UiList* list, UiObject* obj)
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
void func_dryfield_night_trailer_coach_801811D8(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_dryfield_night_trailer_coach_80184F38;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_trailer_coach_8017D75C);
    } else {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_trailer_coach_8017D768);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184F5C, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_dryfield_night_trailer_coach_80180BBC(list, obj);
        } else {
            func_dryfield_night_trailer_coach_80180EB8(list, obj);
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

void func_dryfield_night_trailer_coach_8018138C(Task* task)
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
    list          = &D_dryfield_night_trailer_coach_80184FC0;
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
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_trailer_coach_8017D770);
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

void func_dryfield_night_trailer_coach_80181684(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_dryfield_night_trailer_coach_80181D74;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_dryfield_night_trailer_coach_801816E0(u8* str, s32 decimals)
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

/// Format `value` as a percentage with `decimals` fractional digits into `buf`:
/// print the integer with at least `decimals + 1` digits when it is small enough
/// (so "5" with two decimals becomes "0.05"), otherwise print it unpadded, then
/// shift the last `decimals` digits right by one and drop a '.' in front of
/// them. Appends "%" and returns `buf`.
u8* func_dryfield_night_trailer_coach_80181750(u8* buf, s32 value, s32 decimals)
{
    s32 limit;
    s32 i;
    s32 len;
    s32 n;
    u8* p;

    limit = 1;
    for (i = decimals; i > 0; i--) {
        limit *= 10;
    }

    if (value < limit) {
        func_8002F44C(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    n   = decimals;
    p   = buf;
    len = 0;
    if (n > 0) {
        while (*p != 0) {
            p++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            p[1] = p[0];
            p--;
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_dryfield_night_trailer_coach_80184D44);
    return buf;
}

extern u8 D_80071086;

/// List of the menu panel `func_dryfield_night_trailer_coach_80181844` draws.
extern UiList D_dryfield_night_trailer_coach_80184F10;

/// Texts of the four menu rows below, and the panels two of them open.
extern u8           D_dryfield_night_trailer_coach_80184CC4[];
extern u8           D_dryfield_night_trailer_coach_80184CCC[];
extern u8           D_dryfield_night_trailer_coach_80184CD8[];
extern u8           D_dryfield_night_trailer_coach_80184CE4[];
extern UiObjectDesc D_dryfield_night_trailer_coach_80184F78;
extern UiObjectDesc D_dryfield_night_trailer_coach_80184F94;

void func_dryfield_night_trailer_coach_80181844(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_night_trailer_coach_80184F10;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_dryfield_night_trailer_coach_8017D748);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184F5C, 0, 0, 1, obj);
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
void func_dryfield_night_trailer_coach_80181934(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

void func_dryfield_night_trailer_coach_80181A38(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_trailer_coach_80184CC4, prompt->field_1C, 1, 0);
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

void func_dryfield_night_trailer_coach_80181B1C(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_trailer_coach_80184CCC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184F78, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_night_trailer_coach_80181BE4(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_trailer_coach_80184CD8, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184F94, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_night_trailer_coach_80181CAC(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_trailer_coach_80184CE4, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_trailer_coach_80184F94, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Exit callback of a prompt task that registers its UI object as
/// `Wip_UiHolder`: releases the holder if the task still owns it, then frees
/// the UI object and kills the task.
void func_dryfield_night_trailer_coach_80181D74(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

extern s16 D_80114D08;
extern u32 D_80115694;

/// The scene sub-task while it runs, NULL otherwise.
extern Task* D_dryfield_night_trailer_coach_8018C218;

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.
extern GpAreaApplyRec D_dryfield_night_trailer_coach_80188888;

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the scene
/// sub-task short, applies the story-flag side effects when the scene ends,
/// and restores everything before killing itself.
void func_dryfield_night_trailer_coach_80181DB0(Task* task)
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
            D_dryfield_night_trailer_coach_8018C218 = NULL;
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
            D_dryfield_night_trailer_coach_8018C218 = Task_SpawnFromTable(&D_dryfield_night_trailer_coach_80184FE4, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_dryfield_night_trailer_coach_8018C218);
                task->state++;
            } else if (Task_PollKill(D_dryfield_night_trailer_coach_8018C218, &killOut) != 0) {
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
                    if ((*(u32*)&Mc_SaveData.at4.loc.view & 0xFFFF0000) == 0x1010000) {
                        Gp_ApplyAreaRecs(&D_dryfield_night_trailer_coach_80188888);
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

extern void           func_800E8614(s32 arg0, s32 arg1);
extern void           func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern s32            func_800E3FCC(s32 arg0);
extern s8             D_80071090;
extern s8             D_8007272D;
extern s32            D_dryfield_night_trailer_coach_8018794C;
extern s32            D_dryfield_night_trailer_coach_801879B8;
extern s32            D_dryfield_night_trailer_coach_80187CEC;
extern s32            D_dryfield_night_trailer_coach_80187D00;
extern s32            D_dryfield_night_trailer_coach_80187F58;
extern s32            D_dryfield_night_trailer_coach_801880A8;
extern s32            D_dryfield_night_trailer_coach_801881F8;
extern s32            D_dryfield_night_trailer_coach_80188348;
extern s32            D_dryfield_night_trailer_coach_80188510;
extern s32            D_dryfield_night_trailer_coach_80188708;
extern s32            D_dryfield_night_trailer_coach_80188858;
extern s32            D_dryfield_night_trailer_coach_801889A8;
extern s32            D_dryfield_night_trailer_coach_80188F00;
extern s32            D_dryfield_night_trailer_coach_80189080;
extern s32            D_dryfield_night_trailer_coach_801892C0;
extern GpAreaApplyRec D_dryfield_night_trailer_coach_8018C208;

void func_dryfield_night_trailer_coach_8018231C(Task* task);
void func_dryfield_night_trailer_coach_80182898(Task* task);

/// State handlers of the room's task `func_dryfield_night_trailer_coach_801828CC`
/// runs: its set-up, a per-frame state and the kill.
const TaskFuncTable3 D_dryfield_night_trailer_coach_8017D7DC = {
    {
        func_dryfield_night_trailer_coach_8018231C,
        func_dryfield_night_trailer_coach_80182898,
        taskKill,
    },
};

void func_dryfield_night_trailer_coach_8018231C(Task* task)
{
    task->msgTable = &D_dryfield_night_trailer_coach_8018794C;
    Game_SetPtrSlot(task, 7);
    if (gameGetPtrSlot(0xA) != NULL) {
        Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_night_trailer_coach_801879B8, 0);
        Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_night_trailer_coach_80187CEC, 0);
    }
    if (Mc_SaveData.at4.loc.warp == 2) {
        func_800E8634((s32)&D_dryfield_night_trailer_coach_80187D00, 0, (s32)&D_dryfield_night_trailer_coach_80187F58);
    }
    if (Mc_SaveData.at4.loc.warp == 3) {
        func_800E8634((s32)&D_dryfield_night_trailer_coach_80189080, 0, (s32)&D_dryfield_night_trailer_coach_801892C0);
        Gp_SetCurBit2Flag(0x22, 1);
    }
    if (func_800E3FCC(0xA2) == 0x25) {
        func_800E3FAC(0xA2, 0x26);
    }
    D_80071090                 = 3;
    *(s32*)((u8*)task + 0x30) += 1;
}

void func_dryfield_night_trailer_coach_8018243C(Task* task)
{
    register s32 cap asm("a0");
    s32          flag;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            if (GameFlag_GetNibble(0x61) == 0) {
                func_800E8614((s32)&D_dryfield_night_trailer_coach_801880A8, 1);
                SOFT_BARRIER();
                goto inc;
            }
            if (GameFlag_GetNibble(0xE0) == 0) {
                if (GameFlag_GetNibble(0x7A) >= 4) {
                    GameFlag_SetNibble(0xE0, 1);
                    func_800E8614((s32)&D_dryfield_night_trailer_coach_80188510, 0);
                    goto kill;
                }
            }
            func_800E8614((s32)&D_dryfield_night_trailer_coach_80188348, 0);
        kill:
            taskKill(task);
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0xB) {
                cap  = (s32)&D_dryfield_night_trailer_coach_801881F8;
                flag = 0;
                goto do_8614;
            }
            if (Gp_GetCapEventKey() == 0xC) {
                cap = (s32)&D_dryfield_night_trailer_coach_80188708;
                goto a1_1;
            }
            if (Gp_GetCapEventKey() == 0xD) {
                if (GameFlag_GetNibble(0x5B) == 0) {
                    GameFlag_SetNibble(0x5B, 1);
                    GameFlag_SetNibble(0x4C, 0);
                    Gp_ApplyAreaRecs(&D_dryfield_night_trailer_coach_8018C208);
                    func_800E8634((s32)&D_dryfield_night_trailer_coach_801889A8, 1,
                                  (s32)&D_dryfield_night_trailer_coach_80188F00);
                    func_800E3FAC(0xA2, 0x13);
                    D_8007272D = 2;
                } else {
                    cap = (s32)&D_dryfield_night_trailer_coach_80188858;
                a1_1:
                    flag = 1;
                do_8614:
                    func_800E8614(cap, flag);
                }
            }
            goto inc;
        case 1:
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
        inc:
            task->state = task->state + 1;
            return;
        case 4:
            taskKill(task);
            return;
    }
}

/// Plays the sound event in `spawnArg2` on its first frame and again at frame
/// 0x50, then asks for the task's own kill at frame 0x78.
void func_dryfield_night_trailer_coach_80182610(Task* task)
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

s32 func_dryfield_night_trailer_coach_801826A0(void)
{
    return 0;
}

extern s32 func_80179954(RoomEventMsg* in, RoomEventMsg* out);

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_80179954`. Always returns 1.
s32 func_dryfield_night_trailer_coach_801826A8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179954(in, out);
    return 1;
}

/// Cutscene trigger for the trailer coach at night. Where the day version has
/// its own record and a save-view reset, this one only runs at the two ends of
/// the visit.
///
/// Request 0xE forces area 8 for the scene, fills the room's cutscene record
/// the same way the motel lobby fills its own -- save view 8, slot 1, and the
/// cap file picked by `GameFlag_GetNibble(0x7A)` (file 1 below four, file 2 at
/// four or more) -- then hands it to `D_dryfield_night_trailer_coach_80184FE4`. Request 3
/// spawns entry 0 of the room's task table at `0x8018797C` and request 0x17
/// asks the cap system to run command 0x17. Always returns 0.
extern s8              D_80072170;
extern TaskDesc        D_dryfield_night_trailer_coach_8018797C;
extern RoomCutsceneRec D_dryfield_night_trailer_coach_8018C21C;

s32 func_dryfield_night_trailer_coach_801826EC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xE) {
        D_80072170                                      = 1;
        D_dryfield_night_trailer_coach_8018C21C.field_0 = 8;
        D_dryfield_night_trailer_coach_8018C21C.field_1 = 1;
        if (GameFlag_GetNibble(0x7A) < 4) {
            D_dryfield_night_trailer_coach_8018C21C.field_14 = 0x380;
            D_dryfield_night_trailer_coach_8018C21C.field_3  = 1;
        } else {
            D_dryfield_night_trailer_coach_8018C21C.field_14 = 0x3C0;
            D_dryfield_night_trailer_coach_8018C21C.field_3  = 2;
        }
        D_dryfield_night_trailer_coach_8018C21C.field_2  = 0;
        D_dryfield_night_trailer_coach_8018C21C.field_4  = 0x531B0003;
        D_dryfield_night_trailer_coach_8018C21C.field_8  = 0x531B0005;
        D_dryfield_night_trailer_coach_8018C21C.field_10 = 0x531B0004;
        D_dryfield_night_trailer_coach_8018C21C.field_C  = 0x531B0006;
        Task_SpawnFromTable(&D_dryfield_night_trailer_coach_80184FE4, 0, 3, (s32)&D_dryfield_night_trailer_coach_8018C21C);
    }
    if (arg2 == 3) {
        Task_SpawnFromTable(&D_dryfield_night_trailer_coach_8018797C, 0, 0, 0);
    }
    if (arg2 == 0x17) {
        Gp_RunCapCmd1(0x17);
    }
    return 0;
}

s32 func_dryfield_night_trailer_coach_80182800(void)
{
    return 0;
}

s32 func_dryfield_night_trailer_coach_80182808(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        Gp_EnqueueStageSnd6(0x531B000D, 0, 0);
    }
    return 0;
}

void func_dryfield_night_trailer_coach_8018283C(void)
{
    Gp_StartCapSlot(9, 0, 1);
}

void func_dryfield_night_trailer_coach_80182864(void)
{
    func_800D4D2C((GameFlag_GetNibble(0xE0) == 0) ? 0x20 : 0x21);
}

extern u8 D_8007216C;

void func_dryfield_night_trailer_coach_80182898(Task* task)
{
    char pad[0x10];

    if (D_8007216C == 5) {
        D_80071090 = 0;
    } else {
        D_80071090 = 3;
    }
}

/// Runs the handler for the task's state from a stack copy of
/// `D_dryfield_night_trailer_coach_8017D7DC`.
void func_dryfield_night_trailer_coach_801828CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_trailer_coach_8017D7DC;
    sp.funcs[task->state](task);
}
