#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "rooms/room_common.h"

/// One row of the shop's price ladder (`D_shelter_b1_armory_801821F4`,
/// thirteen rows): a spending threshold and the three item ids the row offers
/// once `Mc_SaveData.shopTiers` has its bit set. Only `items` is read here.
typedef struct ShelterB1ArmoryShopTier {
    /* 0x0 */ s32  spendThreshold;
    /* 0x4 */ s16  items[3];
    /* 0xA */ byte pad_A[2];
} ShelterB1ArmoryShopTier;
STATIC_ASSERT_SIZEOF(ShelterB1ArmoryShopTier, 0xC);

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern char          Gp_StrEmpty[];
extern UiObject*     D_80067634;
extern u8            D_80071072;
extern s16           D_80071076;
extern GpItemScan    D_80072724;
extern UiObjectDesc  D_8010D80C;
extern RoomShopStock D_8010E138[];
extern UiObjectDesc  D_8010EFA0;

/// `Mc_SaveData.at4.loc.view`, spelled by address because the store below
/// relocates against this name.
extern u8 D_8007216C;

/// The 0xFFFF-terminated item id lists `func_shelter_b1_armory_8017D768`
/// chooses from, and the one it returns when no case matches.
extern u16 D_shelter_b1_armory_80181E9C[];
extern u16 D_shelter_b1_armory_80181EA4[];
extern u16 D_shelter_b1_armory_80181EAC[];
extern u16 D_shelter_b1_armory_80181EB4[];
extern u16 D_shelter_b1_armory_80181EC4[];
extern u16 D_shelter_b1_armory_80181ED4[];
extern u16 D_shelter_b1_armory_80181EE4[];
extern u16 D_shelter_b1_armory_80181EEC[];
extern u16 D_shelter_b1_armory_80181EFC[];
extern u16 D_shelter_b1_armory_80181F0C[];
extern u16 D_shelter_b1_armory_80181F1C[];
extern u16 D_shelter_b1_armory_80181F24[];
extern u16 D_shelter_b1_armory_80181F38[];
extern u16 D_shelter_b1_armory_80181F50[];
extern u16 D_shelter_b1_armory_80181F64[];
extern u16 D_shelter_b1_armory_80181F6C[];
extern u16 D_shelter_b1_armory_80181F7C[];
extern u16 D_shelter_b1_armory_80181F94[];
extern u16 D_shelter_b1_armory_80181FA8[];
extern u16 D_shelter_b1_armory_80181FB0[];
extern u16 D_shelter_b1_armory_80181FC4[];
extern u16 D_shelter_b1_armory_80181FE0[];
extern u16 D_shelter_b1_armory_80181FF0[];
extern u16 D_shelter_b1_armory_80181FFC[];
extern u16 D_shelter_b1_armory_80182014[];
extern u16 D_shelter_b1_armory_80182030[];
extern u16 D_shelter_b1_armory_80182044[];
extern u16 D_shelter_b1_armory_8018204C[];
extern u16 D_shelter_b1_armory_80182060[];
extern u16 D_shelter_b1_armory_80182080[];
extern u16 D_shelter_b1_armory_80182090[];
extern u16 D_shelter_b1_armory_8018209C[];
extern u16 D_shelter_b1_armory_801820B4[];
extern u16 D_shelter_b1_armory_801820B8[];
extern u16 D_shelter_b1_armory_801820BC[];
extern u16 D_shelter_b1_armory_801820C4[];
extern u16 D_shelter_b1_armory_801820D4[];
extern u16 D_shelter_b1_armory_801820DC[];
extern u16 D_shelter_b1_armory_801820E4[];
extern u16 D_shelter_b1_armory_801820EC[];
extern u16 D_shelter_b1_armory_801820F8[];
extern u16 D_shelter_b1_armory_80182100[];
extern u16 D_shelter_b1_armory_8018210C[];
extern u16 D_shelter_b1_armory_80182114[];
extern u16 D_shelter_b1_armory_80182120[];
extern u16 D_shelter_b1_armory_8018212C[];
extern u16 D_shelter_b1_armory_80182134[];
extern u16 D_shelter_b1_armory_8018213C[];
extern u16 D_shelter_b1_armory_80182148[];
extern u16 D_shelter_b1_armory_80182154[];
extern u16 D_shelter_b1_armory_8018215C[];
extern u16 D_shelter_b1_armory_80182168[];
extern u16 D_shelter_b1_armory_80182174[];
extern u16 D_shelter_b1_armory_80182180[];
extern u16 D_shelter_b1_armory_80182184[];
extern u16 D_shelter_b1_armory_80182190[];
extern u16 D_shelter_b1_armory_8018219C[];
extern u16 D_shelter_b1_armory_801821A8[];
extern u16 D_shelter_b1_armory_801821B0[];
extern u16 D_shelter_b1_armory_801821BC[];
extern u16 D_shelter_b1_armory_801821C8[];
extern u16 D_shelter_b1_armory_801821D4[];
extern u16 D_shelter_b1_armory_801821DC[];
extern u16 D_shelter_b1_armory_801821E8[];
extern u16 D_shelter_b1_armory_80182378[];

/// The shop's price ladder.
extern ShelterB1ArmoryShopTier D_shelter_b1_armory_801821F4[13];

/// The item id the shop list's cursor last rested on.
extern s32 D_shelter_b1_armory_80182290;

/// Messages and labels of the shop's panels.
extern u8 D_shelter_b1_armory_80182294[];
extern u8 D_shelter_b1_armory_801822A8[];
extern u8 D_shelter_b1_armory_801822B0[];
extern u8 D_shelter_b1_armory_801822C0[];
extern u8 D_shelter_b1_armory_801822C4[];
extern u8 D_shelter_b1_armory_80182300[];
extern u8 D_shelter_b1_armory_80182308[];
extern u8 D_shelter_b1_armory_80182314[];
extern u8 D_shelter_b1_armory_8018231C[];
extern u8 D_shelter_b1_armory_80182324[];
extern u8 D_shelter_b1_armory_80182338[];
extern u8 D_shelter_b1_armory_80182348[];
extern u8 D_shelter_b1_armory_80182368[];
extern u8 D_shelter_b1_armory_80182374[];

/// Row handlers, lists and panel descriptors of the shop's panels.
extern UiListItemFunc D_shelter_b1_armory_8018237C[];
extern UiList         D_shelter_b1_armory_80182384;
extern UiList         D_shelter_b1_armory_801823B0;
extern UiObjectDesc   D_shelter_b1_armory_801823D4;
extern UiObjectDesc   D_shelter_b1_armory_801823F0;
extern UiObjectDesc   D_shelter_b1_armory_8018240C;
extern UiObjectDesc   D_shelter_b1_armory_80182428;
extern UiObjectDesc   D_shelter_b1_armory_80182444;
extern UiObjectDesc   D_shelter_b1_armory_8018247C;
extern UiObjectDesc   D_shelter_b1_armory_80182498;
extern UiObjectDesc   D_shelter_b1_armory_801824B4;

/// Descriptor of the event task the door gate spawns, and the table the
/// room's own tasks are spawned from.
extern TaskDesc D_shelter_b1_armory_801824DC;
extern TaskDesc D_shelter_b1_armory_801824E8[];

/// Message handlers the room's controller task installs in pointer slot 7.
extern GpMsgEntry D_shelter_b1_armory_80182500[];

/// Work pair of the charge panel `func_shelter_b1_armory_8017F30C`: the
/// animated quantity in 24.8 fixed point, and the item map of the slot being
/// charged.
extern s32        D_shelter_b1_armory_80185574;
extern GpItemMap* D_shelter_b1_armory_80185578;

/// The view index `func_shelter_b1_armory_8018034C` saves while it runs and
/// restores when it finishes.
extern u8 D_shelter_b1_armory_8018557C;

/// The event message and request the door gate latched, and the flag saying
/// one was latched.
extern RoomEventMsg D_shelter_b1_armory_80185584;
extern u8           D_shelter_b1_armory_8018558C;
extern RoomEventReq D_shelter_b1_armory_80185590;

void func_shelter_b1_armory_80180740(Task* task);
void func_shelter_b1_armory_80180784(Task* task);

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_shelter_b1_armory_80182378`.
u16* func_shelter_b1_armory_8017D768(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181F7C;
                    case 1:
                        return D_shelter_b1_armory_80181F94;
                    case 2:
                        return D_shelter_b1_armory_80181FA8;
                    case 3:
                        return D_shelter_b1_armory_80181FB0;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181FC4;
                    case 1:
                        return D_shelter_b1_armory_80181FE0;
                    case 2:
                        return D_shelter_b1_armory_80181FF0;
                    case 3:
                        return D_shelter_b1_armory_80181FFC;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182014;
                    case 1:
                        return D_shelter_b1_armory_80182030;
                    case 2:
                        return D_shelter_b1_armory_80182044;
                    case 3:
                        return D_shelter_b1_armory_8018204C;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182060;
                    case 1:
                        return D_shelter_b1_armory_80182080;
                    case 2:
                        return D_shelter_b1_armory_80182090;
                    case 3:
                        return D_shelter_b1_armory_8018209C;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181EC4;
                    case 1:
                        return D_shelter_b1_armory_80181ED4;
                    case 2:
                        return D_shelter_b1_armory_80181EE4;
                    case 3:
                        return D_shelter_b1_armory_80181EEC;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181F38;
                    case 1:
                        return D_shelter_b1_armory_80181F50;
                    case 2:
                        return D_shelter_b1_armory_80181F64;
                    case 3:
                        return D_shelter_b1_armory_80181F6C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181EFC;
                    case 1:
                        return D_shelter_b1_armory_80181F0C;
                    case 2:
                        return D_shelter_b1_armory_80181F1C;
                    case 3:
                        return D_shelter_b1_armory_80181F24;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181E9C;
                    case 1:
                        return D_shelter_b1_armory_80181EA4;
                    case 2:
                        return D_shelter_b1_armory_80181EAC;
                    case 3:
                        return D_shelter_b1_armory_80181EB4;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182148;
                    case 1:
                        return D_shelter_b1_armory_80182154;
                    case 2:
                        return D_shelter_b1_armory_8018215C;
                    case 3:
                        return D_shelter_b1_armory_80182168;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182174;
                    case 1:
                        return D_shelter_b1_armory_80182180;
                    case 2:
                        return D_shelter_b1_armory_80182184;
                    case 3:
                        return D_shelter_b1_armory_80182190;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_8018219C;
                    case 1:
                        return D_shelter_b1_armory_801821A8;
                    case 2:
                        return D_shelter_b1_armory_801821B0;
                    case 3:
                        return D_shelter_b1_armory_801821BC;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_801821C8;
                    case 1:
                        return D_shelter_b1_armory_801821D4;
                    case 2:
                        return D_shelter_b1_armory_801821DC;
                    case 3:
                        return D_shelter_b1_armory_801821E8;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_801820D4;
                    case 1:
                        return D_shelter_b1_armory_801820DC;
                    case 2:
                        return D_shelter_b1_armory_801820E4;
                    case 3:
                        return D_shelter_b1_armory_801820EC;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182120;
                    case 1:
                        return D_shelter_b1_armory_8018212C;
                    case 2:
                        return D_shelter_b1_armory_80182134;
                    case 3:
                        return D_shelter_b1_armory_8018213C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_801820F8;
                    case 1:
                        return D_shelter_b1_armory_80182100;
                    case 2:
                        return D_shelter_b1_armory_8018210C;
                    case 3:
                        return D_shelter_b1_armory_80182114;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_801820B4;
                    case 1:
                        return D_shelter_b1_armory_801820B8;
                    case 2:
                        return D_shelter_b1_armory_801820BC;
                    case 3:
                        return D_shelter_b1_armory_801820C4;
                }
                break;
        }
    }
    return D_shelter_b1_armory_80182378;
}

/// Draws one row of the shop list and handles its input, recording the row's
/// id as the cursor item while the row is selected. Row 0xFFFE is greyed out
/// and unselectable unless `Gp_HasMappedItem` answers non-zero, and opens its
/// own panel; row 0xFFFC is greyed out while the scan holds item 0x8F. Any
/// other row is an item with its price, greyed out when `func_800B7420`
/// refuses it; confirm opens the buy panel and button 0x10 the item's detail
/// panel.
void func_shelter_b1_armory_8017DDD8(DialogPrompt* prompt, UiObject* obj)
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
        D_shelter_b1_armory_80182290 = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->status;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam((s32)D_shelter_b1_armory_801822C4, 0, 0);
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
        func_8002E53C(&req, D_shelter_b1_armory_801822B0);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_shelter_b1_armory_8018247C, 0, 1, 1, obj);
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
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_armory_801822C0, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_shelter_b1_armory_80182428, itemId, 1, 1, obj);
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
            child2 = Ui_SpawnFromDesc(&D_shelter_b1_armory_80182428, itemId, 1, 1, obj);
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
void func_shelter_b1_armory_8017E264(RoomShopList* shop, UiObject* obj, s32 item)
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

/// Fills `shop` with the ids the shop currently offers, then sorts them by
/// `Gp_ItemSortKey`, caps the visible row count at 9 and clears the cursor
/// item.
///
/// The upper halfword of the owning task's `spawnArg1` is the mode, which picks
/// the fixed id list (`func_shelter_b1_armory_8017D768`) and, in game mode
/// 0, which items of each unlocked price row are added: mode 0 ids 0x80-0x9F
/// and 9, 0xA, 0xC, 0x42-0x46; mode 1 ids 0xA0-0xBF; mode 2 ids 0x60-0x7F and
/// 0xD; mode 3 ids 1-0x5F other than those. Mode 3 also adds, for each of the
/// twelve two-bit levels in `Mc_SaveData.shopStock`, the id of that level
/// (the first slot needs level 2). With `Mc_SaveData.demoScene` 1 every row
/// and level is unlocked first.
void func_shelter_b1_armory_8017E3B0(RoomShopList* shop, UiObject* obj)
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
    ids  = func_shelter_b1_armory_8017D768(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_shelter_b1_armory_8017E264(shop, obj, *ids);
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
                        item = D_shelter_b1_armory_801821F4[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_shelter_b1_armory_8017E264(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_shelter_b1_armory_8017E264(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_shelter_b1_armory_8017E264(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_shelter_b1_armory_8017E264(shop, obj, item);
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
                    func_shelter_b1_armory_8017E264(shop, obj, slot * 3 + (id = level + 0xE));
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
    D_shelter_b1_armory_80182290 = -1;
}

/// Titles and captions of the shop's panels.
const u8 D_shelter_b1_armory_8017D6D0[] = "Select";
const u8 D_shelter_b1_armory_8017D6D8[] = "BP";
const u8 D_shelter_b1_armory_8017D6DC[] = "List";
const u8 D_shelter_b1_armory_8017D6E4[] = "TOTAL";
const u8 D_shelter_b1_armory_8017D6EC[] = "Notice";

/// "Charge", with a stray non-zero byte after its terminator that C cannot
/// place, so the string stays assembly.
extern u8 D_shelter_b1_armory_8017D6F4[];
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", D_shelter_b1_armory_8017D6F4);

/// The shop's "Select" panel. On its first frame it allocates the
/// `RoomShopList` work block, fills it through `func_shelter_b1_armory_8017E3B0`
/// and opens the panel `D_shelter_b1_armory_80182498` beside it. Every frame it
/// draws the list and the "BP" caption; menu reports -1 and cancel 6 to the
/// parent. A child that reports 6 is torn down and the list takes input again;
/// one that reports -1 passes it up.
void func_shelter_b1_armory_8017E77C(Task* task)
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
    Ui_DrawText((UiPanel*)obj, (char*)D_shelter_b1_armory_8017D6D0);
    if (task->state == 0) {
        mem  = memCalloc(sizeof(RoomShopList), 0);
        shop = mem;
        if (mem != NULL) {
            /* Keeps the allocation's own register distinct from the tested one,
               so the null test stays on $v0 and the copy fills its delay slot. */
            SOFT_TOUCH_REG(shop);
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_shelter_b1_armory_8018237C;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_shelter_b1_armory_8017E3B0(shop, obj);
            Ui_LayoutListPanel(&shop->list, (UiPanel*)obj);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->field_12      += 8;
            shop->list.field_17 = 8;
            Ui_SpawnFromDesc(&D_shelter_b1_armory_80182498, 0, 0, 0, obj);
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
    func_8002E53C(&req, D_shelter_b1_armory_8017D6D8);

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
void func_shelter_b1_armory_8017E9CC(DialogPrompt* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_armory_801822A8, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_shelter_b1_armory_80182300;
    obj->owner->spawnArg1 = (u16)obj->owner->spawnArg1;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_shelter_b1_armory_80182308;
            obj->owner->spawnArg1 |= 0x10000;
            break;
        case 2:
            text                   = D_shelter_b1_armory_80182314;
            obj->owner->spawnArg1 |= 0x20000;
            break;
        case 3:
            text                   = D_shelter_b1_armory_8018231C;
            obj->owner->spawnArg1 |= 0x30000;
            break;
    }

    if (*func_shelter_b1_armory_8017D768(obj->owner->spawnArg1) == 0xFFFF) {
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
        Ui_SpawnFromDesc(&D_shelter_b1_armory_801823F0, obj->owner->spawnArg1, 1, 1, obj);
        obj->status = 0;
    }
}

/// The shop's "List" panel, whose rows are the modes. Its first frame clears
/// the item previews, opens the list-row panel and the preview panel, and lays
/// out its five-row list. Cancel or menu reports -1. A child reporting 6 is
/// torn down; one reporting -1 releases `Wip_UiHolder` and passes the code up.
void func_shelter_b1_armory_8017EBEC(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2;
    list          = &D_shelter_b1_armory_80182384;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_shelter_b1_armory_8017D6DC);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_shelter_b1_armory_8018240C, task->spawnArg1, 0, 1, obj);
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
void func_shelter_b1_armory_8017ED94(Task* task)
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
    func_8002E53C(&req0, D_shelter_b1_armory_8017D6D8);

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
    func_8002E53C(&req1, (char*)D_shelter_b1_armory_8017D6E4);

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
void func_shelter_b1_armory_8017EF78(DialogPrompt* prompt, UiObject* obj)
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
    func_8002E53C(&req, D_shelter_b1_armory_80182294);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &D_80072724;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_shelter_b1_armory_80182444, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_shelter_b1_armory_80182444, 1, 1, 1, obj);
                }
                obj->status = 0;
            } else if (((RoomShopTask*)obj->owner->parent)->mode == mode) {
                child = Ui_SpawnFromDesc(&D_shelter_b1_armory_801824B4, itemId, 1, 1, obj);
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
            Ui_SpawnFromDesc(&D_shelter_b1_armory_80182444, 0, 1, 1, obj);
            obj->status = 0;
        }
    }
}

/// Notice panel: shows one of three messages picked by `spawnArg1`, sized to
/// the text. Menu reports -1; confirm, cancel or 0xBC frames elapsing tell the
/// parent panel to close with 6.
void func_shelter_b1_armory_8017F18C(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1;
    obj  = task->spawnArg2;
    switch (kind) {
        case 1:
            text = D_shelter_b1_armory_80182338;
            break;
        case 2:
            text = D_shelter_b1_armory_80182348;
            break;
        default:
            text = D_shelter_b1_armory_80182324;
            break;
    }

    Ui_DrawText((UiPanel*)obj, (char*)D_shelter_b1_armory_8017D6EC);
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
void func_shelter_b1_armory_8017F30C(Task* task)
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
    Ui_DrawText((UiPanel*)obj, (char*)D_shelter_b1_armory_8017D6F4);

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
            map                          = Gp_GetItemMap(slotId);
            D_shelter_b1_armory_80185578 = map;
            itemId                       = map->field_1;
            slot                         = Gp_GetItemSlot(itemId);
            if (D_shelter_b1_armory_80185578->field_0 == 0) {
                D_shelter_b1_armory_80185574 = slot->ammoQty;
                slot->ammoQty                = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_shelter_b1_armory_80185574 = slot->attachQty;
                slot->attachQty              = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown            = 0xBC;
            D_shelter_b1_armory_80185574 <<= 8;
            task->state                    = task->state + 1;
        }
    }

    curItem = D_shelter_b1_armory_80185578->field_1;
    relItem = D_shelter_b1_armory_80185578->field_2;
    if (D_shelter_b1_armory_80185578->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                         <<= 8;
    D_shelter_b1_armory_80185574 += 0x40;
    if (qty < D_shelter_b1_armory_80185574) {
        D_shelter_b1_armory_80185574 = qty;
    }

    y = (s16)obj->field_18;
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->field_1C + 2, y + 0x23, D_shelter_b1_armory_80185574 >> 8, 0x606060);
    h = (s16)obj->field_1A;
    func_800C0E20((UiPanel*)obj, (s16)obj->field_1C + 2, (s16)obj->field_1E - 2, h - 6, qty,
                  D_shelter_b1_armory_80185574, 0x1741F);

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
void func_shelter_b1_armory_8017F5D4(Task* task)
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

    item         = D_shelter_b1_armory_80182290;
    obj          = task->spawnArg2;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_shelter_b1_armory_80182290 == Gp_GetPreviewItem()) {
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
        func_8002E53C(&req, D_shelter_b1_armory_80182368);
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
void func_shelter_b1_armory_8017F7A8(Task* task)
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
    Text_DrawPrompt(obj, left + 0x98, y, D_shelter_b1_armory_80182374, 0x606060, 3, 2);
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
    func_8002E53C(&req, D_shelter_b1_armory_8017D6D8);

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
void func_shelter_b1_armory_8017FC28(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_shelter_b1_armory_801822A8);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

/// A list panel over `D_shelter_b1_armory_801823B0`. Cancel reports 6 and
/// menu -1; a child reporting 6 is torn down, one reporting -1 passes it up.
void func_shelter_b1_armory_8017FCE4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_shelter_b1_armory_801823B0;
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

/// Opens the UI object described by `D_shelter_b1_armory_801823D4` for the
/// task's `spawnArg1`, waits until the object reports state -1 or 6, tears it
/// down, and ten frames later restores the frame timing, releases the
/// primitive buffer and kills the task.
void func_shelter_b1_armory_8017FE0C(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_shelter_b1_armory_801823D4, task->spawnArg1, 1, 1, NULL);
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

/// Event gate for a door message. Returns 1 while the request's flag nibble
/// (inverted when `flagId` is negative) is set. Otherwise, if the required
/// item has been collected (or none is required), it latches the message and
/// request, sets the nibble and spawns the event task, returning 2; without
/// the item it runs the request's `field_4` cap command and returns 0.
s32 func_shelter_b1_armory_8017FF40(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                         = req->flagId;
    D_shelter_b1_armory_8018558C = 0;
    neg                          = flag < 0;
    got                          = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_shelter_b1_armory_80185584 = *msg;
                D_shelter_b1_armory_80185590 = *req;
                id                           = req->flagId;
                mode                         = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b1_armory_801824DC, 0, 0, 0);
                D_shelter_b1_armory_8018558C = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns: plays the latched request's cap command and
/// its two voice lines in turn, then warps to the area, warp point and room
/// the latched message names.
void func_shelter_b1_armory_801800A4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b1_armory_80185590.field_0);
            if (D_shelter_b1_armory_80185590.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b1_armory_80185590.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b1_armory_80185590.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b1_armory_80185590.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b1_armory_80185590.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b1_armory_80185590.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b1_armory_80185584.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b1_armory_80185584.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b1_armory_80185584.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// State handlers of the room's controller task: installing its message
/// table, an idle tick, and the kill.
const TaskFuncTable3 D_shelter_b1_armory_8017D714 = {
    {
        func_shelter_b1_armory_80180740,
        func_shelter_b1_armory_80180784,
        taskKill,
    },
};

void func_shelter_b1_armory_80180214(Task* task)
{
    switch (task->state) {
        case 0:
            Display_AcquireRef();
            D_80115768 = 1;
            task->state++;
            break;
        case 3:
            Display_ReleaseRef();
            D_80115768 = 0;
            Gp_MsgPlayerWeapon(0);
            if ((u16)task->spawnArg1 == 1) {
                SndEvt_EnqueueType6(0x540D0008, 0, 0);
            }
            if ((u16)task->spawnArg1 == 2) {
                SndEvt_EnqueueType6(0x540D0009, 0, 0);
            }
            Gp_StartCapSlot(task->spawnArg1 >> 16, 0, 0);
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                if ((u16)task->spawnArg1 == 2) {
                    Gp_SetItemSeenBit(0x105, 1);
                }
                Gp_MsgPlayerWeapon(1);
                gGameSession->eventState = 0;
                taskKill(task);
            }
            break;
    }
}

void func_shelter_b1_armory_8018034C(Task* task)
{
    McSaveData* save;
    u8          view;

    switch (task->state) {
        case 0:
            gGameSession->eventState     = 1;
            gGameSession->hideHud        = 1;
            save                         = &Mc_SaveData;
            view                         = save->at4.loc.view;
            save->at4.loc.view           = 0xD;
            D_shelter_b1_armory_8018557C = view;
            Gp_MsgPlayer3F3(0);
            Gp_RunCapCmd(0x16, 0);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            func_800D4D2C(0x40);
            goto advance;
        case 2:
            task->state = 3;
        case 3:
            gGameSession->eventState = 0;
            gGameSession->hideHud    = 0;
            Gp_MsgPlayer3F3(1);
            Gp_MsgPlayerWeapon(1);
            D_8007216C = D_shelter_b1_armory_8018557C;
        advance:
            task->state = task->state + 1;
            break;
    }
}

/// Answers 1 and spawns the armory task when a pending mode-5 object with
/// `field_48` 0xFF exists and `arg2` is 0x105, 0x121 or 0x122. Event nibble
/// 0xF0 selects the task's parameter; on 0x105 a first visit also sets it.
s32 func_shelter_b1_armory_80180468(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    node = Gp_PendingObj4C;
    while (node != NULL) {
        if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
            found = 1;
            goto check;
        }
        node = node->next;
    }
    found = 0;
check:
    if (found != 0) {
        if (arg2 == 0x105) {
            gGameSession->eventState = 1;
            if (GameFlag_GetNibble(0xF0) != 0) {
                Task_SpawnOnDefaultList(D_shelter_b1_armory_801824E8, 0, 0x170003, 0);
            } else {
                Task_SpawnOnDefaultList(D_shelter_b1_armory_801824E8, 0, 0x180002, 0);
                GameFlag_SetNibble(0xF0, 1);
            }
            return 1;
        }
        if (arg2 == 0x121 || arg2 == 0x122) {
            gGameSession->eventState = 1;
            if (GameFlag_GetNibble(0xF0) != 0) {
                Task_SpawnOnDefaultList(D_shelter_b1_armory_801824E8, 0, 0x170003, 0);
            } else {
                Task_SpawnOnDefaultList(D_shelter_b1_armory_801824E8, 0, 0x190001, 0);
            }
            return 1;
        }
    }
    return 0;
}

s32 func_shelter_b1_armory_801805A8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0xB) {
        req.field_0 = 4;
        req.field_4 = 1;
        req.field_8 = 0x540D0005;
        req.field_C = 0x540D0001;
        req.flagId  = 0xA6;
        req.itemId  = 0;
        return func_shelter_b1_armory_8017FF40(&req, out);
    }
    if (in->msgId != 0xD) {
        return 1;
    }
    if (GameFlag_GetNibble(0xF0) != 0) {
        return 1;
    }
    if (in->field_5 == 0) {
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(0xD);
    }
    return 0;
}

s32 func_shelter_b1_armory_80180698(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 12:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xF0) == 0 ? 0xC : 0x17, 1);
            break;
        case 10:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xF7) != 0 ? 0x10 : 0xA, 1);
            break;
    }
    return 0;
}

/// Handler for slot-7 msg `0x13EF`: the directed action (`field_2` 1) that
/// spawns the armory script.
s32 func_shelter_b1_armory_801806F8(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(D_shelter_b1_armory_801824E8, 1, 0, 0);
    }
    return 0;
}

/// Installs the room's message table in pointer slot 7 and advances the task.
void func_shelter_b1_armory_80180740(Task* task)
{
    task->msgTable = D_shelter_b1_armory_80182500;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The controller task's idle state: does nothing.
void func_shelter_b1_armory_80180784(Task* task)
{
}

/// Runs the task's current state through its three-entry state table, copied
/// onto the stack before the call.
void func_shelter_b1_armory_8018078C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_armory_8017D714;
    sp.funcs[task->state](task);
}
