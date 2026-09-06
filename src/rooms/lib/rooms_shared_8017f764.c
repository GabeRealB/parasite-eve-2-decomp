#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "main/session.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017df68.h"
#include "rooms/rooms_shared_8017f764.h"

extern GpItemScan D_80072724;

/// Shop quantity row: the "Amount" label at the preview slot plus how many of
/// the previewed item the player already holds. Stackable items (0xA0..0xBF)
/// ask the scan for their stack quantity; everything else is counted by
/// walking the item table.
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
void RoomsShared8017f764(Task* task)
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

    item        = RoomsShared8017df68Selected;
    obj         = task->spawnArg2;
    task->flags = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && RoomsShared8017df68Selected == Gp_GetPreviewItem()) {
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
        func_8002E53C(&req, RoomsShared8017f764Amount);
        count = 0;
        guard = 0;
        if ((u32)(item - 0xA0) < 0x20U) {
            count = Gp_ScanStackQty(&D_80072724, item);
        } else {
            rec = Gp_GetItemTable(&D_80072724) + D_80072724.field_0;
            n   = D_80072724.field_1;
            SOFT_USE_REG2(guard, guard);
            for (i = 0; i < n; i++) {
                if (rec[i].field_0 == item) {
                    count++;
                }
            }
        }
        Text_DrawPrompt(obj, (s16)obj->field_1E - 2, y + 0xA, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
        guard = 0;
    }
}
