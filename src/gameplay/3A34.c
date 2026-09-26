#include "common.h"
#include "main/stage.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "main/gfxgte.h"
#include <psyq/gtemac.h>
#include <psyq/libgte.h>

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `Gp_GetObjPan`,
/// `Gp_DebugPanTask` and `Gp_DrawTargetCursor`: one world point projected to
/// the screen, with the depth the projection returned kept beside the screen
/// position. `GpPerspScratch` is the shorter block a projection carves when
/// the screen position is not kept in it.
typedef struct {
    SVECTOR vec;  // the point projected, in the space of the matrix the GTE holds
    s32     dp;   // depth-cue coefficient of the projection (`gte_stdp`)
    s32     flag; // projection status (`gte_stflg`); a negative value leaves no usable position
    s32     otz;  // distance of the point (`gte_stszotz`, `SZ3 >> 2`)
    s16     sx;   // screen X the point landed on (`gte_stsxy`)
    s16     sy;   // screen Y the point landed on
} _GpPanScratch;
STATIC_ASSERT_SIZEOF(_GpPanScratch, 0x18);

void Gp_DrawEquipSummary(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);

extern McItemRec* Gp_SelItemRec;

s32 Gp_ApplyItemUse(McItemRec* arg0)
{
    PlayerStatus* cfg;
    GameActor*    actor;
    McItemScan*   scanEquip;
    McItemScan*   scanQty;
    McItemScan*   scanRel;
    McItemScan*   scanFree;
    McItemScan*   scanId;
    McItemSlot*   slot;
    McItemRec*    table;
    McItemRec*    rec;
    McItemRec*    found;
    s32           id;
    s32           ret;
    s32           flag;
    s32           i;
    u8            count;
    s32           held;
    s32           prevId;
    s32           relId;
    s32           qty;
    s32           k;
    s32           avail;
    s32           slotNum;
    s32           sel;
    McItemRec*    hit;

    ret   = 0;
    flag  = 1;
    id    = arg0->itemId;
    actor = gameGetPtrSlot(3)->work;
    cfg   = &Player_Status;

    if (id != 0) {
        if ((u32)(id - 0x80) < 0x20U) {
            if (actor->field_954 != 2) {
                rec       = NULL;
                scanEquip = &Mc_SaveData.carriedItems;
                prevId    = cfg->weapon + 0x7F;

                cfg->weapon = id - 0x7F;

                table = Gp_GetItemTable(scanEquip);
                table = &table[scanEquip->firstRow];
                count = scanEquip->rowCount;
                for (i = 0; i < count; i++) {
                    if (table->itemId == prevId) {
                        rec = table;
                    }
                    table++;
                }
                if (rec != NULL) {
                    rec->attachSlot = arg0->attachSlot;
                    Gp_RefreshItemRow(arg0);
                }
                Gp_SetItemSeenBit(id, 1);
            }
            ret = 1;
        } else if ((u32)(id - 0xA0) < 0x20U) {
            relId = 0;
            qty   = 0;
            held  = cfg->weapon + 0x7F;
            slot  = Gp_GetItemSlot(held);
            if (Gp_EquipRelatedBank(0, held, id, 0) == 0) {
                Gp_PendingRelatedId = id;
                Gp_RelatedPending   = flag;
                relId               = slot->ammoId;
                if (relId != id) {
                    cfg->weaponSlotItem = id + 0x61;
                    slot->ammoId        = id;
                    slot->ammoQty       = 0;
                }
                Gp_SetItemSeenBit(id, 1);
                ret = 1;
            } else if (Gp_EquipRelatedBank(1, held, id, 0) == 0) {
                Gp_PendingRelatedId = -id;
                Gp_RelatedPending   = flag;
                relId               = slot->attachId;
                if (relId != id) {
                    slot->attachId  = id;
                    slot->attachQty = 0;
                }
                Gp_SetItemSeenBit(id, 1);
                ret = 1;
            }

            if (relId != 0 && relId != 0xFF) {
                scanQty = &Mc_SaveData.carriedItems;
                qty     = Gp_ScanStackQty(scanQty, relId);
                qty    -= Gp_CountEquippedRelated(scanQty, relId);
            }
            if (qty > 0) {
                scanRel = &Mc_SaveData.carriedItems;
                hit     = NULL;
                table   = Gp_GetItemTable(scanRel);
                i       = 0;
                table   = &table[scanRel->firstRow];
                count   = scanRel->rowCount;
                for (; i < count; i++) {
                    if (table->itemId == relId) {
                        hit = table;
                    }
                    table++;
                }
                found = hit;
                if (found != NULL && found->attachSlot == 0) {
                    slotNum  = -1;
                    scanFree = &Mc_SaveData.carriedItems;
                    Gp_GetItemTable(scanFree);
                    for (k = 0; k < 3; k++) {
                        avail = 1;
                        table = Gp_GetItemTable(scanFree);
                        i     = 0;
                        table = &table[scanFree->firstRow];
                        count = scanFree->rowCount;
                        for (; i < count; i++) {
                            if (table->itemId != 0 && table->attachSlot == k + 1) {
                                avail = 0;
                                break;
                            }
                            table++;
                        }
                        if (avail == 1) {
                            slotNum = k + 1;
                            break;
                        }
                    }

                    if (slotNum == -1) {
                        scanId = &Mc_SaveData.carriedItems;
                        hit    = NULL;
                        table  = Gp_GetItemTable(scanId);
                        i      = 0;
                        table  = &table[scanId->firstRow];
                        count  = scanId->rowCount;
                        for (; i < count; i++) {
                            if (table->itemId == id) {
                                hit = table;
                            }
                            table++;
                        }
                        if (hit != NULL) {
                            sel               = hit->attachSlot;
                            hit->attachSlot   = 0;
                            found->attachSlot = sel;
                        }
                    } else {
                        found->attachSlot = slotNum;
                    }
                }
            }
        } else if ((u32)(id - 0x60) >= 0x20U) {
            Gp_UsedItemId = id;

            if ((u32)(id - 1) < 0x41U) {
                switch (id) {
                    case 1:
                    case 2:
                    case 3:
                        if (cfg->hp < cfg->hpMax) {
                            if (id == 1) {
                                cfg->hp += 0x2D;
                            } else if (id == 2) {
                                cfg->hp += 0x5A;
                            } else {
                                cfg->hp += 0x96;
                            }
                            if (cfg->hp > cfg->hpMax) {
                                cfg->hp = cfg->hpMax;
                            }
                            Gp_HealPending = 1;
                            ret            = 1;
                        }
                        break;
                    case 0x3C:
                        if ((u32)Mc_SaveData.hpBonus < 0xFAU) {
                            Mc_SaveData.hpBonus += 5;
                        }
                        Gp_RecalcMaxHp();
                        Gp_HealPending = 1;
                        ret            = 1;
                        cfg->hp        = cfg->hpMax;
                        break;
                    case 4:
                        Gp_TriggerPeState(1, 0xD0);
                        ret = Gp_HealPending = Gp_StateC08.field_16 = 1;
                        break;
                    case 8:
                        Gp_TriggerPeState(1, 7);
                        ret = Gp_HealPending = Gp_StateC08.field_17 = 1;
                        break;
                    case 5:
                        if (cfg->mp < cfg->mpMax || cfg->hp < cfg->hpMax) {
                            cfg->mp += 0x50;
                            cfg->hp += 0x14;
                            if (cfg->mp > cfg->mpMax) {
                                cfg->mp = cfg->mpMax;
                            }
                            if (cfg->hp > cfg->hpMax) {
                                cfg->hp = cfg->hpMax;
                            }
                            Gp_HealPending = 1;
                            ret            = 1;
                        }
                        break;
                    case 6:
                    case 7:
                        if (cfg->mp < cfg->mpMax) {
                            if (id == 6) {
                                cfg->mp += 0x19;
                            } else {
                                cfg->mp += 0x64;
                            }
                            if (cfg->mp > cfg->mpMax) {
                                cfg->mp = cfg->mpMax;
                            }
                            Gp_HealPending = 1;
                            ret            = 1;
                        }
                        break;
                    case 0x3A:
                    case 0x3B:
                        func_800A7CB0((u8)((id - 0x3A) * 3 + 0x2E));
                        func_800A7DB8(id - 0x2B);
                        Gp_SelItemRec = arg0;
                        flag          = 0;
                        ret           = 1;
                        break;
                    case 0x41:
                        func_800A7CB0(0x34);
                        func_800A7DB8(0x11);
                        Gp_SelItemRec = arg0;
                        flag          = 0;
                        ret           = 1;
                        break;
                    case 0x3D:
                        if (cfg->mp < cfg->mpMax || cfg->hp < cfg->hpMax) {
                            cfg->mp        = cfg->mpMax;
                            Gp_HealPending = 1;
                            cfg->hp        = cfg->hpMax;
                        }
                        Gp_HealPending = 1;
                        ret            = 1;
                        break;
                    case 0x3E:
                        Gp_HealPending = 1;
                        ret            = 1;
                        break;
                }
            }

            if (ret == 1 && flag != 0) {
                arg0->itemId     = 0;
                arg0->qty        = 0;
                arg0->attachSlot = 0;
                Gp_SetItemSeenBit(id, 1);
            }
        }
    }
    return ret;
}

s32 Gp_ItemIsUnusable(s32 arg0, McItemRec* arg1)
{
    PlayerStatus* cfg;
    McItemScan*   scan;
    s32           ret;
    s32           val;

    ret = 1;
    cfg = &Player_Status;
    if (arg0 != 0) {
        if ((u32)(arg0 - 0x80) < 0x20U) {
            ret = 0;
        } else if ((u32)(arg0 - 0xA0) < 0x20U) {
            scan = &Mc_SaveData.carriedItems;
            val  = arg1->qty - Gp_CountEquippedRelated(scan, arg0);
            if (val > 0) {
                if (Gp_EquipRelatedItem(scan, cfg->weapon + 0x7F, arg0, 0) == 0) {
                    ret = 0;
                }
            }
        } else if ((u32)(arg0 - 1) < 0x41U) {
            switch (arg0) {
                case 1:
                case 2:
                case 3:
                    if (cfg->hp < cfg->hpMax) {
                        ret = 0;
                    }
                    break;
                case 4:
                    if (Gp_StateC08.field_16 == 0) {
                        ret = 0;
                    }
                    break;
                case 8:
                    if ((s8)Gp_StateC08.field_17 == 0) {
                        ret = 0;
                    }
                    break;
                case 5:
                    if (cfg->mp < cfg->mpMax) {
                        ret = 0;
                    } else if (cfg->hp < cfg->hpMax) {
                        ret = 0;
                    }
                    break;
                case 6:
                case 7:
                    if (cfg->mp < cfg->mpMax) {
                        ret = 0;
                    }
                    break;
                case 0x3A:
                case 0x3B:
                case 0x3C:
                case 0x41:
                    ret = 0;
                    break;
                case 0x3D:
                    if (cfg->mp < cfg->mpMax) {
                        ret = 0;
                    } else if (cfg->hp < cfg->hpMax) {
                        ret = 0;
                    }
                    break;
                case 0x3E:
                    if (func_800B9D80(0x140) == 0) {
                        ret = 0;
                    }
                    break;
            }
        }
    }
    return ret;
}

const char D_80097440[] = { 'A', 'r', 'm', 'o', 'r', 0, 0, 0 };
const char D_80097448[] = { 'A', 't', 't', 'a', 'c', 'h', 'm', 'e', 'n', 't', 's', 0 };

void func_800D6334(Task* task)
{
    TextDrawReq name;
    TextDrawReq label;
    UiObject*   panel;
    McItemRec*  selected;
    McItemRec*  table;
    McItemScan* scan;
    McItemRec*  firstRec;
    McItemRec*  firstTable;
    McItemRec*  useRec;
    McItemRec*  useTable;
    McItemScan* firstScan;
    McItemScan* useScan;
    s32         firstI;
    s32         firstCount;
    s32         useI;
    s32         useCount;
    s32         useSlot;
    s32         usable;
    s32         armor;
    s32         x;
    s32         y;
    s32         selectedX;
    s32         item;
    s32         flags;
    s32         slot;
    s32         i;
    s32         selectedSlot;
    s32         labelX;
    s32         labelY;

    scan            = NULL;
    armor           = Player_Status.armor + 0x5F;
    panel           = task->spawnArg2;
    panel->field_2E = 0;
    panel->field_E  = 0x1C - gDisplayState.vramYOffset;
    Ui_InsetLayout((UiPanel*)panel, 0, 0, 0);
    Ui_DrawText((UiPanel*)panel, (char*)D_80097440);
    usable = 1;
    if (task->state == 0) {
        Gp_HealPending = 0;
        Gp_UsedItemId  = 0;
        if (D_8010F884 >= Gp_GetModLevel(armor)) {
            D_8010F884 = 0;
        }
        Ui_SpawnFromDesc(&D_8010F8B4, 0, 0, 0, panel);
        task->state++;
    }
    x = (s16)panel->field_1C + 4;
    y = (s16)panel->field_18 + 0x2B;
    if (task->state == 1) {
        selectedSlot = D_8010F884;
        selectedX    = x + selectedSlot * 13;
        firstRec     = NULL;
        firstScan    = &Mc_SaveData.carriedItems;
        firstTable   = Gp_GetItemTable(firstScan);
        firstI       = 0;
        firstTable   = &firstTable[firstScan->firstRow];
        firstCount   = firstScan->rowCount;
        for (; firstI < firstCount; firstI++) {
            if (firstTable->attachSlot == selectedSlot + 1) {
                firstRec = firstTable;
                break;
            }
            firstTable++;
        }
        selected = firstRec;
        if (selected != NULL) {
            item            = selected->itemId;
            name.x          = panel->baseX + x;
            name.y          = panel->baseY + 10 + y;
            name.otIndex    = (s16)panel->drawOrder + 1;
            name.field_8    = 0x606060;
            name.glyphTable = 0;
            name.centerMode = 0;
            name.field_E    = 1;
            func_8002E53C(&name, (u8*)Gp_GetItemText(item, 0, 0));
            Gp_DrawStackLeft(panel, x - 15, y + 16, selected, 0x606060, 0);
        } else {
            item = 0;
        }
        flags  = 2;
        usable = 1;
        if (Gp_ItemIsUnusable(item, selected)) {
            flags  = 6;
            usable = 0;
        }
        Gp_DrawItemIcon(panel, selectedX, y, item, flags);
        selectedX = x;
        for (slot = 0; slot < Gp_GetModLevel(armor); slot++, selectedX += 13) {
            if (slot != D_8010F884) {
                selected = NULL;
                scan     = &Mc_SaveData.carriedItems;
                table    = Gp_GetItemTable(scan);
                i        = 0;
                table    = &table[scan->firstRow];
                for (; i < scan->rowCount; i++) {
                    if (table->attachSlot == slot + 1) {
                        selected = table;
                        break;
                    }
                    table++;
                }
                item = 0;
                if (selected != NULL) {
                    item = selected->itemId;
                }
                flags = (Gp_ItemIsUnusable(item, selected) != 0) * 4;
                Gp_DrawItemIcon(panel, selectedX, y, item, flags);
            }
        }
    }
    labelX = (s16)panel->field_1C + 2;
    labelY = (s16)panel->field_18;
    Gp_DrawItemLabel(panel, labelX, labelY + 15, armor, 0x606060, 0);
    Ui_DrawHBar((UiPanel*)panel, (s16)panel->field_1C, (s16)panel->field_1E, (s16)panel->field_18 + 17);
    label.x          = panel->baseX + labelX;
    label.y          = panel->baseY + labelY + 24;
    label.otIndex    = (s16)panel->drawOrder + 1;
    label.field_8    = 0x606060;
    label.glyphTable = 5;
    label.centerMode = 0;
    label.field_E    = 1;
    func_8002E53C(&label, (u8*)D_80097448);
    if (panel->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm)) {
            if (usable == 1) {
                useSlot  = D_8010F884;
                useRec   = NULL;
                useScan  = &Mc_SaveData.carriedItems;
                useTable = Gp_GetItemTable(useScan);
                useI     = 0;
                useTable = &useTable[useScan->firstRow];
                useCount = useScan->rowCount;
                for (; useI < useCount; useI++) {
                    if (useTable->attachSlot == useSlot + 1) {
                        useRec = useTable;
                        break;
                    }
                    useTable++;
                }
                if (Gp_ApplyItemUse(useRec)) {
                    SndEvt_EnqueueType6(3, 0, 0);
                    panel->field_2E = -1;
                    task->state     = 2;
                }
            }
        } else if (Pad_CheckButtons(0, 1, 0x8000)) {
            SndEvt_EnqueueType6(2, 0, 0);
            D_8010F884--;
            if (D_8010F884 < 0) {
                D_8010F884 += Gp_GetModLevel(armor);
            }
        } else if (Pad_CheckButtons(0, 1, 0x2000)) {
            SndEvt_EnqueueType6(2, 0, 0);
            D_8010F884++;
            if (D_8010F884 >= Gp_GetModLevel(armor)) {
                D_8010F884 = 0;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu)) {
            SndEvt_EnqueueType6(4, 0, 0);
            panel->field_2E = -1;
            task->state     = 2;
        }
    }
}

/* After Armor/Attachments from func_800D6334 so overlay .rodata stays packed. */
const char Gp_StrWeapon[] = {
    'W',
    'e',
    'a',
    'p',
    'o',
    'n',
    '\0',
    0x60,
};

s32 Gp_FlushPendingRelated(s32 arg0, s32 arg1)
{
    s32 val;

    val = Gp_PendingRelatedId;
    if (val <= 0) {
        if (val >= 0) {
            return -1;
        }
        val = -val;
    }
    Gp_PendingRelatedId = 0;
    return Gp_EquipRelatedItem(&Mc_SaveData.carriedItems, arg0, val, -1);
}

McItemRec* Gp_FindItemById(s32 arg0)
{
    McItemScan* scan;
    McItemRec*  table;
    s32         i;
    s32         count;
    McItemRec*  rec;

    rec   = NULL;
    scan  = &Mc_SaveData.carriedItems;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->firstRow];
    count = scan->rowCount;
    for (; i < count; i++) {
        if (table->itemId == arg0) {
            rec = table;
        }
        table++;
    }
    return rec;
}

McItemRec* Gp_FindItemByKind(s32 arg0)
{
    McItemScan* scan;
    McItemRec*  table;
    s32         i;
    s32         count;
    McItemRec*  rec;

    rec   = NULL;
    scan  = &Mc_SaveData.carriedItems;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->firstRow];
    count = scan->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot == arg0 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    return rec;
}

McItemRec* Gp_FindItemInScan(s32 arg0, McItemScan* arg1)
{
    McItemRec* table;
    s32        i;
    s32        count;
    McItemRec* rec;

    rec   = NULL;
    table = Gp_GetItemTable(arg1);
    i     = 0;
    table = &table[arg1->firstRow];
    count = arg1->rowCount;
    for (; i < count; i++) {
        if (table->itemId == arg0) {
            rec = table;
        }
        table++;
    }
    return rec;
}

void Gp_DrawWeaponLabel(Task* arg0)
{
    UiPanel* panel;
    s32      x;
    s32      y;

    panel            = arg0->spawnArg2;
    panel->field_C.y = 0x1C - gDisplayState.vramYOffset;
    Ui_InsetLayout(panel, NULL, NULL, 0);
    x = (s16)panel->field_1C;
    y = (s16)panel->field_18;
    Gp_DrawEquipSummary(panel, x + 2, y + 0xF, 1);
    Ui_DrawText(panel, Gp_StrWeapon);
}

/// Re-evaluates each lit transient light slot against the view.
static inline void _gpUpdateRoomCoordSlots(void)
{
    GpCoord64* slot;
    s32        i;

    slot = Gp_RoomCoords;
    for (i = 0; i < 8; i++, slot++) {
        if (slot->framesLeft != 0) {
            Gp_UpdateCoordEx(&slot->data.coord, &gGfxViewCoord);
        }
    }
}

void Gp_UpdateRoomCoords(Task* task)
{
    GpRoomCoordSet* set;
    SVECTOR*        vec;
    GpLight*        light;
    GpPointLight*   point;
    GpSpotLight*    spot;
    GpCoord*        coord;
    s32             i;
    s32             j;

    set = (GpRoomCoordSet*)Gp_GetRoomCoordSet(&gGameSession->at4.loc);
    if (set == NULL) {
        taskKill(task);
        return;
    }

    vec = SCRATCH_PUSH_BYTES(0x1C);
    if (task->state == 0) {
        point = set->arr60;
        for (i = 0; i < set->n60; i++, point++) {
            coord      = &point->head.u.coord;
            coord->sub = &gGfxViewCoord;
            coord->flg = 0;
        }

        spot = set->arr6C;
        for (i = 0; i < set->n6C; i++, spot++) {
            coord      = &spot->head.u.coord;
            coord->sub = &gGfxViewCoord;
            if (spot->dir.vy != 0 || spot->dir.vz != 0) {
                vec->vx = 0;
                vec->vy = -spot->dir.vz;
                vec->vz = spot->dir.vy;
            } else {
                vec->vx = spot->dir.vy;
                vec->vy = -spot->dir.vx;
                vec->vz = 0;
            }
            Gfx_OrthonormalBasis(&coord->coord, &spot->dir, vec);
            coord->flg = 0;
        }

        if (set->n58 > 0) {
            GpLight* dir;

            dir = set->arr58;
            for (i = 0; i < set->n58; i++, dir++) {
                coord      = &dir->u.coord;
                coord->sub = &gGfxViewCoord;
                coord->flg = 0;
            }
        }

        for (j = 0; j < 8; j++) {
            Gp_RoomCoords[j].framesLeft = 0;
            coord                       = &Gp_RoomCoords[j].data.coord;
            coord->sub                  = &gGfxViewCoord;
        }

        task->state++;
    }

    Gp_UpdateCoord(&gGfxViewCoord);

    _gpUpdateRoomCoordSlots();

    point = set->arr60;
    for (i = 0; i < set->n60; i++, point++) {
        coord = &point->head.u.coord;
        Gp_UpdateCoordEx(coord, &gGfxViewCoord);
    }

    spot = set->arr6C;
    for (i = 0; i < set->n6C; i++, spot++) {
        coord = &spot->head.u.coord;
        Gp_UpdateCoordEx(coord, &gGfxViewCoord);
    }

    if (set->n58 > 0) {
        light = set->arr58;
        for (i = 0; i < set->n58; i++, light++) {
            coord = &light->u.coord;
            Gp_UpdateCoordEx(coord, &gGfxViewCoord);
        }
    }

    SCRATCH_POP_BYTES(0x1C);
}

s32 Gp_LightPointRoom(GpPointLight* light, VECTOR3* pos)
{
    GpLight*       base;
    GpAttnScratch* block;
    s32            result;
    s32            tooFar;
    s16            room;

    base = &light->head;
    room = base->u.at.room;
    if (room != 0 && (u8)gGameSession->at4.loc.view != room) {
        return 0;
    }
    block          = SCRATCH_PUSH(GpAttnScratch);
    block->vec.vx  = (base->u.at.world.t[0] - pos->vx) >> 1;
    block->vec.vy  = (base->u.at.world.t[1] - pos->vy) >> 1;
    block->vec.vz  = (base->u.at.world.t[2] - pos->vz) >> 1;
    block->outerSq = light->outer >> 1;
    block->scale   = 0;
    if (block->vec.vx < 0) {
        block->vec.vx = -block->vec.vx;
    }
    if (block->vec.vz < 0) {
        block->vec.vz = -block->vec.vz;
    }
    // Rejects on the X and Z extents alone before paying for the squares.
    tooFar = (u32)block->vec.vx > (u32)block->outerSq;
    if (!tooFar) {
        tooFar = (u32)block->vec.vz > (u32)block->outerSq;
        if (!tooFar) {
            block->outerSq = (light->outer * light->outer) >> 2;
            block->distSq  = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
            tooFar         = (u32)block->outerSq < (u32)block->distSq;
        }
    }
    if (tooFar) {
        result = 0;
    } else {
        block->innerSq = (light->inner * light->inner) >> 2;
        result         = ((light->head.r * 8 + light->head.g * 6 + light->head.b * 2) >> 8) + 0xF00;
        block->scale   = 0x1000;
        if ((u32)block->distSq > (u32)block->innerSq) {
            block->outerSq -= block->innerSq;
            block->distSq  -= block->innerSq;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                result       = (u32)(block->scale * result) >> 12;
            }
        }
    }
    base->u.at.scale = block->scale;
    SCRATCH_POP(GpAttnScratch);
    return result;
}

s32 Gp_LightPoint(GpPointLight* arg0, VECTOR3* arg1)
{
    register void**         scratch asm("a3");
    register u8*            head;
    register GpAttnScratch* tmp asm("a0");
    GpAttnScratch*          block;
    s32                     result;
    register s32            lum;
    GpPointLight*           obj;
    s32                     tooFar;
    s32                     r;
    s32                     g;
    s32                     b;
    s32                     dist;
    s32                     inner;
    s32                     vx;
    s32                     sq;
    u16                     scale;
    u8*                     ptr;

    obj                                     = arg0;
    scratch                                 = SCRATCH_HEAD_ADDR;
    vx                                      = obj->head.u.at.world.t[0];
    head                                    = SCRATCH_HEAD_AT(scratch, u8);
    vx                                     -= arg1->vx;
    vx                                    >>= 1;
    tmp                                     = (GpAttnScratch*)(head - 0x20);
    ((GpAttnScratch*)(head - 0x20))->vec.vx = vx;
    block                                   = tmp;
    block->vec.vy                           = (obj->head.u.at.world.t[1] - arg1->vy) >> 1;
    block->vec.vz                           = (obj->head.u.at.world.t[2] - arg1->vz) >> 1;
    sq                                      = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->distSq                           = sq;
    sq                                      = obj->outer;
    lum                                     = sq * sq;
    sq                                      = lum >> 2;
    lum                                     = block->distSq;
    result                                  = 0;
    block->outerSq                          = sq;
    SCRATCH_HEAD_AT(scratch, GpAttnScratch) = block;
    tooFar                                  = (u32)sq < (u32)lum;
    block->scale                            = 0;
    if (!tooFar) {
        block->innerSq = (obj->inner * obj->inner) >> 2;
        r              = obj->head.r;
        g              = obj->head.g;
        b              = obj->head.b;
        block->scale   = 0x1000;
        lum            = (r * 8 + g * 6 + b * 2) >> 8;
        dist           = block->distSq;
        inner          = block->innerSq;
        result         = lum + 0xF00;
        if ((u32)inner < (u32)dist) {
            s32 temp;

            temp = block->outerSq;
            TOUCH_REG(temp);
            lum = inner;
            TOUCH_REG(lum);
            block->outerSq = temp - inner;
            block->distSq -= lum;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                lum          = block->scale * result;
                result       = (u32)lum >> 12;
            }
        }
    }
    scale                = block->scale;
    ptr                  = SCRATCH_HEAD(u8);
    obj->head.u.at.scale = scale;
    SCRATCH_HEAD(u8)     = ptr + 0x20;
    return result;
}

s32 Gp_LightCone(GpSpotLight* spot, VECTOR3* pos)
{
    GpLight*       light;
    GpSpotScratch* block;
    s32            result;

    light  = &spot->head;
    result = 0;
    if (light->u.at.room != 0) {
        if (gGameSession->at4.loc.view != light->u.at.room) {
            return result;
        }
    }
    SCRATCH_PUSH(GpSpotScratch);
    block          = SCRATCH_HEAD(GpSpotScratch);
    block->vec.vx  = (light->u.at.world.t[0] - pos->vx) >> 1;
    block->vec.vy  = (light->u.at.world.t[1] - pos->vy) >> 1;
    block->vec.vz  = (light->u.at.world.t[2] - pos->vz) >> 1;
    block->distSq  = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->outerSq = (spot->outer * spot->outer) >> 2;
    block->scale   = 0;
    if (block->outerSq < block->distSq) {
        result = 0;
    } else {
        block->innerSq = (spot->inner * spot->inner) >> 2;
        Gfx_NormalizeLightDir(&block->vec, &block->dir);
        block->cosAng = -(block->dir.vx * light->u.at.world.m[0][2] + block->dir.vy * light->u.at.world.m[1][2] + block->dir.vz * light->u.at.world.m[2][2]) >> 12;
        if (rcos(spot->angle >> 1) < block->cosAng) {
            result       = ((spot->head.r * 8 + spot->head.g * 6 + spot->head.b * 2) >> 8) + 0xF00;
            block->scale = 0x1000;
            if (block->distSq > block->innerSq) {
                block->outerSq -= block->innerSq;
                block->distSq  -= block->innerSq;
                while (block->outerSq > 0xFFFF) {
                    block->outerSq >>= 4;
                    block->distSq  >>= 4;
                }
                if (block->outerSq != 0) {
                    block->scale = ((block->outerSq - block->distSq) << 12) / block->outerSq;
                    result       = (block->scale * result) >> 12;
                }
            }
        }
    }
    light->u.at.scale = block->scale;
    SCRATCH_POP(GpSpotScratch);
    return result;
}

void func_800D759C(s32 arg0, GpLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    u8*                          head;
    register GpViewLightScratch* block asm("s0");
    SVECTOR*                     dir;
    MATRIX*                      mtx;
    register MATRIX*             dirMtx asm("s6");
    register MATRIX*             colorMtx asm("s7");
    s32                          val;
    register s32                 scale;

    head     = SCRATCH_HEAD(u8);
    block    = (GpViewLightScratch*)(head - 0x3C);
    dir      = (SVECTOR*)(head - 0x2C);
    mtx      = (MATRIX*)(head - 0x24);
    dirMtx   = arg3->lightMtx;
    colorMtx = arg3->colorMtx;

    ((GpViewLightScratch*)(head - 0x3C))->in.vx = -arg1->u.at.local.t[0];
    block->in.vy                                = -arg1->u.at.local.t[1];
    SCRATCH_HEAD(GpViewLightScratch)            = block;
    block->in.vz                                = -arg1->u.at.local.t[2];
    Gfx_NormalizeLightDir((VECTOR*)block, dir);

    Gp_UpdateCoord(arg1->u.at.parent);
    TransposeMatrix(&gGfxViewCoord.workm, mtx);
    gte_MulMatrix0(mtx, &arg1->u.at.parent->workm, mtx);

    gfxLoadRotSv(mtx, (SVECTOR*)(head - 0x2C));
    gte_rtv0();
    gte_stsv(dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->u.at.scale;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->r);
    gte_gpf12();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_POP_BYTES(0x3C);
}

void func_800D78A4(VECTOR* arg0, GpNearestLight* arg1)
{
    GpRoomCoordSet* set;
    GpPointLight*   point;
    GpPointLight*   current;
    GpSpotLight*    cone;
    VECTOR*         delta;
    u32             best;
    u32             dist;
    s32             i;

    set           = (GpRoomCoordSet*)Gp_GetRoomCoordSet(&gGameSession->at4.loc);
    best          = 0x7FFFFFFF;
    arg1->kind    = -1;
    arg1->field_4 = 0;
    arg1->light   = NULL;
    if (set != NULL) {
        SCRATCH_PUSH_BYTES(0x10);
        delta = SCRATCH_HEAD(VECTOR);
        if (set->n60 > 0) {
            point = set->arr60;
            for (i = 0; i < set->n60; i++, point = current + 1) {
                current = point;
                TOUCH_REG(current);
                delta->vx = (current->head.u.coord.workm.t[0] - arg0->vx) >> 1;
                delta->vy = (current->head.u.coord.workm.t[1] - arg0->vy) >> 1;
                delta->vz = (current->head.u.coord.workm.t[2] - arg0->vz) >> 1;
                dist      = delta->vx * delta->vx + delta->vy * delta->vy + delta->vz * delta->vz;
                if (dist < best) {
                    best        = dist;
                    arg1->kind  = 1;
                    arg1->light = &current->head;
                }
            }
        }
        if (set->n6C > 0) {
            cone = set->arr6C;
            for (i = 0; i < set->n6C; i++, cone++) {
                delta->vx = (cone->head.u.coord.workm.t[0] - arg0->vx) >> 1;
                delta->vy = (cone->head.u.coord.workm.t[1] - arg0->vy) >> 1;
                delta->vz = (cone->head.u.coord.workm.t[2] - arg0->vz) >> 1;
                dist      = delta->vx * delta->vx + delta->vy * delta->vy + delta->vz * delta->vz;
                if (dist < best) {
                    best        = dist;
                    arg1->kind  = 2;
                    arg1->light = &cone->head;
                }
            }
        }
        SCRATCH_POP_BYTES(0x10);
    }
}

static __inline__ void solve_func_800D9794(s32 arg0, GpLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    u8*                      head;
    register GpLightScratch* block;
    SVECTOR*                 dir;
    MATRIX*                  dirMtx;
    MATRIX*                  colorMtx;
    register s32             val;

    head                         = SCRATCH_HEAD(u8);
    block                        = (GpLightScratch*)(head - 0x1C);
    dir                          = (SVECTOR*)(head - 0xC);
    SCRATCH_HEAD(GpLightScratch) = block;
    dirMtx                       = arg3->lightMtx;
    colorMtx                     = arg3->colorMtx;
    Gfx_NormalizeLightDir((VECTOR*)arg1->u.coord.workm.t, dir);
    SOFT_USE_REG(block);
    SOFT_USE_REG(block);

    dirMtx->m[arg0][0] = block->dir.vx;
    dirMtx->m[arg0][1] = block->dir.vy;
    dirMtx->m[arg0][2] = block->dir.vz;

    val          = arg1->u.at.scale;
    block->scale = val;
    __asm__ volatile("mtc2 %0, $8" : "+&r"(val) : "r"(val));
    gte_ldsv(&arg1->r);
    gte_gpf12();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_POP_BYTES(0x1C);
}

static __inline__ void solve_func_800D98C4(s32 arg0, GpLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    u8*             head;
    GpLightScratch* block;
    SVECTOR*        dir;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;
    register s32    val;

    head                         = SCRATCH_HEAD(u8);
    block                        = (GpLightScratch*)(head - 0x1C);
    dir                          = (SVECTOR*)(head - 0xC);
    dirMtx                       = arg3->lightMtx;
    colorMtx                     = arg3->colorMtx;
    block->in.vx                 = arg2->vx - arg1->u.coord.workm.t[0];
    block->in.vy                 = arg2->vy - arg1->u.coord.workm.t[1];
    SCRATCH_HEAD(GpLightScratch) = block;
    block->in.vz                 = arg2->vz - arg1->u.coord.workm.t[2];
    Gfx_NormalizeLightDir(&block->in, dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->u.at.scale;
    block->scale = val;
    __asm__ volatile("mtc2 %0, $8" : "+&r"(val) : "r"(val));
    gte_ldsv(&arg1->r);
    gte_gpf12();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_POP_BYTES(0x1C);
}

static __inline__ void solve_func_800D9A30(s32 arg0, GpLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    u8*             head;
    GpLightScratch* block;
    SVECTOR*        dir;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;
    register s32    val;

    head                         = SCRATCH_HEAD(u8);
    block                        = (GpLightScratch*)(head - 0x1C);
    dir                          = (SVECTOR*)(head - 0xC);
    dirMtx                       = arg3->lightMtx;
    colorMtx                     = arg3->colorMtx;
    block->in.vx                 = arg2->vx - arg1->u.coord.workm.t[0];
    block->in.vy                 = arg2->vy - arg1->u.coord.workm.t[1];
    SCRATCH_HEAD(GpLightScratch) = block;
    block->in.vz                 = arg2->vz - arg1->u.coord.workm.t[2];
    Gfx_NormalizeLightDir(&block->in, dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->u.at.scale;
    block->scale = val;
    __asm__ volatile("mtc2 %0, $8" : "+&r"(val) : "r"(val));
    gte_ldsv(&arg1->r);
    gte_gpf12();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_POP_BYTES(0x1C);
}

static __inline__ s32 solve_luma(GpLight* arg0)
{
    s16 val;

    val = arg0->u.at.room;
    if (val != 0 && (u8)gGameSession->at4.loc.view != val) {
        return 0;
    }
    {
        s32 r, g, b, lum;
        r                = arg0->r;
        g                = arg0->g;
        b                = arg0->b;
        arg0->u.at.scale = 0x1000;
        lum              = r * 8 + g * 6 + b * 2;
        USE_REG3(lum, lum, lum);
        return (lum >> 8) + 0xF00;
    }
}

static __inline__ void solve_rank(GpRec12* slots, s32 val, s32 kind, s32 obj, GpRec12* last)
{
    if (val > 0 && last->field_4 < val) {
        Gp_InsertRankedSlot(slots, val, kind, obj, 2);
    }
}
static __inline__ void solve_rank0(GpRec12* slots, s32 val, s32 kind, s32 obj, GpLightSolveScratch* block)
{
    if (val > 0 && block->slots[3].field_4 < val) {
        Gp_InsertRankedSlot(slots, val, kind, obj, 2);
    }
}
void func_800D7A9C(TmdObject* extra, VECTOR* pos, s32 start, s32 count)
{

    register s32                  startr;
    register GpRoomCoordSet*      set;
    register MATRIX*              colorMtx;
    register GpLightSolveScratch* block;

    s32        n;
    s32        nOcc;
    u32        idx;
    s32        i;
    s32        sum;
    s32        val;
    s32*       cutoffPtr;
    GpCoord64* coord;

    startr   = start;
    set      = (GpRoomCoordSet*)Gp_GetRoomCoordSet(&gGameSession->at4.loc);
    colorMtx = extra->colorMtx;
    nOcc     = 0;
    if (set == NULL) {
        return;
    }

    idx = nOcc;
    TOUCH_REG(idx);
    coord = Gp_RoomCoords;
    n     = set->n58 + set->n60 + set->n6C;
    do {
        if (coord->framesLeft != 0) {
            nOcc++;
        }
        idx++;
        coord++;
    } while ((s32)idx < 8);

    sum = startr + count;
    n  += nOcc;
    if ((u32)sum >= 4U) {
        return;
    }
    if (count == 0) {
        return;
    }

    Gp_FillSVec3x3((GpSVec3x3*)colorMtx, 0, 0, 0);

    if ((u32)(sum - 1) >= (u32)n) {
        func_800D7A9C(extra, pos, startr, count - 1);
        return;
    }

    {
        u8* head;

        head             = SCRATCH_HEAD(u8);
        head            -= 0x7C;
        SCRATCH_HEAD(u8) = head;
        block            = SCRATCH_HEAD(GpLightSolveScratch);
    }

    {
        s32 j;

        j = startr;
        for (; (u32)j < (u32)count;) {
            colorMtx->m[0][j] = 0;
            colorMtx->m[1][j] = 0;
            colorMtx->m[2][j] = 0;
            j++;
        }
    }

    {

        i = 0;
        do {
            block->slots[i].field_4 = -1;
            block->slots[i].field_8 = 0;
            i++;
        } while (i < 4);
    }

    {
        GpCoord*         world;
        register MATRIX* src;

        world           = &gGfxViewCoord;
        block->pos.vx   = pos->vx;
        block->pos.vy   = pos->vy;
        block->pos.vz   = pos->vz;
        block->local.vx = (u16)pos->vx - (u16)world->workm.t[0];
        TOUCH_REG(world);
        src             = &gGfxViewCoord.workm;
        block->local.vy = (u16)pos->vy - (u16)world->workm.t[1];
        block->local.vz = (u16)pos->vz - (u16)world->workm.t[2];

        gte_TransposeMatrix(src, &block->mtx);
    }

    gfxLoadRotSv(&block->mtx, &block->local);
    gte_rtv0();
    gte_stsv(&block->local);

    {
        s32                 pointIndex;
        register GpCoord64* p;

        register GpRec12* last;
        GpPointLight*     obj;

        p          = Gp_RoomCoords;
        pointIndex = 0;
        last       = &block->slots[3];

        block->pos.vx = block->local.vx;
        block->pos.vy = block->local.vy;
        block->pos.vz = block->local.vz;
        do {
            if (p->framesLeft != 0) {
                obj = &p->data.light;
                val = Gp_LightPoint(obj, (VECTOR3*)&block->pos);
                SOFT_USE_REG(obj);
                block->intensity = val;
                solve_rank(block->slots, val, 3, (s32)obj, last);
            }
            pointIndex++;
            p++;
        } while (pointIndex < 8);
    }

    if (set->n60 > 0) {
        register GpPointLight* obj60;

        obj60 = set->arr60;
        i     = 0;

        for (; i < set->n60;) {
            val              = Gp_LightPointRoom(obj60, (VECTOR3*)&block->pos);
            block->intensity = val;
            solve_rank(block->slots, val, 1, (s32)obj60, &block->slots[3]);
            i++;
            obj60++;
        }
    }

    if (set->n6C > 0) {
        register GpSpotLight* obj6C;
        s32                   coneRank;

        obj6C = set->arr6C;
        i     = 0;

        for (; i < set->n6C;) {
            val              = Gp_LightCone(obj6C, (VECTOR3*)&block->pos);
            coneRank         = 2;
            block->intensity = val;
            solve_rank(block->slots, val, coneRank, (s32)obj6C, &block->slots[3]);
            i++;
            obj6C++;
        }
    }

    if (set->n58 > 0) {
        register GpLight* obj58;

        obj58 = set->arr58;
        i     = 0;
        for (; i < set->n58;) {
            val              = solve_luma(obj58);
            block->intensity = val;
            solve_rank0(block->slots, val, 0, (s32)obj58, block);
            i++;
            obj58++;
        }
    }

    colorMtx->t[2] = 0;
    colorMtx->t[1] = 0;
    colorMtx->t[0] = 0;

    {

        s32              end;
        GpSolveSlotView* slotArg;

        GpLight* light;
        GpLight* extraLight;

        s32 delta;
        s32 amb;

        i = startr;
        if ((u32)i < (u32)count) {
            end = i + count;
            /* The slot is addressed as the block advanced by whole records, so its
             * fields load at the slot array's own displacement; indexing
             * block->slots folds that displacement into the pointer instead. */
            slotArg = (GpSolveSlotView*)((GpRec12*)block + end);

            do {
                light = (GpLight*)block->slots[i].field_8;
                if (light != NULL) {
                    if (i == end - 1) {
                        cutoffPtr = &((GpSolveSlotView*)((GpRec12*)block + count))->field_8;
                        if (*cutoffPtr != 0) {
                            GpLight* cutoffLight;
                            s32      attenuation;
                            s32      diff;
                            s32      cutoffScale;
                            cutoffLight = (GpLight*)slotArg->field_8;
                            delta       = 0;
                            if (block->slots[count].field_0 != 0) {
                                attenuation = light->u.at.scale;
                                cutoffScale = cutoffLight->u.at.scale;
                                diff        = attenuation - cutoffScale;
                                if (diff < 0) {
                                    diff = 0;
                                }
                                if (diff < 0x200) {
                                    diff              = (diff * attenuation) >> 9;
                                    delta             = attenuation - diff;
                                    light->u.at.scale = diff;
                                }
                            }
                            extraLight     = (GpLight*)slotArg->field_8;
                            delta        >>= 2;
                            amb            = (slotArg->field_4 >> 2) + delta;
                            colorMtx->t[2] = amb;
                            colorMtx->t[1] = amb;
                            colorMtx->t[0] = amb;
                            colorMtx->t[0] = amb + (extraLight->r >> 6);
                            colorMtx->t[1] = colorMtx->t[1] + (extraLight->g >> 6);
                            colorMtx->t[2] = colorMtx->t[2] + (extraLight->b >> 6);
                        }
                    }

                    switch (block->slots[i].field_0) {
                        case 1:
                        case 3:
                            solve_func_800D98C4(i, (GpLight*)block->slots[i].field_8, &block->pos, extra);
                            break;
                        case 2:
                            solve_func_800D9A30(i, (GpLight*)block->slots[i].field_8, &block->pos, extra);
                            break;
                        default:
                            solve_func_800D9794(i, (GpLight*)block->slots[i].field_8, &block->pos, extra);
                            break;
                    }
                }

                i++;

            } while ((u32)i < (u32)count);
        }
    }

    if ((s8)Gp_OverrideVecFlag == 1) {
        colorMtx->t[0] = Gp_OverrideVec.vx;
        colorMtx->t[1] = Gp_OverrideVec.vy;
        colorMtx->t[2] = Gp_OverrideVec.vz;
    } else {
        GpRoomBoundVec* bound;

        bound = Gp_GetRoomBound(&gGameSession->at4.loc);
        if (colorMtx->t[0] < bound->field_0) {
            colorMtx->t[0] = bound->field_0;
        }
        if (colorMtx->t[1] < bound->field_2) {
            colorMtx->t[1] = bound->field_2;
        }
        if (colorMtx->t[2] < bound->field_4) {
            colorMtx->t[2] = bound->field_4;
        }
    }

    if ((s8)Gp_OverrideVec2Flag == 1) {
        u16*      ov;
        SVECTOR3* row;
        s32       j;

        ov  = (u16*)&Gp_OverrideVec2;
        j   = 0;
        row = (SVECTOR3*)extra->colorMtx;
        do {
            block->local.vx = row[j].vx;
            block->local.vy = row[j].vy;
            block->local.vz = row[j].vz;
            gte_lddp(*ov);
            gte_ldsv(&block->local);
            gte_gpf12();
            gte_stsv(&block->local);
            row[j].vx = block->local.vx;
            row[j].vy = block->local.vy;
            row[j].vz = block->local.vz;
            j++;
            ov++;
        } while (j < 3);
    }

    if (Pad_RemapState->field_1 == 0x13 && D_80760618->field_1 == 1) {
        i = 0;
        do {
            D_80760618->field_30[i] = block->slots[i];
            i++;
        } while (i < 4);
    }

    SCRATCH_POP_BYTES(0x7C);
}

const char D_8009745C[] = {
    '?',
    '\0',
    0x00,
    0x42,
};

void Gp_DebugPanTask(Task* arg0)
{
    Task*          slot;
    Task*          work;
    PlayerStatus*  cfg;
    TmdObject*     extra;
    GpCoord*       coord;
    GameActor*     actor;
    GameActor*     actor2;
    MATRIX*        mtx;
    u8*            head;
    _GpPanScratch* block;
    SVECTOR*       vecp;
    VECTOR         vec;
    TextDrawReq    req;
    s32            i;
    s32            val;

    slot = gameGetPtrSlot(3);
    cfg  = &Player_Status;
    if (slot == NULL) {
        return;
    }

    extra = slot->extra.tmd;
    coord = &(extra->coords)[1];
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x64;
    vec.vz = coord->workm.t[2];

    if (Pad_RemapState->field_1 == 0x13) {
        head                        = SCRATCH_HEAD(u8);
        block                       = (_GpPanScratch*)(head - 0x18);
        SCRATCH_HEAD(_GpPanScratch) = block;
        D_80760618->field_1         = 1;
        func_800D7A9C(extra, &vec, 0, 3);
        func_800D78A4(&vec, &D_80760618->field_24);
        vecp                = &block->vec;
        D_80760618->field_1 = 0;
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_SetTransMatrix(&GsWSMATRIX);
        ((_GpPanScratch*)(head - 0x18))->vec.vx = vec.vx;
        block->vec.vy                           = vec.vy;
        block->vec.vz                           = vec.vz;
        gte_ldv0(vecp);
        gte_rtps();
        gte_stsxy(&((_GpPanScratch*)(head - 0x18))->sx);
        gte_stdp(&((_GpPanScratch*)(head - 0x18))->dp);
        gte_stflg(&((_GpPanScratch*)(head - 0x18))->flag);
        gte_stszotz(&((_GpPanScratch*)(head - 0x18))->otz);
        if (block->flag >= 0) {
            req.x          = block->sx;
            req.y          = block->sy;
            req.otIndex    = 4;
            req.field_8    = 0x37A78;
            req.glyphTable = 0;
            req.centerMode = 1;
            req.field_E    = 0;
            func_8002E53C(&req, (u8*)D_8009745C);
        }
        SCRATCH_POP_BYTES(0x18);
    } else {
        func_800D7A9C(extra, &vec, 0, 3);
        if (D_80114F28 != 0) {
            mtx = extra->colorMtx;
            val = rsin(gDisplayState.loopCount << 6) + 0x1800;
            if ((gDisplayState.loopCount & 1) == 0) {
                val >>= 1;
            }
            mtx->m[0][0] = mtx->m[0][1] = mtx->m[0][2] = 0x200;
            mtx->m[1][0] = mtx->m[1][1] = mtx->m[1][2] = val;
            mtx->m[2][0] = mtx->m[2][1] = mtx->m[2][2] = 0x200;
            D_80114F28                                 = 0;
        } else if ((gDisplayState.animFrame % 3) == 0 && cfg->hp > 0 && gGameSession->eventState == 0) {
            {
                register MATRIX* colorMtx asm("v0");
                if (Gp_StateC08.field_14 > 0 || (Gp_StateC08.field_16 != 0 && (s8)Gp_StateC08.field_17 != 0)) {
                    colorMtx          = extra->colorMtx;
                    colorMtx->m[0][0] = colorMtx->m[0][1] = colorMtx->m[0][2] = 0x400;
                    colorMtx->m[1][0] = colorMtx->m[1][1] = colorMtx->m[1][2] = 0x2000;
                    colorMtx->m[2][0] = colorMtx->m[2][1] = colorMtx->m[2][2] = 0x2000;
                } else if (Gp_StateC08.field_16 != 0) {
                    colorMtx          = extra->colorMtx;
                    colorMtx->m[0][0] = colorMtx->m[0][1] = colorMtx->m[0][2] = 0x400;
                    colorMtx->m[1][0] = colorMtx->m[1][1] = colorMtx->m[1][2] = 0x400;
                    colorMtx->m[2][0] = colorMtx->m[2][1] = colorMtx->m[2][2] = 0x2000;
                } else if ((s8)Gp_StateC08.field_17 != 0) {
                    colorMtx          = extra->colorMtx;
                    colorMtx->m[0][0] = colorMtx->m[0][1] = colorMtx->m[0][2] = 0x2000;
                    colorMtx->m[1][0] = colorMtx->m[1][1] = colorMtx->m[1][2] = 0x2000;
                    colorMtx->m[2][0] = colorMtx->m[2][1] = colorMtx->m[2][2] = 0x400;
                }
                if (cfg->peStateFlags & 0x80) {
                    colorMtx          = extra->colorMtx;
                    colorMtx->m[0][0] = colorMtx->m[0][1] = colorMtx->m[0][2] = 0x2000;
                    colorMtx->m[1][0] = colorMtx->m[1][1] = colorMtx->m[1][2] = 0x400;
                    colorMtx->m[2][0] = colorMtx->m[2][1] = colorMtx->m[2][2] = 0x400;
                }
            }
        }
    }

    actor = slot->work;
    {
        register Task* task asm("v0");
        i = 0;
        do {
            task = (&actor->field_920)[i];
            if (task != NULL) {
                extra           = task->extra.tmd;
                extra->lightMtx = &Gp_DefaultMtx;
                extra->colorMtx = &Gp_DefaultMtx2;
            }
            i++;
        } while (i < 2);
        i = 0;
        do {
            task = (&actor->field_918)[i];
            if (task != NULL) {
                extra           = task->extra.tmd;
                extra->lightMtx = &Gp_DefaultMtx;
                extra->colorMtx = &Gp_DefaultMtx2;
            }
            i++;
        } while (i < 2);
    }

    work = Gp_ActorSlots[1];
    if (work != NULL) {
        {
            /* The target loads the extra into $v0 and copies it into the saved
               register; the non-volatile asm keeps that copy without acting as
               a scheduling barrier. */
            register TmdObject* e asm("v0");
            e      = work->extra.tmd;
            actor2 = work->work;
            SOFT_TOUCH_REG(e);
            extra = e;
        }
        coord           = &(extra->coords)[1];
        extra->colorMtx = &D_80114EF8;
        extra->lightMtx = &D_80114ED8;
        Gp_UpdateCoord(coord);
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1] - 0x64;
        vec.vz = coord->workm.t[2];
        func_800D7A9C(extra, &vec, 0, 3);
        {
            register Task* task asm("v0");
            i = 0;
            do {
                task = (&actor2->field_920)[i];
                if (task != NULL) {
                    extra           = task->extra.tmd;
                    extra->lightMtx = &D_80114ED8;
                    extra->colorMtx = &D_80114EF8;
                }
                i++;
            } while (i < 2);
            i = 0;
            do {
                task = (&actor2->field_918)[i];
                if (task != NULL) {
                    extra           = task->extra.tmd;
                    extra->lightMtx = &D_80114ED8;
                    extra->colorMtx = &D_80114EF8;
                }
                i++;
            } while (i < 2);
        }
    }
}

void Gp_RemapActorColor(GpEnemy* arg0, MATRIX* arg1, s32 arg2)
{
    s32 i;
    s32 val;

    if (arg2 == 1) {
        goto case1;
    } else if (arg2 < 2) {
        goto def;
    } else if (arg2 == 2) {
        goto case2;
    } else if (arg2 == 3) {
        goto case3;
    } else {
        goto def;
    }

case1: {
    s32 t;
    for (i = 0; i < 3; i++) {
        t             = (arg1->m[0][i] * 7 + arg1->m[1][i] * 6 + arg1->m[2][i] * 3) / 33;
        arg1->m[0][i] = t * 4;
        arg1->m[1][i] = t * 2;
        arg1->m[2][i] = t;
    }
}
    return;

case3:
    if ((arg0->colorMode & 0x80) && (arg0->spawnState == 0)) {
        goto flicker;
    }
    arg1->m[0][0] = arg1->m[0][1] = arg1->m[0][2] = 0x180;
    arg1->m[1][0] = arg1->m[1][1] = arg1->m[1][2] = 0x100;
    arg1->m[2][0] = arg1->m[2][1] = arg1->m[2][2] = 0x100;
    return;

case2:
    if ((arg0->colorMode & 0x80) && (arg0->spawnState == 0)) {
        goto flicker;
    }
    arg1->m[0][0] = 0;
    arg1->m[0][1] = 0;
    arg1->m[0][2] = 0;
    arg1->m[1][0] = 0;
    arg1->m[1][1] = 0;
    arg1->m[1][2] = 0;
    arg1->m[2][0] = 0;
    arg1->m[2][1] = 0;
    arg1->m[2][2] = 0;
    return;

def:
    if ((arg0->colorMode & 0x80) && (arg0->spawnState == 0)) {
    flicker:
        val = rsin(gDisplayState.loopCount << 6) + 0x1800;
        if ((gDisplayState.loopCount & 1) == 0) {
            val >>= 1;
        }
        arg1->m[0][0] = arg1->m[0][1] = arg1->m[0][2] = 0x200;
        arg1->m[1][0] = arg1->m[1][1] = arg1->m[1][2] = val;
        arg1->m[2][0] = arg1->m[2][1] = arg1->m[2][2] = 0x200;
        arg0->colorMode                              &= 0x7F;
    } else if (arg0->reactionFlags & 0xC) {
        s32 t;
        for (i = 0; i < 3; i++) {
            t             = (arg1->m[0][i] * 7 + arg1->m[1][i] * 6 + arg1->m[2][i] * 3) / 33;
            arg1->m[0][i] = t * 3;
            arg1->m[1][i] = t;
            arg1->m[2][i] = t * 3;
        }
    }
}

void Gp_UpdateActorColor(GpEnemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3)
{
    TmdObject*      extra;
    MATRIX*         colorMtx;
    s32             mode;
    u8*             head;
    GpColorScratch* block;
    SVECTOR*        col0;
    SVECTOR*        col1;
    GpMtxCol*       src;
    GpMtxCol*       dst;
    s32             i;
    s32             w0;
    s32             w1;

    extra    = arg0->task->extra.tmd;
    colorMtx = extra->colorMtx;
    mode     = arg0->colorMode & 3;
    if ((!(extra->flags & 0x80) && (extra->buffer != NULL)) || (gGameSession->field_65 != 1)) {
        {
            register GpColorScratch* tmp asm("v0");

            head                         = SCRATCH_HEAD(u8);
            tmp                          = (GpColorScratch*)(head - 0x30);
            block                        = tmp;
            SCRATCH_HEAD(GpColorScratch) = tmp;
        }
        func_800D7A9C(extra, arg1, 0, 3);
        if ((s8)arg0->colorBlend <= 0) {
            Gp_RemapActorColor(arg0, colorMtx, mode);
        } else {
            block->mtx.m[0][0] = colorMtx->m[0][0];
            block->mtx.m[0][1] = colorMtx->m[0][1];
            block->mtx.m[0][2] = colorMtx->m[0][2];
            block->mtx.m[1][0] = colorMtx->m[1][0];
            block->mtx.m[1][1] = colorMtx->m[1][1];
            block->mtx.m[1][2] = colorMtx->m[1][2];
            block->mtx.m[2][0] = colorMtx->m[2][0];
            block->mtx.m[2][1] = colorMtx->m[2][1];
            block->mtx.m[2][2] = colorMtx->m[2][2];
            Gp_RemapActorColor(arg0, colorMtx, mode);
            Gp_RemapActorColor(arg0, &block->mtx, (arg0->colorMode >> 2) & 3);
            i    = 0;
            col0 = (SVECTOR*)(head - 0x10);
            col1 = (SVECTOR*)(head - 8);
            src  = (GpMtxCol*)colorMtx;
            w0   = (s8)arg0->colorBlend << 8;
            dst  = (GpMtxCol*)block;
            w1   = 0x1000 - w0;
            do {
                block->col0.vx = src->x;
                block->col0.vy = src->y;
                TOUCH_REG(src);
                block->col0.vz = src->z;
                block->col1.vx = dst->x;
                block->col1.vy = dst->y;
                TOUCH_REG(dst);
                block->col1.vz = dst->z;
                gte_lddp(w1);
                gte_ldsv(col0);
                gte_gpf12();
                gte_lddp(w0);
                gte_ldsv(col1);
                gte_gpl12();
                gte_stsv(col0);
                src->x = block->col0.vx;
                dst    = (GpMtxCol*)&dst->_0;
                src->y = block->col0.vy;
                i++;
                src->z = block->col0.vz;
                src    = (GpMtxCol*)&src->_0;
            } while (i < 3);
            if (Gp_StateF0.field_4 == 0) {
                arg0->colorBlend--;
            }
        }
        SCRATCH_POP_BYTES(0x30);
    }
}

void Gp_LightFalloff(GpPointLight* arg0)
{
    register void**         scratch asm("a1");
    u8*                     head;
    register GpAttnScratch* tmp asm("v1");
    GpAttnScratch*          block;
    register s32            result asm("t0");
    s32                     lum;
    s32                     tooFar;
    s32                     r;
    s32                     g;
    s32                     b;
    s32                     dist;
    s32                     inner;
    s32                     vx;
    u16                     scale;
    u8*                     ptr;

    result                                  = 0;
    scratch                                 = SCRATCH_HEAD_ADDR;
    vx                                      = arg0->head.u.at.local.t[0];
    head                                    = SCRATCH_HEAD_AT(scratch, u8);
    vx                                    >>= 1;
    tmp                                     = (GpAttnScratch*)(head - 0x20);
    ((GpAttnScratch*)(head - 0x20))->vec.vx = vx;
    block                                   = tmp;
    block->vec.vy                           = arg0->head.u.at.local.t[1] >> 1;
    block->vec.vz                           = arg0->head.u.at.local.t[2] >> 1;
    block->distSq                           = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->outerSq                          = (arg0->outer * arg0->outer) >> 2;
    tooFar                                  = (u32)block->outerSq < (u32)block->distSq;
    SCRATCH_HEAD_AT(scratch, GpAttnScratch) = block;
    block->scale                            = 0;
    if (!tooFar) {
        block->innerSq = (arg0->inner * arg0->inner) >> 2;
        r              = arg0->head.r;
        g              = arg0->head.g;
        b              = arg0->head.b;
        block->scale   = 0x1000;
        lum            = (r * 8 + g * 6 + b * 2) >> 8;
        dist           = block->distSq;
        inner          = block->innerSq;
        result         = lum + 0xF00;
        if ((u32)inner < (u32)dist) {
            s32 temp;

            temp = block->outerSq;
            TOUCH_REG(temp);
            lum = inner;
            TOUCH_REG(lum);
            block->outerSq = temp - inner;
            block->distSq -= lum;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                lum          = block->scale * result;
                result       = (u32)lum >> 12;
            }
        }
    }
    scale                      = block->scale;
    ptr                        = SCRATCH_HEAD(u8);
    arg0->head.u.at.world.t[0] = result;
    arg0->head.u.at.scale      = scale;
    SCRATCH_HEAD(u8)           = ptr + 0x20;
}

void Gp_SetLightMode(GpEnemy* arg0, s32 arg1)
{
    u8 val;

    val   = arg0->colorMode;
    arg1 &= 3;
    if ((val & 3) != arg1) {
        arg0->colorMode  = (val & 0xF0) | ((val & 3) << 2) | arg1;
        arg0->colorBlend = 0x10;
    }
}

s32 gpGetObjDepth(GpCoord* coord)
{
    s32 val;

    val = coord->workm.t[2] - gDisplayState.screenDistance;
    if (val >= 0x7FFF) {
        val = 0x7FFF;
    }
    if (val < -0x7FFF) {
        val = -0x7FFF;
    }
    return val >> 8;
}

s32 Gp_GetObjPan(GpCoord* coord)
{
    u8*            head;
    _GpPanScratch* block;
    SVECTOR*       vec;
    s32            ret;

    head                        = SCRATCH_HEAD(u8);
    block                       = (_GpPanScratch*)(head - 0x18);
    SCRATCH_HEAD(_GpPanScratch) = block;
    vec                         = &block->vec;
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    block->vec.vz = 0;
    block->vec.vy = 0;
    block->vec.vx = 0;
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((_GpPanScratch*)(head - 0x18))->sx);
    gte_stdp(&((_GpPanScratch*)(head - 0x18))->dp);
    gte_stflg(&((_GpPanScratch*)(head - 0x18))->flag);
    gte_stszotz(&((_GpPanScratch*)(head - 0x18))->otz);
    if (block->flag >= 0) {
        if (block->sx >= 0xA0) {
            block->sx = 0x9F;
        }
        if (block->sx < -0x9F) {
            block->sx = -0xA0;
        }
        ret = -block->sx / 10;
    } else {
        ret = 0;
    }
    SCRATCH_POP_BYTES(0x18);
    return -ret;
}

void Gp_SetOverrideVec(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        Gp_OverrideVecFlag = 0;
        return;
    }
    Gp_OverrideVecFlag = 1;
    Gp_OverrideVec     = *arg0;
}

void Gp_SetOverrideVec2(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        Gp_OverrideVec2Flag = 0;
        return;
    }
    Gp_OverrideVec2Flag = 1;
    Gp_OverrideVec2     = *arg0;
}

void Gp_SetObjTrans(TmdObject* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    MATRIX* m;

    m       = arg0->colorMtx;
    m->t[0] = arg1;
    m->t[1] = arg2;
    m->t[2] = arg3;
}

GpRoomBoundVec* Gp_GetRoomBound(GpAreaKey* arg0)
{
    GpRoomCoordRec** mid;
    GpRoomCoordRec*  rec;
    GpRoomBoundVec*  result;
    GpRoomBoundVec*  table;

    mid = Gp_RoomCoordTables[arg0->stage - 1];
    rec = NULL;
    if (mid != NULL) {
        rec = mid[arg0->area - 1];
        if (rec != NULL) {
            rec = &rec[arg0->room - 1];
        }
    }
    result = &Gp_RoomBoundDefault;
    if (rec != NULL) {
        table = rec->field_4;
        if (table != NULL) {
            if (table->field_0 >= arg0->view) {
                result = &table[arg0->view];
            }
        }
    }
    return result;
}

s32 Gp_CountRoomCoords(void)
{
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < 8; i++) {
        if (Gp_RoomCoords[i].framesLeft != 0) {
            count++;
        }
    }
    return count;
}

s32 Gp_GetRoomCoordSet(GpAreaKey* arg0)
{
    GpRoomCoordRec** mid;
    GpRoomCoordRec*  rec;
    s32              result;

    result = 0;
    mid    = Gp_RoomCoordTables[arg0->stage - 1];
    rec    = NULL;
    if (mid != NULL) {
        rec = mid[arg0->area - 1];
        if (rec != NULL) {
            rec = &rec[arg0->room - 1];
        }
    }
    if (rec != NULL) {
        result = rec->field_0;
    }
    return result;
}

void func_800D96C8(Task* arg0)
{
    TaskFunc funcs[2] = { Gp_BindDefaultMtx, Gp_DebugPanTask };

    funcs[arg0->state](arg0);
}

s32 Gp_GetObjLuma(GpLight* arg0)
{
    s16 val;

    val = arg0->u.at.room;
    if (val != 0 && (u8)gGameSession->at4.loc.view != val) {
        return 0;
    }
    arg0->u.at.scale = 0x1000;
    return ((arg0->r * 8 + arg0->g * 6 + arg0->b * 2) >> 8) + 0xF00;
}

s32 Gp_GetObjTransX(GpCoord* coord)
{
    return coord->workm.t[0];
}

void func_800D9794(s32 arg0, GpLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    u8*                      head;
    register GpLightScratch* block asm("s0");
    SVECTOR*                 dir;
    MATRIX*                  dirMtx;
    MATRIX*                  colorMtx;
    register s32             val asm("v0");
    register s32             scale asm("a2");

    head                         = SCRATCH_HEAD(u8);
    block                        = (GpLightScratch*)(head - 0x1C);
    dir                          = (SVECTOR*)(head - 0xC);
    SCRATCH_HEAD(GpLightScratch) = block;
    dirMtx                       = arg3->lightMtx;
    colorMtx                     = arg3->colorMtx;
    Gfx_NormalizeLightDir((VECTOR*)arg1->u.coord.workm.t, dir);

    dirMtx->m[arg0][0] = block->dir.vx;
    dirMtx->m[arg0][1] = block->dir.vy;
    dirMtx->m[arg0][2] = block->dir.vz;

    val          = arg1->u.at.scale;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->r);
    gte_gpf12();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_POP_BYTES(0x1C);
}

void func_800D98C4(s32 arg0, GpLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    u8*             head;
    GpLightScratch* block;
    SVECTOR*        dir;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;
    register s32    val asm("v0");
    register s32    scale asm("t0");

    head                         = SCRATCH_HEAD(u8);
    block                        = (GpLightScratch*)(head - 0x1C);
    dir                          = (SVECTOR*)(head - 0xC);
    dirMtx                       = arg3->lightMtx;
    colorMtx                     = arg3->colorMtx;
    block->in.vx                 = arg2->vx - arg1->u.coord.workm.t[0];
    block->in.vy                 = arg2->vy - arg1->u.coord.workm.t[1];
    SCRATCH_HEAD(GpLightScratch) = block;
    block->in.vz                 = arg2->vz - arg1->u.coord.workm.t[2];
    Gfx_NormalizeLightDir(&block->in, dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->u.at.scale;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->r);
    gte_gpf12();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_POP_BYTES(0x1C);
}

void func_800D9A30(s32 arg0, GpLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    u8*             head;
    GpLightScratch* block;
    SVECTOR*        dir;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;
    register s32    val asm("v0");
    register s32    scale asm("t0");

    head                         = SCRATCH_HEAD(u8);
    block                        = (GpLightScratch*)(head - 0x1C);
    dir                          = (SVECTOR*)(head - 0xC);
    dirMtx                       = arg3->lightMtx;
    colorMtx                     = arg3->colorMtx;
    block->in.vx                 = arg2->vx - arg1->u.coord.workm.t[0];
    block->in.vy                 = arg2->vy - arg1->u.coord.workm.t[1];
    SCRATCH_HEAD(GpLightScratch) = block;
    block->in.vz                 = arg2->vz - arg1->u.coord.workm.t[2];
    Gfx_NormalizeLightDir(&block->in, dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->u.at.scale;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->r);
    gte_gpf12();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_POP_BYTES(0x1C);
}

void Gp_InsertRankedSlot(GpRec12* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    GpRec12* rec;
    GpRec12* next;

    if (arg1 <= 0) {
        return;
    }

    rec = (GpRec12*)(arg4 * sizeof(*arg0) + (s32)arg0);
    if (rec->field_4 < arg1) {
        if (arg4 < 3) {
            rec[1] = *rec;
        }
        if (arg4 > 0) {
            Gp_InsertRankedSlot(arg0, arg1, arg2, arg3, arg4 - 1);
        } else {
            arg0->field_4 = arg1;
            arg0->field_0 = arg2;
            arg0->field_8 = arg3;
        }
    } else if (arg4 < 3) {
        next           = rec + 1;
        next->field_4  = arg1;
        rec[1].field_0 = arg2;
        next->field_8  = arg3;
    }
}

void Gp_FillSVec3x3(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    arg0->field_0.vx = arg0->field_0.vy = arg0->field_0.vz = arg1;
    arg0->field_6.vx = arg0->field_6.vy = arg0->field_6.vz = arg2;
    arg0->field_C.vx = arg0->field_C.vy = arg0->field_C.vz = arg3;
}

GpRoomCoordRec* Gp_GetRoomCoordRec(GpAreaKey* arg0)
{
    GpRoomCoordRec** mid;
    GpRoomCoordRec*  rec;

    mid = Gp_RoomCoordTables[arg0->stage - 1];
    rec = NULL;
    if (mid != NULL) {
        rec = mid[arg0->area - 1];
        if (rec != NULL) {
            rec = &rec[arg0->room - 1];
        }
    }
    return rec;
}

void func_800D9CC8(Task* arg0)
{
    Task_CallExit(arg0);
}

void Gp_CopyDefaultBound(GpRoomBoundVec* bound)
{
    *bound = Gp_RoomBoundDefault;
}

void Gp_BindDefaultMtx(Task* arg0)
{
    Task*        slot;
    TmdObject*   extra;
    GameActor*   actor;
    s32          result;
    s32          i;
    MATRIX*      mtxA;
    MATRIX*      mtxB;
    register s32 addr asm("v0");

    slot  = gameGetPtrSlot(3);
    extra = slot->extra.tmd;
    if (slot != NULL) {
        result = Gp_GetRoomCoordSet(&gGameSession->at4.loc);
        i      = 0;
        if (result == 0) {
            taskKill(arg0);
            return;
        }
        addr = (s32)&Gp_DefaultMtx;
        TOUCH_REG(addr);
        mtxA = (MATRIX*)addr;
        addr = (s32)&Gp_DefaultMtx2;
        TOUCH_REG(addr);
        mtxB                = (MATRIX*)addr;
        arg0->spawnArg2     = (void*)result;
        extra->lightMtx     = mtxA;
        extra->colorMtx     = mtxB;
        actor               = slot->work;
        Gp_OverrideVecFlag  = 0;
        Gp_OverrideVec2Flag = 0;
        D_80114F28          = 0;
        do {
            extra           = (&actor->field_920)[i]->extra.tmd;
            extra->lightMtx = mtxA;
            extra->colorMtx = mtxB;
            i++;
        } while (i < 2);
        arg0->state++;
        Gp_DebugPanTask(arg0);
    }
}

void Gp_DrawTargetCursor(void)
{
    GpLinkNode*    node;
    GameSession*   sess;
    u8             stateA;
    void**         scratch;
    u8*            head;
    _GpPanScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    register s32   small asm("s4");
    s32            frame;
    s32            tu;
    s32            tv;
    s32            u0;
    s32            u1;
    u32            mask;
    s32            val;
    s32            n;

    node = Gp_LinkList;
    if (Pad_RemapState->field_A != 0) {
        return;
    }
    Gp_UpdateLockSlots();
    sess = gGameSession;
    if (sess->field_65 == 1) {
        return;
    }
    stateA = (u8)Gp_StateC08.field_A;
    if ((u8)(stateA - 2) < 2) {
        return;
    }
    if ((s8)stateA == 1) {
        return;
    }
    if (sess->eventState != 0) {
        return;
    }
    if (sess->hideHud != 0) {
        return;
    }
    if (node != NULL) {
        scratch = SCRATCH_HEAD_ADDR;
        do {
            if (node->state.b.targeted == 0) {
                goto next;
            }
            if (node->state.b.flags & 1) {
                goto next;
            }
            head                                    = SCRATCH_HEAD_AT(scratch, u8);
            ((_GpPanScratch*)(head - 0x18))->vec.vx = (u16)GP_NODE_ENEMY(node)->bodyPos.vx;
            {
                register u8* tmp asm("v0");
                tmp   = head - 0x18;
                block = (_GpPanScratch*)tmp;
            }
            block->vec.vy                           = (u16)GP_NODE_ENEMY(node)->bodyPos.vy;
            block->vec.vz                           = (u16)GP_NODE_ENEMY(node)->bodyPos.vz;
            SCRATCH_HEAD_AT(scratch, _GpPanScratch) = block;
            Gp_UpdateCoord(GP_NODE_ENEMY(node)->coord);
            small = 0;
            gte_SetRotMatrix(&GP_NODE_ENEMY(node)->coord->workm);
            gte_SetTransMatrix(&GP_NODE_ENEMY(node)->coord->workm);
            gte_ldv0(&block->vec);
            gte_rtps();
            gte_stsxy(&((_GpPanScratch*)(head - 0x18))->sx);
            gte_stdp(&((_GpPanScratch*)(head - 0x18))->dp);
            gte_stflg(&((_GpPanScratch*)(head - 0x18))->flag);
            gte_stszotz(&((_GpPanScratch*)(head - 0x18))->otz);
            if (D_80115260 != node) {
                if (D_80115260 == NULL) {
                    D_80115264 = 0xFF;
                } else {
                    D_80115264 = 0;
                }
                D_80115260 = node;
            }
            if (D_80115264 < 5) {
                D_8010F9EC += ((block->sx << 8) - D_8010F9EC) >> 1;
                D_8010F9F0 += ((block->sy << 8) - D_8010F9F0) >> 1;
                if (block->sx != (D_8010F9EC >> 8)) {
                    goto inc;
                }
                if (block->sy == (D_8010F9F0 >> 8)) {
                    val = 0xFF;
                    goto store;
                }
            inc:
                val = D_80115264 + 1;
            store:
                D_80115264 = val;
                __asm__ volatile("" : : "m"(D_80115264));
                small     = 1;
                block->sx = D_8010F9EC >> 8;
                block->sy = D_8010F9F0 >> 8;
            } else {
                D_8010F9EC = block->sx << 8;
                D_8010F9F0 = block->sy << 8;
            }
            ds              = &gDisplayState;
            (u16) block->sy = (u16)block->sy - (s8)(u8)ds->vramYOffset;
            n               = ds->animFrame;
            n               = (u32)n % 24U;
            frame           = (u32)n / 3U;
            prim            = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor  = prim + 1;
            if (small == 1) {
                prim->x0 = prim->x2 = (u16)block->sx - 8;
                prim->x1 = prim->x3 = (u16)block->sx + 8;
                prim->y0 = prim->y1 = (u16)block->sy - 8;
                prim->y2 = prim->y3 = (u16)block->sy + 8;
            } else {
                prim->x0 = prim->x2 = (u16)block->sx - 0x10;
                prim->x1 = prim->x3 = (u16)block->sx + 0x10;
                prim->y0 = prim->y1 = (u16)block->sy - 0x10;
                prim->y2 = prim->y3 = (u16)block->sy + 0x10;
            }
            mask = 0xFFFFFF;
            tu   = (frame & 3) << 5;
            u0   = tu + 0x40;
            TOUCH_REG4(tu, u0, frame, mask);
            tv = (frame >> 2) << 5;
            u1 = tu + 0x60;
            TOUCH_REG2(tv, u1);
            prim->v0    = tv;
            prim->v1    = tv;
            prim->v2    = tv + 0x20;
            prim->v3    = tv + 0x20;
            prim->clut  = 0x3C81;
            prim->tpage = 0x3E;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->u0 = u0;
            prim->u1 = u1;
            prim->u2 = u0;
            prim->u3 = u1;
            {
                register u32 hi asm("a1");
                u32*         otp;
                u32*         pp;
                u32          paddr;
                u32          t0;
                register u32 t1 asm("v1");
                otp            = (u32*)gGpuCurrentOt;
                hi             = 0xFF000000;
                pp             = (u32*)prim;
                *pp            = (*pp & hi) | (*otp & mask);
                paddr          = (u32)prim & mask;
                t0             = *(u32*)scratch;
                t1             = *otp;
                t0            += 0x18;
                t1             = t1 & hi;
                t1             = t1 | paddr;
                *(u32*)scratch = t0;
                *otp           = t1;
            }
            break;
        next:
            node = node->next;
        } while (node != NULL);
    }
    if (node == NULL) {
        D_80115260 = NULL;
        D_80115264 = 0;
    }
}

void* Gp_ScanLockNodes(Task* arg0, VECTOR3* out, s32 flag)
{
    u8*                head;
    GpLockScanScratch* block;
    GameActor*         actor;
    GpCoord*           coord;
    GpCoord*           nodeCoord;
    GpLinkNode*        node;
    GpLinkNode*        best;
    s32                bestAngle;
    u32                bestDist;
    s32                baseAngle;
    s32                angle;
    u32                dist;
    s32                sub;
    SVECTOR            tmp;
    SVECTOR*           srcp;

    best = NULL;
    head = SCRATCH_HEAD(u8);
    {
        register u8* newhead asm("a1");
        newhead = head - 0x38;
        block   = (GpLockScanScratch*)newhead;
    }
    actor                           = arg0->work;
    coord                           = arg0->extra.tmd->coords;
    block->src.vx                   = (u16)coord->coord.t[0];
    block->src.vy                   = (u16)coord->coord.t[1] - 1000;
    block->src.vz                   = (u16)coord->coord.t[2];
    SCRATCH_HEAD(GpLockScanScratch) = block;
    Gp_UpdateCoord(&gGfxViewCoord);
    srcp = &((GpLockScanScratch*)(head - 0x38))->src;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(srcp);
    gte_rtv0();
    gte_stsv(&block->self);
    (u16) block->self.vx = (u16)block->self.vx + (u16)gGfxViewCoord.workm.t[0];
    (u16) block->self.vy = (u16)block->self.vy + (u16)gGfxViewCoord.workm.t[1];
    (u16) block->self.vz = (u16)block->self.vz + (u16)gGfxViewCoord.workm.t[2];

    if (actor->field_90C != NULL && flag != 0) {
        node      = actor->field_90C;
        baseAngle = ratan2(GP_NODE_ENEMY(node)->playerRelPos.vx, GP_NODE_ENEMY(node)->playerRelPos.vz);
    } else {
        baseAngle = 0;
    }
    bestAngle = 0x3000;
    bestDist  = 0x7FFFFFFF;
    dist      = 0;
    for (node = Gp_LinkList; node != NULL; node = node->next) {
        if (node->state.b.flags & 1) {
            continue;
        }
        angle = ratan2(GP_NODE_ENEMY(node)->playerRelPos.vx, GP_NODE_ENEMY(node)->playerRelPos.vz);
        if (flag == 0) {
            dist = GP_NODE_ENEMY(node)->playerRelPos.vz * GP_NODE_ENEMY(node)->playerRelPos.vz + GP_NODE_ENEMY(node)->playerRelPos.vx * GP_NODE_ENEMY(node)->playerRelPos.vx + GP_NODE_ENEMY(node)->playerRelPos.vy * GP_NODE_ENEMY(node)->playerRelPos.vy;
            if (angle < 0) {
                angle = -angle;
            }
            if (dist <= 0x300000) {
                sub    = 0x300000 - dist;
                sub  >>= 13;
                sub   *= 3;
                angle -= sub;
                if (angle < 0) {
                    angle = 0;
                }
                dist += sub / 3;
            }
            angle >>= 10;
            if (node == actor->field_90C) {
                angle += 0x1000;
            }
            if (angle == bestAngle && dist > bestDist) {
                angle = 0x2000;
            }
        } else {
            angle -= baseAngle;
            if (angle < 0) {
                angle += 0x1000;
            }
            if (angle >= 0x1000) {
                angle -= 0x1000;
            }
            if (flag == 1) {
                angle = -angle;
            }
            if (node == actor->field_90C) {
                angle += 0x1000;
            }
        }
        if (angle > bestAngle) {
            continue;
        }
        Gp_UpdateCoord(GP_NODE_ENEMY(node)->coord);
        block->node.vx = (u16)GP_NODE_ENEMY(node)->bodyPos.vx;
        block->node.vy = (u16)GP_NODE_ENEMY(node)->bodyPos.vy;
        block->node.vz = (u16)GP_NODE_ENEMY(node)->bodyPos.vz;
        nodeCoord      = GP_NODE_ENEMY(node)->coord;
        tmp            = block->node;
        gte_SetRotMatrix(&nodeCoord->workm);
        gte_ldv0(&tmp);
        gte_rtv0();
        gte_stsv(&block->node);
        block->node.vx += (u16)GP_NODE_ENEMY(node)->coord->workm.t[0];
        block->node.vy += (u16)GP_NODE_ENEMY(node)->coord->workm.t[1];
        block->node.vz += (u16)GP_NODE_ENEMY(node)->coord->workm.t[2];
        if (func_800E0308(&block->node, &block->self) != 1) {
            bestAngle = angle;
            best      = node;
            bestDist  = dist;
        }
    }
    if (best != NULL) {
        Gp_GetLockPos(best, out);
    }
    SCRATCH_POP_BYTES(0x38);
    return best;
}

void func_800DA6E8(void* arg0, s32 arg1, s32 arg2)
{
    GpSlot70* found;
    s32       i;
    GpSlot70* p;

    found = NULL;
    i     = 0;
    p     = Gp_LockSlots;
loop:
    if (p->field_0 == arg0) {
        if (arg1 >= 0) {
            if (p->field_4 >= 0) {
                found = p;
                goto done;
            }
            p++;
        } else if (p->field_4 < 0) {
            found = p;
            goto done;
        } else {
            p++;
        }
    } else {
        p++;
    }
    i++;
    if (i < 0x20) {
        goto loop;
    }
done:
    if (found == NULL) {
        i = 0;
        p = Gp_LockSlots;
    loop2:
        if (p->field_0 == NULL) {
            found          = p;
            p->field_0     = arg0;
            found->field_4 = 0;
        } else {
            i++;
            p++;
            if (i < 0x20) {
                goto loop2;
            }
        }
        if (found != NULL) {
            goto update;
        }
    } else {
    update:
        found->field_6  = 0x14;
        found->field_4 += arg1;
    }
}

static __inline__ void project_slot(s32* sxy, GpSlot70* slot)
{
    u8*                      head;
    GpLinkNode*              src;
    register GpPerspScratch* block asm("v1");

    src           = slot->field_0;
    head          = SCRATCH_HEAD(u8);
    block         = (GpPerspScratch*)(head - 0x14);
    block->vec.vx = GP_NODE_ENEMY(src)->bodyPos.vx;
    block->vec.vy = GP_NODE_ENEMY(src)->bodyPos.vy;
    block->vec.vz = GP_NODE_ENEMY(src)->bodyPos.vz;
    COMPILER_BARRIER();
    SCRATCH_HEAD(void) = block;
    gte_SetRotMatrix(&GP_NODE_ENEMY(src)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(src)->coord->workm);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(sxy);
    gte_stdp(&((GpPerspScratch*)(head - 0x14))->p);
    gte_stflg(&((GpPerspScratch*)(head - 0x14))->flag);
    gte_stszotz(&((GpPerspScratch*)(head - 0x14))->otz);
    SCRATCH_POP_BYTES(0x14);
}

void Gp_UpdateLockSlots(void)
{
    RECT          rect;
    u8            buf[16];
    TextDrawReq   req;
    s32           i;
    GpSlot70*     slot;
    u8*           bufp;
    TextDrawReq*  reqp;
    register s16* p6 asm("s4");
    s32           x;
    s32           y;
    s32           val;
    s32           x14;
    s32           color;
    s32           ot;
    void*         obj;
    GpLinkNode*   node;
    s32           found;

    slot = Gp_LockSlots;
    i    = 0;
    bufp = buf;
    reqp = &req;
    p6   = &slot->field_6;
    do {
        obj = slot->field_0;
        if (obj == NULL) {
            goto empty;
        }
        node  = Gp_LinkList;
        found = 0;
        if (node != NULL) {
            do {
                if (obj == node) {
                    found = 1;
                    goto check_found;
                }
                node = node->next;
            } while (node != NULL);
        }
    check_found:
        if (found != 0) {
            project_slot((s32*)&slot->field_8, slot);
        } else {
            slot->field_0 = (void*)4;
        }

        val = p6[-1];
        if (val >= 0) {
            x = p6[1] + 0xA;
            y = p6[2] + 4;
        } else {
            x = p6[1] - 0xA;
            y = p6[2] - 0x10;
        }
        if (x < -0x88) {
            x = (x & 7) - 0x8F;
        }
        if (x >= 0x89) {
            x = -(x & 7) + 0x8F;
        }
        if (y >= 0x55) {
            y = (y & 7) + 0x4D;
        }
        if (y < -0x64) {
            y = -(y & 7) - 0x5D;
        }

        color = 0x37A78;
        x14   = x + 0xE;
        ot    = -0xA;
        USE_REG3(color, x14, ot);
        req.field_8    = color;
        req.glyphTable = 5;
        req.centerMode = 2;
        req.x          = x14;
        req.y          = y;
        req.otIndex    = ot;
        req.field_E    = 1;

        val = p6[-1];
        if (val < 0) {
            req.field_8 = 0x808008;
            val         = -val;
        }
        if (val >= 0x2710) {
            val = 0x270F;
        }

        req.x       = x14;
        req.field_E = 0;
        func_8002E53C(reqp, Text_ItoaSigned(bufp, val));
        req.x       = x14;
        req.field_E = 4;
        func_8002E53C(reqp, Text_ItoaSigned(bufp, val));

        rect.x = x - 0x10;
        rect.y = y - 8;
        rect.w = 0x20;
        rect.h = 0xC;
        if (val >= 0x3E8) {
            rect.x = x - 0x18;
            rect.w = 0x28;
        } else if (val < 0x64) {
            rect.x = x - 8;
            rect.w = 0x18;
        }
        Ui_DrawTextInRect(&rect, -0xA, 2, NULL);

        {
            u16 timer;
            timer = p6[0];
            timer--;
            p6[0] = timer;
            if ((s32)(timer << 16) > 0) {
                goto next;
            }
        }
        p6[-1]        = 0;
        p6[0]         = 0;
        slot->field_0 = NULL;
        goto next;

    empty:
        p6[-1] = 0;
        p6[0]  = 0;
    next:
        i++;
        p6 += 6;
        slot++;
    } while (i < 0x20);
}

void Gp_UnlinkNode(GpLinkNode* node)
{
    s32             i;
    Task* volatile* p;
    Task*           work;
    GameActor*      actor;
    GpLinkNode**    list;

    i = 0;
    p = Gp_ActorSlots;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->work;
            if (actor->field_90C == node) {
                actor->field_90C = NULL;
            }
        }
        i++;
        p++;
    } while (i < 2);

    if (node->state.b.onList == 1) {
        list = &Gp_LinkList;
        if (Gp_LinkList != node) {
            do {
                if (*list == NULL) {
                    goto done;
                }
                list = &(*list)->next;
            } while (*list != node);
        }
        if (*list != NULL) {
            *list = node->next;
        }
    done:
        node->state.b.onList   = 0;
        node->state.b.targeted = 0;
    }
}

void Gp_LinkNode(GpLinkNode* node)
{
    GpLinkNode** p;
    register s32 val asm("v0");

    if (node->state.b.onList == 0) {
        p = &Gp_LinkList;
        if (Gp_LinkList != NULL) {
            do {
                p = &(*p)->next;
            } while (*p != NULL);
        }
        *p                     = node;
        val                    = node->state.b.flags;
        node->next             = NULL;
        node->state.b.targeted = 0;
        node->state.b.onList   = 1;
    } else {
        val = node->state.b.flags;
    }
    node->state.b.flags = val & 0xFE;
}

s32 Gp_NodeSlotMask(GpLinkNode* node)
{
    s32             mask;
    s32             i;
    s32             one;
    Task* volatile* p;
    Task*           work;

    mask = 0;
    i    = mask;
    one  = 1;
    p    = Gp_ActorSlots;
    do {
        work = *p;
        if (work != NULL) {
            if (((GameActor*)work->work)->field_90C == node) {
                mask |= one << i;
            }
        }
        i++;
        p++;
    } while (i < 2);
    return mask;
}

void Gp_AssignNodeSlot0(GpLinkNode* node)
{
    Task*       work;
    GameActor*  actor;
    GpLinkNode* previous;
    u8          val;

    work = Gp_ActorSlots[0];
    if (work != NULL) {
        actor    = work->work;
        previous = actor->field_90C;
        if (previous != NULL) {
            previous->state.b.targeted = 0;
        }
        actor->field_90C = node;
    }
    val                    = node->state.b.flags;
    node->state.b.targeted = 1;
    node->state.b.flags    = val & 0xFE;
}

void Gp_ClearNodeSlots(GpLinkNode* node)
{
    s32             i;
    Task* volatile* p;
    Task*           work;
    GameActor*      actor;
    u8              val;

    i = 0;
    p = Gp_ActorSlots;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->work;
            if (actor->field_90C == node) {
                actor->field_90C = NULL;
            }
        }
        i++;
        p++;
    } while (i < 2);
    val                    = node->state.b.flags;
    node->state.b.targeted = 0;
    node->state.b.flags    = val | 1;
}

void* Gp_FindLockNode(Task* arg0)
{
    VECTOR3 pos;

    return Gp_ScanLockNodes(arg0, &pos, 0);
}

void* Gp_FindLockNodePad(Task* arg0)
{
    VECTOR3  pos;
    VECTOR3* p;
    s32      flag;

    p = &pos;
    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return Gp_ScanLockNodes(arg0, p, flag);
}

void* Gp_FindLockNodeAt(Task* arg0, VECTOR3* pos)
{
    s32 flag;

    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return Gp_ScanLockNodes(arg0, pos, flag);
}

/* After D_8009745C from Gp_DebugPanTask so overlay .rodata stays packed. */
const char Gp_StrGetLockPosNull[] = {
    '#',
    '#',
    '#',
    '#',
    '#',
    '#',
    '#',
    'g',
    'e',
    't',
    '_',
    'l',
    'o',
    'c',
    'k',
    '_',
    'p',
    'o',
    's',
    ' ',
    '-',
    '-',
    '-',
    '>',
    ' ',
    'N',
    'U',
    'L',
    'L',
    '!',
    '!',
    '!',
    '\n',
    '\0',
    0x8C,
    0x16,
};

void Gp_GetLockPos(GpLinkNode* arg0, VECTOR3* out)
{
    GpCoord* world;
    GpCoord* coord;
    u8*      head;
    MATRIX*  mat;

    if (arg0 == NULL) {
        printf(Gp_StrGetLockPosNull);
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
        return;
    }

    coord = GP_NODE_ENEMY(arg0)->coord;
    world = &gGfxViewCoord;
    if (coord == world) {
        out->vx = GP_NODE_ENEMY(arg0)->bodyPos.vx;
        out->vy = GP_NODE_ENEMY(arg0)->bodyPos.vy;
        out->vz = GP_NODE_ENEMY(arg0)->bodyPos.vz;
        return;
    }

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x28;
    Gp_UpdateCoord(coord);
    mat = (MATRIX*)(head - 0x20);
    Gp_WorldToLocal(&world->workm, &coord->workm, mat);
    gte_SetRotMatrix(mat);
    gte_SetTransMatrix(mat);
    gte_ldlvl(&GP_NODE_ENEMY(arg0)->bodyPos);
    gte_rtirtr();
    gte_stlvl(out);
    SCRATCH_POP_BYTES(0x28);
}

void Gp_ClearLockSlots(void)
{
    s32       i;
    GpSlot70* p;

    p = Gp_LockSlots;
    i = 0;
    do {
        i++;
        p->field_0 = NULL;
        p->field_4 = 0;
        p->field_6 = 0;
        p++;
    } while (i < 0x20);
}

void Gp_ResetLinkState(void)
{
    Gp_LinkList = NULL;
    Gp_ClearLockSlots();
    D_8010F9F0 = 0xFFF00000;
    D_8010F9EC = 0xFFF00000;
}

s32 Gp_ProjectToSxy(GpLinkNode* arg0, s32* sxy)
{
    u8*             head;
    GpPerspScratch* block;
    s32             ret;

    head                         = SCRATCH_HEAD(u8);
    block                        = (GpPerspScratch*)(head - 0x14);
    block->vec.vx                = GP_NODE_ENEMY(arg0)->bodyPos.vx;
    block->vec.vy                = GP_NODE_ENEMY(arg0)->bodyPos.vy;
    SCRATCH_HEAD(GpPerspScratch) = block;
    block->vec.vz                = GP_NODE_ENEMY(arg0)->bodyPos.vz;
    COMPILER_BARRIER();
    gte_SetRotMatrix(&GP_NODE_ENEMY(arg0)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(arg0)->coord->workm);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(sxy);
    gte_stdp(&((GpPerspScratch*)(head - 0x14))->p);
    gte_stflg(&((GpPerspScratch*)(head - 0x14))->flag);
    gte_stszotz(&((GpPerspScratch*)(head - 0x14))->otz);
    ret = block->otz;
    SCRATCH_POP_BYTES(0x14);
    return ret;
}

void Gp_ClearSlotNodeFlags(void)
{
    s32             i;
    Task* volatile* p;
    Task*           work;
    GpLinkNode*     node;

    i = 0;
    p = Gp_ActorSlots;
    do {
        work = *p;
        if (work != NULL) {
            node = ((GameActor*)work->work)->field_90C;
            if (node != NULL) {
                node->state.b.targeted = 0;
            }
        }
        i++;
        p++;
    } while (i < 2);
}

s32 Gp_GrantLocationItems(McItemScan* arg0)
{
    GpAreaKey* loc;
    GpGiveRec* rec;
    s32        key;
    s32        ret;
    s32        i;
    u16        item;
    s8         mode;
    u8         stage;
    u8         area;
    u8         sub;

    ret   = 0;
    loc   = &gGameSession->at4.loc;
    stage = loc->stage;
    area  = loc->area;
    sub   = loc->place;
    key   = (stage << 24) | (area << 16) | (sub << 8);
    mode  = Mc_SaveData.gameMode;
    if ((mode == 0) || (mode == 2)) {
        rec = D_8010F9F4[stage];
    } else {
        rec = D_8010FA0C[stage];
    }
    if (rec->field_0 != -1) {
        do {
            if (rec->field_0 == key) {
                for (i = 0; i < 4; i++) {
                    item = rec->items[i];
                    if (item != 0) {
                        if ((i != 3) || (func_800B9D80(0x80000) != 0)) {
                            if (func_800B7420(item) == 0) {
                                ret = 1;
                                if (i == 3) {
                                    ret = 2;
                                }
                                Gp_GiveItem(arg0, item, -1);
                            }
                        }
                    }
                }
                return ret;
            }
            rec++;
        } while (rec->field_0 != -1);
    }
    return ret;
}

s32 Gp_LoadActorImage(Task* arg0, GpImgRec* arg1, RECT* arg2)
{
    s32        ret;
    TmdObject* extra;
    s32        x;

    extra = arg0->extra.tmd;
    ret   = 0;
    if (arg1 != NULL) {
        arg1->rect.x = ((s8)extra->tpage << 6) + (x = (arg2->x + 1) / 2 + 0x180);
        arg1->rect.y = arg2->y + 0x100;
        arg1->rect.w = arg2->w;
        arg1->rect.h = arg2->h;
        Gp_LoadImages(arg1);
    } else {
        ret = 1;
    }
    return ret;
}

void Gp_LoadImages(GpImgRec* arg0)
{
    void**         scratch;
    RECT*          head;
    register RECT* temp asm("v0");
    RECT*          dest;
    s32            done;
    register s32   max asm("s4");

    done                           = 0;
    scratch                        = SCRATCH_HEAD_ADDR;
    max                            = 0xFF;
    head                           = SCRATCH_HEAD_AT(scratch, RECT);
    temp                           = head - 1;
    dest                           = temp;
    SCRATCH_HEAD_AT(scratch, RECT) = dest;

    do {
        if (arg0->field_0 == 0) {
            dest->x = arg0->rect.x;
            dest->y = arg0->rect.y;
            dest->w = arg0->rect.w;
            dest->h = arg0->rect.h;
            LoadImage(dest, arg0->data);
        } else {
            done = 1;
        }
        arg0++;
    } while (done == 0);

    SCRATCH_POP(RECT);
    SOFT_USE_REG(max);
}

void Gp_InitStateF0(void)
{
    GpStateF0*  p;
    McSaveData* save;
    u8          val;

    p                  = &Gp_StateF0;
    Gp_StateF0.field_0 = 0;
    p->field_1         = 0;
    p->field_2         = 0;
    p->field_3         = 0;
    p->field_4         = 0;
    p->field_5         = 0;
    p->field_6         = 0;
    p->field_8         = 0;
    p->field_C         = 0;
    p->field_10        = 0;
    p->field_14        = 0;
    p->field_18        = 0;
    p->field_19        = 0;
    p->field_1A        = 0;
    p->field_1B        = 0;
    p->field_1C        = 0;
    p->field_1D        = 0;
    p->field_1E        = 0;
    p->field_1F        = 0;
    p->field_20        = 0;
    p->field_21        = 0;
    p->field_22        = 0;
    p->field_23        = 0;
    p->field_24        = 0;
    p->field_25        = 0;
    p->field_26        = 0;
    p->field_27        = 0;
    p->field_28        = 0;
    p->field_29        = 0;
    p->field_2A        = 0;
    if (Gp_IsDebugAttachRoom() == 1) {
        p->field_2B = 0;
    } else {
        save        = &Mc_SaveData;
        val         = (u8)save->gameMode;
        p->field_2B = val;
        if (val == 0) {
            if (save->clearCount != 0) {
                p->field_2B = 4;
            }
        }
    }
}

void Gp_ArmStateF0(s32 arg0)
{
    if (Gp_StateF0.field_0 == 0) {
        Gp_StateF0.field_0 = 1;
    }
}

void Gp_SetStateF0Bit(s32 arg0)
{
    if (arg0 != 0) {
        Gp_StateF0.field_2 |= 1 << (arg0 - 1);
    }
}

void Gp_SetStateF0Byte3(s32 arg0)
{
    Gp_StateF0.field_3 = arg0;
}

/// `arg0` is 0 at every overlay call site; this build ignores it.
void Gp_IncStateF0Ref(s32 arg0)
{
    Gp_StateF0.field_6++;
}

/// `arg1` is a per-caller release id that every overlay caller passes (0x07,
/// 0x10, 0x15, 0x19, 0x1B, 0x23, 0x26, ...); this build ignores it.
void Gp_ReleaseStateF0Add(Task* arg0, s32 arg1)
{
    GpStateF0*  p;
    GpStateF0*  q;
    GpPairSrcE* rec;

    p = &Gp_StateF0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            Gp_StateF0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            if (!(gGameSession->flowFlags & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
        rec = ((GpEnemy*)arg0->spawnArg2)->param;
        if (rec != NULL) {
            q            = &Gp_StateF0;
            q->field_8  += rec->exp;
            q->field_C  += rec->bp;
            q->field_10 += rec->mp;
        }
    }
}

void Gp_ReleaseStateF0Clear(void)
{
    GpStateF0* p;

    p = &Gp_StateF0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            Gp_StateF0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            p->field_8         = 0;
            p->field_C         = 0;
            p->field_10        = 0;
            if (!(gGameSession->flowFlags & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}

void Gp_ReleaseStateF0(Task* arg0, s32 arg1)
{
    GpStateF0* p;

    p = &Gp_StateF0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            Gp_StateF0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            if (!(gGameSession->flowFlags & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}

void Gp_TickWorldCollision(void)
{
    if (gameGetPtrSlot(3) != NULL) {
        Gp_UpdatePlayerMove();
        Gp_CollideListGrid(Gp_ObjList0);
        Gp_CollideListGrid(Gp_ObjList1);
        Gp_CollideListGrid(Gp_ObjList2);
        Gp_CollideListGrid(Gp_ObjList3);
        Gp_CollideListGrid(Gp_ObjList4);
        Gp_CollideListGrid(Gp_ObjList7);
        Gp_CollideListGrid(Gp_ObjList8);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList3);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList8);
        Gp_RunPairHandler(Gp_ObjList0);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList6);
        Gp_CollideLists(Gp_ObjList2, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList2, Gp_ObjList8);
        Gp_RunPairHandler(Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList3, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList4, Gp_ObjList8);
        if (Gp_PendingObj4CFlag != 0) {
            Gp_ClearPendingObj4C();
        }
        func_800E0608(Gp_ObjList0, 0x9007, 0x9004);
        if (gGameSession->field_12C == 0) {
            func_800E06AC(Gp_ObjList0, 0xA007, 0xA004);
        }
    }
}

void Gp_RunPairHandler(GpObj* node)
{
    GpObj*      other;
    GpPairRule* rec;
    s32         rowOff;
    s32         temp;
    u16         flags;
    u16         handler;
    u16         swap;
    u8          kind;
    u8          otherKind;

    for (; node != NULL; node = node->next) {
        flags = node->flags;
        other = node->next;
        if (flags & 0x8000) {
            kind = (node->flags & 7) - 1;
            if (other != NULL) {
                rowOff = kind << 4;
                for (; other != NULL; other = other->next) {
                    if (other->flags & 0x8000) {
                        otherKind = (other->flags & 7) - 1;
                        temp      = (otherKind << 2) + rowOff;
                        rec       = &D_8010FA4C[0][0] + (temp >> 2);
                        swap      = rec->swap;
                        handler   = rec->handler;
                        if (swap == 0) {
                            Gp_PairHandlers[handler](node, other, handler);
                        } else {
                            Gp_PairHandlers[handler](other, node, handler);
                        }
                    }
                }
            }
        }
    }
}

void func_800DBA20(GpObj* arg0, GpObj* arg1, GpSphereScratch* arg2)
{
    s32               a3v;
    GpRec18*          slot;
    register GpRec18* otable asm("t0");
    register GpRec18* otherSlot asm("v1");
    unsigned int      recFlags;
    u16               f0;
    s32               key;

    if (arg1->key == 0) {
        return;
    }

    a3v = 0;
    switch (arg0->flags & 7) {
        case 0:
            break;
        case 1:
            a3v = (s32)arg0->ctx.recs;
            break;
        case 2:
            a3v = (s32)arg0->ctx.node->ctx.recs;
            break;
        case 3:
            a3v = (s32)arg0->ctx.d4rec->recs;
            break;
        empty_or: /* between case 3 and 4 so the empty-slot trampoline matches */
        {
            register s32 tmp asm("v0");
            tmp         = a3v & 0xF0;
            slot->flags = recFlags | (tmp + 1);
            goto fill;
        }
        case 4:
            a3v = (s32)arg0->ctx.dir->field_8;
            break;
    }
    slot = (GpRec18*)a3v;
    if (slot == NULL) {
        return;
    }

    a3v = arg0->flags;
    if (a3v & 0x800) {
        recFlags = slot->flags;
        if (recFlags & 1) {
            {
                register s32 cmp asm("v0");
                cmp = 0x100000;
                a3v = 0xFFFF0000;
                if ((slot->key & a3v) != cmp) {
                    a3v    = (((s32)slot->at10.node.high << 16) & a3v) | slot->at10.node.low;
                    otable = NULL;
                    switch (((GpObj*)a3v)->flags & 7) {
                        case 0:
                            break;
                        case 1:
                            otable = ((GpObj*)a3v)->ctx.recs;
                            break;
                        case 2:
                            otable = ((GpObj*)a3v)->ctx.node->ctx.recs;
                            break;
                        case 3:
                            otable = ((GpObj*)a3v)->ctx.d4rec->recs;
                            break;
                        case 4:
                            otable = ((GpObj*)a3v)->ctx.dir->field_8;
                            break;
                    }
                    otherSlot = otable;
                    if (otherSlot == NULL) {
                        return;
                    }
                    key = arg0->key;
                loop:
                    if (otherSlot->key != key) {
                        if (otherSlot->flags & 2) {
                            return;
                        }
                        otherSlot++;
                        goto loop;
                    }
                    f0                        = otherSlot->flags;
                    otherSlot->key            = 0;
                    otherSlot->depth          = 0;
                    otherSlot->point.vx       = 0;
                    otherSlot->point.vy       = 0;
                    otherSlot->point.vz       = 0;
                    otherSlot->at10.normal.vx = 0;
                    otherSlot->at10.normal.vy = 0;
                    otherSlot->at10.normal.vz = 0;
                    otherSlot->flags          = f0 & 0xFFFE;
                }
            }
            recFlags    = slot->flags;
            recFlags    = recFlags | ((arg0->flags & 0xF0) + 1);
            slot->flags = recFlags;
            goto fill;
        } else {
            goto empty_or;
        }
    } else {
        while (1) {
            recFlags = slot->flags;
            if (!(recFlags & 1)) {
                break;
            }
            if (recFlags & 2) {
                return;
            }
            slot++;
        }
        slot->flags = recFlags | ((arg0->flags & 0xF0) + 1);
    }

fill:
    slot->key                     = arg1->key;
    slot->depth                   = (u16)arg2->rsum;
    slot->point                   = arg2->src;
    *(SVECTOR*)&slot->at10.normal = arg2->extra;
}

s32 Gp_PairHandler1(GpObj* arg0, GpObj* arg1)
{
    u8*              head;
    GpSphereScratch* block;
    register s32     dx asm("v0");
    register s32     a asm("a0");
    register s32     b asm("v1");
    register s32     c asm("a1");
    s32              ret;
    register s32     t0 asm("a2");
    s32              dz;
    s32              rsum;

    head                          = SCRATCH_HEAD(u8);
    block                         = (GpSphereScratch*)(head - 0x48);
    SCRATCH_HEAD(GpSphereScratch) = block;
    Gp_ObjWorldPos(arg0, (VECTOR3*)(head - 0x34));
    Gp_ObjWorldPos(arg1, (VECTOR3*)(head - 0x24));

    dx              = block->pos0.vx;
    a               = block->pos1.vx;
    b               = block->pos0.vy;
    c               = block->pos1.vy;
    dx             -= a;
    a               = block->pos0.vz;
    b              -= c;
    block->delta.vy = b;
    b               = block->pos1.vz;
    ret             = 0;
    block->delta.vx = dx;
    COMPILER_BARRIER();
    if (dx < 0) {
        dx = -dx;
    }
    dz              = a - b;
    block->delta.vz = dz;
    if ((dx > 0x7FFF) || (ABS(dz) > 0x7FFF)) {
        SCRATCH_POP_BYTES(0x48);
        return 0;
    }

    COMPILER_BARRIER();
    dx            = block->delta.vx;
    t0            = dx * dx;
    dx            = block->delta.vy;
    c             = dx * dx;
    dx            = block->delta.vz;
    a             = dx * dx;
    rsum          = (u16)arg0->radius + (u16)arg1->radius;
    block->rsum32 = rsum;
    dx            = t0 + c + a;
    if (dx < (b = rsum * rsum)) {
        s32 rad;

        a                             = (s32)arg0;
        c                             = (s32)arg1;
        dx                            = (u16)block->pos1.vx;
        t0                            = (s32)block;
        ((SVECTOR*)(head - 0x48))->vx = dx;
        dx                            = (u16)block->pos1.vy;
        b                             = (u16)block->pos1.vz;
        rad                           = (u16)block->rsum32;
        ret                           = 1;
        block->extra.vx               = 0;
        block->extra.vy               = 0;
        block->extra.vz               = 0;
        block->src.vy                 = dx;
        block->src.vz                 = b;
        block->rsum                   = rad;
        func_800DBA20((GpObj*)a, (GpObj*)c, (GpSphereScratch*)t0);

        ((SVECTOR*)(head - 0x48))->vx = (s16)block->pos0.vx;
        block->src.vy                 = (s16)block->pos0.vy;
        block->src.vz                 = (s16)block->pos0.vz;
        block->extra.vx               = 0;
        block->extra.vy               = 0;
        block->extra.vz               = 0;
        block->rsum                   = (s16)block->rsum32;
        func_800DBA20(arg1, arg0, block);
    }

    SCRATCH_POP_BYTES(0x48);
    return ret;
}

s32 Gp_PairHandler3(GpObj* arg0, GpObj* arg1)
{
    u8*               head;
    GpCapsuleScratch* block;
    VECTOR*           ends;
    GpActorD4Rec*     rec;
    s32               proj;
    s32               ret;
    s32               tapered;
    VECTOR3*          pos;
    s32               ratioDelta;
    s32               radiusSquared;
    s32               dx0;
    s32               dy0;
    s32               dz0;
    s32               dx1;
    s32               dy1;
    s32               dz1;
    s32               dx2;
    s32               dy2;
    s32               dz2;
    s32               dx3;
    s32               dy3;
    s32               dz3;
    s32               dx4;
    s32               dy4;
    s32               dz4;
    s32               len;
    s32               plen;
    s32               r0;
    s32               r1;
    s32               radius;

    head               = SCRATCH_HEAD(u8);
    pos                = (VECTOR3*)(head - 0x78);
    SCRATCH_HEAD(void) = head - 0x8C;
    rec                = arg1->ctx.d4rec;
    block              = (GpCapsuleScratch*)(head - 0x8C);
    Gp_ObjWorldPos(arg0, pos);
    ends = (VECTOR*)(head - 0x68);
    func_800DEC80(arg1, ends, (SVECTOR*)(head - 0x18), 0);

    block->scaled.vx = (block->normal.vx * (u16)arg0->radius) >> 12;
    block->scaled.vy = (block->normal.vy * (u16)arg0->radius) >> 12;
    block->scaled.vz = (block->normal.vz * (u16)arg0->radius) >> 12;

    block->planeA.vx = block->end0.vx + block->scaled.vx;
    block->planeA.vy = block->end0.vy + block->scaled.vy;
    block->planeA.vz = block->end0.vz + block->scaled.vz;
    block->planeB.vx = block->end1.vx - block->scaled.vx;
    block->planeB.vy = block->end1.vy - block->scaled.vy;
    block->planeB.vz = block->end1.vz - block->scaled.vz;

    dx0 = (block->sphere.vx - block->planeA.vx) * block->normal.vx;
    dy0 = (block->sphere.vy - block->planeA.vy) * block->normal.vy;
    dz0 = (block->sphere.vz - block->planeA.vz) * block->normal.vz;
    ret = 0;
    if (dx0 + dy0 + dz0 > 0) {
        SCRATCH_POP_BYTES(0x8C);
        return 0;
    }

    dx1  = (block->sphere.vx - block->planeB.vx) * block->normal.vx;
    dy1  = (block->sphere.vy - block->planeB.vy) * block->normal.vy;
    dz1  = (block->sphere.vz - block->planeB.vz) * block->normal.vz;
    proj = (dx1 + dy1 + dz1) >> 12;
    if (proj <= 0) {
        SCRATCH_POP_BYTES(0x8C);
        return 0;
    }

    r1      = rec->end0Radius;
    tapered = r1 != rec->end1Radius;
    if (!tapered) {
        radius        = (u16)arg0->radius + r1;
        block->hit.vx = (u16)block->planeB.vx + ((block->normal.vx * proj) >> 12);
        block->hit.vy = (u16)block->planeB.vy + ((block->normal.vy * proj) >> 12);
        block->hit.vz = (u16)block->planeB.vz + ((block->normal.vz * proj) >> 12);
        proj          = radius;
        goto check;
    }

    if (arg1->flags & 0xC00) {
        gte_SetRotMatrix(&arg1->coord->workm);
        block->scaled.vx = (u16)rec->end0.vx + (u16)arg1->pos.vx;
        block->scaled.vy = (u16)rec->end0.vy + (u16)arg1->pos.vy;
        block->scaled.vz = (u16)rec->end0.vz + (u16)arg1->pos.vz;
        gte_ldv0((SVECTOR*)(head - 8));
        gte_rtv0();
        gte_stlvnl(ends);
        block->end0.vx += (arg1->coord)->workm.t[0];
        block->end0.vy += (arg1->coord)->workm.t[1];
        block->end0.vz += (arg1->coord)->workm.t[2];
    }

    block->delta.vx = block->end0.vx - block->end1.vx;
    block->delta.vy = block->end0.vy - block->end1.vy;
    block->delta.vz = block->end0.vz - block->end1.vz;
    dx2             = block->delta.vx * block->delta.vx;
    dy2             = block->delta.vy * block->delta.vy;
    dz2             = block->delta.vz * block->delta.vz;
    len             = SquareRoot0(dx2 + dy2 + dz2);

    block->scaled.vx = (block->normal.vx * proj) >> 12;
    block->scaled.vy = (block->normal.vy * proj) >> 12;
    block->scaled.vz = (block->normal.vz * proj) >> 12;
    dx3              = block->scaled.vx * block->scaled.vx;
    dy3              = block->scaled.vy * block->scaled.vy;
    dz3              = block->scaled.vz * block->scaled.vz;
    proj             = len;
    plen             = SquareRoot0(dx3 + dy3 + dz3);

    r0         = (rec->end0Radius << 12) / rec->end1Radius;
    proj       = (plen << 12) / proj;
    ratioDelta = r0 - 0x1000;
    SOFT_TOUCH_REG_USE(ratioDelta, r0);
    r1            = (u16)arg0->radius;
    proj          = r1 + ((((ratioDelta * proj) >> 12) * rec->end1Radius >> 12) + rec->end1Radius);
    block->hit.vx = (u16)block->scaled.vx + (u16)block->planeB.vx;
    block->hit.vy = (u16)block->scaled.vy + (u16)block->planeB.vy;
    block->hit.vz = (u16)block->scaled.vz + (u16)block->planeB.vz;

check:
    block->scaled.vx = (u16)block->hit.vx - (u16)block->sphere.vx;
    block->scaled.vy = (u16)block->hit.vy - (u16)block->sphere.vy;
    block->scaled.vz = (u16)block->hit.vz - (u16)block->sphere.vz;
    dx4              = block->scaled.vx * block->scaled.vx;
    dy4              = block->scaled.vy * block->scaled.vy;
    dz4              = block->scaled.vz * block->scaled.vz;
    radiusSquared    = proj * proj;
    if (dx4 + dy4 + dz4 < radiusSquared) {
        block->rsum     = 0;
        block->src.vx   = (u16)block->end1.vx;
        block->src.vy   = (u16)block->end1.vy;
        block->src.vz   = (u16)block->end1.vz;
        block->extra.vx = (u16)block->normal.vx;
        block->extra.vy = (u16)block->normal.vy;
        block->extra.vz = (u16)block->normal.vz;
        func_800DBA20(arg0, arg1, (GpSphereScratch*)block);
        if (!tapered) {
            block->src = block->hit;
        } else {
            block->src.vx = (u16)block->sphere.vx;
            block->src.vy = (u16)block->sphere.vy;
            block->src.vz = (u16)block->sphere.vz;
        }
        if (arg1->flags & 0x800) {
            block->extra.vx = (s32)arg0;
            block->extra.vy = (s32)arg0 >> 16;
        } else {
            block->extra.vx = 0;
            block->extra.vy = 0;
        }
        block->extra.vz = 0;
        block->rsum     = 0;
        func_800DBA20(arg1, arg0, (GpSphereScratch*)block);
        ret = 1;
    }

    SCRATCH_POP_BYTES(0x8C);
    return ret;
}

void Gp_CollideObjGrid(GpObj* arg0)
{
    u8*               head;
    GpGridHitScratch* block;
    VECTOR3*          pos;
    GpGridFace*       face;
    GpRec18*          slot;
    register s16*     cell asm("s7");
    s32               id;
    s32               i;
    s32               n;
    s32               outside;
    s32               val;
    s32               faceDot;
    s32               edgeDot;
    u16               dist;
    u16               flags;

    head               = SCRATCH_HEAD(u8);
    pos                = (VECTOR3*)(head - 0x80);
    SCRATCH_HEAD(void) = head - 0x88;
    block              = (GpGridHitScratch*)(head - 0x88);
    Gp_ObjWorldPos(arg0, pos);
    Gp_LocalToGrid(pos, &block->grid);

    if ((u16)block->grid.vx < Gp_GridParams->field_1C && (u16)block->grid.vz < Gp_GridParams->field_1E) {
        cell = Gp_GridParams->field_10[block->grid.vx * Gp_GridParams->field_1E + block->grid.vz];
        if (cell != NULL) {
            for (;; cell++) {
                id = *cell;
                if (id == -1) {
                    goto done;
                }
                face = &Gp_GridParams->field_C[id];
                if (face->verts[0] == 0 && face->verts[1] == 0) {
                    continue;
                }

                COMPILER_BARRIER();
                gte_SetRotMatrix(&Gp_GridParams->field_0->workm);
                gte_ldv0(&Gp_GridParams->field_8[face->verts[0]]);
                gte_rtv0();
                gte_stlvnl(&block->verts[0]);
                block->verts[0].vx += Gp_GridParams->field_0->workm.t[0];
                block->verts[0].vy += Gp_GridParams->field_0->workm.t[1];
                block->verts[0].vz += Gp_GridParams->field_0->workm.t[2];

                gte_ldv0(&Gp_GridParams->field_4[face->field_8]);
                gte_rtv0();
                gte_stlvnl(&block->normal);

                faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
                           block->normal.vz * block->verts[0].vz) >>
                          12;
                dist = ((block->normal.vx * block->pos.vx + block->normal.vy * block->pos.vy +
                         block->normal.vz * block->pos.vz) >>
                        12) -
                       faceDot;
                if ((u16)arg0->radius >= ABS((s16)dist)) {
                    goto edges;
                }
                continue;

            mark_outside:
                outside = 1;
                goto edges_done;

            fill:
                slot->flags       = flags | 1;
                slot->depth       = (u16)arg0->radius - dist;
                slot->key         = face->field_A | 0x100000;
                slot->point.vx    = 0;
                slot->point.vy    = 0;
                slot->point.vz    = 0;
                slot->at10.normal = Gp_GridParams->field_4[face->field_8];
                continue;

            edges:
                n = (face->verts[3] != 0xFFFF) ? 4 : 3;
                for (i = 1; i < n; i++) {
                    gte_ldv0(&Gp_GridParams->field_8[face->verts[i]]);
                    gte_rtv0();
                    gte_stlvnl(&block->verts[i]);
                    block->verts[i].vx += Gp_GridParams->field_0->workm.t[0];
                    block->verts[i].vy += Gp_GridParams->field_0->workm.t[1];
                    block->verts[i].vz += Gp_GridParams->field_0->workm.t[2];
                }

                outside = 0;
                for (i = n - 3; i < n * 2 - 3; i++) {
                    block->delta.vx =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vx - block->verts[Gp_FaceEdgePairs[i].field_2].vx;
                    block->delta.vy =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vy - block->verts[Gp_FaceEdgePairs[i].field_2].vy;
                    block->delta.vz =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vz - block->verts[Gp_FaceEdgePairs[i].field_2].vz;
                    VectorNormal(&block->delta, &block->unit);
                    gte_ldopv1(&block->normal);
                    gte_ldopv2(&block->unit);
                    gte_op12();
                    gte_stlvnl(&block->delta);

                    edgeDot = (block->delta.vx * block->verts[Gp_FaceEdgePairs[i].field_0].vx +
                               block->delta.vy * block->verts[Gp_FaceEdgePairs[i].field_0].vy +
                               block->delta.vz * block->verts[Gp_FaceEdgePairs[i].field_0].vz) >>
                              12;
                    val = (s16)(((block->delta.vx * block->pos.vx + block->delta.vy * block->pos.vy +
                                  block->delta.vz * block->pos.vz) >>
                                 12) -
                                edgeDot);
                    if (val - 10 > 0) {
                        goto mark_outside;
                    }
                }
            edges_done:
                if (outside) {
                    continue;
                }

                slot = arg0->ctx.recs;
                for (;;) {
                    flags = slot->flags;
                    if (!(flags & 1)) {
                        goto fill;
                    }
                    if (flags & 2) {
                        goto done;
                    }
                    slot++;
                }
            }
        }
    }

done:
    SCRATCH_POP_BYTES(0x88);
}

void Gp_CollideObjGridDir(GpObj* arg0)
{
    u8*               head;
    GpGridHitScratch* block;
    VECTOR3*          pos;
    GpGridFace*       face;
    GpRec18*          slot;
    GpObjDirRec*      rec;
    register s16*     cell asm("s5");
    s32               id;
    s32               i;
    s32               n;
    s32               outside;
    s32               val;
    s32               faceDot;
    s32               edgeDot;
    s32               dist;
    s32               extra;
    s32               faceKind;
    u16               flags;

    head               = SCRATCH_HEAD(u8);
    pos                = (VECTOR3*)(head - 0x80);
    SCRATCH_HEAD(void) = head - 0x88;
    block              = (GpGridHitScratch*)(head - 0x88);
    rec                = arg0->ctx.dir;
    Gp_ObjWorldPos(arg0, pos);
    Gp_LocalToGrid(pos, &block->grid);

    if ((u16)block->grid.vx < Gp_GridParams->field_1C && (u16)block->grid.vz < Gp_GridParams->field_1E) {
        cell = Gp_GridParams->field_10[block->grid.vx * Gp_GridParams->field_1E + block->grid.vz];
        if (cell != NULL) {
            for (;; cell++) {
                id = *cell;
                if (id == -1) {
                    goto done;
                }
                face = &Gp_GridParams->field_C[id];
                if (face->verts[0] == 0 && face->verts[1] == 0) {
                    continue;
                }
                COMPILER_BARRIER();
                if (Gp_GridParams->field_4[face->field_8].vy < -0xDDA) {
                    continue;
                }

                gte_SetRotMatrix(&Gp_GridParams->field_0->workm);
                gte_ldv0(&Gp_GridParams->field_8[face->verts[0]]);
                gte_rtv0();
                gte_stlvnl(&block->verts[0]);
                block->verts[0].vx += Gp_GridParams->field_0->workm.t[0];
                block->verts[0].vy += Gp_GridParams->field_0->workm.t[1];
                block->verts[0].vz += Gp_GridParams->field_0->workm.t[2];

                gte_ldv0(&Gp_GridParams->field_4[face->field_8]);
                gte_rtv0();
                gte_stlvnl(&block->normal);

                if (rec->dir.vx * block->normal.vx + rec->dir.vy * block->normal.vy +
                        rec->dir.vz * block->normal.vz >
                    0x280000) {
                    continue;
                }

                faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
                           block->normal.vz * block->verts[0].vz) >>
                          12;
                {
                    register s32 planeDist asm("v0");

                    planeDist = ((block->normal.vx * block->pos.vx + block->normal.vy * block->pos.vy +
                                  block->normal.vz * block->pos.vz) >>
                                 12) -
                                faceDot;
                    dist = planeDist;
                    TOUCH_REG(planeDist);
                    if ((u16)arg0->radius >= ABS((s16)planeDist)) {
                        goto edges;
                    }
                }
                continue;

            mark_outside:
                outside = 1;
                goto edges_done;

            edges:
                n = (face->verts[3] != 0xFFFF) ? 4 : 3;
                for (i = 1; i < n; i++) {
                    gte_ldv0(&Gp_GridParams->field_8[face->verts[i]]);
                    gte_rtv0();
                    gte_stlvnl(&block->verts[i]);
                    block->verts[i].vx += Gp_GridParams->field_0->workm.t[0];
                    block->verts[i].vy += Gp_GridParams->field_0->workm.t[1];
                    block->verts[i].vz += Gp_GridParams->field_0->workm.t[2];
                }

                extra   = 0;
                outside = 0;
                for (i = n - 3; i < n * 2 - 3; i++) {
                    block->delta.vx =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vx - block->verts[Gp_FaceEdgePairs[i].field_2].vx;
                    block->delta.vy =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vy - block->verts[Gp_FaceEdgePairs[i].field_2].vy;
                    block->delta.vz =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vz - block->verts[Gp_FaceEdgePairs[i].field_2].vz;
                    VectorNormal(&block->delta, &block->unit);
                    gte_ldopv1(&block->normal);
                    gte_ldopv2(&block->unit);
                    gte_op12();
                    gte_stlvnl(&block->delta);

                    edgeDot = (block->delta.vx * block->verts[Gp_FaceEdgePairs[i].field_0].vx +
                               block->delta.vy * block->verts[Gp_FaceEdgePairs[i].field_0].vy +
                               block->delta.vz * block->verts[Gp_FaceEdgePairs[i].field_0].vz) >>
                              12;
                    val = (s16)(((block->delta.vx * block->pos.vx + block->delta.vy * block->pos.vy +
                                  block->delta.vz * block->pos.vz) >>
                                 12) -
                                edgeDot);
                    if (val - (u16)arg0->radius > 0) {
                        outside = 1;
                        goto edges_done;
                    }
                    if (val > 0) {
                        if ((s16)dist < 0) {
                            goto mark_outside;
                        }
                        extra = 0x200;
                    }
                }
            edges_done:
                if (outside) {
                    continue;
                }

                slot = arg0->ctx.dir->field_8;
                for (;;) {
                    flags = slot->flags;
                    if (flags & 1) {
                        if ((slot->key & -0x100) == (extra | 0x100000)) {
                            if (slot->at10.normal.vx == Gp_GridParams->field_4[face->field_8].vx &&
                                slot->at10.normal.vy == Gp_GridParams->field_4[face->field_8].vy &&
                                slot->at10.normal.vz == Gp_GridParams->field_4[face->field_8].vz) {
                                if (slot->depth < (s32)(u16)arg0->radius - (s16)dist) {
                                    slot->depth = (u16)arg0->radius - dist;
                                }
                                goto next_face;
                            }
                        }
                    } else {
                        slot->flags       = flags | 1;
                        slot->depth       = (u16)arg0->radius - dist;
                        faceKind          = face->field_A | 0x100000;
                        slot->key         = extra | faceKind;
                        slot->point.vx    = 0;
                        slot->point.vy    = 0;
                        slot->point.vz    = 0;
                        slot->at10.normal = Gp_GridParams->field_4[face->field_8];
                        goto next_face;
                    }
                    if (slot->flags & 2) {
                        goto done;
                    }
                    slot++;
                }

            next_face:;
            }
        }
    }

done:
    SCRATCH_POP_BYTES(0x88);
}

s32 func_800DD324(s32 faceId, VECTOR* seg, SVECTOR* ray, s32 arg3)
{
    u8*               head;
    GpGridRayScratch* block;
    GpGridFace*       face;
    s32               i;
    s32               n;
    s16               faceDot;
    s32               denom;
    s32               t;
    s32               edgeDot;
    s32               val;
    s32               limit;

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x70;
    face               = &Gp_GridParams->field_C[faceId];
    block              = (GpGridRayScratch*)(head - 0x70);

    gte_SetRotMatrix(&Gp_GridParams->field_0->workm);
    gte_ldv0(&Gp_GridParams->field_8[face->verts[0]]);
    gte_rtv0();
    gte_stlvnl(&block->verts[0]);
    block->verts[0].vx += Gp_GridParams->field_0->workm.t[0];
    block->verts[0].vy += Gp_GridParams->field_0->workm.t[1];
    block->verts[0].vz += Gp_GridParams->field_0->workm.t[2];

    gte_ldv0(&Gp_GridParams->field_4[face->field_8]);
    gte_rtv0();
    gte_stlvnl(&block->normal);

    faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
               block->normal.vz * block->verts[0].vz) >>
              12;
    denom = (block->normal.vx * ray[0].vx + block->normal.vy * ray[0].vy + block->normal.vz * ray[0].vz) >> 12;

    if (denom >= 0) {
        SCRATCH_POP_BYTES(0x70);
        return 0;
    }
    if ((((block->normal.vx * seg[1].vx + block->normal.vy * seg[1].vy + block->normal.vz * seg[1].vz) >> 12) -
         faceDot) <= 0) {
        SCRATCH_POP_BYTES(0x70);
        return 0;
    }
    t = -(((((block->normal.vx * seg[0].vx + block->normal.vy * seg[0].vy + block->normal.vz * seg[0].vz) >> 12) -
            faceDot)
           << 12)) /
        denom;
    if (t >= 0) {
        SCRATCH_POP_BYTES(0x70);
        return 0;
    }

    ray[1].vx = seg[0].vx + ((ray[0].vx * t) >> 12);
    ray[1].vy = seg[0].vy + ((ray[0].vy * t) >> 12);
    ray[1].vz = seg[0].vz + ((ray[0].vz * t) >> 12);

    n = (face->verts[3] == 0xFFFF) ? 3 : 4;

    gte_SetRotMatrix(&Gp_GridParams->field_0->workm);
    for (i = 1; i < n; i++) {
        gte_ldv0(&Gp_GridParams->field_8[face->verts[i]]);
        gte_rtv0();
        gte_stlvnl(&block->verts[i]);
        block->verts[i].vx += Gp_GridParams->field_0->workm.t[0];
        block->verts[i].vy += Gp_GridParams->field_0->workm.t[1];
        block->verts[i].vz += Gp_GridParams->field_0->workm.t[2];
    }

    for (i = n - 3; i < n * 2 - 3; i++) {
        block->delta.vx = block->verts[Gp_FaceEdgePairs[i].field_0].vx - block->verts[Gp_FaceEdgePairs[i].field_2].vx;
        block->delta.vy = block->verts[Gp_FaceEdgePairs[i].field_0].vy - block->verts[Gp_FaceEdgePairs[i].field_2].vy;
        block->delta.vz = block->verts[Gp_FaceEdgePairs[i].field_0].vz - block->verts[Gp_FaceEdgePairs[i].field_2].vz;
        VectorNormal(&block->delta, &block->unit);
        gte_ldopv1(&block->normal);
        gte_ldopv2(&block->unit);
        gte_op12();
        gte_stlvnl(&block->delta);

        edgeDot = (block->delta.vx * block->verts[Gp_FaceEdgePairs[i].field_0].vx +
                   block->delta.vy * block->verts[Gp_FaceEdgePairs[i].field_0].vy +
                   block->delta.vz * block->verts[Gp_FaceEdgePairs[i].field_0].vz) >>
                  12;
        limit = 5;
        val   = ((block->delta.vx * ray[1].vx + block->delta.vy * ray[1].vy + block->delta.vz * ray[1].vz) >> 12) -
              edgeDot;
        if (arg3 != 0) {
            limit = 10;
        }
        if ((s16)val - limit > 0) {
            SCRATCH_POP_BYTES(0x70);
            return 0;
        }
    }
    SCRATCH_POP_BYTES(0x70);
    return 1;
}

void func_800DD940(GpObj* arg0)
{
    u8*             head;
    GpFloorScratch* block;
    GpRec18*        slot;
    s32             i;
    u16             flags;

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x50;
    block              = (GpFloorScratch*)(head - 0x50);
    for (i = 0; i < Gp_GridParams->field_22; i++) {
        D_80115450[i] = 0;
    }
    func_800DDC2C(arg0);
    func_800E0994(arg0, block->seg, block->ray);
    block->origin.vx = block->seg[0].vx;
    block->origin.vy = block->seg[0].vy;
    block->origin.vz = block->seg[0].vz;
    for (i = 0; i < Gp_GridParams->field_22; i++) {
        if (D_80115450[i] &&
            Gp_GridParams->field_4[Gp_GridParams->field_C[i].field_8].vy < -0xDDA &&
            func_800DD324(i, block->seg, block->ray, (s32)arg0)) {
            slot  = arg0->ctx.dir->field_8;
            flags = slot->flags;
            if (flags & 1) {
                if ((u32)(slot->key & 0xF) < (u32)Gp_GridParams->field_C[i].field_A) {
                    slot->key = Gp_GridParams->field_C[i].field_A | 0x100100;
                }
            } else {
                slot->flags = flags | 1;
                slot->key   = Gp_GridParams->field_C[i].field_A | 0x100100;
            }
            slot->point       = block->ray[1];
            slot->at10.normal = Gp_GridParams->field_4[Gp_GridParams->field_C[i].field_8];
            block->delta.vx   = block->origin.vx - block->ray[1].vx;
            block->delta.vy   = block->origin.vy - block->ray[1].vy;
            block->delta.vz   = block->origin.vz - block->ray[1].vz;
            slot->depth       = SquareRoot0(block->delta.vx * block->delta.vx +
                                            block->delta.vy * block->delta.vy + block->delta.vz * block->delta.vz);
            block->seg[0].vx  = block->ray[1].vx;
            block->seg[0].vy  = block->ray[1].vy;
            block->seg[0].vz  = block->ray[1].vz;
        }
    }
    SCRATCH_POP_BYTES(0x50);
}

void func_800DDC2C(GpObj* arg0)
{
    s32            i;
    GpEdgeScratch* block;
    SVECTOR*       dir;
    MATRIX*        mat;

    dir              = &arg0->ctx.dir->dir;
    block            = SCRATCH_PUSH(GpEdgeScratch);
    mat              = &block->mat;
    block->src[0].vx = (u16)arg0->pos.vx + ((dir->vx * (u16)arg0->radius) >> 12);
    block->src[0].vy = 0;
    block->src[0].vz = (u16)arg0->pos.vz + ((dir->vz * (u16)arg0->radius) >> 12);
    block->src[1].vx = (u16)arg0->pos.vx + (-(dir->vx * (u16)arg0->radius) >> 12);
    block->src[1].vy = 0;
    block->src[1].vz = (u16)arg0->pos.vz + (-(dir->vz * (u16)arg0->radius) >> 12);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &arg0->coord->workm, mat);
    gte_SetRotMatrix(mat);
    for (i = 0; i < 2; i++) {
        gte_ldv0(&block->src[i]);
        gte_rtv0();
        gte_stlvnl(&block->pos[i]);
        block->pos[i].vx = block->pos[i].vx + block->mat.t[0] + Gp_GridParams->field_14;
        block->pos[i].vy = 0;
        block->pos[i].vz = block->pos[i].vz + block->mat.t[2] + Gp_GridParams->field_18;
    }
    func_800DE2C0(block->pos, 0);
    SCRATCH_POP(GpEdgeScratch);
}

void func_800DDDF8(GpObj* node)
{
    GpObj*               obj;
    s32                  i;
    GpSegmentHitScratch* block;
    u8*                  head0;
    GpRec18*             slot;
    void**               head;
    s32                  flags;
    GpActorD4Rec*        rec;
    s32                  mask;
    s32                  idx;
    s32                  t;
    GpGridParams*        grid2;

    obj              = node;
    head0            = SCRATCH_HEAD(u8);
    head0           -= 0x30;
    SCRATCH_HEAD(u8) = head0;
    block            = (GpSegmentHitScratch*)head0;
    for (i = 0; i < Gp_GridParams->field_22; i++) {
        D_80115450[i] = 0;
    }

    func_800DE150(obj);
    func_800DEC80(obj, block->pos, block->ray, 1);

    /* The single-pass scope preserves GCC 2.8.1 register allocation. */
    do {
        for (i = 0; i < Gp_GridParams->field_22; i++) {
            if (D_80115450[i] != 0) {
                if (func_800DD324(i, block->pos, block->ray, (s32)obj) != 0) {
                    rec  = obj->ctx.d4rec;
                    slot = rec->recs;
                    if (obj->flags & 0x400) {
                        if (Gp_RoomParamTables[gGameSession->at4.loc.stage - 1]
                                              [gGameSession->at4.loc.area - 1]
                                              [Gp_GridParams->field_C[i].field_A]
                                                  ->field_1 == 0) {
                            mask              = 0x100000;
                            slot->depth       = 0;
                            slot->flags      |= 1;
                            idx               = Gp_GridParams->field_C[i].field_A;
                            slot->key         = idx | mask;
                            slot->point       = block->ray[1];
                            grid2             = Gp_GridParams;
                            slot->at10.normal = grid2->field_4[grid2->field_C[i].field_8];
                            block->pos[0].vx  = block->ray[1].vx;
                            block->pos[0].vy  = block->ray[1].vy;
                            block->pos[0].vz  = block->ray[1].vz;
                        }
                    } else {
                        t    = i * sizeof(GpGridFace);
                        head = SCRATCH_HEAD_ADDR;
                        for (;;) {
                            flags = slot->flags;
                            if (!(flags & 1)) {
                                slot->flags       = flags | 1;
                                slot->depth       = 0;
                                mask              = 0x100000;
                                idx               = ((GpGridFace*)(t + (s32)Gp_GridParams->field_C))->field_A;
                                slot->key         = idx | mask;
                                slot->point       = block->ray[1];
                                grid2             = Gp_GridParams;
                                slot->at10.normal = grid2->field_4[((GpGridFace*)(t + (s32)grid2->field_C))->field_8];
                                if (slot->flags & 2) {
                                    SCRATCH_POP_BYTES_AT(head, 0x30);
                                    return;
                                }
                                break;
                            }
                            mask = (u16)flags;
                            SOFT_USE_REG(flags);
                            idx = 3;
                            if (mask != idx) {
                                goto next_slot;
                            }
                            SCRATCH_POP_BYTES_AT(head, 0x30);
                            return;
                        next_slot:
                            slot++;
                        }
                    }
                }
            }
        }

    } while (0);
    SCRATCH_POP_BYTES(0x30);
}

void func_800DE150(GpObj* arg0)
{
    s32            i;
    u8*            head;
    GpEdgeScratch* block;
    SVECTOR*       src;
    GpCoord*       coord;
    MATRIX*        mat;

    coord              = arg0->coord;
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = head - 0x50;
    block              = (GpEdgeScratch*)(head - 0x50);
    mat                = (MATRIX*)(head - 0x20);
    src                = (SVECTOR*)arg0->ctx.d4rec;
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, mat);
    gte_SetRotMatrix(mat);
    for (i = 0; i < 2; i++) {
        block->src[i].vx = (u16)src[i].vx + (u16)arg0->pos.vx;
        block->src[i].vy = 0;
        block->src[i].vz = (u16)src[i].vz + (u16)arg0->pos.vz;
        gte_ldv0(&block->src[i]);
        gte_rtv0();
        gte_stlvnl(&block->pos[i]);
        block->pos[i].vx = block->pos[i].vx + block->mat.t[0] + Gp_GridParams->field_14;
        block->pos[i].vy = 0;
        block->pos[i].vz = block->pos[i].vz + block->mat.t[2] + Gp_GridParams->field_18;
    }
    func_800DE2C0(block->pos, 1);
    SCRATCH_POP_BYTES(0x50);
}

void func_800DE2C0(VECTOR* arg0, s32 arg1)
{
    u8*            head;
    GpMarkScratch* block;
    GpGridParams*  p;
    GpGridParams*  p2;
    s32            thresh2;
    u32            cellSize;
    s32            half;
    s32            range;
    s32            thresh;
    s32            i;
    s32            j;
    s32            dot;
    s32            proj;
    s32            vz0;
    s32            vz1;
    s16*           ids;
    s16            id;

    head          = SCRATCH_HEAD(u8);
    cellSize      = Gp_GridParams->field_20;
    block         = (GpMarkScratch*)(SCRATCH_HEAD(void) = head - 0x28);
    block->vec.vx = arg0[0].vx - arg0[1].vx;
    block->vec.vy = 0;
    vz0           = arg0[0].vz;
    vz1           = arg0[1].vz;
    block->vec.vz = vz0 - vz1;
    half          = cellSize >> 1;
    range         = ((half * 0xB5) >> 7) + 1;
    VectorNormalS(&block->vec, &block->nrm);

    if ((block->nrm.vx == 0) && (block->nrm.vz == 0)) {
        for (i = 0; i < Gp_GridParams->field_1C; i++) {
            thresh = range * range;
            for (j = 0; j < Gp_GridParams->field_1E; j++) {
                p              = Gp_GridParams;
                block->cell.vx = i * p->field_20 + (p->field_20 >> 1);
                block->cell.vz = j * p->field_20 + (p->field_20 >> 1);
                block->d.vx    = (u16)block->cell.vx - (u16)arg0[0].vx;
                block->d.vz    = (u16)block->cell.vz - (u16)arg0[0].vz;
                if ((block->d.vx * block->d.vx) + (block->d.vz * block->d.vz) < thresh) {
                    ids = p->field_10[i * p->field_1E + j];
                    if (ids != NULL) {
                        while (*ids != -1) {
                            id             = *ids;
                            D_80115450[id] = 1;
                            ids++;
                        }
                    }
                }
            }
        }
    } else {
        block->d.vx = (block->nrm.vx * range) >> 12;
        block->d.vz = (block->nrm.vz * range) >> 12;
        arg0[0].vx += block->d.vx;
        arg0[0].vz += block->d.vz;
        arg0[1].vx -= block->d.vx;
        arg0[1].vz -= block->d.vz;
        for (i = 0; i < Gp_GridParams->field_1C; i++) {
            thresh2 = range * range;
            for (j = 0; j < Gp_GridParams->field_1E; j++) {
                p2             = Gp_GridParams;
                block->cell.vx = i * p2->field_20 + (p2->field_20 >> 1);
                block->cell.vz = j * p2->field_20 + (p2->field_20 >> 1);
                dot            = ((block->cell.vx - arg0[0].vx) * block->nrm.vx) + ((block->cell.vz - arg0[0].vz) * block->nrm.vz);
                if (dot <= 0) {
                    proj = (((block->cell.vx - arg0[1].vx) * block->nrm.vx) + ((block->cell.vz - arg0[1].vz) * block->nrm.vz)) >> 12;
                    if (proj > 0) {
                        block->d.vx = ((u16)arg0[1].vx + ((block->nrm.vx * proj) >> 12)) - (u16)block->cell.vx;
                        block->d.vz = ((u16)arg0[1].vz + ((block->nrm.vz * proj) >> 12)) - (u16)block->cell.vz;
                        if ((block->d.vx * block->d.vx) + (block->d.vz * block->d.vz) < thresh2) {
                            ids = p2->field_10[i * p2->field_1E + j];
                            if (ids != NULL) {
                                while (*ids != -1) {
                                    id             = *ids++;
                                    D_80115450[id] = 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    SCRATCH_POP_BYTES(0x28);
}

s32 func_800DE7CC(SVECTOR* arg0, SVECTOR* arg1, SVECTOR* arg2, SVECTOR* arg3)
{
    GpGridParams*    params;
    s32              ret;
    GpRayHitScratch* block;
    s32              i;

    params = Gp_GridParams;
    ret    = 0;
    if (params == NULL) {
        return ret;
    }

    {
        u8* head;

        head             = SCRATCH_HEAD(u8);
        i                = 0;
        head            -= 0x40;
        SCRATCH_HEAD(u8) = head;
        block            = (GpRayHitScratch*)head;
        if (ret < params->field_22) {
            do {
                D_80115450[i] = 0;
                i++;
            } while (i < Gp_GridParams->field_22);
        }
    }
    func_800DEAFC(arg0, arg1);
    block->from.vx  = arg0->vx;
    block->from.vy  = arg0->vy;
    block->from.vz  = arg0->vz;
    block->to.vx    = arg1->vx;
    block->to.vy    = arg1->vy;
    block->to.vz    = arg1->vz;
    block->delta.vx = block->from.vx - block->to.vx;
    block->delta.vy = block->from.vy - block->to.vy;
    block->delta.vz = block->from.vz - block->to.vz;
    VectorNormalS(&block->delta, &block->dir);
    for (i = 0; i < Gp_GridParams->field_22; i++) {
        if (D_80115450[i] == 0) {
            continue;
        }
        if (Gp_RoomParamTables[gGameSession->at4.loc.stage - 1][gGameSession->at4.loc.area - 1]
                              [Gp_GridParams->field_C[i].field_A]
                                  ->field_1 != 0) {
            continue;
        }
        if (func_800DD324(i, &block->from, &block->dir, 0) == 0) {
            continue;
        }
        if (arg2 != NULL) {
            arg2->vx = block->hit.vx;
            arg2->vy = block->hit.vy;
            arg2->vz = block->hit.vz;
        }
        if (arg3 != NULL) {
            arg3->vx = Gp_GridParams->field_4[Gp_GridParams->field_C[i].field_8].vx;
            arg3->vy = Gp_GridParams->field_4[Gp_GridParams->field_C[i].field_8].vy;
            arg3->vz = Gp_GridParams->field_4[Gp_GridParams->field_C[i].field_8].vz;
        }
        block->from.vx = block->hit.vx;
        block->from.vy = block->hit.vy;
        block->from.vz = block->hit.vz;
        ret            = 1;
    }
    SCRATCH_POP_BYTES(0x40);
    return ret;
}

void func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1)
{
    u8*                    head;
    GpGridPairScratch*     block;
    VECTOR*                out;
    register GpGridParams* p asm("a2");

    head                            = SCRATCH_HEAD(u8);
    block                           = (GpGridPairScratch*)(head - 0x40);
    block->in.vx                    = arg0->vx;
    block->in.vy                    = arg0->vy;
    block->in.vz                    = arg0->vz;
    out                             = (VECTOR*)(head - 0x30);
    SCRATCH_HEAD(GpGridPairScratch) = block;
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &block->in, out);
    p              = Gp_GridParams;
    block->pos0.vx = (s16)((u16)block->out.vx + (u16)p->field_14 - (u16)p->field_0->coord.t[0]);
    block->pos0.vy = 0;
    block->pos0.vz = (s16)((u16)block->out.vz + (u16)p->field_18 - (u16)p->field_0->coord.t[2]);
    block->in.vx   = arg1->vx;
    block->in.vy   = arg1->vy;
    block->in.vz   = arg1->vz;
    ApplyTransposeMatrixLV(&p->field_0->workm, &block->in, out);
    p              = Gp_GridParams;
    block->pos1.vx = (s16)((u16)block->out.vx + (u16)p->field_14 - (u16)p->field_0->coord.t[0]);
    block->pos1.vy = 0;
    block->pos1.vz = (s16)((u16)block->out.vz + (u16)p->field_18 - (u16)p->field_0->coord.t[2]);
    func_800DE2C0((VECTOR*)(head - 0x20), 0);
    SCRATCH_POP_BYTES(0x40);
}

void func_800DEC80(GpObj* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    GpObj*                  obj;
    void**                  scratch;
    register u8*            head asm("v0");
    register s32            found asm("t1");
    GpActorD4Rec*           rec;
    register GpNormScratch* block asm("a0");
    register VECTOR*        pos asm("t0");
    register SVECTOR*       src asm("a3");
    SVECTOR*                local;
    s32                     temp;
    GpRec18*                slot;
    s32                     flags;

    obj     = arg0;
    scratch = SCRATCH_HEAD_ADDR;
    TOUCH_REG2(obj, scratch);
    head                         = SCRATCH_HEAD_AT(scratch, u8);
    found                        = 0;
    head                        -= 0x18;
    SCRATCH_HEAD_AT(scratch, u8) = head;
    rec                          = obj->ctx.d4rec;
    block                        = (GpNormScratch*)head;

    if (arg3 == 0) {
        if (obj->flags & 0x800) {
            temp = (s32)rec;
            TOUCH_REG(temp);
            slot = ((GpActorD4Rec*)temp)->recs;
            for (;;) {
                flags = slot->flags;
                if (flags & 1) {
                    arg1->vx = slot->point.vx;
                    arg1->vy = slot->point.vy;
                    arg1->vz = slot->point.vz;
                    found    = 1;
                    goto done_search;
                }
                if (flags & 2) {
                    goto done_search;
                }
                slot++;
            }
        } else if (obj->flags & 0x400) {
            slot = obj->ctx.d4rec->recs;
            for (;;) {
                if (slot->flags & 1) {
                    if ((slot->key & 0xFFFF0000) == 0x100000) {
                        arg1->vx = slot->point.vx;
                        arg1->vy = slot->point.vy;
                        arg1->vz = slot->point.vz;
                        found    = 1;
                        goto done_search;
                    }
                }
                if (slot->flags & 2) {
                    goto done_search;
                }
                slot++;
            }
        }
    } else if (obj->flags & 0x400) {
        slot = obj->ctx.d4rec->recs;
        for (;;) {
            if (slot->flags & 1) {
                if ((slot->key & 0xFFFF0000) == 0x100000) {
                    arg1->vx = slot->point.vx;
                    arg1->vy = slot->point.vy;
                    arg1->vz = slot->point.vz;
                    found    = 1;
                    goto done_search;
                }
            }
            if (slot->flags & 2) {
                break;
            }
            slot++;
        }
    }

done_search:
    gte_SetRotMatrix(&obj->coord->workm);
    if (found < 2) {
        local = &block->local;
        temp  = found << 4;
        pos   = (VECTOR*)(temp + (s32)arg1);
        temp  = found << 3;
        src   = (SVECTOR*)(temp + (s32)rec);
        do {
            block->local.vx = (u16)src->vx + (u16)obj->pos.vx;
            block->local.vy = (u16)src->vy + (u16)obj->pos.vy;
            block->local.vz = (u16)src->vz + (u16)obj->pos.vz;
            gte_ldv0(local);
            gte_rtv0();
            gte_stlvnl(&block->vec);
            pos->vx = block->vec.vx + (obj->coord)->workm.t[0];
            pos->vy = block->vec.vy + (obj->coord)->workm.t[1];
            pos->vz = block->vec.vz + (obj->coord)->workm.t[2];
            src++;
            found++;
            pos++;
        } while (found < 2);
    }

    block->vec.vx = arg1[0].vx - arg1[1].vx;
    block->vec.vy = arg1[0].vy - arg1[1].vy;
    block->vec.vz = arg1[0].vz - arg1[1].vz;
    VectorNormalS(&block->vec, arg2);

    SCRATCH_POP_BYTES(0x18);
}

static __inline__ void Gp_ObjWorldPosInline(GpObj* obj, VECTOR* pos)
{
    u8*     h;
    VECTOR* vec;
    h                  = SCRATCH_HEAD(u8);
    vec                = (VECTOR*)(h - 0x30);
    SCRATCH_HEAD(void) = vec;
    gte_SetRotMatrix(&obj->coord->workm);
    gte_ldv0(&obj->pos);
    gte_rtv0();
    gte_stlvnl(vec);
    pos->vx = (obj->coord)->workm.t[0] + ((VECTOR*)(h - 0x30))->vx;
    pos->vy = (obj->coord)->workm.t[1] + vec->vy;
    pos->vz = (obj->coord)->workm.t[2] + vec->vz;
    SCRATCH_POP_BYTES(0x30);
}

void func_800DEF80(GpObj* node, GpObj4C* other)
{
    GpQuadHitScratch* block;
    s32               distSq;
    s32               kind;
    s32               dot;
    s32               dist;
    s32               tmp;
    s32               i;
    VECTOR *          va, *vb;
    s16               faceDot;

    SCRATCH_PUSH(GpQuadHitScratch);
    block = SCRATCH_HEAD(GpQuadHitScratch);
    Gp_ObjWorldPosInline(node, &block->nodePos);
    gte_SetRotMatrix(&other->field_8->workm);
    gte_ldv0(&other->field_C);
    gte_rtv0();
    gte_stlvnl(&block->world);
    block->world.vx += other->field_8->workm.t[0];
    block->world.vy += other->field_8->workm.t[1];
    block->world.vz += other->field_8->workm.t[2];

    block->delta.vx = block->world.vx - block->nodePos.vx;
    block->delta.vy = block->world.vy - block->nodePos.vy;
    block->delta.vz = block->world.vz - block->nodePos.vz;
    distSq          = block->delta.vx * block->delta.vx + block->delta.vy * block->delta.vy +
             block->delta.vz * block->delta.vz;
    tmp = other->field_44 + node->radius;
    if (tmp * tmp < distSq) {
        SCRATCH_POP(GpQuadHitScratch);
        return;
    }

    kind = other->field_4A & 7;
    if (kind == 2) {
        GpCoord* c;
        s32      m0, m1, m2, a;

        c    = node->coord;
        a    = other->field_3C.vx;
        m0   = a * c->coord.m[0][2];
        a    = other->field_3C.vy;
        m1   = a * c->coord.m[1][2];
        a    = other->field_3C.vz;
        m2   = a * c->coord.m[2][2];
        dot  = m0 + m1;
        dot += m2;
        if (dot > -0xC00000) {
            SCRATCH_POP(GpQuadHitScratch);
            return;
        }
    } else if (kind == 4) {
        if (distSq <= 0x3D08F) {
            other->field_4B = 1;
            SCRATCH_POP(GpQuadHitScratch);
            return;
        }
        block->local.vx = other->field_C.vx;
        block->local.vy = node->coord->coord.t[1] + node->pos.vy;
        block->local.vz = other->field_C.vz;
        gte_SetRotMatrix(&other->field_8->workm);
        gte_ldv0(&block->local);
        gte_rtv0();
        gte_stlvnl(&block->delta);
        block->delta.vx = block->nodePos.vx - (block->delta.vx + other->field_8->workm.t[0]);
        block->delta.vy = block->nodePos.vy - (block->delta.vy + other->field_8->workm.t[1]);
        block->delta.vz = block->nodePos.vz - (block->delta.vz + other->field_8->workm.t[2]);
        VectorNormal(&block->delta, &block->delta);
        {
            GpCoord* c;
            s32      n0, n1, n2;

            c    = node->coord;
            n0   = block->delta.vx * c->workm.m[0][2];
            n1   = block->delta.vy * c->workm.m[1][2];
            n2   = block->delta.vz * c->workm.m[2][2];
            dot  = n0 + n1;
            dot += n2;
        }
        if (dot > -0xC00000) {
            SCRATCH_POP(GpQuadHitScratch);
            return;
        }
    }

    gte_ldv0(&other->field_14[0]);
    gte_rtv0();
    gte_stlvnl(&block->verts[0]);
    block->verts[0].vx += block->world.vx;
    block->verts[0].vy += block->world.vy;
    block->verts[0].vz += block->world.vz;

    gte_ldv0(&other->field_34);
    gte_rtv0();
    gte_stlvnl(&block->normal);

    faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
               block->normal.vz * block->verts[0].vz) >>
              12;
    dist = ((block->normal.vx * block->nodePos.vx + block->normal.vy * block->nodePos.vy +
             block->normal.vz * block->nodePos.vz) >>
            12) -
           faceDot;
    if (dist >= 0 || dist < -node->radius) {
        SCRATCH_POP(GpQuadHitScratch);
        return;
    }

    for (i = 1; i < 4; i++) {
        gte_ldv0(&other->field_14[i]);
        gte_rtv0();
        gte_stlvnl(&block->verts[i]);
        block->verts[i].vx += block->world.vx;
        block->verts[i].vy += block->world.vy;
        block->verts[i].vz += block->world.vz;
    }

    for (i = 1; i < 5; i++) {
        va              = &block->verts[(u16)Gp_FaceEdgePairs[i].field_0];
        vb              = &block->verts[(u16)Gp_FaceEdgePairs[i].field_2];
        block->delta.vx = va->vx - vb->vx;
        block->delta.vy = va->vy - vb->vy;
        block->delta.vz = va->vz - vb->vz;
        gte_ldopv1(&block->normal);
        gte_ldopv2(&block->delta);
        gte_op12();
        gte_stlvnl(&block->cross);
        tmp   = block->cross.vx * block->nodePos.vx + block->cross.vy * block->nodePos.vy;
        tmp  += block->cross.vz * block->nodePos.vz;
        tmp >>= 12;
        tmp  -= (block->cross.vx * va->vx + block->cross.vy * va->vy + block->cross.vz * va->vz) >> 12;
        if (tmp >= 0) {
            SCRATCH_POP(GpQuadHitScratch);
            return;
        }
    }

    other->field_4B = 1;
    SCRATCH_POP(GpQuadHitScratch);
}

void func_800DF6AC(GpObj* arg0, GpObj4C* arg1, VECTOR3* arg2)
{
    VECTOR*    va;
    VECTOR*    vb;
    GpU16Pair* pair;
    u8*        t;
    s32        off;
    GpObj*     obj;
    u8*        head;
    VECTOR*    block;
    s32        i;
    void**     sp;
    s32        dot;
    s32        dx2;
    s32        tmp2;
    s32        tmp3;
    s32        rawDot;
    s32        pointDot;
    s32        radius3;
    s32        plane3;
    VECTOR*    vec4;
    s32        tmp5;
    obj                  = arg0;
    head                 = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(VECTOR) = (VECTOR*)(head - 0xB0);
    block                = SCRATCH_HEAD(VECTOR);
    block[10].vx         = (obj->coord)->coord.t[0] - arg2->vx;
    block[10].vy         = (obj->coord)->coord.t[1] - arg2->vy;
    block[10].vz         = (obj->coord)->coord.t[2] - arg2->vz;
    SquareRoot0(block[10].vx * block[10].vx + block[10].vy * block[10].vy + block[10].vz * block[10].vz);
    VectorNormal((VECTOR*)(head - 0x10), (VECTOR*)(head - 0x10));

    dot = (arg1->field_34.vx * block[10].vx) + (arg1->field_34.vy * block[10].vy) + (arg1->field_34.vz * block[10].vz);
    if (dot >= 0) {
        SCRATCH_POP_BYTES(0xB0);
        return;
    }
    Gp_ObjWorldPosInline(obj, (VECTOR*)(head - 0x30));
    gte_SetRotMatrix(&arg1->field_8->workm);
    gte_ldv0(&arg1->field_C);
    gte_rtv0();
    gte_stlvnl((VECTOR*)(head - 0x70));
    block[4].vx += arg1->field_8->workm.t[0];
    block[4].vy += arg1->field_8->workm.t[1];
    block[4].vz += arg1->field_8->workm.t[2];
    block[6].vx  = block[4].vx - block[8].vx;
    block[6].vy  = block[4].vy - block[8].vy;
    block[6].vz  = block[4].vz - block[8].vz;
    dx2          = block[6].vx * block[6].vx + block[6].vy * block[6].vy + block[6].vz * block[6].vz;
    tmp2         = (u16)arg1->field_44 + (u16)obj->radius;
    if ((tmp2 * tmp2) < dx2) {
        SCRATCH_POP_BYTES(0xB0);
        return;
    }

    gte_ldv0(&arg1->field_14[0]);
    gte_rtv0();
    gte_stlvnl(block);
    ((VECTOR*)(head - 0xB0))->vx += block[4].vx;
    block[0].vy                  += block[4].vy;
    block[0].vz                  += block[4].vz;

    gte_ldv0(&arg1->field_34);
    gte_rtv0();
    gte_stlvnl((VECTOR*)(head - 0x60));

    rawDot   = (block[5].vx * ((VECTOR*)(head - 0xB0))->vx) + (block[5].vy * block[0].vy) + (block[5].vz * block[0].vz);
    pointDot = (block[5].vx * block[8].vx) + (block[5].vy * block[8].vy) + (block[5].vz * block[8].vz);
    tmp3     = (rawDot << 4) >> 16;
    plane3   = (pointDot >> 12) - tmp3;
    if (plane3 >= 0) {
        SCRATCH_POP_BYTES(0xB0);
        return;
    }

    radius3 = -(s32)(u16)obj->radius;
    if (plane3 >= radius3) {
        goto body;
    }

restore_early:
    SCRATCH_POP_BYTES(0xB0);
    return;

restore_reload:
    sp = SCRATCH_HEAD_ADDR;
    goto do_restore;

body:
    i    = 1;
    vec4 = (VECTOR*)(head - 0xA0);
    off  = 0x1C;
    do {
        gte_ldv0((SVECTOR*)((u8*)arg1 + off));
        gte_rtv0();
        gte_stlvnl(vec4);
        vec4->vx += block[4].vx;
        TOUCH_REG(vec4);
        off      += 8;
        vec4->vy += block[4].vy;
        i++;
        vec4->vz += block[4].vz;
        vec4++;
    } while (i < 4);

    i    = 1;
    t    = (u8*)Gp_FaceEdgePairs;
    pair = (GpU16Pair*)(t + 4);
    do {
        TOUCH_REG(block);
        {
            s32 ia;
            s32 ib;
            ia = pair->field_0;
            ib = pair->field_2;
            va = &block[ia];
            vb = &block[ib];
        }
        block[6].vx = va->vx - vb->vx;
        block[6].vy = va->vy - vb->vy;
        block[6].vz = va->vz - vb->vz;
        gte_ldopv1(&block[5]);
        gte_ldopv2(&block[6]);
        gte_op12();
        gte_stlvnl(&block[7]);
        tmp2   = (block[7].vx * block[8].vx) + (block[7].vy * block[8].vy);
        tmp2  += block[7].vz * block[8].vz;
        tmp2 >>= 12;
        tmp5   = (block[7].vx * va->vx) + (block[7].vy * va->vy) + (block[7].vz * va->vz);
        i++;
        tmp2 -= tmp5 >> 12;
        if (tmp2 >= 0) {
            goto restore_reload;
        }
        pair++;
    } while (i < 5);

    sp             = SCRATCH_HEAD_ADDR;
    arg1->field_4B = 1;
do_restore:
    SCRATCH_POP_BYTES_AT(sp, 0xB0);
}

s32 func_800DFCCC(GpObj3A* arg0, SVECTOR* arg1, SVECTOR* arg2, VECTOR* arg3)
{
    u8*               tmp;
    u8*               head;
    GpFaceHitScratch* block;
    s32               scratch_dot;
    s32               dir_dot;
    s32               t;
    s32               p0;
    s32               p1;
    s32               p2;
    s32               va_dot;
    s32               hit_dot;
    s32               prod;
    s32               hit_z;
    void**            sp;
    s32               i;
    s32               nx;
    s32               ny;
    s32               nz;
    VECTOR*           out;
    s32               off;
    GpU16Pair*        pair;
    GpU16Pair*        base;
    VECTOR*           va;
    VECTOR*           vb;
    s32               idx0;
    s32               idx1;

    head  = SCRATCH_HEAD(u8);
    tmp   = head - 0x80;
    block = (GpFaceHitScratch*)tmp;
    SOFT_TOUCH_REG_USE(block, tmp);

    SCRATCH_HEAD(u8) = tmp;

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&arg0->origin);
    gte_rtv0();
    gte_stlvnl((VECTOR*)(head - 0x40));
    block->origin.vx += gGfxViewCoord.workm.t[0];
    block->origin.vy += gGfxViewCoord.workm.t[1];
    block->origin.vz += gGfxViewCoord.workm.t[2];

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&arg0->verts[0]);
    gte_rtv0();
    gte_stlvnl(block);
    ((VECTOR*)(head - 0x80))->vx += block->origin.vx;
    block->verts[0].vy           += block->origin.vy;
    block->verts[0].vz           += block->origin.vz;

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&arg0->normal);
    gte_rtv0();
    gte_stlvnl((VECTOR*)(head - 0x30));

    nx          = block->normal.vx;
    ny          = block->normal.vy;
    nz          = block->normal.vz;
    scratch_dot = (nx * ((VECTOR*)(head - 0x80))->vx + ny * block->verts[0].vy + nz * block->verts[0].vz) >> 12;
    dir_dot     = (nx * arg3->vx + ny * arg3->vy + nz * arg3->vz) >> 12;

    if (dir_dot == 0) {
        goto fail_early;
    }
    p0 = nx * arg1->vx;
    p1 = ny * arg1->vy;
    p2 = nz * arg1->vz;
    t  = ((p0 + p1 + p2) >> 12) - (s16)scratch_dot;
    t  = -(t << 12) / dir_dot;
    if (t == 0) {
    fail_early:
        SCRATCH_POP_BYTES(0x80);
        return 0;
    }

    gte_SetRotMatrix(&gGfxViewCoord.workm);

    i   = 1;
    out = (VECTOR*)(head - 0x70);
    off = 0x18;
    do {
        gte_ldv0((SVECTOR*)((u8*)arg0 + off));
        gte_rtv0();
        gte_stlvnl(out);
        out->vx += block->origin.vx;
        TOUCH_REG(out);
        off     += 8;
        out->vy += block->origin.vy;
        i++;
        out->vz += block->origin.vz;
        out++;
    } while (i < 4);

    block->origin.vx = arg1->vx + ((arg3->vx * t) >> 12);
    block->origin.vy = arg1->vy + ((arg3->vy * t) >> 12);
    hit_z = block->origin.vz = arg1->vz + ((arg3->vz * t) >> 12);

    if ((block->origin.vx - arg1->vx) * (block->origin.vx - arg2->vx) + (block->origin.vy - arg1->vy) * (block->origin.vy - arg2->vy) + (hit_z - arg1->vz) * (hit_z - arg2->vz) < 0) {
        goto do_poly;
    }
    sp = (void**)0x1F800000;
    sp = (void**)((s32)sp | 0x3FC);
    SCRATCH_POP_BYTES_AT(sp, 0x80);
    return 0;
fail_poly:
    sp = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(sp, 0x80);
    return 0;

do_poly:

    i    = 1;
    base = (GpU16Pair*)Gp_FaceEdgePairs;
    pair = base + 1;
    do {
        TOUCH_REG(block);
        idx0           = pair->field_0;
        idx1           = pair->field_2;
        va             = &block->verts[idx0];
        vb             = &block->verts[idx1];
        block->edge.vx = va->vx - vb->vx;
        block->edge.vy = va->vy - vb->vy;
        block->edge.vz = va->vz - vb->vz;
        gte_ldopv1(&block->normal);
        gte_ldopv2(&block->edge);
        gte_op12();
        gte_stlvnl(&block->cross);
        va_dot  = block->cross.vx * va->vx + block->cross.vy * va->vy + block->cross.vz * va->vz;
        hit_dot = block->cross.vx * block->origin.vx + block->cross.vy * block->origin.vy + block->cross.vz * block->origin.vz;
        prod    = (hit_dot >> 12) - ((va_dot << 4) >> 16);
        i++;
        if (prod > 0) {
            goto fail_poly;
        }
        pair++;
    } while (i < 5);
    sp = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(sp, 0x80);
    tmp = 0;
    return 1;
}

void Gp_ClearObjHeads(void)
{
    Gp_ObjList0         = NULL;
    Gp_ObjList1         = NULL;
    Gp_ObjList2         = NULL;
    Gp_ObjList3         = NULL;
    Gp_ObjList4         = NULL;
    Gp_ObjList5         = NULL;
    Gp_ObjList6         = NULL;
    Gp_ObjList7         = NULL;
    Gp_ObjList8         = NULL;
    Gp_GridParams       = 0;
    Gp_PendingObj4C     = NULL;
    Gp_Obj4CList        = NULL;
    D_80115550          = NULL;
    Gp_PendingObj4CFlag = 0;
}

s32 func_800E0308(SVECTOR* arg0, SVECTOR* arg1)
{
    void**           scratch;
    u8*              head;
    register VECTOR* vec asm("s1");
    GpObj3A*         node;
    s32              ret;

    ret                          = 0;
    scratch                      = SCRATCH_HEAD_ADDR;
    node                         = D_80115550;
    head                         = SCRATCH_HEAD_AT(scratch, u8);
    ((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
    head                         = head - 0x10;
    vec                          = (VECTOR*)head;
    TOUCH_REG_USE(vec, head);
    vec->vy                          = arg1->vy - arg0->vy;
    SCRATCH_HEAD_AT(scratch, VECTOR) = vec;
    vec->vz                          = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    SCRATCH_POP_BYTES(0x10);
    return ret;
}

void Gp_CollideLists(GpObj* a, GpObj* b)
{
    GpObj*      other;
    GpPairRule* rec;
    s32         rowOff;
    s32         temp;
    u16         flags;
    u16         handler;
    u16         swap;
    u8          kind;
    u8          otherKind;

    for (; a != NULL; a = a->next) {
        flags = a->flags;
        if (flags & 0x8000) {
            kind  = (a->flags & 7) - 1;
            other = b;
            if (other != NULL) {
                rowOff = kind << 4;
                for (; other != NULL; other = other->next) {
                    if (other->flags & 0x8000) {
                        otherKind = (other->flags & 7) - 1;
                        temp      = (otherKind << 2) + rowOff;
                        rec       = &D_8010FA4C[0][0] + (temp >> 2);
                        swap      = rec->swap;
                        handler   = rec->handler;
                        if (swap == 0) {
                            Gp_PairHandlers[handler](a, other, handler);
                        } else {
                            Gp_PairHandlers[handler](other, a, handler);
                        }
                    }
                }
            }
        }
    }
}

void Gp_CollideListGrid(GpObj* node)
{
    u16 flags;

    if (Gp_GridParams != 0) {
        for (; node != NULL; node = node->next) {
            flags = node->flags;
            if (flags & 0x4000) {
                switch (flags & 7) {
                    case 0:
                        break;
                    case 1:
                        Gp_CollideObjGrid(node);
                        break;
                    case 2:
                        break;
                    case 3:
                        func_800DDDF8(node);
                        break;
                    case 4:
                        if (node->flags & 0x200) {
                            func_800DD940(node);
                        }
                        Gp_CollideObjGridDir(node);
                        break;
                }
            }
        }
    }
}

void func_800E0608(GpObj* node, s32 mask, s32 match)
{
    GpObj4C* other;

    other = Gp_PendingObj4C;
    for (; node != NULL; node = node->next) {
        if ((node->flags & mask) == (u16)match) {
            for (; other != NULL; other = other->next) {
                if (other->field_4A & 0x40) {
                    func_800DEF80(node, other);
                }
            }
        }
    }
}

void func_800E06AC(GpObj* node, s32 mask, s32 match)
{
    GpObj4C*   other;
    GameActor* actor;
    s32        idx;
    s32        msk;
    u16        mch;

    other = Gp_Obj4CList;
    idx   = 3;
    msk   = mask;
    mch   = match;
    actor = gameGetPtrSlot(idx)->work;
    for (; node != NULL; node = node->next) {
        if ((node->flags & msk) == mch) {
            for (; other != NULL; other = other->next) {
                if (other->field_4A & 0x40) {
                    func_800DF6AC(node, other, (VECTOR3*)&actor->field_10);
                }
            }
        }
    }
}

s32 Gp_PairNop(void)
{
    return 0;
}

void Gp_LocalToGrid(VECTOR3* arg0, SVECTOR3* arg1)
{
    u8*           head;
    VECTOR*       vec;
    GpGridParams* p;
    s32           val;

    head = SCRATCH_HEAD(u8);
    vec = SCRATCH_HEAD(VECTOR) = (VECTOR*)(head - 0x10);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)arg0, vec);
    p   = Gp_GridParams;
    val = ((VECTOR*)(head - 0x10))->vx + p->field_14 - p->field_0->coord.t[0];
    if (val >= 0) {
        arg1->vx = val / p->field_20;
    } else {
        arg1->vx = -1;
    }
    p        = Gp_GridParams;
    arg1->vy = 0;
    val      = vec->vz + p->field_18 - p->field_0->coord.t[2];
    if (val >= 0) {
        arg1->vz = val / p->field_20;
    } else {
        arg1->vz = -1;
    }
    SCRATCH_POP_BYTES(0x10);
}

void Gp_ObjWorldPos(GpObj* arg0, VECTOR3* arg1)
{
    u8*      head;
    VECTOR3* vec;

    head                  = SCRATCH_HEAD(u8);
    vec                   = (VECTOR3*)(head - 0x30);
    SCRATCH_HEAD(VECTOR3) = vec;
    COMPILER_BARRIER();
    gte_SetRotMatrix(&arg0->coord->workm);
    gte_ldv0(&arg0->pos.vx);
    gte_rtv0();
    gte_stlvnl(vec);
    arg1->vx = (arg0->coord)->workm.t[0] + ((VECTOR3*)(head - 0x30))->vx;
    arg1->vy = (arg0->coord)->workm.t[1] + vec->vy;
    arg1->vz = (arg0->coord)->workm.t[2] + vec->vz;
    SCRATCH_POP_BYTES(0x30);
}

void func_800E0994(GpObj* arg0, VECTOR* arg1, SVECTOR* arg2)
{
    u8*            head;
    GpAxisScratch* block;
    s32            i;

    head = SCRATCH_HEAD(u8);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x20;
        block = (GpAxisScratch*)tmp;
    }
    block->local[0].vx          = 0;
    block->local[0].vy          = (u16)arg0->pos.vy + (u16)arg0->radius;
    block->local[0].vz          = 0;
    block->local[1].vx          = 0;
    block->local[1].vy          = (u16)arg0->pos.vy - (u16)arg0->radius;
    block->local[1].vz          = 0;
    SCRATCH_HEAD(GpAxisScratch) = block;
    gte_SetRotMatrix(&arg0->coord->workm);
    for (i = 0; i < 2; i++) {
        gte_ldv0(&block->local[i]);
        gte_rtv0();
        gte_stlvnl(&block->vec);
        arg1[i].vx = block->vec.vx + (arg0->coord)->workm.t[0];
        arg1[i].vy = block->vec.vy + (arg0->coord)->workm.t[1];
        arg1[i].vz = block->vec.vz + (arg0->coord)->workm.t[2];
    }
    block->vec.vx = arg1[0].vx - arg1[1].vx;
    block->vec.vy = arg1[0].vy - arg1[1].vy;
    block->vec.vz = arg1[0].vz - arg1[1].vz;
    VectorNormalS(&block->vec, arg2);
    SCRATCH_POP_BYTES(0x20);
}

void Gp_ClearPendingObj4C(void)
{
    GpObj4C* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
        }
    }
}

void Gp_WorldToGrid(VECTOR3* arg0, SVECTOR3* arg1)
{
    s32           val;
    GpGridParams* p;

    p   = Gp_GridParams;
    val = arg0->vx + p->field_14;
    if (val >= 0) {
        arg1->vx = val / p->field_20;
    } else {
        arg1->vx = -1;
    }
    p        = Gp_GridParams;
    arg1->vy = 0;
    val      = arg0->vz + p->field_18;
    if (val >= 0) {
        arg1->vz = val / p->field_20;
    } else {
        arg1->vz = -1;
    }
}

s32 func_800E0C10(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3)
{
    u8*             head;
    GpSlideScratch* s;
    GpRec18*        rec;
    s32             i;
    s32             j;
    s32             count;
    s32             mask;
    s32             ret;

    count = 0;
    ret   = 0;
    mask  = 0;
    /* `list` is a VLA, so its alloca has to be emitted after the three
     * initializations above; the inner block is what pins that order. */
    {
        s16 list[arg2];

        if (arg2 == 0) {
            return count;
        }

        head               = SCRATCH_HEAD(u8);
        SCRATCH_HEAD(void) = head - 0x34;
        s                  = (GpSlideScratch*)(head - 0x34);

        s->acc[0].vx = 0;
        s->acc[0].vy = 0;
        s->acc[0].vz = 0;
        s->acc[1].vx = 0;
        s->acc[1].vy = 0;
        s->acc[1].vz = 0;
        s->count     = 0;

        for (i = 0; i < arg2; i++) {
            rec = &arg0[i];
            if ((rec->flags & 1) && (rec->key & 0xFFFF0000) == 0x100000) {
                mask |= 1 << rec->key;
                if (Gp_RoomParams[rec->key & 7] == 0) {
                    if (rec->at10.normal.vy >= -0xDDA) {
                        s->acc[0].vx += rec->at10.normal.vx * rec->depth;
                        s->acc[0].vy += rec->at10.normal.vy * rec->depth;
                        s->acc[0].vz += rec->at10.normal.vz * rec->depth;
                        list[count++] = i;
                    } else {
                        s->acc[1].vx  = 0;
                        s->acc[1].vy += rec->at10.normal.vy * rec->depth;
                        s->acc[1].vz  = 0;
                        s->count++;
                    }
                }
                ret = 1;
            }
        }

        if (arg3 != NULL) {
            *arg3 = mask;
        }

        for (i = 0; i < count; i++) {
            for (j = 1; j < count; j++) {
                s->acc[2].vx = arg0[list[i]].at10.normal.vx * arg0[list[j]].at10.normal.vx;
                s->acc[2].vz = arg0[list[i]].at10.normal.vz * arg0[list[j]].at10.normal.vz;
                if (s->acc[2].vx < -0x800000 || s->acc[2].vz < -0x800000) {
                    ret = 2;
                }
            }
        }

        arg1->vx.w = s->acc[0].vx << 4;
        arg1->vy.w = s->acc[0].vy << 4;
        arg1->vz.w = s->acc[0].vz << 4;
        if (s->count != 0) {
            arg1->vx.w += (s->acc[1].vx / s->count) << 4;
            arg1->vy.w += (s->acc[1].vy / s->count) << 4;
            arg1->vz.w += (s->acc[1].vz / s->count) << 4;
        }

        SCRATCH_POP_BYTES(0x34);
        return ret;
    }
}

s32 func_800E0FEC(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3)
{
    u8*            head;
    GpPushScratch* s;
    GpRec18*       rec;
    s32            i;
    s32            j;
    s32            count;
    s32            mask;
    s32            ret;
    s32            prev;
    u8             list[0x20];

    ret   = 0;
    count = 0;
    mask  = 0;
    prev  = 0;
    if (arg2 == 0) {
        return ret;
    }

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x40;
    s                  = (GpPushScratch*)(head - 0x40);

    for (i = 0; i < 3; i++) {
        s->acc[i].vx = 0;
        s->acc[i].vy = 0;
        s->acc[i].vz = 0;
    }

    for (i = 0; i < arg2; i++) {
        rec = &arg0[i];
        if ((rec->flags & 1) && (rec->key & 0xFFFF0000) == 0x100000) {
            mask |= 1 << rec->key;
            if (Gp_RoomParams[rec->key & 7] == 0) {
                switch ((u32)(rec->key & 0xF00) >> 8) {
                    case 0:
                        s->acc[0].vx += rec->depth * rec->at10.normal.vx;
                        s->acc[0].vy += rec->depth * rec->at10.normal.vy;
                        s->acc[0].vz += rec->depth * rec->at10.normal.vz;
                        list[count++] = i;
                        break;
                    case 1:
                        s->acc[1].vx = 0;
                        s->acc[1].vy = -(rec->depth << 12);
                        s->acc[1].vz = 0;
                        break;
                    case 2:
                        if (rec->at10.normal.vy == 0 && ((s16)prev == 0 || rec->depth < (s16)prev)) {
                            s->acc[2].vx = rec->depth * rec->at10.normal.vx;
                            s->acc[2].vy = 0;
                            s->acc[2].vz = rec->depth * rec->at10.normal.vz;
                            prev         = (u16)rec->depth;
                        }
                        break;
                }
            }
            ret = 1;
        }
    }

    for (i = 0; i < count; i++) {
        for (j = 1; j < count; j++) {
            s->acc[3].vx = arg0[list[i]].at10.normal.vx * arg0[list[j]].at10.normal.vx;
            s->acc[3].vz = arg0[list[i]].at10.normal.vz * arg0[list[j]].at10.normal.vz;
            if (s->acc[3].vx < -0x800000 || s->acc[3].vz < -0x800000) {
                ret = 2;
            }
        }
    }

    if (arg3 != NULL) {
        *arg3 = mask;
    }

    if (count != 0) {
        arg1->vx.w = (s->acc[0].vx + s->acc[1].vx) << 4;
        arg1->vy.w = (s->acc[0].vy + s->acc[1].vy) << 4;
        arg1->vz.w = (s->acc[0].vz + s->acc[1].vz) << 4;
    } else {
        arg1->vx.w = (s->acc[1].vx + s->acc[2].vx) << 4;
        arg1->vy.w = (s->acc[1].vy + s->acc[2].vy) << 4;
        arg1->vz.w = (s->acc[1].vz + s->acc[2].vz) << 4;
    }

    SCRATCH_POP_BYTES(0x40);
    return ret;
}

s32 Gp_FindNearestSlot(GpObj* arg0, s32 arg1)
{
    u8*            head;
    GpNearScratch* block;
    GpActorD4Rec*  rec;
    GpRec18*       slot;
    s32            minDist;
    s32            index;
    s32            best;
    s32            dx;
    s32            dy;
    s32            dz;
    s32            dist;

    minDist            = -1;
    index              = 0;
    best               = index;
    rec                = arg0->ctx.d4rec;
    head               = SCRATCH_HEAD(u8);
    slot               = rec->recs;
    SCRATCH_HEAD(void) = (void*)(head - 0x28);
    block              = (GpNearScratch*)(head - 0x28);
    gte_SetRotMatrix(&arg0->coord->workm);
    block->local.vx = (u16)rec->end1.vx + (u16)arg0->pos.vx;
    block->local.vy = (u16)rec->end1.vy + (u16)arg0->pos.vy;
    block->local.vz = (u16)rec->end1.vz + (u16)arg0->pos.vz;
    gte_ldv0((SVECTOR*)(head - 8));
    gte_rtv0();
    gte_stlvnl(block);
    block->world.vx = ((VECTOR3*)(head - 0x28))->vx + (arg0->coord)->workm.t[0];
    block->world.vy = block->vec.vy + (arg0->coord)->workm.t[1];
    block->world.vz = block->vec.vz + (arg0->coord)->workm.t[2];

    for (;;) {
        if ((slot->flags & 1) && ((slot->key & 0xFFFF0000) == arg1)) {
            dx            = slot->point.vx - block->world.vx;
            block->vec.vx = dx;
            dy            = slot->point.vy - block->world.vy;
            block->vec.vy = dy;
            dz            = slot->point.vz - block->world.vz;
            block->vec.vz = dz;
            dist          = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
            if ((u32)dist < (u32)minDist) {
                minDist = dist;
                best    = index + 1;
            }
        }
        if (slot->flags & 2) {
            break;
        }
        slot++;
        index++;
    }

    SCRATCH_POP_BYTES(0x28);
    return best;
}

void Gp_LinkObj(s32 arg0, GpObj* arg1)
{
    u16    flags;
    GpObj* head;
    GpObj* node;
    GpObj* temp;

    head  = Gp_ObjLists[arg0];
    flags = arg1->flags;
    if (!(flags & 0x8)) {
        if ((flags & 0x7) < 5) {
            arg1->flags = flags | 0x8;
            temp        = head->next;
            if (temp != NULL) {
                node = temp;
                while (node->next != NULL) {
                    node = node->next;
                }
                node->next = arg1;
                arg1->prev = node;
            } else {
                head->next = arg1;
                arg1->prev = head;
            }
            arg1->next = NULL;
        }
    }
}

void Gp_UnlinkObj(GpObj* node)
{
    u16    flags;
    GpObj* next;
    GpObj* prev;

    flags = node->flags;
    if (flags & 0x8) {
        next        = node->next;
        node->flags = flags & 0x7;
        prev        = node->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = node->prev;
            node->next = NULL;
        } else {
            prev->next = NULL;
        }
        node->prev = NULL;
    }
}

void Gp_LinkObj4A(s32 arg0, GpObj4A* arg1)
{
    u8       flags;
    GpObj4A* head;
    GpObj4A* node;
    GpObj4A* temp;

    head  = Gp_Obj4ALists[arg0];
    flags = arg1->field_4A;
    if (!(flags & 0x20)) {
        arg1->field_4A = flags | 0x20;
        temp           = head->next;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next = arg1;
            arg1->prev = node;
        } else {
            head->next = arg1;
            arg1->prev = head;
        }
        arg1->next = NULL;
    }
}

void Gp_UnlinkObj4A(s32 arg0, GpObj4A* arg1)
{
    u8       flags;
    GpObj4A* next;
    GpObj4A* prev;

    flags = arg1->field_4A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_4A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            prev->next = NULL;
        }
        arg1->prev = NULL;
    }
}

void Gp_ClearObj4AList(s32 arg0)
{
    GpObj4A* head;
    GpObj4A* node;
    GpObj4A* next;
    GpObj4A* temp;
    s32      flags;
    s32      mask;

    head = Gp_Obj4ALists[arg0];
    temp = head->next;
    if (temp != NULL) {
        node       = temp;
        head->next = NULL;
        mask       = ~0x78;
    loop:
        flags          = node->field_4A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_4A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void Gp_LinkObj3A(s32 arg0, GpObj3A* arg1)
{
    u8       flags;
    GpObj3A* head;
    GpObj3A* node;
    GpObj3A* temp;

    head  = Gp_Obj3ALists[arg0];
    flags = arg1->field_3A;
    if (!(flags & 0x20)) {
        arg1->field_3A = flags | 0x20;
        temp           = head->next;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next = arg1;
            arg1->prev = node;
        } else {
            head->next = arg1;
            arg1->prev = head;
        }
        arg1->next = NULL;
    }
}

void Gp_UnlinkObj3A(s32 arg0, GpObj3A* arg1)
{
    u8       flags;
    GpObj3A* next;
    GpObj3A* prev;

    flags = arg1->field_3A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_3A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            prev->next = NULL;
        }
        arg1->prev = NULL;
    }
}

void Gp_ClearObj3AList(s32 arg0)
{
    GpObj3A* head;
    GpObj3A* node;
    GpObj3A* next;
    GpObj3A* temp;
    s32      flags;
    s32      mask;

    head = Gp_Obj3ALists[arg0];
    temp = head->next;
    if (temp != NULL) {
        node       = temp;
        head->next = NULL;
        mask       = ~0x78;
    loop:
        flags          = node->field_3A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_3A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void Gp_InitRec18Table(GpRec18* arg0, s32 arg1, s32 arg2)
{
    Mem_Set(arg0, 0, arg1 * 0x18);
    arg0[arg1 - 1].flags = 2;
}

void Gp_LoadRoomParams(void)
{
    s32              i;
    GameSession*     session;
    GpRoomParamRec** recs;

    for (i = 7; i >= 0; i--) {
        Gp_RoomParams[i] = 0;
    }

    session = gGameSession;
    recs    = Gp_RoomParamTables[session->at4.loc.stage - 1][session->at4.loc.area - 1];
    for (i = 0; i < 8; i++) {
        Gp_RoomParams[i] = recs[i]->field_3;
    }
}

s32 Gp_FindRec18(GpRec18* arg0, s32 arg1)
{
    s32 result;
    s32 index;

    result = 0;
    for (index = 1;; index++) {
        if (arg0->flags & 1) {
            if (arg1 == 0) {
                return 1;
            }
            if (arg0->key == arg1) {
                result = index;
            }
        }
        if ((arg0++)->flags & 2) {
            break;
        }
    }
    return result;
}

s32 Gp_CountRec18Hi(GpRec18* arg0, s32 arg1)
{
    s32 count;

    count = 0;
    do {
        if ((arg0->flags & 1) && ((arg0->key & 0xFFFF0000) == arg1)) {
            count += 1;
        }
    } while (!((arg0++)->flags & 2));
    return count;
}

void Gp_ClearRec18Occupied(GpRec18* arg0)
{
    for (;;) {
        if (arg0->flags & 1) {
            arg0->flags         &= 2;
            arg0->depth          = 0;
            arg0->key            = 0;
            arg0->point.vx       = 0;
            arg0->point.vy       = 0;
            arg0->point.vz       = 0;
            arg0->at10.normal.vx = 0;
            arg0->at10.normal.vy = 0;
            arg0->at10.normal.vz = 0;
        }
        if (arg0->flags & 2) {
            break;
        }
        arg0++;
    }
}

s32 func_800E1ACC(u8* arg0)
{
    s32 val;
    s32 ret;

    val = *arg0 << 12;
    if (val != 0) {
        ret = cln(val) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

s32 func_800E1B24(s32 arg0)
{
    s32 mask[2];
    s32 val;
    s32 tmp;
    s32 ret;

    val     = 1 << arg0;
    mask[0] = val;
    tmp     = (u8)val << 12;
    if (tmp != 0) {
        ret = cln(tmp) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

void Gp_CommitObj4CSave(void)
{
    GpObj4C* node;

    for (node = Gp_Obj4CList; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
            if ((u8)gGameSession->at4.loc.view == node->field_48) {
                Mc_SaveData.at4.loc.view = node->field_49;
            }
        }
    }
}

s32 Gp_TakePendingObj4C(u16* arg0, u8* arg1, u8* arg2)
{
    GpObj4C* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            Gp_PendingObj4CFlag = 1;
            *arg0               = node->field_46;
            *arg1               = node->field_48;
            *arg2               = node->field_49;
            return 1;
        }
    }
    return 0;
}

void Gp_ClaimSlot18(GpEnemy* arg0, s32 arg1)
{
    GpRec18*   slot;
    GpRec18*   temp;
    s32        one;
    GpStateF0* p;

    temp = arg0->recs;
    if (temp != NULL) {
        slot = temp;
        one  = 1;
        while (1) {
            if ((*(s32*)&slot->flags & 3) != one) {
                break;
            }
            slot++;
        }
        slot->key            = arg1;
        slot->depth          = 0;
        slot->point.vx       = 0;
        slot->point.vy       = 0;
        slot->point.vz       = 0;
        slot->at10.normal.vx = 0;
        slot->at10.normal.vy = 0;
        slot->at10.normal.vz = 0;
        slot->flags         |= 1;
        p                    = &Gp_StateF0;
        p->field_5++;
    }
}

void Gp_OrientAlong(VECTOR* arg0, MATRIX* arg1, s32 arg2)
{
    u8*              head;
    GpDirMatScratch* block;
    SVECTOR*         vec;
    MATRIX*          mat1;
    MATRIX*          mat2;
    s32              sin_yaw;
    s32              yaw;
    s32              pitch;

    head                          = SCRATCH_HEAD(u8);
    block                         = (GpDirMatScratch*)(head - 0x4C);
    vec                           = (SVECTOR*)block;
    SCRATCH_HEAD(GpDirMatScratch) = block;
    VectorNormalS(arg0, vec);

    mat1       = (MATRIX*)(head - 0x44);
    yaw        = ratan2(((SVECTOR*)(head - 0x4C))->vx, vec->vz) & 0xFFF;
    block->yaw = yaw;
    sin_yaw    = rsin(yaw);
    block->pitch =
        ratan2(vec->vy, (vec->vx * sin_yaw + vec->vz * rcos(block->yaw)) >> 12) & 0xFFF;

    ((SVECTOR*)(head - 0x4C))->vx = 0;
    vec->vz                       = 0;
    vec->vy                       = block->yaw;
    RotMatrix(vec, mat1);

    mat2                          = (MATRIX*)(head - 0x24);
    pitch                         = block->pitch;
    ((SVECTOR*)(head - 0x4C))->vx = -pitch;
    vec->vy                       = 0;
    vec->vz                       = 0;
    RotMatrix(vec, mat2);

    gte_SetRotMatrix(mat1);
    gte_ldclmv(mat2);
    gte_rtir();
    gte_stclmv(mat1);
    gte_ldclmv(&mat2->m[0][1]);
    gte_rtir();
    gte_stclmv(&mat1->m[0][1]);
    gte_ldclmv(&mat2->m[0][2]);
    gte_rtir();
    gte_stclmv(&mat1->m[0][2]);

    ((SVECTOR*)(head - 0x4C))->vx = 0;
    vec->vy                       = 0;
    vec->vz                       = arg2;
    RotMatrix(vec, mat2);

    gte_SetRotMatrix(mat1);
    gte_ldclmv(mat2);
    gte_rtir();
    gte_stclmv(arg1);
    gte_ldclmv(&mat2->m[0][1]);
    gte_rtir();
    gte_stclmv(&arg1->m[0][1]);
    gte_ldclmv(&mat2->m[0][2]);
    gte_rtir();
    gte_stclmv(&arg1->m[0][2]);

    SCRATCH_POP_BYTES(0x4C);
}

u32 Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3)
{
    u8  flag;
    u32 dmg;

    flag = 0;
    if ((arg0 & 0xFFFF0000) != 0x20000) {
        return 0;
    }

    if ((arg0 & 0x8000) == 0) {
        u8  lo;
        u32 base;
        u32 raw;
        u32 rand;
        s32 pct;
        u8  col;
        s32 sel;
        s32 val;
        s32 mult;
        u32 tmp;
        s32 extra;

        if ((arg0 & 0x80) == 0) {
            if ((arg0 & 0x7F) < 0x21) {
                flag = 1;
            }
        }
        lo   = arg0 & 0x7F;
        arg0 = (arg0 >> 8) & 0x3F;
        raw  = Gp_IdParamLo[lo].field_0;
        base = raw << 8;
        if (flag != 0) {
            if ((Player_Status.peStateFlags & 0x80) != 0) {
                base = base * 150 / 100;
            }
        }

        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        rand        = (u32)Gp_LcgState >> 16;
        pct         = (u16)(rand % 20) + 100;
        base        = base * pct / 100;

        col = arg1 / 1000;
        if (col < 0x10) {
            sel = (u8)D_80113864[col];
        } else {
            sel = 5;
        }
        val = (D_80113568[arg0][sel] << 8) / 100;

        if (arg2 == 0) {
            mult = 0x100;
        } else if (Gp_IdParamLo[lo].field_4 == arg2) {
            mult = arg3;
        } else {
            mult = 0x100;
        }

        tmp = base * val >> 8;
        dmg = tmp * mult >> 16;

        if (flag != 0) {
            extra = Gp_StateC08.field_D;
            if (extra != 0) {
                dmg = dmg * D_80113D0C[(extra / 16 - 1) * 2 + (s8)(extra % 16)][0] / 100;
            }
            if (func_800B9D80(0x10000) != 0) {
                dmg = dmg * 120 / 100;
            }
        }

        dmg = dmg * D_80113F90[Gp_StateF0.field_2B] / 100;
        if (dmg == 0) {
            if (base != 0) {
                dmg = 1;
            }
        }
    } else {
        u32 rnd;
        s32 pc;

        dmg = Gp_IdParamHi[arg0 & 0x7F].field[4];
        if ((arg0 & 0x7F) >= 0x19 && (arg0 & 0x7F) < 0x1C) {
            if (Gp_StateF0.field_5 != 0) {
                dmg = dmg / Gp_StateF0.field_5;
            } else {
                dmg = 0;
            }
        }

        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        rnd         = (u32)Gp_LcgState >> 16;
        pc          = (u16)(rnd % 10) + 100;
        dmg         = dmg * pc / 100;

        if (func_800B9D80(0x20000) != 0) {
            dmg = dmg * 150 / 100;
        }

        dmg = dmg * D_80113F90[Gp_StateF0.field_2B] / 100;
    }
    return dmg;
}

s32 Gp_ScaleDamage(s32 arg0, s32 arg1, s32* arg2, s32 arg3)
{
    u32 ret;
    s32 lo;
    u32 val;
    s32 extra;
    s32 hp;
    u16 col;

    if ((arg0 & 0xFFFF0000) != 0x40000) {
        return 0;
    }

    lo = arg0 & 0xFFF;
    if (arg2 != NULL) {
        *arg2 = ((u32)arg0 >> 12) & 0xF;
    }

    if (arg3 == 0) {
        hp    = Player_Status.hp;
        col   = D_80113F54[hp / 10];
        val   = Gp_DmgRows[Gp_StateF0.field_2B].field_A[col] << 8;
        extra = Gp_StateC08.field_C;
        if (extra != 0) {
            val = val * D_80113CFC[(extra / 16 - 1) * 2 + (s8)(extra % 16)] / 100;
        }
    } else {
        hp  = Mc_SaveData.companionHp;
        col = D_80113F54[hp / 10];
        val = Gp_DmgRows[Gp_StateF0.field_2B].field_0[col] << 8;
    }

    val = val / 100;
    ret = lo * val >> 8;
    if (ret == 0) {
        if (lo != 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_RollEnemyChance(GpEnemy* arg0, u32 arg1, s32 arg2)
{
    Task*          slot;
    GpCoord*       pcoord;
    u8*            head;
    GpDistScratch* blk;
    s32            dist;
    u16            sel;
    s32            kind;
    s32            col;
    s32            val;
    s32            chance;
    u16            base;
    s32            extra;
    s32            rand;

    slot = gameGetPtrSlot(3);
    if (slot == NULL) {
        return 0;
    }
    if ((arg1 & 0x8000) != 0) {
        return 0;
    }

    base = (arg0->param->critChance << 12) / 100;
    if (base == 0) {
        return 0;
    }

    head                        = SCRATCH_HEAD(u8);
    blk                         = (GpDistScratch*)(head - 0x20);
    SCRATCH_HEAD(GpDistScratch) = blk;
    Gp_UpdateCoord(arg0->coord);

    ((VECTOR3*)(head - 0x20))->vx = arg0->bodyPos.vx;
    blk->local.vy                 = arg0->bodyPos.vy;
    blk->local.vz                 = arg0->bodyPos.vz;

    gte_SetRotMatrix(&arg0->coord->workm);
    gte_ldv0(&blk->local);
    gte_rtv0();
    gte_stlvnl(head - 0x10);

    blk->world.vx = arg0->coord->workm.t[0] + blk->world.vx;
    blk->world.vy = arg0->coord->workm.t[1] + blk->world.vy;
    blk->world.vz = arg0->coord->workm.t[2] + blk->world.vz;

    pcoord                        = slot->extra.tmd->coords;
    ((VECTOR3*)(head - 0x20))->vx = blk->world.vx - pcoord->workm.t[0];
    blk->local.vy                 = blk->world.vy - pcoord->workm.t[1];
    blk->local.vz                 = blk->world.vz - pcoord->workm.t[2];

    dist = SquareRoot0(((VECTOR3*)(head - 0x20))->vx * ((VECTOR3*)(head - 0x20))->vx +
                       blk->local.vy * blk->local.vy + blk->local.vz * blk->local.vz);

    sel = dist / 1000;
    sel = sel < 0x10 ? D_80113864[sel] : 5;

    kind = (arg1 >> 8) & 0x3F;
    if (Gp_IdParamLo[arg1 & 0x7F].field_4 == 6) {
        val = (D_80113858[sel] << 12) / 100;
    } else {
        val = (D_80113568[kind][sel] << 12) / 100;
    }

    if ((arg1 & 0x4000) != 0) {
        col = 7;
    } else {
        col = 6;
    }

    chance = (((D_80113568[kind][col] << 12) / 100) * base >> 12) * val >> 12;
    if ((arg0->reactionFlags & 2) != 0) {
        chance <<= 1;
    }

    extra = Gp_StateC08.field_D;
    if (extra != 0) {
        chance = chance * D_80113D0C[(extra / 16 - 1) * 2 + (s8)(extra % 16)][1] / 100;
    }
    if (arg2 != 0) {
        chance *= arg2;
    }

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    rand        = (u32)Gp_LcgState >> 16 & 0xFFF;
    SCRATCH_POP_BYTES(0x20);
    return rand < chance;
}

void Gp_ApplyObjKind(GpEnemy* arg0, s32 arg1)
{
    u16 raw;
    s32 kind;
    s32 val;
    s32 limit;
    s32 rand;

    if ((arg1 & 0x8000) == 0) {
        raw = Gp_IdParamLo[arg1 & 0x7F].field_4;
        TOUCH_REG(raw);
        kind = raw;
    } else {
        raw = Gp_IdParamHi[arg1 & 0x7F].field[5];
        TOUCH_REG(raw);
        kind = raw;
    }

    switch (kind) {
        case 0:
            break;
        case 1:
            arg0->reactionFlags |= 1;
            break;
        case 2:
            arg0->flag2Steps     = 0;
            arg0->flag2Timer     = 0;
            arg0->reactionFlags |= 2;
            if ((arg1 & 0x8000) == 0) {
                arg0->flag2Grade = 0;
                return;
            }
            if ((arg1 & 0x3F) == 0x31) {
                arg0->flag2Grade = 0;
                return;
            }
            arg0->flag2Grade = Gp_StateC08.field_0 % 10U;
            break;
        case 3:
            val         = arg0->param->flag4Chance;
            limit       = (val << 12) / 100;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            rand        = (u32)Gp_LcgState >> 16 & 0xFFF;
            if (rand < limit) {
                arg0->flag4Ticks     = 0;
                arg0->reactionFlags |= 4;
                Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
                arg0->flag4Delay     = ((u32)Gp_LcgState >> 16 & 0xF) + 0x53;
                if ((arg1 & 0x8000) == 0) {
                    arg0->flag4Grade = 0;
                    return;
                }
                arg0->flag4Grade = Gp_StateC08.field_0 % 10U;
            }
            break;
    }
}

s32 Gp_PackObjPair(GpEnemy* arg0, s32 arg1)
{
    GpU16Pair* pairs;
    s32        ret;

    if (arg0->param == NULL) {
        return 0;
    }
    pairs = arg0->param->pairTable;
    ret   = pairs[arg1].field_0 & 0xFFF;
    ret  |= (pairs[arg1].field_2 & 0xF) << 12;
    ret  |= 0x40000;
    return ret;
}

s32 Gp_PackPair(GpU16Pair* pairs, s32 index)
{
    s32 ret;

    if (pairs == NULL) {
        return 0;
    }
    ret  = pairs[index].field_0 & 0xFFF;
    ret |= (pairs[index].field_2 & 0xF) << 12;
    ret |= 0x40000;
    return ret;
}

void func_800E2C78(GpEnemy* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 val;

    if ((u32)((arg1 & 0x7F) - 0x19) < 3U) {
        val = arg0->hp;
        if ((u32)val < (u32)arg2) {
            Gp_StateF0.field_14 += val;
            return;
        }
        Gp_StateF0.field_14 += arg2;
    }
}

s32 Gp_LookupIdField(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    switch (arg1) {
        case 0:
            ret = Gp_IdField0[(u16)arg0].field_0;
            break;
        case 1:
            ret = Gp_IdField1[(u16)arg0].field_0;
            break;
    }
    return ret;
}

s32 Gp_GetIdParam0(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].field_4;
    } else {
        ret = Gp_IdParamHi[arg0 & 0x7F].field[5];
    }
    return ret;
}

s32 Gp_GetIdParam1(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].field_6;
    } else {
        ret = Gp_IdParamHi[arg0 & 0x7F].field[6];
    }
    return ret;
}

void Gp_SetObjFlag4(GpEnemy* arg0, s32 arg1, s32 arg2)
{
    s32 val;
    s32 limit;
    s32 rand;

    val         = arg0->param->flag4Chance;
    limit       = (val << 12) / 100;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    rand        = (u32)Gp_LcgState >> 16 & 0xFFF;
    if (rand < limit) {
        arg0->flag4Ticks     = 0;
        arg0->reactionFlags |= 4;
        Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
        arg0->flag4Delay     = ((u32)Gp_LcgState >> 16 & 0xF) + 0x53;
        if ((arg1 & 0x8000) == 0) {
            arg0->flag4Grade = 0;
            return;
        }
        arg0->flag4Grade = Gp_StateC08.field_0 % 10U;
    }
}

s32 Gp_TickObjFlag4(GpEnemy* arg0)
{
    s32 ret;
    s32 val;
    s32 scale;

    ret = 0;
    arg0->flag4Delay--;
    if (arg0->flag4Delay == 0) {
        arg0->flag4Ticks++;
        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
        arg0->flag4Delay = ((u32)Gp_LcgState >> 16 & 0xF) + 0x53;
        val              = arg0->param->hpMax;
        scale            = D_80113D38[arg0->flag4Grade];
        ret              = (val * scale) / 100;
        if (ret == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_ObjFlag4Expired(GpEnemy* arg0)
{
    s32          val;
    s32          ret;
    register s32 scale asm("v1");

    ret = 0;
    val = arg0->param->flag4Ticks;
    if (!(arg0->reactionFlags & 4)) {
        return 1;
    }
    if (val == 0) {
        return 0;
    }
    scale = D_80113D28[arg0->flag4Grade];
    if (arg0->flag4Ticks >= (val * scale) / 100) {
        ret = 1;
    }
    return ret;
}

void Gp_SetObjFlag1(GpEnemy* arg0)
{
    arg0->reactionFlags |= 1;
}

void Gp_SetObjFlag2(GpEnemy* arg0, s32 arg1, s32 arg2)
{
    arg0->flag2Steps     = 0;
    arg0->flag2Timer     = 0;
    arg0->reactionFlags |= 2;
    if ((arg1 & 0x8000) == 0) {
        arg0->flag2Grade = 0;
        return;
    }
    if ((arg1 & 0x3F) == 0x31) {
        arg0->flag2Grade = 0;
        return;
    }
    arg0->flag2Grade = Gp_StateC08.field_0 % 10U;
}

s32 Gp_TickObjFlag2(GpEnemy* arg0)
{
    s32 ret;
    s32 limit;
    s32 val;
    s32 scale;

    ret = 0;
    val = arg0->param->flag2Ticks;
    if (val == 0) {
        return ret;
    }
    scale = D_80113D30[arg0->flag2Grade];
    limit = (val * scale) / 100;
    if (arg0->flag2Steps < limit) {
        arg0->flag2Timer++;
        if (arg0->flag2Timer >= 0x1F) {
            arg0->flag2Steps++;
            if (arg0->flag2Steps >= limit) {
                Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                arg0->flag2Timer = (u32)Gp_LcgState >> 16 & 0x3F;
            } else {
                arg0->flag2Timer = 0;
            }
        }
    } else {
        arg0->flag2Timer--;
        if (arg0->flag2Timer == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_GetIdParam2(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].field_8;
    } else {
        ret = Gp_IdParamHi[arg0 & 0x7F].field[7];
    }
    return ret;
}

void func_800E31E8(Task* arg0)
{
    s32         flag;
    s32         index;
    s32         area;
    s32         room;
    s32         base;
    s32         kind;
    GpTaskDesc* table;
    GpTaskDesc* desc;
    Task*       slotTask;
    s32         slot;

    gGameSession->eventState = 0;
    gGameSession->hideHud    = 0;
    D_80115598               = 0;
    gGameSession->flowFlags  = 0;
    flag                     = GameFlag_GetNibble(0x11F);
    switch (flag) {
        case 1:
            if (gGameSession->at4.loc.stage == 3) {
                gStageSceneMusicEntry = 1;
            } else {
                gGameSession->flowFlags = 3;
            }
            break;
        case 2:
            gStageSceneMusicEntry = 9;
            break;
    }
    index = 0;
    base  = gGameSession->at4.loc.stage * 10000 + gGameSession->at4.loc.area * 100;
    room  = base + gGameSession->at4.loc.room;
    table = D_8010FABC[gGameSession->at4.loc.stage];
    area  = base;
    desc  = table;
    kind  = 0x200000;
loop:
    if (desc->flagsAndPriority == kind &&
        (desc->task.arg.value == room || desc->task.arg.value == area)) {
        Task_SpawnFromTable(&table->task, index, 0, 0);
        goto done;
    }
    if ((u16)(desc++)->flagsAndPriority != 0xFFFF) {
        index++;
        goto loop;
    }
    slotTask = arg0;
    TOUCH_REG(slotTask);
    slot = 7;
    TOUCH_REG(slot);
    arg0->msgTable = D_8010FAD4;
    Game_SetPtrSlot(slotTask, slot);
done:
    arg0->state++;
}

void Gp_EvtCapTask(Task* arg0)
{
    s32 flags;
    s32 bit0;
    s32 mode;
    s32 flag;

    flag  = 1;
    flags = arg0->spawnArg1;
    switch (arg0->state) {
        case 0:
            bit0 = flags & 1;
            if (bit0 != 0) {
                Gp_MsgPlayerWeapon(0);
                Gp_StateF0.field_4 = flag;
            }
            if (flags & 2) {
                Gp_MsgPlayer3F3(0);
            }
            if (flags & 4) {
                mode = 2;
            } else if (bit0 == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            Gp_RunCapCmd((s32)arg0->spawnArg2, mode);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (flags & 1) {
                Gp_MsgPlayerWeapon(1);
                Gp_StateF0.field_4 = 0;
            }
            if (flags & 2) {
                Gp_MsgPlayer3F3(1);
            }
            if (D_80115598 != 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F2, (s32)arg0->spawnArg2 + 0x64, 0);
            }
            taskKill(arg0);
            break;
    }
}
