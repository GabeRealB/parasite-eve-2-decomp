#include "items.h"

#include <psyq/sys/types.h>

#include "common.h"

#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "gameplay/items.h"
#include "scene_runtime.h"

#include "main/mc.h"
#include "main/task_types.h"

u16 Gp_PubItemReady;

u32 D_80114DCC;

u16 Gp_PubItemQty;

InventoryItemRow* Gp_SelItemRec;

s32 D_80114DD8;

u16 Gp_PubItemLoc;

u16 D_80114DDE;

s32 D_80114DE0;

s32 D_80114DE4;

s32 D_80114DE8;

u16 Gp_PubItemId;

EquipmentWeaponLoadOptionsTable Gp_RelatedQty0      = { { { 32, { 160, 161, 162 } }, { 20, { 160, 161, 162 } }, { 100, { 160, 161, 162 } }, { 7, { 160, 161, 162 } }, { 12, { 160, 161, 162 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 6, { 166, 167, 168 } }, { 0, { 0, 0, 0 } }, { 1, { 169, 170, 171 } }, { 12, { 169, 170, 171 } }, { 3, { 172, 173, 174 } }, { 7, { 172, 173, 174 } }, { 20, { 172, 173, 174 } }, { 30, { 175, 176, 177 } }, { 200, { 175, 176, 177 } }, { 0, { 0, 0, 0 } }, { 1, { 0, 0, 0 } }, { 60, { 175, 176, 177 } }, { 90, { 175, 176, 177 } }, { 100, { 181, 185, 187 } }, { 12, { 172, 173, 174 } }, { 0, { 0, 0, 0 } }, { 30, { 175, 176, 177 } }, { 30, { 175, 176, 177 } }, { 30, { 175, 176, 177 } }, { 30, { 175, 176, 177 } }, { 30, { 175, 176, 177 } }, { 30, { 160, 161, 162 } }, { 60, { 160, 161, 162 } }, { 90, { 160, 161, 162 } } } };
GpItemAttr                      Gp_ModStatAttrs[32] = {
    { 4096, 10, 3, 0, 0 },
    { 65, 60, 8, 30, 0 },
    { 33, 40, 5, 10, 0 },
    { 2, 0, 5, 10, 0 },
    { 258, 20, 6, 0, 0 },
    { 264, 50, 7, 10, 0 },
    { 4112, 100, 5, 0, 0 },
    { 4224, 5, 3, 20, 0 },
    { 272, 60, 5, 0, 0 },
    { 4098, 20, 6, 20, 0 },
    { 576, 0, 4, 50, 0 },
    { 136, 30, 7, 50, 0 },
    { 8, 0, 4, 20, 0 },
    { 132, 0, 10, 100, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
    { 0, 0, 4, 0, 0 },
};
GpItemA0 Gp_StackLimits[32] = {
    { 50, 0, 500 },
    { 50, 0, 500 },
    { 50, 0, 500 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 50, 0, 500 },
    { 25, 0, 500 },
    { 231, 0, 999 },
    { 4, 0, 100 },
    { 4, 0, 100 },
    { 4, 0, 100 },
    { 10, 0, 200 },
    { 10, 0, 200 },
    { 10, 0, 200 },
    { 80, 0, 800 },
    { 231, 0, 999 },
    { 231, 0, 999 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 1 },
};

s32 Gp_LookupBit2Item(s32 arg0)
{
    GpBit2List*      lists;
    AreaObjectPlace* rec;
    u16*             tail;
    GpItemA0*        attrs;
    s32              idx;
    s32              matched;
    u16              item;
    u16              extra;
    s32              term;
    s32              found;

    idx   = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage;
    lists = Gp_Bit2Banks[idx].field_0;
    found = 0;
    if (lists != NULL) {
        if (lists->field_0.sentinel != 0x7FFFFFFF) {
            term = AREA_OBJECT_PLACE_END;
            do {
                rec     = lists->field_0.records;
                matched = 0;
                if (rec != NULL) {
                    if (rec->flagIndex != term) {
                        attrs = Gp_StackLimits;
                        tail  = &rec->kind;
                        do {
                            if (rec->flagIndex == arg0) {
                                item          = *tail;
                                extra         = PARENT_OF(tail, AreaObjectPlace, kind)->state;
                                Gp_PubItemId  = arg0;
                                Gp_PubItemLoc = item;
                                D_80114DDE    = extra;
                                if (item < 0x100U) {
                                    if (func_800B7420(*tail) != 0) {
                                        if ((u32)(*tail - 0x80) < 0x20U) {
                                            Gp_PubItemLoc = 0x3D;
                                        } else {
                                            Gp_PubItemLoc = 0xD;
                                        }
                                        Gp_PubItemQty   = 1;
                                        Gp_PubItemReady = 1;
                                    } else if ((u32)(*tail - 0xA0) < 0x20U) {
                                        if (Gp_GetCurBit2Flag(arg0) != 3) {
                                            idx           = *tail - 0xA0;
                                            Gp_PubItemQty = attrs[idx].perBuy;
                                        } else {
                                            idx           = *tail - 0xA0;
                                            Gp_PubItemQty = attrs[idx].maxHeld;
                                        }
                                        Gp_PubItemReady = 1;
                                    } else {
                                        Gp_PubItemQty   = 1;
                                        Gp_PubItemReady = 1;
                                    }
                                }
                                found   = 1;
                                matched = found;
                                break;
                            }
                            rec++;
                            tail = &PARENT_OF(tail, AreaObjectPlace, kind)[1].kind;
                        } while (rec->flagIndex != term);
                    }
                }
                if (matched == 1) {
                    break;
                }
                lists++;
            } while (lists->field_0.sentinel != 0x7FFFFFFF);
        }
    }
    return found;
}
