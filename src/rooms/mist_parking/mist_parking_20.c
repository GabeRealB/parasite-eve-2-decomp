#include "common.h"
#include <psyq/libgte.h>
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/4CC.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"
#include "main/task.h"
#include "rooms/mist_parking.h"

extern GpItemScan    D_80072724;
extern RoomShopStock D_8010E138[];
extern UiObjectDesc  D_8010EFA0;
extern char          Gp_StrEmpty[];

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
