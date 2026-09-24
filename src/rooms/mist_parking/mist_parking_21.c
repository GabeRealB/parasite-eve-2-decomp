#include "common.h"
#include <psyq/libgte.h>
#include "gameplay/3688.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"
#include "main/wipsys.h"
#include "gameplay/268.h"
#include "main/mc.h"
#include "gameplay/4CC.h"
#include "main/session.h"
#include "rooms/mist_parking.h"

extern char Gp_StrEmpty[];

extern UiObject*    D_80067634;
extern UiObjectDesc D_8010D80C;

extern u8 D_80071072;

extern GpItemScan    D_80072724;
extern RoomShopStock D_8010E138[];

/// Titles, labels and messages of the shop's panels.
extern u8 D_mist_parking_8017D6D0[];
extern u8 D_mist_parking_8017D6D8[];
extern u8 D_mist_parking_8017D6DC[];
extern u8 D_mist_parking_8017D6E4[];
extern u8 D_mist_parking_8017D6EC[];
extern u8 D_mist_parking_8017D6F4[];
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
            } else if (((RoomShopTask*)obj->owner->parent)->mode == mode) {
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
