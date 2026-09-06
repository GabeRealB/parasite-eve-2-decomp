#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017f49c.h"

/// The charge station's transfer panel: steps through the mapped item slots,
/// takes the slot's current level as the bar's starting value and its related
/// quantity as the target, then animates the bar up to it over 0xBC frames.
/// Confirm or cancel (or the timer running out) advances to the next slot;
/// running out of slots reports code 6 to the parent. Seven rooms carry this
/// body, each with its own "Charge" label and work pair.
void RoomsShared8017f49c(Task* task)
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
    Ui_DrawText((UiPanel*)obj, (char*)RoomsShared8017f49cCharge);

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
            map                    = Gp_GetItemMap(slotId);
            RoomsShared8017f49cMap = map;
            itemId                 = map->field_1;
            slot                   = Gp_GetItemSlot(itemId);
            if (RoomsShared8017f49cMap->field_0 == 0) {
                RoomsShared8017f49cQty = slot->field_1;
                slot->field_1          = Gp_GetRelatedQty(itemId, 0);
            } else {
                RoomsShared8017f49cQty = slot->field_3;
                slot->field_3          = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown      = 0xBC;
            RoomsShared8017f49cQty <<= 8;
            task->state              = task->state + 1;
        }
    }

    curItem = RoomsShared8017f49cMap->field_1;
    relItem = RoomsShared8017f49cMap->field_2;
    if (RoomsShared8017f49cMap->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                   <<= 8;
    RoomsShared8017f49cQty += 0x40;
    if (qty < RoomsShared8017f49cQty) {
        RoomsShared8017f49cQty = qty;
    }

    y = (s16)obj->field_18;
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->field_1C + 2, y + 0x23, RoomsShared8017f49cQty >> 8, 0x606060);
    h = (s16)obj->field_1A;
    func_800C0E20((UiPanel*)obj, (s16)obj->field_1C + 2, (s16)obj->field_1E - 2, h - 6, qty,
                  RoomsShared8017f49cQty, 0x1741F);

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
