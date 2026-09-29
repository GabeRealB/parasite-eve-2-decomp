#include "item_menu.h"

#include "types.h"

#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "items.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

typedef struct {
    s8 idB0;
    s8 idB1;
    s8 idB2;
    s8 idB3;
    u8 cmd;
    u8 param0;
    u8 param1;
    u8 param2;
} CdCmdEntryS;

extern const char Gp_StrSelectTitle[];

extern char Gp_StrDetachArmorHelp[];

/// Draws `item`'s name, its `func_800C22D8` marker in `mode`, the variant
/// marker for items 0x0F-0x32 and its icon at (`x`, `y`) in `obj`. Nothing is
/// drawn while `obj` is in mode 5.
static inline void _gpDrawItemNameAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item, s32 mode);

/// Draws `item` as `_gpDrawItemNameAt` does, at the prompt row's position and
/// in its colour.
static inline void _gpDrawItemName(UiList* prompt, UiObject* obj, s32 item, s32 mode);

/// Whether item `id` is the equipped weapon, the equipped armour, or the ammo
/// or attachment loaded in the equipped weapon.
static inline s32 _gpIsEquippedItem(s32 id);

static void Gp_CountEquippableRows(UiList* arg0, UiObject* arg1);

const char Gp_StrPEnergy[] = "P.Energy";

const char Gp_StrOption[] = "Option";

const char Gp_StrExit[] = "Exit";

const char Gp_StrSlash[] = "/";

const char Gp_StrHp[] = "HP";

const char Gp_StrMp[] = "MP";

const char Gp_StrExp[] = "EXP";

const char Gp_StrBp[] = "BP";

const char Gp_StrArmor[] = "Armor";

const char Gp_StrAttachments[] = "Attachments";

const char Gp_StrWeaponTitle[] = "Weapon";

const char Gp_StrE[] = "E";

const char Gp_StrItemHdr[] = "Item";

const char Gp_StrAttachments2[] = "ATTACHMENTs";

const char Gp_StrSelectTitle[] = "Select";

const char Gp_StrNextReplay[] = "NEXT REPLAY SUPPLY";

const char Gp_StrSpecs[] = "Specifications";

const char Gp_StrOperation[] = "OPERATION";

const char Gp_StrAddHp[] = "ADD HP";

const char D_8009707C[] = "-";

const char Gp_StrAddMp[] = "ADD MP";

const char Gp_StrAttachments3[] = "ATTACHMENTS";

const char Gp_StrSpecialFeat[] = "SPECIAL FEATURES";

const char Gp_StrPowerCaps[] = "POWER";

const char Gp_StrCapacity[] = "CAPACITY";

const char Gp_StrSpecial[] = "SPECIAL";

const char Gp_StrApplicableWpn[] = "APPLICABLE WEAPONS";

const char Gp_StrNotice[] = "Notice";

const char Gp_StrKeyItem[] = "Key Item";

const char gGpStrWeight[] = "Weight";

const char gGpStrRate[] = "Rate";

const char gGpStrRange[] = "Range";

const char gGpStrPower[] = "Power";

const char gGpStrAttachDot[] = "Attach.";

const char Gp_StrAttention[] = "Attention";

const char Gp_StrSelectWeapon[] = "Select Weapon";

const char Gp_StrEquip[] = "Equip";

const char Gp_StrSelectAmmo[] = "Select AMMO";

const char Gp_StrSelectArmor[] = "Select Armor";

const char Gp_StrReload[] = "Reload";

const char Gp_StrAttach[] = "Attach";

char Gp_StrCustomizeHelp[]     = "Customize game settings.";
char Gp_StrRemoveAmmoHelp[]    = "Remove loaded ammunition.";
char Gp_StrDetachArmorHelp[]   = "Detach items from armor.";
char Gp_StrChangeOrderHelp[36] = "Change the order of items carried.\000\335";

/// Draws `item`'s name, its `func_800C22D8` marker in `mode`, the variant
/// marker for items 0x0F-0x32 and its icon at (`x`, `y`) in `obj`. Nothing is
/// drawn while `obj` is in mode 5.
static inline void _gpDrawItemNameAt(UiObject* obj, s32 x, s32 y, s32 color, s32 item, s32 mode)
{
    TextDrawReq req;
    s32         temp;

    if (obj->panel.field_8 != 5) {
        req.x          = obj->panel.field_20.u + 0x11 + x;
        req.y          = obj->panel.field_22.u + (y - 6);
        req.otIndex    = obj->panel.field_14.s + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        Text_DrawString(&req, Gp_GetItemText(item, 0, 0));
        func_800C22D8(obj, x, y, item, mode);
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }
}

/// Draws `item` as `_gpDrawItemNameAt` does, at the prompt row's position and
/// in its colour.
static inline void _gpDrawItemName(UiList* prompt, UiObject* obj, s32 item, s32 mode)
{
    _gpDrawItemNameAt(obj, prompt->field_18, prompt->field_1A, prompt->field_1C, item, mode);
}

/// Shows `item`'s name in the holder (the empty-slot text for item 0) and
/// makes it the preview in slot 0.
#define GP_SHOW_ITEM_IN_HOLDER(item)                               \
    do {                                                           \
        if ((item) == 0) {                                         \
            Ui_SetHolderParam(Gp_StrEmpty, 0, 0);                  \
        } else {                                                   \
            Ui_SetHolderParam(Gp_GetItemText((item), 1, 0), 0, 0); \
        }                                                          \
        Gp_SetPreviewItem((item), 0);                              \
    } while (0)

/// Whether item `id` is the equipped weapon, the equipped armour, or the ammo
/// or attachment loaded in the equipped weapon.
static inline s32 _gpIsEquippedItem(s32 id)
{
    s32           ret;
    PlayerStatus* p;

    ret = 0;
    p   = &Player_Status;
    if ((((u32)(id - 0x80) < 0x20U) && (p->weapon == id - 0x7F)) ||
        (((u32)(id - 0x60) < 0x20U) && (p->armor == id - 0x5F)) ||
        (((u32)(id - 0xA0) < 0x20U) && (p->weapon != 0) &&
         ((Gp_GetItemSlot(p->weapon + 0x7F)->ammoId == id) ||
          (Gp_GetItemSlot(p->weapon + 0x7F)->attachId == id)))) {
        ret = 1;
    }
    return ret;
}

void Gp_DrawRemoveArmorRow(UiList* prompt, UiObject* obj)
{
    McItemScan* scan;
    McItemRec*  rec;
    s32         item;

    scan = &Mc_SaveData[0].state.carriedItems;
    rec  = Gp_NthEquippableRec(scan, prompt->field_8, 0);
    if (rec != NULL) {
        item = rec->itemId;
        {
            u8          buf[0x20];
            TextDrawReq req;
            s32         x;
            s32         y;
            s32         color;
            s32         qty;

            x     = prompt->field_18;
            y     = prompt->field_1A;
            color = prompt->field_1C;
            if ((u32)(item - 0xA0) < 0x20U) {
                qty            = rec->qty - Gp_CountEquippedRelated(scan, item);
                req.x          = obj->panel.field_20.u + 0x84 + x;
                req.y          = obj->panel.field_22.u + (y - 3);
                req.otIndex    = obj->panel.field_14.s + 1;
                req.field_8    = color;
                req.glyphTable = 5;
                req.centerMode = 2;
                req.field_E    = 0;
                Text_DrawString(&req, Text_ItoaSigned(buf, qty));
                Ui_LayoutWithMode0(obj, x + 0x69, y - 8, 0x1B, 7, 0x102010);
            }
        }

        if (rec->attachSlot > 0) {
            _gpDrawItemName(prompt, obj, item, 2);
        } else {
            _gpDrawItemName(prompt, obj, item, 1);
        }

        if (((obj->panel.field_0.w >> 16) == 1 || obj->panel.field_0.w == 1) && prompt->field_10 == prompt->field_8) {
            if (item == 0) {
                Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            } else {
                Ui_SetHolderParam(Gp_GetItemText(item, 1, 0), 0, 0);
            }
        }

        if (prompt->field_C == 1) {
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
                UiList*    menu;
                McItemRec* table;
                s32        i;
                s32        count;

                menu = &D_8010E8AC;
                SndEvt_EnqueueType6(3, 0, 0);
                table = Gp_GetItemTable(scan);
                count = scan->rowCount;
                table = &table[scan->firstRow];
                for (i = 0; i < count; i++) {
                    if (table[i].attachSlot == menu->field_10 + 1) {
                        Gp_RefreshItemRow(&table[i]);
                        break;
                    }
                }
                rec->attachSlot = menu->field_10 + 1;
                obj->field_2E   = 9;
            } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
                SndEvt_EnqueueType6(3, 0, 0);
                Ui_SpawnFromDesc(&D_8010EFA0, item | 0x10000, 1, 1, obj);
                obj->panel.field_0.w = 0;
            }
        }
    } else {
        if (((obj->panel.field_0.w >> 16) == 1 || obj->panel.field_0.w == 1) && prompt->field_10 == prompt->field_8) {
            Ui_SetHolderParam(Gp_StrDetachArmorHelp, 0, 0);
        }
        {
            TextDrawReq req;
            s32         off;

            req.x          = obj->panel.field_20.u + prompt->field_18;
            off            = obj->panel.field_22.u - 6;
            req.y          = prompt->field_1A + off;
            req.otIndex    = obj->panel.field_14.s + 1;
            req.field_8    = prompt->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Gp_StrRemoveArmor);
        }
        if (prompt->field_C == 1) {
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
                s32 slot;
                s32 i;
                s32 count;

                slot = D_8010E8AC.field_10 + 1;
                SndEvt_EnqueueType6(3, 0, 0);
                rec   = Gp_GetItemTable(scan);
                count = scan->rowCount;
                rec   = &rec[scan->firstRow];
                for (i = 0; i < count; i++, rec++) {
                    if (rec->attachSlot == slot) {
                        Gp_RefreshItemRow(rec);
                        break;
                    }
                }
                obj->field_2E = 9;
            }
        }
    }
}

static void Gp_CountEquippableRows(UiList* arg0, UiObject* arg1)
{
    McItemRec*  table;
    s32         i;
    s32         count;
    McItemScan* scan;

    scan  = &Mc_SaveData[0].state.carriedItems;
    table = Gp_GetItemTable(scan);
    count = 0;
    table = &table[scan->firstRow];
    for (i = 0; i < scan->rowCount; i++, table++) {
        if ((Gp_ItemDescs[table->itemId].field_3 & 4) || (table->itemId == 0)) {
            continue;
        }
        if ((u8)(table->itemId + 0x80) < 0x20 && _gpIsEquippedItem(table->itemId)) {
            continue;
        }
        count++;
    }
    arg0->field_4   = count + 1;
    arg0->field_5.u = 4;
}

void Gp_EquipSelectMenuTask(Task* arg0)
{
    UiObject*  obj;
    UiList*    menu;
    McItemRec* rec;
    s32        val;
    Task*      parent;

    obj           = arg0->spawnArg2.pointer;
    menu          = &D_8010E8D4;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, Gp_StrSelectTitle);
    val = 0;
    if (arg0->state == 0) {
        Gp_CountEquippableRows(menu, obj);
        Ui_LayoutListPanel(menu, &(obj)->panel);
        menu->field_17                   += 0x4C;
        obj->panel.bounds.unsignedRect.h += 0x4C;
        menu->field_A                     = 1;
        menu->field_10                    = 0;
        menu->field_9.u                   = 0;
        parent                            = arg0->parent;
        Ui_SetState4(parent->spawnArg2.pointer, parent);
        Ui_SpawnFromDesc(&D_8010EC3C, 3, val, 0x10, obj);
        arg0->state = arg0->state + 1;
    }
    Ui_DrawHBar(&(obj)->panel, obj->panel.field_1C.s, (s16)obj->panel.field_1E.u, (s16)obj->panel.field_18.u + 0x4A);
    Ui_UpdateListNoAnim(menu, obj);
    rec = Gp_NthEquippableRec(&Mc_SaveData[0].state.carriedItems, menu->field_10, 0);
    if (rec != NULL) {
        val = rec->itemId;
    }
    Gp_ItemRowSelect(menu, obj, val, 2);
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 9;
        }
    }
    func_800CF148(obj, arg0);
    if (arg0->spawnArg1.value == 0) {
        if (obj->field_2E == 9) {
            obj->field_2E = 6;
        }
    }
}

void Gp_EnqueueItemPreviewCd(s32 arg0, s32 arg1)
{
    s32          flags[3];
    CdCmdEntry   saved[3];
    CdCmdQueue*  queue;
    CdCmdEntryS* entry;
    s32          type;
    s32          index;
    s32          nibble;
    s32          hi;
    s32          lo;
    s32          i;

    queue = &CdCmd_Queue;
    if (arg0 == 0) {
        return;
    }
    if (gDisplayState.debugMode == -1) {
        if (Mc_SaveData[0].state.demoScene != 0xC) {
            return;
        }
    }
    if (queue->field_214 == 1) {
        return;
    }

    if (arg0 >= 0x500) {
        type  = 6;
        index = arg0;
    } else if (arg0 >= 0x300) {
        type   = 7;
        nibble = arg0 & 3;
        hi     = (arg0 & 0x30) >> 4;
        lo     = (arg0 & 0xC) >> 2;
        if (nibble == 0) {
            nibble = 1;
        }
        index = (hi * 3 + lo) * 3 + nibble;
    } else if ((u32)arg0 >= 0x180U) {
        return;
    } else if ((u32)(arg0 - 1) < 0x5FU) {
        type  = 3;
        index = arg0;
    } else if ((u32)(arg0 - 0x60) < 0x20U) {
        type  = 5;
        index = arg0 - 0x5F;
    } else if ((u32)(arg0 - 0x80) < 0x20U) {
        type  = 1;
        index = arg0 - 0x7F;
    } else if ((u32)(arg0 - 0xA0) < 0x20U) {
        type  = 4;
        index = arg0 + 0x61;
    } else {
        type  = 2;
        index = arg0;
    }

    if (arg1 & 0xFF) {
        D_80114D88 = 1;
    }

    flags[2] = -1;
    flags[1] = -1;
    flags[0] = -1;
    CdCmd_ResetEntryIter();

    while ((entry = (CdCmdEntryS*)CdCmd_NextEntry()) != NULL) {
        if (entry->idB1 == 3 && entry->idB2 == -8 && entry->idB3 == -3) {
            saved[0] = *(CdCmdEntry*)entry;
            flags[0] = 0;
        } else if (entry->idB1 == 0 && entry->idB2 == 0 && entry->idB3 == -2) {
            saved[1] = *(CdCmdEntry*)entry;
            flags[1] = 0;
        } else if (entry->idB1 == 3 && entry->idB2 == 0 && entry->idB3 == -2) {
            saved[2] = *(CdCmdEntry*)entry;
            flags[2] = 0;
        }
    }
    flags[arg1 & 0xFF] = -1;
    CdCmd_DropPending();
    for (i = 0; i < 3; i++) {
        if (flags[i] != -1) {
            cdCmdEnqueueEntry(&saved[i]);
        }
    }

    CdCmd_EnqueueLoadFile(type, index & 0xFF, arg1 & 0xFF);
}

/// Sets bit 0x100 in `flags`, which makes `func_800C7AE8` skip drawing the
/// item preview, while the CD queue is still busy loading it.
#define GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags) \
    do {                                     \
        if (CdCmd_IsIdle() == 0) {           \
            (flags) |= 0x100;                \
        }                                    \
    } while (0)
