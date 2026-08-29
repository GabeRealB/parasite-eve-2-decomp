#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/4CC.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern s32          Pad_MaskConfirm;
extern s32          Pad_MaskCancel;
extern s32          Pad_MaskMenu;
extern char         Gp_StrSwitch[];
extern UiObject*    D_80067634;
extern u8           D_8010D828[];
extern GpItemRec*   Gp_SelItemRec;
extern UiObjectDesc D_8010EFA0;

void Gp_ClearPreviewItems(void);

void Gp_ItemMoveChild(UiObject* arg0, Task* arg1)
{
    GpItemMoveState* mem;
    UiObject*        obj;
    GpItemRec*       tbl;
    GpItemScan*      scanSrc;
    s32              i;
    s32              base;
    s32              flag;
    s32              val;
    GpItemScan*      dst;
    GpItemScan*      src;
    GpItemRec*       recDst;
    GpItemRec*       recSrc;
    s32              rowDst;
    s32              rowSrc;
    s32              idDst;
    s32              idSrc;
    s32              qtyDst;
    s32              qtySrc;
    GpItemScan*      scan;
    GpItemRec*       recA;
    GpItemRec*       recB;
    s32              rowA;
    s32              rowB;
    s32              idA;
    s32              idB;
    s32              qtyA;
    s32              qtyB;
    s32              subA;
    s32              subB;

    obj = arg1->parent->spawnArg2;
    mem = (GpItemMoveState*)arg1->parent->idMap;
    switch (arg0->field_2E) {
        case 0x26:
            scanSrc = &Gp_MoveScanSrc;
            tbl     = Gp_GetItemTable(scanSrc);
            base    = scanSrc->field_0;
            i       = 0;
            if (scanSrc->field_1 != 0) {
                do {
                    if (tbl[base].field_0 != 0) {
                        Gp_GiveItem(&Gp_MoveScanDst, tbl[base].field_0, tbl[base].field_2);
                        tbl[base].field_0 = 0;
                        tbl[base].field_2 = 0;
                    }
                    i++;
                    base++;
                } while (i < scanSrc->field_1);
            }
            /* fallthrough */
        case -1:
            flag = 0;
            if (mem->objs[0]->owner->flags != 0) {
                flag = Gp_CountScanItems(&Gp_MoveScanSrc) > 0;
            }
            mem->objs[mem->field_8]->owner->state     = 1;
            mem->objs[mem->field_8 ^ 1]->owner->state = 1;
            if ((arg0->field_2E != 0x26) && flag) {
                val = Gp_CanMoveItems();
                SndEvt_EnqueueType6(4, 0, 0);
                mem->field_8 = 0;
                Ui_SpawnFromDesc(&D_8010D7F0, val, 1, 1, mem->objs[0]);
                break;
            }
            /* fallthrough */
        case 0x27:
            Game_Session->field_2 = 0;
            obj->field_2E         = -1;
            break;
        case 6:
            Ui_TeardownTree(arg0, arg1);
            mem->objs[mem->field_8]->owner->state     = 1;
            mem->objs[mem->field_8 ^ 1]->owner->state = 1;
            mem->field_8                              = mem->field_8 ^ 1;
            mem->objs[mem->field_8]->status           = 0;
            mem->field_8                              = mem->field_8 ^ 1;
            mem->objs[mem->field_8]->status           = 1;
            break;
        case 0x23:
            mem->field_10                             = mem->field_8;
            mem->field_14                             = Gp_InvLists[mem->field_8].field_10;
            mem->objs[mem->field_8]->owner->state     = 2;
            mem->objs[mem->field_8 ^ 1]->owner->state = 2;
            mem->objs[mem->field_8]->status           = 0;
            mem->field_8                              = mem->field_8 ^ 1;
            mem->objs[mem->field_8]->status           = 1;
            break;
        case 0x25:
            if (mem->field_10 != mem->field_8) {
                if (mem->field_8 == 0) {
                    rowSrc = mem->field_18;
                    rowDst = mem->field_14;
                } else {
                    rowSrc = mem->field_14;
                    rowDst = mem->field_18;
                }
                dst    = &Gp_MoveScanDst;
                recDst = Gp_GetScanSlot(dst, rowDst, 0);
                qtyDst = recDst->field_2;
                idDst  = recDst->field_0;
                Gp_RemoveItem(dst, recDst, qtyDst);
                src    = dst - 1;
                recSrc = Gp_GetScanSlot(src, rowSrc, 0);
                qtySrc = recSrc->field_2;
                idSrc  = recSrc->field_0;
                Gp_RemoveItem(src, recSrc, qtySrc);
                Gp_SetScanItem(dst, rowDst, idSrc, qtySrc);
                Gp_SetScanItem(src, rowSrc, idDst, qtyDst);
                if ((u8)(recDst->field_0 + 0x80) < 0x20) {
                    Gp_ClearEquipSlot(recDst->field_0);
                }
            } else {
                rowA = mem->field_14;
                rowB = mem->field_18;
                if (rowA != rowB) {
                    scan = &Gp_MoveScanSrc + mem->field_10;
                    recA = Gp_GetScanSlot(scan, rowA, 0);
                    qtyA = recA->field_2;
                    idA  = recA->field_0;
                    subA = (s8)recA->field_1;
                    Gp_RemoveItem(scan, recA, qtyA);
                    recB = Gp_GetScanSlot(scan, rowB, 0);
                    qtyB = recB->field_2;
                    idB  = recB->field_0;
                    subB = (s8)recB->field_1;
                    Gp_RemoveItem(scan, recB, qtyB);
                    Gp_SetScanItem(scan, rowA, idB, qtyB)->field_1 = subB;
                    Gp_SetScanItem(scan, rowB, idA, qtyA)->field_1 = subA;
                }
            }
            mem->objs[mem->field_8]->owner->state     = 1;
            mem->objs[mem->field_8 ^ 1]->owner->state = 1;
            break;
        case 0x24:
            mem->objs[mem->field_8]->owner->state     = 1;
            mem->objs[mem->field_8 ^ 1]->owner->state = 1;
            if (mem->field_10 != mem->field_8) {
                mem->objs[mem->field_8]->status = 0;
                mem->field_8                    = mem->field_8 ^ 1;
                mem->objs[mem->field_8]->status = 1;
            }
            break;
        case 0xA:
            mem->objs[mem->field_8]->status = 0;
            mem->field_8                    = mem->field_8 ^ 1;
            mem->objs[mem->field_8]->status = 1;
            break;
    }
}

/* Kept next to Gp_ItemMoveChild's jump table so the overlay .rodata stays packed;
   Gp_StrBullet follows from func_800BDF6C's still-assembled .rodata. */
const char Gp_StrBattleField[] = "Battle Field";
const char Gp_StrItemBox[]     = "Item Box";
const char Gp_StrPlayerItem[]  = "Player Item";

void Gp_ItemMoveTask(Task* arg0)
{
    UiObject*        obj;
    GpItemMoveState* mem;
    s32              i;
    GpItemScan*      src;
    GpItemScan**     scans;
    u16              item;
    s32              val;
    register s32     code asm("v0");
    Task*            owner;
    Task*            child;
    Task*            next;
    Task*            head;
    void             (*cb)(UiObject*, Task*);

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        Wip_UiHolder = NULL;
        D_80067634   = NULL;
        Gp_ClearPreviewItems();
        mem = Mem_Calloc(0x1C, 0);
        i   = 0;
        if (mem == NULL) {
            code          = -1;
            obj->field_2E = code;
            code          = 0x34;
            obj->status   = 0;
            goto end;
        }
        scans           = Gp_ScanPtrs;
        arg0->idMap     = (TaskIdMap*)mem;
        Gp_ItemMoveWork = mem;
        mem->field_C    = 0;
        do {
            if (i == 0) {
                item           = Gp_PubItemLoc;
                src            = scans[item & 0xFF];
                Gp_MoveItemKey = item;
            } else {
                src = &Mc_SaveData.field_5BC;
            }
            (&Gp_MoveScanSrc)[i] = *src;
            i++;
        } while (i < 2);
        Gp_SortItems(&Gp_MoveScanSrc, 0);
        if (arg0->spawnArg1 == 1) {
            val          = Gp_CanMoveItems();
            mem->field_8 = 0;
            mem->objs[0] = Ui_SpawnFromDesc(D_8010D6F4, 0x100, 0, 1, obj);
            mem->objs[1] = Ui_SpawnFromDesc(D_8010D6F4 + 1, 0x101, 0, 1, obj);
            Ui_SpawnFromDesc(D_8010D6F4 + 9, val, 1, 1, mem->objs[0]);
        } else {
            mem->field_8 = 0;
            mem->objs[0] = Ui_SpawnFromDesc(D_8010D6F4, 0, 1, 1, obj);
            mem->objs[1] = Ui_SpawnFromDesc(D_8010D6F4 + 1, 1, 0, 1, obj);
            obj->status  = 0;
        }
        Ui_SpawnFromDesc(&D_8010D80C, 0, 0, 1, obj);
        Game_Session->field_2 = 1;
        arg0->state           = arg0->state + 1;
    }

    cb    = Gp_ItemMoveChild;
    owner = obj->owner;
    child = owner->firstChild;
    if (child != NULL) {
        do {
            next = child->nextSibling;
            cb(child->spawnArg2, child);
            head  = owner->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
    code = 0x34;
end:
    obj->field_2C = code;
}

void Gp_ItemMoveRow(DialogPrompt* arg0, UiObject* arg1)
{
    register GpItemRec* rec asm("s1");
    register s32        item asm("s0");
    register Task*      owner asm("a1");
    s32                 item2;
    s32                 status;
    s32                 one;
    s32                 selected;
    s32                 flag;
    s32                 flags;
    s32                 idx;
    UiObject*           spawned;
    WipSysConfig*       cfg;

    rec = Gp_GetScanSlot(&Gp_MoveScanSrc + arg1->owner->spawnArg1, arg0->field_8, 0);
    TOUCH_REG(rec);
    one  = 1;
    item = rec->field_0;
    if (arg0->field_C != one) {
        owner = arg1->owner;
        if ((owner->state != one) && (arg0->field_8 == Gp_ItemMoveWork->field_14) &&
            (owner->spawnArg1 == Gp_ItemMoveWork->field_10)) {
            arg0->field_1C = 0x37A78;
        }
    }
    status = arg1->status;
    {
        register s32 one2 asm("a0");
        one2 = 1;
        if (((status >> 16) == one2) || (status == one2)) {
            if (arg0->field_10 == arg0->field_8) {
                Gp_SetPreviewItem(item, 0);
                Gp_SetHolderItemText(item);
            }
        }
    }
    if (arg1->owner->spawnArg1 == 0) {
        Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 0);
    } else if ((s8)rec->field_1 <= 0) {
        Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 1);
    } else {
        Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 2);
    }
    if ((u32)(item - 0xA0) < 0x20U) {
        Gp_DrawQty(arg1, arg0->field_18, arg0->field_1A, rec->field_2, arg0->field_1C);
    }
    selected = arg0->field_C;
    if (selected == 1) {
        Gp_SelItemRec = rec;
        if (arg1->owner->state == selected) {
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
                SndEvt_EnqueueType6(3, 0, 0);
                spawned = Ui_SpawnFromDesc(&D_8010D764, arg1->owner->spawnArg1, 1, 1, arg1);
                if (spawned != NULL) {
                    spawned->field_C = arg1->baseX + (u16)arg1->field_1C + 0x14;
                    spawned->field_E = (arg1->baseY + (u16)arg0->field_1A) - 0x14;
                    arg1->status     = 0;
                }
            } else if ((Pad_CheckButtons(0, 1, 0x10) != 0) && (item != 0)) {
                SndEvt_EnqueueType6(3, 0, 0);
                Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
                arg1->status = 0;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            idx   = arg1->owner->spawnArg1;
            item2 = Gp_GetScanSlot(&Gp_MoveScanSrc + idx, Gp_InvLists[idx].field_10, 0)->field_0;
            SndEvt_EnqueueType6(3, 0, 0);
            owner = arg1->owner;
            item  = -1;
            if (Gp_ItemMoveWork->field_10 != owner->spawnArg1) {
                flags = owner->flags;
                flag  = 0;
                if (Gp_ItemDescs[item2].field_3 & 1) {
                    flag = flags == 1;
                }
                if ((Gp_MoveItemKey == 0x703) && (item2 == 0x81) && (Mc_SaveData.field_7 == selected)) {
                    flag = 1;
                }
                if (flag) {
                    item = 0x20;
                } else if (((u32)(item2 - 0xA0) < 0x20U) && (arg1->owner->flags == 0)) {
                    item = 8;
                } else if (arg1->owner->spawnArg1 == 1) {
                    cfg = &Wip_SysConfig;
                    if ((item2 == cfg->field_21 + 0x7F) || (item2 == cfg->field_23 + 0x5F)) {
                        item = 0xA;
                    }
                }
            }
            if (item >= 0) {
                Gp_SpawnItemPrompt(arg1, item, 0, 1);
                arg1->status = 0;
            } else {
                Gp_ItemMoveWork->field_18 = arg0->field_8;
                arg1->field_2E            = 0x25;
            }
        }
    }
}

void Gp_ItemPaneTask(Task* arg0)
{
    UiObject*   obj;
    UiList*     menu;
    GpItemScan* scan;
    s32         count;
    s32         n;
    s32         status;
    Task*       owner;
    Task*       child;
    Task*       next;
    Task*       head;
    void        (*cb)(UiObject*, Task*);

    menu          = &Gp_InvLists[(u8)arg0->spawnArg1];
    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        if (arg0->spawnArg1 >= 0x100) {
            arg0->spawnArg1 = arg0->spawnArg1 & 0xFF;
            arg0->flags     = 1;
        } else {
            arg0->flags = 0;
        }
        {
            register s32         val asm("v0");
            register GpItemScan* s asm("v0");

            s             = &Gp_MoveScanSrc;
            val           = s[arg0->spawnArg1].field_1;
            menu->field_4 = val;
            menu->field_5 = val;
            if ((s8)val >= 0xB) {
                menu->field_5 = 0xA;
            }
        }
        menu->field_10 = 0;
        menu->field_9  = 0;
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        menu->field_A = 1;
        arg0->state   = arg0->state + 1;
    }

    if (arg0->spawnArg1 == 0) {
        if (arg0->flags == 1) {
            Ui_DrawText((UiPanel*)obj, Gp_StrBattleField);
        } else {
            Ui_DrawText((UiPanel*)obj, Gp_StrItemBox);
        }
    } else {
        Ui_DrawText((UiPanel*)obj, Gp_StrPlayerItem);
    }
    Ui_ComputeVisibleRows(menu, (s32)obj);
    menu->field_A = 1;
    if (menu->field_10 >= menu->field_4) {
        menu->field_10 = menu->field_4 - 1;
    }
    n = menu->field_4;
    if ((s8)menu->field_5 >= n) {
        menu->field_9 = 0;
    }
    if (menu->field_4 != 0) {
        Ui_UpdateListNoAnim(menu, obj);
    }

    scan  = &Gp_MoveScanSrc + arg0->spawnArg1;
    count = scan->field_1;
    count = count < Gp_CountScanItems(scan);
    if (count != 0) {
        obj->field_4 |= 0x20000;
    } else {
        obj->field_4 &= ~0x20000;
    }

    status = obj->status;
    if (status == 1) {
        if (menu->field_4 == 0) {
            Ui_SmoothCursor((UiMiniObj*)obj, obj->field_1C + 4, (s16)obj->field_18 + 0xA);
        }
        if (arg0->state == status) {
            if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
                obj->status   = 0;
                obj->field_2E = -1;
            } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
                obj->status   = 0;
                obj->field_2E = -1;
            } else if (Pad_CheckButtons(0, 0, 0x5000) == 0) {
                if (arg0->spawnArg1 == 0) {
                    if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
                        goto do_snd;
                    }
                }
                if (arg0->spawnArg1 == status) {
                    if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
                        goto do_snd;
                    }
                }
                if (Pad_CheckButtons(0, 1, 3) == 0) {
                    goto children;
                }
            do_snd:
                SndEvt_EnqueueType6(2, 0, 0);
                obj->field_2E = 0xA;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
            obj->field_2E = 0x24;
        } else if (Pad_CheckButtons(0, 0, 0x5000) == 0) {
            if (arg0->spawnArg1 == 0) {
                if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
                    obj->field_2E = 0xA;
                    goto children;
                }
            }
            if (arg0->spawnArg1 == status) {
                if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
                    obj->field_2E = 0xA;
                    goto children;
                }
            }
            if (Pad_CheckButtons(0, 1, 3) != 0) {
                obj->field_2E = 0xA;
            }
        }
    }

children:
    cb    = Gp_CloseItemPane;
    owner = obj->owner;
    child = owner->firstChild;
    if (child != NULL) {
        do {
            next = child->nextSibling;
            cb(child->spawnArg2, child);
            head  = owner->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BD6DC);

void Gp_ItemActionConfirm(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq   req;
    s32           selected;
    s32           idx;
    GpItemRec*    rec;
    s32           item;
    s32           flag;
    s32           flags;
    Task*         owner;
    WipSysConfig* cfg;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrSwitch);

    selected = arg0->field_C;
    if (selected == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            idx  = arg1->owner->spawnArg1;
            rec  = Gp_GetScanSlot(&Gp_MoveScanSrc + idx, Gp_InvLists[idx].field_10, 0);
            item = rec->field_0;
            SndEvt_EnqueueType6(3, 0, 0);

            owner = arg1->owner;
            flags = owner->parent->flags;
            flag  = 0;
            if (Gp_ItemDescs[item].field_3 & 1) {
                flag = flags == 1;
            }
            if ((Gp_MoveItemKey == 0x703) && (item == 0x81) && (Mc_SaveData.field_7 == selected)) {
                flag = 1;
            }
            if (flag) {
                Gp_SpawnItemPrompt(arg1, 0x1E, 0, 0);
                arg1->status = 0;
            } else if (arg1->owner->spawnArg1 == 1) {
                cfg = &Wip_SysConfig;
                if ((item == cfg->field_21 + 0x7F) || (item == cfg->field_23 + 0x5F)) {
                    Gp_SpawnItemPrompt(arg1, 7, 0, 0);
                    arg1->status = 0;
                } else {
                    arg1->field_2E = 0x23;
                }
            } else {
                arg1->field_2E = 0x23;
            }
        }
    }
}

void Gp_FillItemActions(UiList* arg0, UiObject* arg1)
{
    GpItemRec*      rec;
    s32             item;
    s32             count;
    s32             idx;
    UiListItemFunc* table;
    Task*           owner;
    GpItemScan*     scan;

    owner = arg1->owner;
    idx   = owner->spawnArg1;
    scan  = &Gp_MoveScanSrc + idx;
    rec   = Gp_GetScanSlot(scan, Gp_InvLists[idx].field_10, 0);
    item  = 0;
    if (rec != NULL) {
        item = rec->field_0;
    }
    if (item == 0) {
        Gp_ItemActionFns[0] = Gp_ItemActionConfirm;
        count               = 1;
    } else {
        Gp_ItemActionFns[0] = func_800BD6DC;
        table               = Gp_ItemActionFns;
        count               = 1;
        if (((u32)(item - 0xA0) >= 0x20U) || (arg1->owner->flags != 0)) {
            table[1] = Gp_ItemActionConfirm;
            count    = 2;
        }
        if (((u32)(item - 1) < 3U) || (item == 5) || (item == 6) || (item == 7) || (item == 0x3C) || (item == 0x3D)) {
            Gp_ItemActionFns[count] = Gp_DrawUsePrompt;
            count                   = count + 1;
        }
    }
    arg0->field_5 = count;
    arg0->field_4 = count;
}

void Gp_ItemActionListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    UiObject* child;
    s32       flag;
    Task*     parent;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    menu          = &Gp_ItemActionList;
    if (arg0->state == 0) {
        parent = arg0->parent;
        if (parent != NULL) {
            arg0->flags = parent->flags;
        }
        if (((s16)obj->field_E + (s16)obj->field_12) >= 0x65) {
            obj->field_E = 0x64 - obj->field_12;
        }
        Gp_FillItemActions(menu, obj);
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            if (obj->owner->flags != 0) {
                obj->field_2E = 6;
            } else {
                obj->status   = 0;
                obj->field_2E = -1;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            obj->field_2E = 6;
        }
    }
    child = (UiObject*)arg0->firstChild;
    if (child != NULL) {
        child = ((Task*)child)->spawnArg2;
        flag  = child->field_2E;
        switch (flag) {
            case -1:
                obj->status   = 0;
                obj->field_2E = flag;
                break;
            case 9:
                obj->field_2E = 6;
                break;
            case 6:
                obj->status = 1;
                Ui_TeardownTree(child, child->owner);
                break;
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BDF6C);

/* After Gp_StrBullet from func_800BDF6C so the overlay .rodata stays packed. */
const GpPromptTexts Gp_ItemPromptTexts = { Gp_StrAll, Gp_StrSelect, Gp_StrDiscard, Gp_StrEnd };
const VECTOR        D_80093DB0         = { 0, -100, 0, 0 };

void Gp_ItemMenuPrompt(DialogPrompt* arg0, UiObject* arg1)
{
    GpPromptTexts        texts;
    s32                  one;
    register s32         i asm("s2");
    register GpItemScan* scan asm("s3");
    GpItemRec*           rec;
    GpItemSlot*          slot;
    s32                  attach;

    texts = Gp_ItemPromptTexts;
    if (arg0->field_8 == 0) {
        if (arg1->owner->spawnArg1 == 0) {
            arg0->field_1C = Ui_LookupTable(arg1, 2);
            if (arg0->field_C == 1) {
                arg0->field_B  = 1;
                arg0->field_22 = 0x41;
                arg0->field_C  = 0;
            }
        }
    }
    {
        register s32 mode asm("v0");

        mode = arg0->field_8;
        one  = 1;
        Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, texts.texts[mode], arg0->field_1C, one, 0);
    }

    if (arg0->field_C == one) {
        if (arg0->field_8 == 2) {
            if (arg0->field_22 == 0x21) {
                arg0->field_22 = 0;
                return;
            }
        }
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            switch (arg0->field_8) {
                case 0:
                    SndEvt_EnqueueType6(3, 0, 0);
                    arg1->field_2E = 0x26;
                    break;
                case 1:
                    SndEvt_EnqueueType6(3, 0, 0);
                    arg1->field_2E = 6;
                    break;
                case 2:
                    SndEvt_EnqueueType6(4, 0, 0);
                    scan = &Mc_SaveData.field_5BC;
                    rec  = Gp_GetItemTable(scan);
                    i    = 0;
                    rec  = &rec[scan->field_0];
                    if (scan->field_1 != 0) {
                        do {
                            if ((u8)(rec->field_0 + 0x80) < 0x20) {
                                slot   = Gp_GetItemSlot(rec->field_0);
                                attach = slot->field_0;
                                if ((attach != 0) && (attach != 0xB9)) {
                                    if (Gp_SumScanQty(scan, attach) == 0) {
                                        slot->field_1 = 0;
                                    }
                                }
                                attach = slot->field_2;
                                if ((attach != 0) && (attach != 0xFF) && (attach != 0xB5) && (attach != 0xBB) &&
                                    (attach != 0xBD) && (attach != 0xBE)) {
                                    if (Gp_SumScanQty(scan, attach) == 0) {
                                        slot->field_3 = 0;
                                    }
                                }
                            }
                            i++;
                            rec++;
                        } while (i < scan->field_1);
                    }
                    arg1->field_2E = 0x27;
                    break;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            if (arg0->field_10 == 2) {
                scan = &Mc_SaveData.field_5BC;
                rec  = Gp_GetItemTable(scan);
                i    = 0;
                rec  = &rec[scan->field_0];
                if (scan->field_1 != 0) {
                    do {
                        if ((u8)(rec->field_0 + 0x80) < 0x20) {
                            slot   = Gp_GetItemSlot(rec->field_0);
                            attach = slot->field_0;
                            if ((attach != 0) && (attach != 0xB9)) {
                                if (Gp_SumScanQty(scan, attach) == 0) {
                                    slot->field_1 = 0;
                                }
                            }
                            attach = slot->field_2;
                            if ((attach != 0) && (attach != 0xFF) && (attach != 0xB5) && (attach != 0xBB) &&
                                (attach != 0xBD) && (attach != 0xBE)) {
                                if (Gp_SumScanQty(scan, attach) == 0) {
                                    slot->field_3 = 0;
                                }
                            }
                        }
                        i++;
                        rec++;
                    } while (i < scan->field_1);
                }
                arg1->field_2E = 0x27;
            } else {
                arg0->field_22 = 0x21;
                arg0->field_10 = 2;
            }
        }
    }
}

void Gp_ItemPickupTilt(Task* arg0)
{
    GameSession*   session;
    GameActorExt*  extra;
    GpItemObj8*    obj;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* rot;
    VECTOR         vec;
    VECTOR         vec2;
    MATRIX*        mem;
    GpItemObj2*    done;
    u32            mapId;
    s32            room;
    s32            check;
    u16            item;

    extra   = arg0->extra;
    obj     = arg0->spawnArg2;
    session = Game_Session;
    mapId   = *(u32*)&session->field_4 & 0xFFFF00FF;
    item    = obj->field_A;
    coord   = (GsCOORDINATE2*)extra->field_8;
    rot     = coord + 2;
    room    = *(u8*)&session->field_4;
    if (D_801153F4 == 2) {
        extra->field_C |= 0x80;
    } else {
        extra->field_C &= 0xFF7F;
    }
    mapId &= 0xFFFF0000;
    if (mapId == 0x4100000) {
        if ((u32)(room - 8) >= 2) {
            extra->field_C |= 0x80;
        }
    } else if (mapId == 0x41F0000) {
        check = 3;
        goto compare_room;
    } else if (mapId == 0x4140000) {
        check = 0x11;
    compare_room:
        if (room != check) {
            extra->field_C |= 0x80;
        }
    }
    if (arg0->state == 0) {
        mem = (MATRIX*)Mem_Calloc(0x40, 0);
        if (mem != NULL) {
            vec             = D_80093DB0;
            extra->field_1C = mem;
            extra->field_20 = mem + 1;
            Gp_UpdateCoord((GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8);
            func_800D7A9C(extra, &vec, 0, 3);
            arg0->idMap = (TaskIdMap*)mem;
        }
        arg0->field_24 = D_8010D828;
        arg0->flags    = 0;
        extra->field_C = 0;
        arg0->state++;
    } else if (arg0->state == 1) {
        if (Gp_GetCurBit2Flag(obj->field_8) != 2) {
            if (arg0->flags != 0) {
                arg0->killCountdown = 0;
                switch (mapId) {
                    case 0x1060000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                        SndEvt_EnqueueType6(0x51060009, temp,
                                            (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                        break;
                    }
                    case 0x10C0000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                        SndEvt_EnqueueType6(0x510C0005, temp,
                                            (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                    }
                    case 0x21B0000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                        SndEvt_EnqueueType6(0x521B000B, temp,
                                            (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                        break;
                    }
                    case 0x31B0000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                        SndEvt_EnqueueType6(0x531B000B, temp,
                                            (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                        break;
                    }
                    case 0x4100000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                        SndEvt_EnqueueType6(0x54100012, temp,
                                            (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                        break;
                    }
                    case 0x41F0000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                        SndEvt_EnqueueType6(0x541F0015, temp,
                                            (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                        break;
                    }
                    case 0x4270000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                        SndEvt_EnqueueType6(0x54270008, temp,
                                            (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                        break;
                    }
                }
                arg0->state++;
            }
        }
    } else if (arg0->state == 2) {
        arg0->killCountdown++;
        Gfx_RotMatrixX(&rot->coord, arg0->killCountdown << 5, 1);
        rot->flg = 0;
        if (arg0->killCountdown >= 0x14) {
            Display_InitModeObj(Task_GetDesc(1, 0x26), 0, (s32)arg0->spawnArg2, 0);
            arg0->state++;
        }
    } else if (arg0->state >= 3) {
        if (arg0->state == 3) {
            switch (mapId) {
                case 0x1060000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                    SndEvt_EnqueueType6(0x5106000A, temp,
                                        (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                    break;
                }
                case 0x10C0000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                    SndEvt_EnqueueType6(0x510C0006, temp,
                                        (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                    break;
                }
                case 0x21B0000:
                    break;
                case 0x31B0000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                    SndEvt_EnqueueType6(0x531B000C, temp,
                                        (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                    break;
                }
                case 0x4100000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                    SndEvt_EnqueueType6(0x54100013, temp,
                                        (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                    break;
                }
                case 0x41F0000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                    SndEvt_EnqueueType6(0x541F0016, temp,
                                        (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                    break;
                }
                case 0x4270000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan((GpObj38*)((GameActorExt*)arg0->extra)->field_8);
                    SndEvt_EnqueueType6(0x54270009, temp,
                                        (s8)Gp_GetObjDepth((GpObj38*)((GameActorExt*)arg0->extra)->field_8));
                    break;
                }
            }
            arg0->state++;
        }
        arg0->killCountdown -= 4;
        if (arg0->killCountdown <= 0) {
            arg0->killCountdown = 0;
        }
        Gfx_RotMatrixX(&rot->coord, arg0->killCountdown << 5, 1);
        rot->flg = 0;
        if (arg0->killCountdown == 0) {
            arg0->flags = 0;
            done        = (GpItemObj2*)arg0->extraState;
            if (done != NULL) {
                done->field_2    = 1;
                arg0->extraState = 0;
            }
            arg0->state = 1;
        }
    }
    vec2 = D_80093DB0;
    Gp_UpdateCoord((GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8);
    func_800D7A9C(extra, &vec2, 0, 3);
    USE_REG(item);
}

void Gp_ForEachUiChild(UiObject* arg0, void (*arg1)(UiObject*, Task*))
{
    Task* owner;
    Task* child;
    Task* next;
    Task* head;

    owner = arg0->owner;
    child = owner->firstChild;
    if (child != NULL) {
        do {
            next = child->nextSibling;
            arg1(child->spawnArg2, child);
            head  = owner->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
}

s32 Gp_ItemUseRestricted(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    if (Gp_ItemDescs[arg0].field_3 & 1) {
        ret = arg1 == 1;
    }
    if ((Gp_MoveItemKey == 0x703) && (arg0 == 0x81) && (Mc_SaveData.field_7 == 1)) {
        ret = 1;
    }
    return ret;
}

void Gp_CloseItemPane(UiObject* arg0, Task* arg1)
{
    UiObject* parent;

    parent = arg1->parent->spawnArg2;
    switch (arg0->field_2E) {
        case -1:
            if (parent->owner->flags) {
                parent->status = 1;
                Ui_TeardownTree(arg0, arg0->owner);
            } else {
                Ui_TeardownTree(arg0, arg0->owner);
                parent->status   = 0;
                parent->field_2E = -1;
            }
            break;
        case 6:
            parent->status = 1;
            Ui_TeardownTree(arg0, arg0->owner);
            break;
        case 0x23:
            Ui_TeardownTree(arg0, arg0->owner);
            parent->field_2E = 0x23;
            break;
        case 38:
        case 39:
            parent->field_2E = arg0->field_2E;
            break;
    }
}

void Gp_ItemMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    menu          = &Gp_ItemMenuList;
    if (arg0->state == 0) {
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        if (arg0->spawnArg1 == 0) {
            menu->field_10 = 1;
        } else {
            menu->field_10 = 0;
        }
        menu->field_A = 1;
        arg0->state  += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
}

void Gp_HolderPromptTask(Task* arg0)
{
    UiObject* obj;
    s32       val;
    s32       color;
    s32       one;
    u8*       text;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        Wip_UiHolder = (WipUiHolder*)obj;
        arg0->state += 1;
    }
    val = arg0->spawnArg1;
    if (val != 0) {
        color = 0x606060;
        one   = 1;
        Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, (u8*)val, color, one, 0);
        text = Text_SkipLines((u8*)val, one);
        Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x1E, text, color, one, 0);
    }
}

s32 Gp_BindItemObj2(Task* arg0, s32 arg1, GpItemObj2* arg2)
{
    s32         flag;
    GpItemObj8* obj;

    obj              = arg0->spawnArg2;
    flag             = 1;
    arg0->flags      = flag;
    arg0->extraState = (s32)arg2;
    if (Gp_GetCurBit2Flag(obj->field_8) == 2) {
        arg2->field_2 = flag;
    }
    return 0;
}

void Gp_PublishItemObj(Task* arg0)
{
    u16 item;
    s32 count;

    {
        register GpItemObj8* obj asm("v1");
        register u16         ritem asm("a0");

        obj           = arg0->spawnArg2;
        ritem         = obj->field_8;
        Gp_PubItemId  = ritem;
        ritem         = obj->field_A;
        Gp_PubItemLoc = ritem;
        item          = ritem;
    }
    if (item < 0xA0) {
        if ((u16)(item - 0x60) < 0x20U) {
            if (func_800B7420(Gp_PubItemLoc) != 0) {
                Gp_PubItemLoc = 0xD;
            }
        } else if ((u16)(item - 0x80) < 0x20U) {
            if (func_800B7420(Gp_PubItemLoc) != 0) {
                Gp_PubItemLoc = 0x3D;
            }
        }
        Gp_PubItemQty   = 1;
        Gp_PubItemReady = 1;
    } else {
        count           = Gp_StackLimits[Gp_PubItemLoc - 0xA0].field_0;
        Gp_PubItemReady = 1;
        Gp_PubItemQty   = count;
    }
    GameMain_SetFrameTiming(0);
    Wip_UiHolder        = NULL;
    arg0->killCountdown = 1;
    arg0->state         = arg0->state + 1;
}

void Gp_FadeTileTask(Task* arg0)
{
    s32       flag;
    s32       yoff;
    s32       otIdx;
    s32       color;
    TILE*     tile;
    DR_TPAGE* dr;

    flag = 0;
    if (arg0->state == 0) {
        if (arg0->spawnArg1 == 0) {
            GameMain_SetFrameTiming(1);
            Display_State.field_103 = 0;
            arg0->killCountdown     = 7;
        } else if ((arg0->spawnArg1 == 2) || (arg0->spawnArg1 == 4)) {
            arg0->killCountdown = 8;
        } else {
            arg0->killCountdown = 0;
        }
        arg0->state = arg0->state + 1;
    }

    if (arg0->spawnArg1 == 4) {
        if (Display_State.field_100 == 2) {
            arg0->killCountdown--;
        } else {
            GameMain_SetFrameTiming(1);
        }
    } else if ((arg0->spawnArg1 == 0) || (arg0->spawnArg1 == 2)) {
        flag = 0;
        arg0->killCountdown--;
    } else {
        flag                 = 1;
        arg0->killCountdown += flag;
    }

    yoff = 0;
    if (arg0->spawnArg1 == 2) {
        yoff  = Display_State.vramYOffset;
        otIdx = 0;
    } else if (arg0->spawnArg1 == 5) {
        otIdx = 0x3B;
    } else {
        otIdx = 0x3F;
    }

    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    tile->y0 = -0x78 - yoff;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    if (arg0->killCountdown < 8) {
        color    = (arg0->killCountdown << 5) + 0x1F;
        tile->b0 = color;
        tile->g0 = color;
        tile->r0 = color;
    } else {
        color    = 0xFF;
        tile->b0 = color;
        tile->g0 = color;
        tile->r0 = color;
    }

    addPrim((u_long*)((otIdx << 2) + (s32)Gpu_CurrentOt), tile);
    dr             = (DR_TPAGE*)Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000640;
    addPrim((u_long*)((otIdx << 2) + (s32)Gpu_CurrentOt), dr);

    if ((flag == 0) && (arg0->killCountdown <= 0)) {
        if (arg0->spawnArg1 == 4) {
            GameMain_SetFrameTiming(0);
        }
        Task_Kill(arg0);
    } else if (flag == 1) {
        if (arg0->killCountdown >= 8) {
            if (arg0->spawnArg1 == 5) {
                Display_State.field_100 = 0;
            } else {
                Display_State.field_103 = flag;
                GameMain_SetFrameTiming(0);
            }
            Task_Kill(arg0);
        }
    }
}
