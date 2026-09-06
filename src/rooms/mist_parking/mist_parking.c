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
extern u8             D_mist_parking_80186450[];
extern u8             D_mist_parking_801864BC[];
extern u8             D_mist_parking_801864C4[];
extern u8             D_mist_parking_801864D0[];
extern u8             D_mist_parking_801864D8[];
extern u8             D_mist_parking_801864E0[];
extern u8             D_mist_parking_801864F4[];
extern u8             D_mist_parking_80186504[];
extern u8             RoomsShared8017f764Amount[];
extern u8             RoomsShared8017f938Times[];
extern u8             D_mist_parking_80186718[];
extern UiListItemFunc D_mist_parking_80186538[];
extern UiList         D_mist_parking_80186540;
extern UiObjectDesc   D_mist_parking_801865AC;
extern UiObjectDesc   D_mist_parking_80186654;
extern UiObjectDesc   D_mist_parking_801865C8;
extern UiObjectDesc   RoomsShared8017ff9cDesc;
extern UiObjectDesc   D_mist_parking_80186600;
extern UiObjectDesc   D_mist_parking_80186670;
extern char           Gp_StrEmpty[];
extern s32            D_mist_parking_80195310;
extern GpItemMap*     D_mist_parking_80195314;

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D5C0);

u16* func_mist_parking_8017D8F8(s32 arg0);

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
/// mode, which decides both the fixed id list (`func_mist_parking_8017D8F8`)
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
    ids  = func_mist_parking_8017D8F8(mode);

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

void func_mist_parking_8017EB5C(DialogPrompt* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, RoomsShared8017fdb8Msg, prompt->field_1C, one, 0);
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

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", RoomsShared8017f938Bp);

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
    Ui_DrawText((UiPanel*)obj, "List");
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
    WipSysConfig* cfg;
    McItemScan*   scan;
    s8*           p;
    s32           x;
    s32           y;
    s32           y2;
    s32           col;
    s32           capacity;
    s32           count;

    obj = task->spawnArg2;
    cfg = &Wip_SysConfig;
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
    func_8002E53C(&req0, RoomsShared8017f938Bp);

    Text_ItoaUnsigned((u8*)digits, cfg->field_C);
    Text_DrawPrompt(obj, col, y + 0x19, (u8*)digits, 0x606060, 3, 2);

    y2              = y + 0x28;
    req1.x          = obj->baseX + x;
    req1.y          = obj->baseY + (y2 - 6);
    req1.otIndex    = (s16)obj->drawOrder + 1;
    req1.field_8    = 0x606060;
    req1.glyphTable = 5;
    req1.centerMode = 0;
    req1.field_E    = 1;
    func_8002E53C(&req1, "TOTAL");

    p        = total;
    scan     = &Mc_SaveData.field_5BC;
    count    = Gp_CountScanItems(scan);
    capacity = scan->field_1;
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
    WipSysConfig* cfg;
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
        cfg   = &Wip_SysConfig;
        price = Gp_ItemDescs[itemId].price;
        scan  = &D_80072724;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->field_C >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_mist_parking_80186600, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_mist_parking_80186600, 1, 1, 1, obj);
                }
                obj->status = 0;
            } else if (((RoomShopTask*)obj->owner->parent)->mode == mode) {
                child = Ui_SpawnFromDesc(&D_mist_parking_80186670, itemId, 1, 1, obj);
                if (child != NULL) {
                    Ui_ClampDialogRect((UiPanel*)child, (UiPanel*)prompt, (UiPanel*)obj);
                    obj->status = 0;
                }
            } else {
                cfg->field_C -= price;
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

    Ui_DrawText((UiPanel*)obj, "Notice");
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

/* "Charge" plus the non-zero byte the original object left in the alignment
   pad; sized to the whole 8-byte block so it is emitted byte for byte. */
static const char ChargeMsg[8] = "Charge\0\342";

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
    Ui_DrawText((UiPanel*)obj, (char*)ChargeMsg);

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
                D_mist_parking_80195310 = slot->field_1;
                slot->field_1           = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_mist_parking_80195310 = slot->field_3;
                slot->field_3           = Gp_GetRelatedQty(itemId, 1);
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
