#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include "main/display.h"
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
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/4CC.h"
#include "rooms/room_common.h"

extern UiObject*      D_80067634;
extern UiObjectDesc   D_8010EFA0;
extern GpItemScan     D_80072724;
extern RoomShopStock  D_8010E138[];
extern u8             D_80071072;
extern u8             RoomsShared8017f938Bp[];
extern u8             RoomsShared8017fdb8Msg[];
extern u8             RoomsShared8017f108Msg[];
extern u8             RoomsShared8017eb5cMode0[];
extern u8             RoomsShared8017eb5cMode1[];
extern u8             RoomsShared8017eb5cMode2[];
extern u8             RoomsShared8017eb5cMode3[];
extern u8             RoomsShared8017f31cMsg0[];
extern u8             RoomsShared8017f31cMsg1[];
extern u8             RoomsShared8017f31cMsg2[];
extern u8             RoomsShared8017f764Amount[];
extern u8             RoomsShared8017f938Times[];
extern u8             D_mist_parking_80186718[];
extern UiListItemFunc D_mist_parking_80186538[];
extern UiList         RoomsShared8017ed7cList;
extern UiObjectDesc   RoomsShared8017eb5cListDesc;
extern UiObjectDesc   D_mist_parking_80186654;
extern UiObjectDesc   RoomsShared8017ed7cRowsDesc;
extern UiObjectDesc   RoomsShared8017ff9cDesc;
extern UiObjectDesc   RoomsShared8017f108NoticeDesc;
extern UiObjectDesc   RoomsShared8017f108BuyDesc;
extern char           Gp_StrEmpty[];
extern s32            RoomsShared8017f49cQty;
extern GpItemMap*     RoomsShared8017f49cMap;

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D5C0);

u16* RoomsShared8017eb5cIdList(s32 arg0);

/// One row of the vending machine's price ladder (`D_mist_parking_801863B0`,
/// thirteen rows). `spendThreshold` is the running total the player has to have
/// spent for the row to unlock — the last row's is `S32_MAX`, so it never does
/// on its own — and `items` are the three ids the row then offers.
/// `Mc_SaveData.field_934` holds one bit per row and is what this builder reads.
typedef struct {
    /* 0x0 */ s32  spendThreshold;
    /* 0x4 */ s16  items[3];
    /* 0xA */ byte pad_A[2];
} MistParkingShopTier;

extern MistParkingShopTier D_mist_parking_801863B0[13];
extern s32                 RoomsShared8017df68Selected;

/// `main`'s rounded-rect panel fill (`Ui_LayoutWithMode0` / `Ui_LayoutWithMode1`
/// are the two named wrappers): draws a `w` x `h` box at (`x`, `y`) relative to
/// the object's base, `mode` selecting the fill style.
/// Fills `shop` with the ids the vending machine currently offers, then sorts
/// them by `Gp_ItemSortKey` and caps the visible row count at 9.
///
/// The upper halfword of the owning task's `spawnArg1` picks the machine's
/// mode, which decides both the fixed id list (`RoomsShared8017eb5cIdList`)
/// and which of a price row's items the machine will stock: mode 0 takes tools
/// (0x80-0x9F) plus a handful of key items, mode 1 armour (0xA0-0xBF), mode 2
/// weapon parts (0x60-0x7F) and mode 3 everything up to 0x5F that the other
/// three modes do not carry. Mode 3 additionally offers the twelve two-bit
/// stock levels the save keeps in `Mc_SaveData.field_938`, whose first slot
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
    ids  = RoomsShared8017eb5cIdList(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        RoomsShared8017e3f4(shop, obj, *ids);
        ids++;
    }

    if (Mc_SaveData.field_23 == 1) {
        Mc_SaveData.field_934 = 0x1FFF;
        Mc_SaveData.field_938 = -1;
    }

    if (Mc_SaveData.field_F == 0) {
        if (Mc_SaveData.field_934 != 0) {
            for (tier = 0; tier < 13; tier++) {
                unlocked = Mc_SaveData.field_934 & (1 << tier);
                if (unlocked != 0) {
                    for (j = 0; j < 3; j++) {
                        item = D_mist_parking_801863B0[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    RoomsShared8017e3f4(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    RoomsShared8017e3f4(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    RoomsShared8017e3f4(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    RoomsShared8017e3f4(shop, obj, item);
                                }
                                break;
                        }
                    }
                }
            }
        }

        if ((mode >> 16) == 3) {
            for (slot = 0; slot < 0xC; slot++) {
                level = (Mc_SaveData.field_938 >> (slot * 2)) & 3;
                if (slot == 0 ? level >= 2 : level > 0) {
                    /* The assignment keeps `+ 0xE` on the level instead of
                       letting GCC reassociate it onto the row base. */
                    RoomsShared8017e3f4(shop, obj, slot * 3 + (id = level + 0xE));
                }
            }
        }
    }

    list = (RoomShopList*)obj->owner->idMap;
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
    RoomsShared8017df68Selected = -1;
}

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
    Ui_DrawText((UiPanel*)obj, "Select");
    if (task->state == 0) {
        mem  = Mem_Calloc(sizeof(RoomShopList), 0);
        shop = mem;
        if (mem != NULL) {
            /* Keeps the allocation's own register distinct from the tested one,
               so the null test stays on $v0 and the copy fills its delay slot. */
            SOFT_TOUCH_REG(shop);
            task->idMap        = (TaskIdMap*)shop;
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
    Ui_UpdateListNoAnim(task->idMap, obj);
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
    func_8002E53C(&req, RoomsShared8017f938Bp);

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

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", RoomsShared8017f938Bp);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", RoomsShared8017ed7cTitle);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", RoomsShared8017ef24Total);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", RoomsShared8017f31cNotice);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", RoomsShared8017f49cCharge);
