#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/action_prompt.h"
#include "area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/captions.h"
#include "direction_input.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "loading.h"
#include "gameplay/map.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include "rooms/acropolis_east_elevator_hall.h"

#include "rooms/acropolis_square.h"

#include "rooms/acropolis_west_elevator_hall.h"

#include "rooms/dryfield_motel_room_6.h"

#include "rooms/dryfield_night_garage.h"

#include "rooms/dryfield_night_motel_room_6.h"

#include "rooms/dryfield_night_trailer_coach.h"

#include "rooms/dryfield_trailer_coach.h"

#include "rooms/mist_parking.h"

#include "rooms/shelter_1f_heliport.h"

#include "rooms/shelter_b1_armory.h"

#include "rooms/shelter_b1_underground_parking.h"

/// Workspace in which the map screen's area-shape pass scales one model vertex onto the map.
///
/// A `MenuMapAreaShape` model lies flat in its X/Z plane, in units the stage's
/// scale factor turns into map-screen pixels. The pass reserves this block on
/// the scratch stack, has the GTE store each scaled vertex in `scaled`, and
/// forms the primitive's corner from it and the origin: right of the origin by
/// the scaled X, above it by the scaled Z. The scaled Y, the height off the map
/// plane, is stored with the rest and not used. Nothing keeps a pointer into
/// the block past its release.
///
/// Every shape is placed at the map-screen origin, the centre of the map
/// picture, so the origin is always (0, 0). The bytes after it are reserved
/// with the block and left untouched, so what they were laid out to hold is
/// unproven.
typedef struct {
    SVECTOR scaled;       // Vertex scaled to map-screen pixels, in the model's axes
    s16     originX;      // Map-screen X the model's origin is placed at, in pixels from the map picture's centre
    s16     originY;      // Map-screen Y the model's origin is placed at, measured the same way
    byte    unknown_C[4]; // Reserved with the block and never accessed; role unproven
} _MenuMapAreaShapeScratch;
STATIC_ASSERT_SIZEOF(_MenuMapAreaShapeScratch, 0x10);

/// Workspace in which the map screen's icon pass stages the centre of one icon.
///
/// Each `MenuMapIcon` drawn is a 16x16 sprite about a point in map-screen
/// coordinates: the pass reserves this block on the scratch stack, copies the
/// icon's centre into it, places the sprite's corner from it and releases the
/// block once the sprite is queued. Nothing keeps a pointer into the block past
/// its release.
///
/// The five halfwords are written the same way, and `x` and `y` read back the
/// same way, as the ones in the middle of `_MenuMapCentreScratch`, the larger
/// block the map screen's other drawers reserve. Only `x` and `y` are read, so
/// what the rest of the block was laid out to hold is unproven.
typedef struct {
    s16  x;            // Map-screen X of the icon's centre, in pixels from the map picture's centre
    s16  y;            // Map-screen Y of the icon's centre, measured the same way
    u16  field_4;      // Cleared with every block and never read; role unproven
    u16  field_6;      // Cleared with every block and never read; role unproven
    u16  field_8;      // Cleared with every block and never read; role unproven
    byte unknown_A[2]; // Reserved with the block and never accessed; role unproven
} _MenuMapIconCentreScratch;
STATIC_ASSERT_SIZEOF(_MenuMapIconCentreScratch, 0xC);

/// Workspace in which a map-screen drawer stages the centre of what it draws.
///
/// The player cursor, each flag marker and the map picture are drawn about a
/// point in map-screen coordinates: the drawer reserves this block on the
/// scratch stack, writes the point, builds its primitive's corners from it and
/// releases the block once the primitive is queued. The picture is drawn about
/// the origin, so map-screen coordinates run from the picture's centre. Nothing
/// keeps a pointer into the block past its release.
///
/// Only `x`, `y` and the three halfwords after them are ever written, and only
/// `x` and `y` are read back. The bytes around them are reserved with the block
/// and left untouched, so what the block was laid out to hold is unproven.
typedef struct {
    byte unknown_0[0xC]; // Reserved with the block and never accessed; role unproven
    u16  x;              // Map-screen X of the centre, as a raw 16-bit encoding of a signed pixel coordinate
    u16  y;              // Map-screen Y of the centre, encoded the same way
    u16  field_10;       // Cleared with every block and never read; role unproven
    u16  field_12;       // Cleared with every block and never read; role unproven
    u16  field_14;       // Cleared with every block and never read; role unproven
    byte unknown_16[6];  // Reserved with the block and never accessed; role unproven
} _MenuMapCentreScratch;
STATIC_ASSERT_SIZEOF(_MenuMapCentreScratch, 0x1C);

ActionPrompt D_80114D28[2];

#define D_8010EBCC D_8010EAB4[10]

#define D_8010EE88 D_8010EAB4[35]

#define D_8010EFBC D_8010EAB4[46]

extern char D_8010F8F0[];

extern char Gp_StrReturnGame[];

extern char D_8010F91C[];

extern char Gp_StrUseAttachHelp[];

extern char Gp_StrUseKeyHelp[];

extern char Gp_StrCheckMap[];

static inline s32 _gpIsItemRowFree(InventoryItemRow* arg0);

static InventoryItemRow* func_800CE980(InventoryItemRange* arg0, s32 arg1);

static s32 func_800CEA00(InventoryItemRange* arg0, s32 arg1);

static s32 Gp_IsEquippedItem(s32 arg0);

static s32 func_800CEC5C(InventoryItemRow* arg0);

static InventoryItemRow* func_800CECC0(InventoryItemRange* arg0, s32 arg1);

static UiObject* Gp_OpenItemCmdMenu(UiList* arg0, UiObject* arg1, InventoryItemRow* arg2, s32 arg3);

static void func_800CEE5C(UiObject* arg0);

static s32 func_800CF204(CdCmdEntry* entry);

static s32 Gp_NthStockRelated(InventoryItemRange* arg0, s32 arg1, s32 arg2);

static void Gp_SpawnItemUsePrompt(UiList* arg0, UiObject* arg1);

static void Gp_DrawMapCursor(Task* arg0);

static void func_800D0614(Task* arg0);

static void Gp_DrawMapMarks(Task* arg0);

static void func_800D0C34(Task* arg0);

static s32 Gp_DrawMapIcons(Task* arg0, u8 arg1, u8 arg2);

static void Gp_EnqueueMapRoomCd(void);

static s8 func_800D1434(u32 roomId, u8 flagId);

static void func_800D15D0(Task* arg0);

static void func_800D1F90(Task* arg0);

static u8 Gp_GetMapRoomId(void);

static void func_800D2020(u8 arg0);

static void func_800D3660(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);

static void func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2);

static void func_800D4270(UiObject* obj, TmdSource* mesh, s32 mode, s32 dp);

static void Gp_DrawExamineCmd(UiObject* arg0, Task* arg1, u8* arg2, s32 arg3);

static void Gp_DrawPushCmd(UiObject* arg0, Task* arg1);

static inline s32 _gpIsItemRowFree(InventoryItemRow* arg0)
{
    PlayerStatus* p;
    s32           ret;
    s32           id;
    s8            attachmentSlot;

    p              = &gPlayerStatus;
    ret            = 1;
    attachmentSlot = arg0->attachSlot;
    id             = arg0->itemId;
    if ((attachmentSlot != INVENTORY_ATTACHMENT_NONE) || (((u32)(id - 0x60) < 0x20U) && (p->armor == id - 0x5F)) ||
        (((u32)(id - 0x80) < 0x20U) && (p->weapon == id - 0x7F))) {
        ret = 0;
    }
    return ret;
}

char D_8010F8F0[]          = "Check game settings.";
char Gp_StrReturnGame[]    = "Return to the game.";
char D_8010F91C[]          = "Secret Debug menu";
char Gp_StrUseAttachHelp[] = "Use and attach items.";
char Gp_StrUseKeyHelp[]    = "Use key items.";
char Gp_StrCheckMap[]      = "Check the map.";

// "EXP"
// "MP"

void Gp_DrawPeEnergyCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         color;
    s32         status;
    s32         one;

    color = arg0->colorRgb;
    if (Gp_IsDebugAttachRoom() != 0) {
        color = Ui_LookupTable(arg1, 2);
    } else {
        status = arg1->panel.control.word;
        one    = 1;
        if (((status >> 16) == one) || (status == one)) {
            if (arg0->selectedItemIndex == arg0->currentItemIndex) {
                Ui_SetHolderParam(Gp_StrReleasePe, 0, 0);
            }
        }
    }

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&req, Gp_StrPEnergy);

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (Gp_IsDebugAttachRoom() != 0) {
            arg0->actionResult = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg1->resultValue = 0xC;
            arg1->result      = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_DrawOptionCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;
    s32         two;
    UiObject*   obj;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&req, Gp_StrOption);

    status = arg1->panel.control.word;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            Ui_SetHolderParam(Gp_StrCustomizeHelp, 0, 0);
            two = 2;
            if (arg1->owner->spawnArg1.value != two) {
                CdCmd_DropPending();
                CdCmd_EnqueueLoadFile(1, 0, 0);
                Gp_ClearPreviewItems();
                arg1->owner->spawnArg1.value = two;
            }
        }
    }

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            obj = (UiObject*)arg1->owner->spawnArg2.pointer;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            obj->resultValue = 0x24;
            obj->result      = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_DrawExitCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&req, Gp_StrExit);

    status = arg1->panel.control.word;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            Ui_SetHolderParam(Gp_StrReturnGame, 0, 0);
        }
    }

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            arg1->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

static InventoryItemRow* func_800CE980(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow* table;
    s32               i;
    s32               count;
    InventoryItemRow* rec;

    table = Gp_GetItemTable(arg0);
    i     = 0;
    rec   = NULL;
    table = &table[arg0->firstRow];
    count = arg0->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot == arg1 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    return rec;
}

static s32 func_800CEA00(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow* table;
    s32               i;
    s32               count;
    InventoryItemRow* rec;

    table = Gp_GetItemTable(arg0);
    i     = 0;
    rec   = NULL;
    table = &table[arg0->firstRow];
    count = arg0->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot == arg1 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    if (rec == NULL) {
        return 0;
    }
    return rec->itemId;
}

void Gp_WeaponSummaryTask(Task* arg0)
{
    UiObject*     obj;
    UiObjectDesc* desc;

    obj = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        desc = &D_8010EBCC;
        Ui_SpawnFromDesc(desc, 0, 0, 0, obj);
        Ui_SpawnFromDesc(desc + 1, 0, 0, 0, obj);
        arg0->state = arg0->state + 1;
    }
    obj->result = USER_INTERFACE_RESULT_NONE;
    Gp_DrawEquipSummary(&(obj)->panel, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, 0);
    uiDrawTitle(&(obj)->panel, Gp_StrWeaponTitle);
}

void Gp_SetHolderItemText(s32 arg0)
{
    if (arg0 == 0) {
        Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
    } else {
        Ui_SetHolderParam(Gp_GetItemText(arg0, 1, 0), 0, 0);
    }
}

static s32 Gp_IsEquippedItem(s32 arg0)
{
    s32           ret;
    PlayerStatus* p;

    ret = 0;
    p   = &gPlayerStatus;
    if ((((u32)(arg0 - 0x80) < 0x20U) && (p->weapon == arg0 - 0x7F)) ||
        (((u32)(arg0 - 0x60) < 0x20U) && (p->armor == arg0 - 0x5F)) ||
        (((u32)(arg0 - 0xA0) < 0x20U) && (p->weapon != PLAYER_STATUS_EQUIPMENT_NONE) &&
         ((Gp_GetItemSlot(p->weapon + 0x7F)->primaryItemId == arg0) ||
          (Gp_GetItemSlot(p->weapon + 0x7F)->secondaryItemId == arg0)))) {
        ret = 1;
    }
    return ret;
}

static s32 func_800CEC5C(InventoryItemRow* arg0)
{
    return _gpIsItemRowFree(arg0);
}

static InventoryItemRow* func_800CECC0(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow* table;
    s32               i;
    InventoryItemRow* rec;

    table = Gp_GetItemTable(arg0);
    rec   = NULL;
    table = &table[arg0->firstRow];
    for (i = 0; i < arg0->rowCount; i++, table++) {
        if (_gpIsItemRowFree(table) == 1) {
            arg1--;
        }
        if (arg1 < 0) {
            rec = table;
            break;
        }
    }
    return rec;
}

static UiObject* Gp_OpenItemCmdMenu(UiList* arg0, UiObject* arg1, InventoryItemRow* arg2, s32 arg3)
{
    UiObject* obj;
    s32       one;

    obj           = NULL;
    Gp_SelItemRec = arg2;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        one = 1;
        obj = Ui_SpawnFromDesc(&D_8010EE6C, arg3, one, one, arg1);
        if (obj != NULL) {
            uiPositionRowDialog(&(obj)->panel, arg0, &(arg1)->panel);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else {
        Gp_CheckItemInfoButton(arg1);
    }
    return obj;
}

static void func_800CEE5C(UiObject* arg0)
{
    Task*     owner;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* obj;
    s32       one;
    s32       mask;
    s32       flag;

    owner = arg0->owner;
    head  = owner->firstChild;
    if (head != NULL) {
        one   = 1;
        child = head;
        mask  = (u32)~USER_INTERFACE_PANEL_DIMMED;
        do {
            obj  = child->spawnArg2.pointer;
            flag = obj->result;
            next = child->nextSibling;
            switch (flag) {
                case USER_INTERFACE_RESULT_CANCEL:
                    arg0->result = flag;
                    break;
                case USER_INTERFACE_RESULT_CONFIRM:
                    uiStartTreeClosing(obj, obj->owner);
                    arg0->panel.control.word = one;
                    arg0->panel.style       &= mask;
                    break;
                case 0x23:
                    uiStartTreeClosing(obj, obj->owner);
                    arg0->panel.control.word = one;
                    Gp_ItemOrderMode         = one;
                    arg0->panel.style       &= mask;
                    break;
            }
            head  = owner->firstChild;
            child = next;
            if (child == head) {
                break;
            }
        } while (head != NULL);
    }
}

void Gp_DrawSortCmd(UiList* arg0, UiObject* arg1)
{
    s32 status;
    s32 one;

    one = 1;
    if (Gp_ItemOrderMode == one) {
        arg0->colorRgb = Ui_LookupTable(arg1, 2);
    }
    textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, (const u8*)Gp_StrSort, arg0->colorRgb, one, TEXT_ALIGNMENT_LEFT);
    status = arg1->panel.control.word;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            if (Gp_ItemOrderMode == one) {
                arg0->actionResult    = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
                arg0->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
            } else {
                Ui_SetHolderParam(Gp_StrChangeOrderHelp, 0, 0);
            }
        }
    }
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Gp_SortItems(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, 1);
        }
    }
}

void func_800CF090(UiList* arg0, UiObject* arg1)
{
    PlayerStatus*              p;
    InventoryItemRange*        scan;
    volatile InventoryItemRow* table;
    s32                        count;
    s32                        i;

    count = 0;
    p     = &gPlayerStatus;
    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->firstRow];
    for (; i < scan->rowCount; i++) {
        if (((u32)(table->itemId - 0x60) < 0x20U) && (p->armor != table->itemId - 0x5F)) {
            count++;
        }
        table++;
    }
    arg0->itemCount                     = count;
    arg0->visibleRowCount.unsignedValue = 4;
}

void func_800CF148(UiObject* arg0, Task* arg1)
{
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;
    s32       val;

    child = arg1->firstChild;
    if (child != NULL) {
        val = 6;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->result;
            next     = child->nextSibling;
            switch (flag) {
                case USER_INTERFACE_RESULT_DISMISS:
                    arg0->result = flag;
                    break;
                case USER_INTERFACE_RESULT_CANCEL:
                    arg0->result = flag;
                    break;
                case USER_INTERFACE_RESULT_CONFIRM:
                    uiStartTreeClosing(childObj, childObj->owner);
                    arg0->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    break;
            }
            head  = arg1->firstChild;
            child = next;
            if (child == head) {
                break;
            }
            if (head == NULL) {
                break;
            }
        } while (1);
    }
}

static s32 func_800CF204(CdCmdEntry* entry)
{
    return cdCmdEnqueueEntry(entry);
}

s32 Gp_GetPreviewItem(void)
{
    return Gp_PreviewItems[0];
}

void Gp_DrawItemDescLine(UiList* arg0, UiObject* arg1)
{
    const u8* text;
    s8        idx;
    s32       id;

    idx = arg0->currentItemIndex;
    id  = (u16)arg1->owner->spawnArg1.value;
    if ((idx < 2) && (id < 0x100)) {
        text = Gp_GetItemText(id, idx + 1, 1);
        textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, text, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    } else {
        text = textSkipLines(Fs_GetChunkPayload(), arg0->currentItemIndex + 5);
        textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, text, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
}

void func_800CF330(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2.pointer;
    if (obj != NULL) {
        if (D_80067634 == obj) {
            D_80067634 = NULL;
        }
    }
    uiObjectTaskExit(arg0);
}

void Gp_DrawUseCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrUse);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Ui_SpawnFromDesc(&D_8010EF84, 0, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void Gp_EquipHeld(s32 arg0)
{
    PlayerStatus*     p;
    InventoryItemRow* rec;
    InventoryItemRow* prev;
    u8                field21;

    p       = &gPlayerStatus;
    rec     = Gp_FindItemById(arg0);
    field21 = p->weapon;
    if (field21 != arg0 - 0x7F) {
        if (field21 != 0) {
            prev = Gp_FindItemById(field21 + 0x7F);
            if (rec->attachSlot > INVENTORY_ATTACHMENT_NONE) {
                prev->attachSlot = rec->attachSlot;
            } else {
                Gp_ClearEquipSlotSel(prev->itemId, 0);
            }
        }
        p->weapon = arg0 - 0x7F;
        Gp_RefreshItemRow(rec);
        Gp_SetItemSeenBit(arg0, 1);
    }
}

static s32 Gp_NthStockRelated(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    s32       i;
    s32       result;
    s32       item;
    s32       qty;
    s32       mode;
    s32       idx;
    s32       temp;
    const u8* table0;
    const u8* table1;

    result = 0;
    mode   = Gp_ReloadMode;
    if (mode != 2) {
        i = 0;
        // Keep the row byte offset separate from the consumable-choice index.
        table0 = (const u8*)Gp_RelatedQty0.rows;
        idx    = arg2 - EQUIPMENT_WEAPON_ITEM_FIRST;
        do {
            temp = i + idx * (s32)sizeof(EquipmentWeaponLoadOptions);
            item = table0[temp + OFFSET_OF(EquipmentWeaponLoadOptions, acceptedItemIds)];
            qty  = Gp_ScanStackQty(arg0, item);
            qty -= Gp_CountEquippedRelated(arg0, item);
            if (qty > 0) {
                arg1--;
                if (arg1 < 0) {
                    result = item;
                    break;
                }
            }
            i++;
        } while (i < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds));
    }
    if (mode != 1) {
        if (arg1 >= 0) {
            i      = 0;
            table1 = (const u8*)Gp_RelatedQty1.rows;
            idx    = arg2 - EQUIPMENT_WEAPON_ITEM_FIRST;
            do {
                temp = i + idx * (s32)sizeof(EquipmentWeaponLoadOptions);
                item = table1[temp + OFFSET_OF(EquipmentWeaponLoadOptions, acceptedItemIds)];
                qty  = Gp_ScanStackQty(arg0, item);
                qty -= Gp_CountEquippedRelated(arg0, item);
                if (qty > 0) {
                    arg1--;
                    if (arg1 < 0) {
                        result = item;
                        break;
                    }
                }
                i++;
            } while (i < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds));
        }
    }
    return result;
}

void Gp_SizeEquippedPanel(UiPanel* arg0, s32 arg1)
{
    s32 width;
    s32 temp;

    width = textMeasureLineWidth((const u8*)Gp_GetItemText(arg1, 0, 0)) + 0xB;
    temp  = textMeasureLineWidth((const u8*)Gp_StrEquipped);
    if (width < temp) {
        width = temp;
    }
    uiSetPanelContentSize(arg0, width + 5, uiGetTextRowsHeight(2) + 1);
    arg0->bounds.rect.x = (-arg0->bounds.rect.w) >> 1;
}

void func_800CF6E8(UiObject* arg0, s32 arg1)
{
    u8* text;
    s32 color;
    s32 one;
    s32 x;

    text  = Gp_GetItemText(arg1, 0, 0);
    color = 0x606060;
    one   = 1;
    textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrEquipped, color, one, TEXT_ALIGNMENT_LEFT);
    x = textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0x1E, text, 0x37A78, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(arg0, x, arg0->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, color, one, TEXT_ALIGNMENT_LEFT);
}

void Gp_DrawUsePrompt(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrUse);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Gp_SpawnItemUsePrompt(arg0, arg1);
            arg0->actionResult = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}

void Gp_DrawMovePrompt(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrMove);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg0->actionResult = USER_INTERFACE_LIST_ACTION_MOVE;
        }
    }
}

void Gp_DrawExchangeSlotCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    UiObject*   obj;
    s32         one;
    s32         x;
    s32         y;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrExchange);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            one = 1;
            obj = Ui_SpawnFromDesc(&D_8010ED00, one, one, 0x10, arg1);
            if (obj != NULL) {
                y                                = -0x5C;
                obj->panel.bounds.unsignedRect.y = y;
                x                                = -8;
                obj->panel.bounds.unsignedRect.x = x;
            }
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            arg0->actionResult       = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}

void func_800CFA34(UiObject* arg0, Task* arg1)
{
    Gp_UseHealItemPanel(arg0, arg1, Gp_SelItemRec->itemId);
}

void func_800CFA60(Task* arg0)
{
    UiObjectTaskFunc fn;
    UiObject*        obj;

    fn  = D_8010D3A0[arg0->spawnArg1.value];
    obj = arg0->spawnArg2.pointer;
    if (fn != NULL) {
        fn(obj, obj->owner);
    }
}

void func_800CFAA8(UiObject* arg0, Task* arg1)
{
    Gp_InvokePeItemPanel(arg0, arg1, arg1->spawnArg1.value);
}

void Gp_DrawOkCmd(UiList* arg0, UiObject* arg1)
{
    textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, (const u8*)Gp_StrOk, arg0->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg0->actionResult              = USER_INTERFACE_RESULT_CONFIRM;
            arg0->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_OK;
        }
    }
}

void Gp_DrawCancelCmd(UiList* arg0, UiObject* arg1)
{
    textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, (const u8*)Gp_StrCancel, arg0->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg0->actionResult              = USER_INTERFACE_RESULT_CONFIRM;
            arg0->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_CANCEL;
        }
    }
}

void Gp_DrawYesCmd(UiList* arg0, UiObject* arg1)
{
    s32 temp;

    textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, (const u8*)Gp_StrYes, arg0->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    temp = arg0->rowInputEnabled;
    if (temp == 1) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg0->actionResult              = USER_INTERFACE_RESULT_CONFIRM;
            arg0->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_YES;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            arg0->navigationStep = temp;
            arg0->actionResult   = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
        }
    }
}

void Gp_DrawNoCmd(UiList* arg0, UiObject* arg1)
{
    textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, (const u8*)Gp_StrNo, arg0->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            arg0->actionResult              = USER_INTERFACE_RESULT_CONFIRM;
            arg0->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_NO;
        }
    }
}

void func_800CFD78(Task* arg0)
{
    if (arg0->state == 0) {
        D_80114DCC = GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
    }
    switch (D_80114DCC) {
        case GAME_LOCATION_KEY(1, 1, 0, 0):
            func_acropolis_square_8017F41C(arg0);
            break;
        case GAME_LOCATION_KEY(1, 2, 0, 0):
            func_acropolis_east_elevator_hall_8017F2F8(arg0);
            break;
        case GAME_LOCATION_KEY(1, 17, 0, 0):
            func_acropolis_west_elevator_hall_8017F304(arg0);
            break;
        case GAME_LOCATION_KEY(2, 30, 0, 0):
            func_dryfield_motel_room_6_80181184(arg0);
            break;
        case GAME_LOCATION_KEY(3, 30, 0, 0):
            func_dryfield_night_motel_room_6_801811A0(arg0);
            break;
        default:
            taskKill(arg0);
            break;
    }
}

static void Gp_SpawnItemUsePrompt(UiList* arg0, UiObject* arg1)
{
    u8                id;
    s32               one;
    UiObjectTaskFunc* slot;

    id   = Gp_SelItemRec->itemId;
    slot = &D_8010D3A0[id];
    if (*slot != NULL) {
        one = 1;
        if (Ui_SpawnFromDesc(&D_8010EE88, (s32)(id), one, one, arg1) != NULL) {
            Gp_SetItemSeenBit(id, 1);
        }
        arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    } else {
        Gp_SpawnItemPrompt(arg1, 0x12, 0, 0);
        arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
}

void Gp_MapTaskState2(Task* arg0)
{
    UiObject* obj;
    UiObject* child;
    u8*       flags;
    u8        room;

    obj   = arg0->spawnArg2.pointer;
    flags = Gp_MapFlagIds[gGameSession->location.loc.stage - 1];
    Gp_DrawMapCursor(arg0);
    func_800D0C34(arg0);
    func_800D0614(arg0);
    Gp_DrawMapMarks(arg0);
    func_800D15D0(arg0);
    if (gDisplayState.keepGraphics != 0) {
        Display_SetDrawMode(1);
    } else {
        Display_SetDrawMode(0);
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | PAD_BUTTON_SELECT) != 0) {
            obj->resultValue = 0x101;
            func_800D1F90(arg0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
            if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                Gp_LoadViewAndCd(1);
            }
            arg0->state = 3;
            return;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            func_800D1F90(arg0);
            obj->result = USER_INTERFACE_RESULT_CANCEL;
            if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                Gp_LoadViewAndCd(1);
            }
            arg0->state = 3;
            return;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            if (flags[Gp_MapRoomId] == 0xFF) {
                return;
            }
            for (room = Gp_MapRoomId + 1; room <= D_8010F130[gGameSession->location.loc.stage - 1]; room++) {
                if (func_800D1434(room, flags[room]) == 1) {
                    if ((s8)Gp_MapRoomId != room) {
                        Gp_MapRoomId = room;
                        Display_SetDrawMode(3);
                        sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
                        Gp_EnqueueMapRoomCd();
                        arg0->state = 1;
                    }
                    return;
                }
            }
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            if (flags[Gp_MapRoomId] == 0xFF) {
                return;
            }
            for (room = Gp_MapRoomId - 1; room != 0; room--) {
                if (func_800D1434(room, flags[room]) == 1) {
                    if ((s8)Gp_MapRoomId != room) {
                        Gp_MapRoomId = room;
                        Display_SetDrawMode(3);
                        sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
                        Gp_EnqueueMapRoomCd();
                        arg0->state = 1;
                    }
                    return;
                }
            }
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            D_8010F13D = func_800E3FCC(0xA2);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Ui_SpawnFromDesc(&D_8010F15C, 0, 1, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
    if (arg0->firstChild != NULL) {
        child = arg0->firstChild->spawnArg2.pointer;
        if (child->result == USER_INTERFACE_RESULT_CONFIRM) {
            obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            uiStartTreeClosing(child, child->owner);
        }
        if (child->result == USER_INTERFACE_RESULT_CANCEL) {
            func_800D1F90(arg0);
            obj->result = USER_INTERFACE_RESULT_CANCEL;
            if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                Gp_LoadViewAndCd(1);
            }
            arg0->state = 3;
        }
    }
}

static void Gp_DrawMapCursor(Task* arg0)
{
    UiObject*              obj;
    GameActor*             actor;
    MenuMapArea*           rec;
    PlayerStatus*          cfg;
    _MenuMapCentreScratch* centre;
    s32                    off;
    s32                    base;
    SPRT_16*               p;
    DR_TPAGE*              dr;
    s32                    u0;
    s32                    ang;

    obj   = arg0->spawnArg2.pointer;
    cfg   = &gPlayerStatus;
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    rec   = Gp_MapRecTables[gGameSession->location.loc.stage - 1];
    rec   = rec + gGameSession->location.loc.area;
    if (rec->page != (s8)Gp_MapRoomId) {
        return;
    }

    centre           = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapCentreScratch);
    centre->field_14 = 0;
    centre->field_12 = 0;
    centre->field_10 = 0;
    off              = (rec->originX - cfg->coordMtx->t[0]) / rec->scaleX;
    base             = rec->mapX;
    centre->x        = base - off;
    off              = (rec->originZ - cfg->coordMtx->t[2]) / rec->scaleZ;
    base             = rec->mapY;
    centre->y        = base + off;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    ang            = (rsin(gDisplayState.loopCount << 6) + 0x1000) >> 5;
    if (ang == 0x100) {
        ang = 0xFF;
    }
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = ((ang & 0xFF) << 0x10) | ((ang & 0xFF) << 8) | (ang & 0xFF);
    setlen(p, 3);
    setcode(p, 0x7E);
    p->clut = GetClut(0, 0x101);

    ang = (u16)actor->rotation.vy;
    if (((ang - 0xF00) & 0xFFFF) < 0x100U) {
        u0 = 0x40;
    } else if (ang < 0x100U) {
        u0 = 0x40;
    } else if (((ang - 0x100) & 0xFFFF) < 0x200U) {
        u0 = 0x50;
    } else if (((ang - 0x300) & 0xFFFF) < 0x200U) {
        u0 = 0x60;
    } else if (((ang - 0x500) & 0xFFFF) < 0x200U) {
        u0 = 0x70;
    } else if (((ang - 0x700) & 0xFFFF) < 0x200U) {
        u0 = 0x80;
    } else if (((ang - 0x900) & 0xFFFF) < 0x200U) {
        u0 = 0x90;
    } else if (((ang - 0xB00) & 0xFFFF) < 0x200U) {
        u0 = 0xA0;
    } else if (((ang - 0xD00) & 0xFFFF) < 0x200U) {
        u0 = 0xB0;
    } else {
        goto noDir;
    }
    p->u0 = u0;
    p->v0 = 0x10;
noDir:

    p->x0 = centre->x - 8;
    p->y0 = centre->y - 8;
    addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x1C], p);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 0, 0, 0xE);
    addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x1C], dr);
    SCRATCH_STACK_RELEASE_BLOCK(_MenuMapCentreScratch);
}

static void func_800D0614(Task* arg0)
{
    UiObject*              obj;
    _MenuMapCentreScratch* centre;
    POLY_FT4*              p;
    SPRT*                  sprt;
    DR_TPAGE*              dr;

    obj              = arg0->spawnArg2.pointer;
    p                = gGpuPrimCursor;
    centre           = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapCentreScratch);
    gGpuPrimCursor   = p + 1;
    centre->field_14 = 0;
    centre->field_12 = 0;
    centre->field_10 = 0;
    centre->y        = 0;
    centre->x        = 0;
    setPolyFT4(p);
    setRGB0(p, 0x80, 0x80, 0x80);
    p->clut = 0x4000;
    setSemiTrans(p, 1);
    p->tpage = GetTPage(1, 0, 0x380, 0x20);
    p->u0 = p->u2 = 1;
    p->v0 = p->v1 = 0x20;
    p->u3 = p->u1 = 0xFF;
    p->v3 = p->v2 = 0xF0;
    p->x0 = p->x2 = centre->x - 0x7F;
    p->y0 = p->y1 = centre->y - 0x68;
    p->x1 = p->x3 = centre->x + 0x7F;
    p->y2 = p->y3 = centre->y + 0x68;
    addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue + 2], p);
    SCRATCH_STACK_RELEASE_BLOCK(_MenuMapCentreScratch);

    sprt           = gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
    setlen(sprt, 4);
    setcode(sprt, 0x67);
    sprt->clut = GetClut(0x70, 0x101);
    sprt->u0   = 0xC0;
    sprt->w    = 0x20;
    sprt->h    = 0x18;
    sprt->x0   = 0x7E;
    sprt->v0   = 0;
    sprt->y0   = -0x64;
    addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x19], sprt);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 0, 0, 0xE);
    addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x19], dr);
}

static void Gp_DrawMapMarks(Task* arg0)
{
    Task*                 keep;
    GameSession*          session;
    GameFlagStageHeader** banks;
    GameFlagStageHeader*  bank;
    s32                   flags[2];
    u8*                   flagTbl;
    MenuMapAreaShape*     shapes;
    MenuMapAreaShape**    shapeTables;
    UiObject*             obj;
    s32                   color;
    s32                   i;
    s32                   which;
    s32                   bit;
    s32                   idx;
    s32                   one;
    u8                    stage;
    s32                   stageM1;

    keep        = arg0;
    color       = 0x5D7;
    session     = gGameSession;
    banks       = Gp_FlagBanks;
    shapeTables = (keep, Gp_MapMarkTables);
    stage       = session->location.loc.stage;
    obj         = arg0->spawnArg2.pointer;
    stageM1     = stage - 1;
    bank        = banks[stage];
    shapes      = shapeTables[stageM1];
    flagTbl     = Gp_MapFlagIds[stageM1];
    if (stage == 1) {
        color = 0x83B;
    }
    flags[0] = bank->visitedAreas[0];
    flags[1] = bank->visitedAreas[1];
    if (session->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT) {
        bank      = banks[2];
        flags[0] |= bank->visitedAreas[0];
        flags[1] |= bank->visitedAreas[1];
    }
    i = 0;
    if (Gp_MapMarkCounts[session->location.loc.stage - 1] != 0) {
        one = 1;
        do {
            if (shapes[(u8)i].page == (s8)Gp_MapRoomId) {
                if (shapes[(u8)i].model == NULL) {
                    Gp_DrawMapIcons(arg0, (u8)i, 0);
                } else {
                    which = 0;
                    if ((u8)i >= 0x21U) {
                        which = 1;
                        bit   = one << ((u8)i - 0x21);
                    } else {
                        bit = one << ((u8)i - 1);
                    }
                    if (shapes[(u8)i].pairedArea != MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE) {
                        if (shapes[(u8)i].pairedArea >= 0x21U) {
                            bit |= one << (shapes[(u8)i].pairedArea - 0x21);
                        } else {
                            bit |= one << (shapes[(u8)i].pairedArea - 1);
                        }
                    }
                    idx = i;
                    if (Gp_MapRoomOff == 3) {
                        if ((u8)i == 0xE) {
                            idx = 0x22;
                        }
                        if ((u8)i == 0x1B) {
                            idx = 0x23;
                        }
                    }
                    if (gameFlagGetNibble(flagTbl[Gp_MapRoomId]) == 0) {
                        if ((bit & flags[which]) == 0) {
                            if (Gp_DrawMapIcons(arg0, (u8)i, 1) != 0) {
                                func_800D4270(obj, shapes[(u8)idx].model, 1, (u16)color);
                            } else {
                                func_800D4270(obj, shapes[(u8)idx].model, 0, (u16)color);
                            }
                        } else if ((bit & Gp_AreaIdBits[which]) != 0) {
                            func_800D4270(obj, shapes[(u8)idx].model, 3, (u16)color);
                            Gp_DrawMapIcons(arg0, (u8)i, 0);
                        } else {
                            Gp_DrawMapIcons(arg0, (u8)i, 0);
                        }
                    } else if ((bit & flags[which]) == 0) {
                        func_800D4270(obj, shapes[(u8)idx].model, 1, (u16)color);
                        Gp_DrawMapIcons(arg0, (u8)i, 1);
                    } else if ((bit & Gp_AreaIdBits[which]) != 0) {
                        func_800D4270(obj, shapes[(u8)idx].model, 3, (u16)color);
                        Gp_DrawMapIcons(arg0, (u8)i, 0);
                    } else {
                        Gp_DrawMapIcons(arg0, (u8)i, 0);
                    }
                }
            }
            i++;
        } while ((u8)i < Gp_MapMarkCounts[gGameSession->location.loc.stage - 1]);
    }
}

static void func_800D0C34(Task* arg0)
{
    UiObject*              obj;
    MenuMapMarker*         markers;
    GameFlagStageHeader*   bank;
    _MenuMapCentreScratch* centre;
    SPRT_16*               p;
    DR_TPAGE*              dr;
    s32                    flags[2];
    u8                     i;
    s16                    which;
    s32                    bit;
    u16                    state;
    u8                     stage;
    u8                     area;

    i        = 0;
    stage    = gGameSession->location.loc.stage;
    obj      = arg0->spawnArg2.pointer;
    markers  = D_8010F0E0[stage - 1];
    bank     = Gp_FlagBanks[stage];
    flags[0] = bank->visitedAreas[0];
    flags[1] = bank->visitedAreas[1];
    for (;;) {
        if (markers[i].page == 0) {
            return;
        }
        area = markers[i].area;
        if (area == MENU_MAP_MARKER_AREA_NEVER) {
            i++;
            continue;
        }
        if (area != MENU_MAP_MARKER_AREA_ANY) {
            // Show the marker only once its area has been visited.
            if (area >= 0x21) {
                bit   = 1 << (markers[i].area - 0x21);
                which = 1;
            } else {
                bit   = 1 << (markers[i].area - 1);
                which = 0;
            }
            if (!(bit & flags[which])) {
                i++;
                continue;
            }
        }
        state = Gp_LookupStageFlag(i);
        if (markers[i].page != (s8)Gp_MapRoomId) {
            i++;
            continue;
        }
        if (state == 2 || state == 0x802) {
            centre           = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapCentreScratch);
            centre->field_14 = 0;
            centre->field_12 = 0;
            centre->field_10 = 0;
            centre->x        = markers[i].x;
            p                = gGpuPrimCursor;
            gGpuPrimCursor   = p + 1;
            centre->y        = markers[i].y;
            setlen(p, 3);
            setcode(p, 0x7F);
            if (state == 2) {
                p->clut = GetClut(0x30, 0x101);
                p->u0   = 0x60;
                p->v0   = 0;
            } else if (state == 0x802) {
                p->clut = GetClut(0x60, 0x101);
                p->u0   = 0x90;
                p->v0   = 0;
            }
            p->x0 = centre->x - 8;
            p->y0 = centre->y - 8;
            addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x1B], p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x1B], dr);
            SCRATCH_STACK_RELEASE_BLOCK(_MenuMapCentreScratch);
        }
        i++;
    }
}

static s32 Gp_DrawMapIcons(Task* arg0, u8 arg1, u8 arg2)
{
    UiObject*    obj;
    MenuMapIcon* icons;
    u8           i;
    s32          ret;
    u8           otOff;
    s32          lum;

    otOff = 0;
    i     = 0;
    ret   = 0;
    obj   = arg0->spawnArg2.pointer;
    icons = D_8010F0CC[gGameSession->location.loc.stage - 1];
    lum   = (rsin(gDisplayState.loopCount << 6) + 0x1000) >> 5;

    for (;;) {
        _MenuMapIconCentreScratch* centre;
        SPRT_16*                   p;
        DR_TPAGE*                  dr;
        u16                        clut;

        if (icons[i].page == 0) {
            break;
        }
        if (icons[i].kind == MENU_MAP_ICON_KIND_OBJECTIVE) {
            if (icons[i].condition != func_800E3FCC(0xA2)) {
                i++;
                continue;
            }
        } else if (icons[i].condition != 0) {
            if (gameFlagGetNibble(icons[i].condition) == 0) {
                i++;
                continue;
            }
        }
        if ((arg2 != 0) && (icons[i].kind < MENU_MAP_ICON_KIND_OBJECTIVE)) {
            i++;
            continue;
        }
        if ((icons[i].page == (s8)Gp_MapRoomId) && (icons[i].area == arg1)) {
            centre          = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapIconCentreScratch);
            centre->field_8 = 0;
            centre->field_6 = 0;
            centre->field_4 = 0;
            centre->x       = icons[i].x;
            p               = gGpuPrimCursor;
            gGpuPrimCursor  = p + 1;
            centre->y       = icons[i].y;
            if (icons[i].kind == MENU_MAP_ICON_KIND_OBJECTIVE) {
                if (lum == 0x100) {
                    lum = 0xFF;
                }
                GPU_PRIMITIVE_COLOR_WORD(p, 0) = ((lum & 0xFF) << 0x10) | ((lum & 0xFF) << 8) | (lum & 0xFF);
            }
            setlen(p, 3);
            setcode(p, 0x7C);
            if (icons[i].kind != MENU_MAP_ICON_KIND_OBJECTIVE) {
                setcode(p, 0x7D);
            }
            setSemiTrans(p, 1);
            switch (icons[i].kind) {
                case 0:
                    clut    = GetClut(0x20, 0x101);
                    otOff   = 0x1B;
                    p->clut = clut;
                    p->u0   = 0x50;
                    p->v0   = 0;
                    break;
                case MENU_MAP_ICON_KIND_TELEPHONE:
                    clut    = GetClut(0x10, 0x101);
                    otOff   = 0x1C;
                    p->clut = clut;
                    p->u0   = 0x40;
                    p->v0   = 0;
                    break;
                case MENU_MAP_ICON_KIND_OBJECTIVE:
                    clut    = GetClut(0x40, 0x101);
                    otOff   = 0x1C;
                    ret     = 1;
                    p->clut = clut;
                    p->u0   = 0x70;
                    p->v0   = 0;
                    break;
            }
            p->x0 = centre->x - 8;
            p->y0 = centre->y - 8;
            addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - otOff], p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - otOff], dr);
            SCRATCH_STACK_RELEASE_BLOCK(_MenuMapIconCentreScratch);
        }
        i++;
    }
    return ret;
}

static void Gp_EnqueueMapRoomCd(void)
{
    u8  param1[8];
    u8  param2[8];
    s32 room;
    u8  stage;

    Gp_MapRoomOff             = 0;
    gGameSession->loadedSndId = 0;
    if ((gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER) && ((s8)Gp_MapRoomId == 6) && (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == 0)) {
        Gp_MapRoomOff = 1;
    }
    if (gGameSession->location.loc.stage == GAME_STAGE_SHELTER_NEO_ARK) {
        room = (s8)Gp_MapRoomId;
        if ((room == 1) && (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) == room)) {
            Gp_MapRoomOff = 3;
        }
    }
    param1[2] = 3;
    param1[3] = 0;
    param1[0] = Gp_MapRoomId + Gp_MapRoomOff;
    stage     = gGameSession->location.loc.stage;
    param2[1] = 0;
    param2[3] = 0;
    param2[2] = 0;
    param2[0] = stage;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
    D_800626E8 = 1;
}

static s8 func_800D1434(u32 roomId, u8 flagId)
{
    GameFlagStageHeader* bank;
    MenuMapArea*         recs;
    s32                  flags[2];
    s32                  i;
    s32                  which;
    s32                  bit;
    s32                  one;
    s32                  skip;

    bank = Gp_FlagBanks[gGameSession->location.loc.stage];
    if (gGameSession->location.loc.stage != GAME_STAGE_SHELTER_NEO_ARK) {
        if (flagId != 0xFF) {
            if (flagId == 0x80) {
                return 0;
            }
            which = flagId != 0;
            if (which && (gameFlagGetNibble(flagId) != 0)) {
                return 1;
            }
            recs     = Gp_MapRecTables[gGameSession->location.loc.stage - 1];
            flags[0] = bank->visitedAreas[0];
            flags[1] = bank->visitedAreas[1];
            i        = 0;
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT) {
                bank      = Gp_FlagBanks[2];
                flags[0] |= bank->visitedAreas[0];
                flags[1] |= bank->visitedAreas[1];
            }
            if (recs->page != MENU_MAP_AREA_PAGE_END) {
                skip = MENU_MAP_AREA_PAGE_NONE;
                one  = 1;
                do {
                    recs++;
                    i++;
                    if (recs->page != skip) {
                        which = 0;
                        if ((u8)i >= 0x21U) {
                            which = 1;
                            bit   = one << ((u8)i - 0x21);
                        } else {
                            bit = one << ((u8)i - 1);
                        }
                        if (bit & flags[which]) {
                            if (recs->page == (u8)roomId) {
                                return 1;
                            }
                        }
                    }
                } while (recs->page != MENU_MAP_AREA_PAGE_END);
            }
        }
    }
    return 0;
}

static void func_800D15D0(Task* arg0)
{
    UiObject* obj;
    u8*       flagIds;
    SPRT*     p;
    DR_TPAGE* dr;
    DR_TPAGE* rightDr;
    s32       i;
    s32       lum;
    s8        ret;
    u8        stage;

    stage   = gGameSession->location.loc.stage;
    obj     = arg0->spawnArg2.pointer;
    flagIds = Gp_MapFlagIds[stage - 1];
    if (stage == 5) {
        return;
    }
    if (flagIds[Gp_MapRoomId] == 0xFF) {
        return;
    }

    i = Gp_MapRoomId - 1;
    while ((u8)i != 0) {
        if (flagIds[(u8)i] == 0) {
            break;
        }
        if (flagIds[(u8)i] == 0xFF) {
            break;
        }
        ret = func_800D1434((u8)i, flagIds[(u8)i]);
        if (ret == 1) {
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            lum            = (rsin(gDisplayState.loopCount << 5) + 0x1000) >> 5;
            if (lum == 0x100) {
                lum = 0xFF;
            }
            GPU_PRIMITIVE_COLOR_WORD(p, 0) = ((lum & 0xFF) << 0x10) | ((lum & 0xFF) << 8) | (lum & 0xFF);
            setlen(p, 4);
            setcode(p, 0x66);
            p->clut = GetClut(0x50, 0x101);
            p->u0   = 0x88;
            p->w    = 8;
            p->h    = 0x10;
            p->x0   = -0x89;
            p->v0   = 0;
            p->y0   = -7;
            addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x1C], p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x1C], dr);
            break;
        }
        i--;
    }

    i = Gp_MapRoomId + 1;
    while ((u8)i <= D_8010F130[gGameSession->location.loc.stage - 1]) {
        if (flagIds[(u8)i] == 0) {
            return;
        }
        if (flagIds[(u8)i] == 0xFF) {
            return;
        }
        ret = func_800D1434((u8)i, flagIds[(u8)i]);
        if (ret == 1) {
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            lum            = (rsin(gDisplayState.loopCount << 5) + 0x1000) >> 5;
            if (lum == 0x100) {
                lum = 0xFF;
            }
            GPU_PRIMITIVE_COLOR_WORD(p, 0) = ((lum & 0xFF) << 0x10) | ((lum & 0xFF) << 8) | (lum & 0xFF);
            setlen(p, 4);
            setcode(p, 0x66);
            p->clut = GetClut(0x50, 0x101);
            p->u0   = 0x80;
            p->w    = 8;
            p->h    = 0x10;
            p->x0   = 0x82;
            p->v0   = 0;
            p->y0   = -7;
            addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x1C], p);
            rightDr        = gGpuPrimCursor;
            gGpuPrimCursor = rightDr + 1;
            setDrawTPage(rightDr, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue - 0x1C], rightDr);
            return;
        }
        i++;
    }
}

void Gp_HelpPanelTask(Task* arg0)
{
    UiObject* obj;
    s32       status;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrHelp);
    switch (arg0->state) {
        case 0:
            CdCmd_EnqueueLoadFile(8, D_8010F13D, 0);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Ui_SpawnFromDesc(&D_8010F178, 0, 0, 1, obj);
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            textDrawUiLines(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x14, Fs_GetChunkPayload(), 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
            status = obj->panel.control.word;
            if (status == 1) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | PAD_BUTTON_TRIANGLE) != 0) {
                    obj->resultValue = status;
                    obj->result      = USER_INTERFACE_RESULT_CONFIRM;
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                    obj->result = USER_INTERFACE_RESULT_CANCEL;
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                }
            }
            break;
    }
}

void Gp_DrawMapName(Task* arg0)
{
    TextDrawReq      req;
    TextDrawReq      req2;
    GameSession*     session;
    MenuMapAreaName* names;
    u8*              text;
    UiObject*        obj;
    s32              width;

    session = gGameSession;
    names   = Gp_MapNameTables[session->location.loc.stage - 1];
    obj     = arg0->spawnArg2.pointer;
    if (names != NULL) {
        text = names[session->location.loc.area - 1].text;
        if (arg0->state == 0) {
            req.x          = 0;
            req.y          = 0;
            req.otIndex    = obj->panel.otIndex.signedValue + 1;
            req.colorRgb   = 0;
            req.glyphTable = TEXT_GLYPH_TABLE_LARGE;
            req.alignment  = TEXT_ALIGNMENT_RIGHT;
            req.drawMode   = TEXT_DRAW_FILL_ONLY;
            textAlignLine(&req, text);
            width = -req.x + 4;
            uiSetPanelContentSize(&(obj)->panel, width, uiGetTextRowsHeight(1));
            arg0->state = arg0->state + 1;
        }
        req2.x          = obj->panel.contentLeft.unsignedValue + (obj->panel.contentOriginX.unsignedValue + 2);
        req2.y          = obj->panel.contentTop.unsignedValue + (obj->panel.contentOriginY.unsignedValue + 0xB);
        req2.otIndex    = obj->panel.otIndex.signedValue + 1;
        req2.colorRgb   = 0x806020;
        req2.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        req2.alignment  = TEXT_ALIGNMENT_LEFT;
        req2.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req2, text);
    }
}

void Gp_MapTask(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = Gp_MapTaskStates;
    handlers.funcs[arg0->state](arg0);
}

void Gp_MapPanelInit(Task* arg0)
{
    RECT          rect;
    GameSession*  session;
    MenuMapArea** table;
    s32           idx;
    u8            f6;
    MenuMapArea*  recs;
    u8            val;

    if (gDisplayState.keepGraphics == 0) {
        rect.x = 0x380;
        rect.w = 0x80;
        rect.y = 0;
        rect.h = 0x100;
        Display_SetDrawMode(0);
        StoreImage2(&rect, (u_long*)(Gpu_PrimHeapBase - 0x25800));
    }
    Gp_RebuildAreaIdBits();
    session      = gGameSession;
    table        = Gp_MapRecTables;
    idx          = session->location.loc.stage - 1;
    f6           = session->location.loc.area;
    recs         = table[idx];
    recs         = recs + f6;
    val          = recs->page;
    Gp_MapRoomId = val;
    Gp_EnqueueMapRoomCd();
    arg0->state = arg0->state + 1;
}

void Gp_MapFirstDrawTask(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2.pointer;
    if (CdCmd_IsIdle() & 0xFFFF) {
        obj->panel.animationTicks = 1;
        Gp_DrawMapCursor(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        Gp_DrawMapMarks(arg0);
        func_800D15D0(arg0);
        arg0->state = arg0->state + 1;
    } else {
        obj->panel.animationTicks = (u16)obj->panel.animationTicks + 1;
    }
}

void Gp_MapDrawTask(Task* arg0)
{
    RECT rect;

    if (arg0->spawnArg1.value != 0) {
        return;
    }

    arg0->killCountdown--;
    if (arg0->killCountdown == 0) {
        if (gDisplayState.keepGraphics == 0) {
            rect.x = 0x380;
            rect.w = 0x80;
            rect.y = 0;
            rect.h = 0x100;
            Gp_LoadViewImages();
            LoadImage2(&rect, (u_long*)(Gpu_PrimHeapBase - 0x25800));
        }
        arg0->spawnArg1.value++;
    } else if (arg0->killCountdown >= 2) {
        Gp_DrawMapCursor(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        Gp_DrawMapMarks(arg0);
    }
}

static void func_800D1F90(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2.pointer;
    displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
    arg0->killCountdown       = 4;
    obj->panel.animationTicks = 0;
    arg0->spawnArg1.value     = 0;
}

static u8 Gp_GetMapRoomId(void)
{
    GameSession*  session;
    MenuMapArea** table;
    s32           idx;
    u8            f6;
    MenuMapArea*  recs;

    session = gGameSession;
    table   = Gp_MapRecTables;
    idx     = session->location.loc.stage - 1;
    f6      = session->location.loc.area;
    recs    = table[idx];
    recs    = recs + f6;

    Gp_MapRoomId = recs->page;
    return Gp_MapRoomId;
}

static void func_800D2020(u8 arg0)
{
    RECT rect;

    if (gDisplayState.keepGraphics != 0) {
        return;
    }

    rect.x = 0x380;
    rect.w = 0x80;
    rect.h = 0x100;
    rect.y = 0;
    if (arg0 == 0) {
        Display_SetDrawMode(0);
        StoreImage2(&rect, (u_long*)(Gpu_PrimHeapBase - 0x25800));
    } else {
        Gp_LoadViewImages();
        LoadImage2(&rect, (u_long*)(Gpu_PrimHeapBase - 0x25800));
    }
}

void Gp_PeMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     owner;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    obj         = arg0->spawnArg2.pointer;
    owner       = obj->owner;
    obj->result = USER_INTERFACE_RESULT_NONE;
    menu        = &D_8010F5D0;
    if (owner->state == 0) {
        Ui_LayoutListPanel(menu, &(obj)->panel);
        owner->state = owner->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
    head = owner->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->result;
            next     = child->nextSibling;
            switch (flag) {
                case USER_INTERFACE_RESULT_CONFIRM:
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    uiStartTreeClosing(childObj, childObj->owner);
                    break;
                case USER_INTERFACE_RESULT_DISMISS:
                    obj->result = USER_INTERFACE_RESULT_CONFIRM;
                    break;
                case USER_INTERFACE_RESULT_CANCEL:
                    obj->result = flag;
                    break;
            }
            child = next;
        } while (child != owner->firstChild);
    }
}

void Gp_DrawReviveCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         flags;
    s32         item;

    flags = arg1->owner->spawnArg1.value;
    if (flags & 3) {
        req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
        req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
        req.otIndex    = arg1->panel.otIndex.signedValue + 1;
        req.colorRgb   = arg0->colorRgb;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, Gp_StrStrengthen);
    } else {
        req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
        req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
        req.otIndex    = arg1->panel.otIndex.signedValue + 1;
        req.colorRgb   = arg0->colorRgb;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, Gp_StrRevive);
    }
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            item = -1;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if ((flags & 3) == 3) {
                item = 0xD;
            }
            if (item >= 0) {
                Gp_SpawnItemPrompt(arg1, item, 0, 0);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                Ui_SpawnFromDesc(&D_8010F7A4, flags, 1, 0xA, arg1);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
    }
}

void Gp_PeCommandMenuTask(Task* arg0)
{
    UiObject*          obj;
    UiList*            menu;
    Task*              owner;
    Task*              child;
    Task*              next;
    Task*              head;
    UiObject*          childObj;
    UiListRowCallback* table;
    s32                flag;
    s32                two;

    menu = &D_8010F5FC;
    obj  = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        table                               = menu->rowCallbacks;
        table[0]                            = Gp_DrawReviveCmd;
        table[1]                            = Gp_DrawPeSlotCmd;
        two                                 = 2;
        menu->visibleRowCount.unsignedValue = two;
        menu->itemCount                     = two;
        if ((arg0->spawnArg1.value & 3) != 3) {
            Gp_SetPreviewItem(arg0->spawnArg1.value + 1, 0);
        }
    }
    owner       = obj->owner;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (owner->state == 0) {
        Ui_LayoutListPanel(menu, &(obj)->panel);
        owner->state = owner->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
    head = owner->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->result;
            next     = child->nextSibling;
            switch (flag) {
                case USER_INTERFACE_RESULT_CONFIRM:
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    uiStartTreeClosing(childObj, childObj->owner);
                    break;
                case USER_INTERFACE_RESULT_DISMISS:
                    obj->result = USER_INTERFACE_RESULT_CONFIRM;
                    break;
                case USER_INTERFACE_RESULT_CANCEL:
                    obj->result = flag;
                    break;
            }
            child = next;
        } while (child != owner->firstChild);
    }
}

void Gp_DiscardWarnTask(Task* arg0)
{
    InventoryItemRow* rec;
    s32               id;
    Task*             child;
    UiObject*         childObj;
    UiObject*         parentObj;
    UiObject*         obj;
    s32               mode;
    u8*               text;
    UiObject*         spawned;

    rec = Gp_SelItemRec;
    id  = rec->itemId;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    mode        = 0x10;
    if (Gp_ItemDescs[id].flags & ITEM_FLAG_NO_DISCARD) {
        mode = 1;
    } else if (((u32)(id - 0xA0) < 0x20U) && (Gp_CountEquippedRelated(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, id) > 0)) {
        mode = 3;
    } else if (Gp_IsEquippedItem(id) != 0) {
        mode = 2;
    }
    if (mode != 0x10) {
        arg0->spawnArg1.value = mode;
        Gp_NoticePanelTask(arg0);
        return;
    }
    text = Gp_PromptTexts[0];
    if (arg0->state == 0) {
        uiSizePanelForTextWide(&(obj)->panel, text);
        spawned = func_800CD89C(obj);
        if (spawned != NULL) {
            spawned->panel.bounds.unsignedRect.x = (obj->panel.bounds.unsignedRect.x + obj->panel.bounds.unsignedRect.w) - 0x18;
        }
        arg0->state += 1;
    }
    uiDrawPanelLabelWithChildFocus(&(obj)->panel, Gp_StrAttention2);
    textDrawUiLines(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, text, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);

    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        if (childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
            parentObj = arg0->parent->spawnArg2.pointer;
            if (childObj->resultValue == 0x33) {
                if ((u32)(id - 0x80) < 0x20U) {
                    EquipmentWeaponLoad* slot;
                    PlayerStatus*        cfg;

                    slot = Gp_GetItemSlot(id);
                    cfg  = &gPlayerStatus;
                    Gp_ClearEquipSlot(id);
                    slot->field_4 = 0;
                    if (cfg->weapon == (id - 0x7F)) {
                        cfg->weapon = PLAYER_STATUS_EQUIPMENT_NONE;
                    }
                } else if ((u32)(id - 0xA0) < 0x20U) {
                    s32                  i;
                    EquipmentWeaponLoad* slot;

                    i = 0x80;
                    do {
                        slot = Gp_GetItemSlot(i);
                        if (slot->primaryItemId == id) {
                            slot->primaryItemId = INVENTORY_ITEM_NONE;
                            slot->primaryQty    = 0;
                        }
                        if (slot->secondaryItemId == id) {
                            slot->secondaryItemId = INVENTORY_ITEM_NONE;
                            slot->secondaryQty    = 0;
                        }
                        i += 1;
                    } while (i < 0xA0);
                } else if ((u32)(id - 0x60) < 0x20U) {
                    PlayerStatus* cfg;

                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus[id - 0x60] = 0;
                    cfg                                                                = &gPlayerStatus;
                    if (cfg->armor == (id - 0x5F)) {
                        cfg->armor = PLAYER_STATUS_EQUIPMENT_NONE;
                    }
                }
                Gp_RemoveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, rec, -1);
            }
            parentObj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_DrawPeSlotRow(UiList* arg0, UiObject* arg1)
{
    s32       count;
    s32       item;
    s32       idx;
    s32       slot;
    s32       off;
    s32       base;
    s32       status;
    s32       one;
    UiObject* obj;

    idx   = arg1->owner->spawnArg1.value;
    slot  = arg0->currentItemIndex;
    count = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[slot + idx * 3];
    off   = idx * 16;
    base  = slot * 4 + 0x300;
    item  = off + base + count;
    if (count == 0) {
        arg0->colorRgb = Ui_LookupTable(arg1, 2);
    }
    Gp_DrawItemLabel(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 0);
    if (count != 0) {
        func_800C2538(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, count, arg0->colorRgb);
    }
    status = arg1->panel.control.word;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            Ui_SetHolderParamAlt(item, 0, 0);
        }
    }
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        Gp_SetPreviewItem(item, 0);
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            one = 1;
            obj = Ui_SpawnFromDesc(&D_8010F670, item, one, one, arg1);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if (obj != NULL) {
                uiPositionRowDialog(&(obj)->panel, arg0, &(arg1)->panel);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            one = 1;
            Ui_SpawnFromDesc(&D_8010F7F8, item, one, one, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void func_800D29B0(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    UiObject* childObj;
    u8*       levels;
    s32       flag;
    s32       last;
    s32       textIndex;

    obj          = arg0->spawnArg2.pointer;
    textIndex    = arg0->spawnArg1.value;
    arg0->status = 0;
    menu         = &D_80114DF8[arg0->spawnArg1.value];
    uiDrawPanelLabel(&(obj)->panel, D_8010F644[textIndex]);
    if (arg0->state == 0) {
        menu->rowCallbacks                        = D_8010F620;
        menu->itemCount                           = 3;
        menu->visibleRowCount.unsignedValue       = 3;
        menu->wrapNavigation                      = 0;
        menu->rowHeight                           = 0xF;
        menu->currentItemIndex                    = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        levels = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[arg0->spawnArg1.value * 3];
        if (levels[2] != 0 || (levels[0] == 3 && levels[1] == levels[0])) {
            menu->itemCount = menu->visibleRowCount.unsignedValue = 3;
        } else {
            menu->itemCount = menu->visibleRowCount.unsignedValue = 2;
        }
        menu->firstVisibleItemIndex.unsignedValue = 0;
        menu->selectedItemIndex                   = 0;
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        if (arg0->spawnArg1.value & 1) {
            obj->panel.bounds.unsignedRect.y = obj->panel.bounds.unsignedRect.h - 0x50;
        }
        arg0->state++;
    }
    levels = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[arg0->spawnArg1.value * 3];
    if (levels[2] != 0 || (levels[0] == 3 && levels[1] == levels[0])) {
        menu->itemCount = menu->visibleRowCount.unsignedValue = 3;
    } else {
        menu->itemCount = menu->visibleRowCount.unsignedValue = 2;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (menu->actionResult == USER_INTERFACE_LIST_ACTION_AT_END && !(arg0->spawnArg1.value & 1)) {
            UiObject* verticalObj;
            UiList*   verticalMenu;

            verticalObj  = arg0->nextSibling->spawnArg2.pointer;
            verticalMenu = &D_80114DF8[arg0->spawnArg1.value] + 1;
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            verticalObj->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
            verticalMenu->selectedItemIndex = 0;
            obj->panel.control.word         = USER_INTERFACE_PANEL_INACTIVE;
        } else if (menu->actionResult == USER_INTERFACE_LIST_ACTION_AT_START && (arg0->spawnArg1.value & 1)) {
            UiObject* verticalObj;
            UiList*   verticalMenu;

            verticalObj  = arg0->nextSibling->nextSibling->nextSibling->spawnArg2.pointer;
            verticalMenu = &D_80114DF8[arg0->spawnArg1.value] - 1;
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            verticalObj->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
            verticalMenu->selectedItemIndex = verticalMenu->itemCount - 1;
            obj->panel.control.word         = USER_INTERFACE_PANEL_INACTIVE;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
            if (!(arg0->spawnArg1.value & 2)) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                    UiObject* nextObj;
                    UiList*   nextMenu;

                    nextObj  = arg0->nextSibling->nextSibling->spawnArg2.pointer;
                    nextMenu = &D_80114DF8[arg0->spawnArg1.value] + 2;
                    sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                    nextObj->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
                    last                        = nextMenu->itemCount - 1;
                    nextMenu->selectedItemIndex = menu->selectedItemIndex;
                    if (last < nextMenu->selectedItemIndex) {
                        nextMenu->selectedItemIndex = last;
                    }
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
                UiObject* nextObj;
                UiList*   nextMenu;

                nextObj  = arg0->nextSibling->nextSibling->spawnArg2.pointer;
                nextMenu = &D_80114DF8[arg0->spawnArg1.value] - 2;
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                nextObj->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
                last                        = nextMenu->itemCount - 1;
                nextMenu->selectedItemIndex = menu->selectedItemIndex;
                if (last < nextMenu->selectedItemIndex) {
                    nextMenu->selectedItemIndex = last;
                }
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
    }
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        flag     = childObj->result;
        if (flag == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(childObj, childObj->owner);
            obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
        } else if (flag == USER_INTERFACE_RESULT_CANCEL) {
            obj->result = flag;
        }
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_FOCUS_TRANSFER) {
        obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }
}

void Gp_DrawCastCostLines(UiObject* arg0, s32 arg1)
{
    TextDrawReq req;
    s16         y;
    s32         mask;
    s32         lineY;
    s32         color;
    s32         one;
    u8*         text;

    y     = arg0->panel.contentTop.unsignedValue;
    mask  = arg1 & 3;
    lineY = y + 0xF;
    text  = Gp_GetItemText(arg1, 1, 1);
    color = 0x606060;
    one   = 1;
    textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, lineY, text, color, one, TEXT_ALIGNMENT_LEFT);
    text = Gp_GetItemText(arg1, 2, one);
    textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, y + 0x1E, text, color, one, TEXT_ALIGNMENT_LEFT);
    y     = arg0->panel.contentTop.unsignedValue;
    lineY = y + 0xF;
    uiDrawVerticalSeparator(&(arg0)->panel, y, arg0->panel.contentBottom.signedValue, 0x2F);
    if (mask) {
        req.x          = arg0->panel.contentOriginX.unsignedValue + 0x34;
        req.y          = (s16)(arg0->panel.contentOriginY.unsignedValue - 6) + lineY;
        req.otIndex    = arg0->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, Gp_StrCastCost);
        func_800D3660(arg0, arg1, 0, 0x34, y + 0x1A, ATTACHMENT_LEVEL_CAST_COST);
    }
}

void Gp_NoticePanelTask(Task* arg0)
{
    UiObject* obj;
    s32       one;
    u8*       text;
    s32       color;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrNotice3);

    color = 0x606060;
    text  = Gp_NoticeTexts[(u16)arg0->spawnArg1.value];

    if (arg0->state == 0) {
        uiSizePanelForTextDefault(&(obj)->panel, text);
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }

    one = 1;
    {
        s32 drawMode = one;

        textDrawUiLines(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, text, color, drawMode, TEXT_ALIGNMENT_LEFT);
    }

    arg0->killCountdown--;
    if (obj->panel.control.word == one) {
        if ((arg0->killCountdown <= 0) || (padCheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            obj->result         = USER_INTERFACE_RESULT_CONFIRM;
            arg0->killCountdown = 0x7FFF;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }

    if ((s16)(arg0->spawnArg1.value >> 16) == 0) {
        if (obj->result == USER_INTERFACE_RESULT_CONFIRM) {
            obj->result = USER_INTERFACE_RESULT_DISMISS;
        }
    }
}

void Gp_PeUpgradePanelTask(Task* arg0)
{
    u8            str[0x20];
    TextDrawReq   req;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    UiObject*     obj;
    UiObject*     frame;
    UiObject*     childObj;
    Task*         child;
    Task*         next;
    s32           id;
    s32           row2;
    s32           col2;
    s32           lvl2;
    s32           row3;
    s32           col3;
    s32           lvl3;
    s32           row;
    s32           col;
    s32           lvl;
    s32           y;
    s32           bonusIdx;
    s32           x;
    s32           cost;
    PlayerStatus* cfg;
    s32           price;

    obj         = arg0->spawnArg2.pointer;
    id          = arg0->spawnArg1.value;
    obj->result = USER_INTERFACE_RESULT_NONE;

    if (arg0->state == 0) {
        uiSetPanelContentSize(&(obj)->panel, 0, uiGetTextRowsHeight(2) + 1);
        frame = func_800CD814(obj);
        if (frame != NULL) {
            frame->panel.animationTicks         = obj->panel.animationTicks - 8;
            frame->panel.bounds.unsignedRect.y += 4;
        }
        Ui_SpawnFromDesc(D_8010F7C0, id, 0, 1, obj);
        arg0->state = arg0->state + 1;
    }

    x = 0x20;
    y = obj->panel.contentTop.signedValue + 0xF;

    req.x          = obj->panel.contentOriginX.unsignedValue + x;
    req.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 8) + y;
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = 0x606060;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, D_8009720C);

    req2.x          = obj->panel.contentOriginX.unsignedValue + x;
    req2.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 2) + y;
    req2.otIndex    = obj->panel.otIndex.signedValue + 1;
    req2.colorRgb   = 0x606060;
    req2.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req2.alignment  = TEXT_ALIGNMENT_RIGHT;
    req2.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req2, Gp_StrCost);

    row  = ((id + 1) & 0x30) >> 4;
    col  = ((id + 1) & 0xC) >> 2;
    lvl  = (id + 1) & 3;
    cost = Gp_IdParamHi.rows[(row * 3 + col) * 3 + lvl].column.expCost;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
        cost = (cost * 4) / 5;
    } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
        cost = (cost * 2) / 5;
    }
    textDrawUiLine(obj, x + 0x30, y, textItoaSigned(str, cost & 0xFFFF), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    y += 0xF;

    req3.x          = obj->panel.contentOriginX.unsignedValue + x;
    req3.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 8) + y;
    req3.otIndex    = obj->panel.otIndex.signedValue + 1;
    req3.colorRgb   = 0x606060;
    req3.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req3.alignment  = TEXT_ALIGNMENT_RIGHT;
    req3.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req3, Gp_StrBonus);

    req4.x          = obj->panel.contentOriginX.unsignedValue + x;
    req4.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 2) + y;
    req4.otIndex    = obj->panel.otIndex.signedValue + 1;
    req4.colorRgb   = 0x606060;
    req4.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req4.alignment  = TEXT_ALIGNMENT_RIGHT;
    req4.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req4, D_80097220);

    bonusIdx = ATTACHMENT_LEVEL_MP_BONUS;
    row2     = ((id + 1) & 0x30) >> 4;
    col2     = ((id + 1) & 0xC) >> 2;
    lvl2     = (id + 1) & 3;
    textDrawUiLine(obj, x + 0x30, y, textItoaSigned(str, Gp_IdParamHi.rows[(row2 * 3 + col2) * 3 + lvl2].value[bonusIdx]),
                   0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    if (arg0->firstChild != NULL) {
        child = arg0->firstChild;
        do {
            childObj = child->spawnArg2.pointer;
            next     = child->nextSibling;
            row3     = ((id + 1) & 0x30) >> 4;
            col3     = ((id + 1) & 0xC) >> 2;
            lvl3     = (id + 1) & 3;
            if (childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
                if (childObj->resultValue == 0x33) {
                    cfg   = &gPlayerStatus;
                    price = Gp_IdParamHi.rows[(row3 * 3 + col3) * 3 + lvl3].column.expCost;
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
                        price = (price * 4) / 5;
                    } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
                        price = (price * 2) / 5;
                    }
                    if (cfg->exp < (price & 0xFFFF)) {
                        Ui_SpawnFromDesc(&D_8010F788, 0xC, 1, 1, obj);
                        uiStartTreeClosing(childObj, childObj->owner);
                    } else {
                        price = Gp_IdParamHi.rows[(row3 * 3 + col3) * 3 + lvl3].column.expCost;
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
                            price = (price * 4) / 5;
                        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
                            price = (price * 2) / 5;
                        }
                        cfg->exp                                                                                         -= price & 0xFFFF;
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[((id & 0xC) >> 2) + ((id & 0x30) >> 4) * 3] = (id & 3) + 1;
                        Gp_RecalcMaxMp();
                        cfg->mp        = cfg->mpMax;
                        Gp_HpMpWork.mp = cfg->mp;
                        obj->result    = USER_INTERFACE_RESULT_DISMISS;
                    }
                } else {
                    obj->result = USER_INTERFACE_RESULT_DISMISS;
                }
            } else if (childObj->result == USER_INTERFACE_RESULT_DISMISS) {
                obj->result = USER_INTERFACE_RESULT_DISMISS;
            }
            child = next;
        } while (child != arg0->firstChild);
    }
}

static void func_800D3660(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    struct
    {
        u8          buf[0x20];
        TextDrawReq req;
        TextDrawReq req2;
    } loc;
    s32   span;
    s32   width;
    s32   raw;
    s32   val;
    s32   max;
    s32   x;
    SPRT* p;
    s32   rawPrev;
    s32   prev;
    s32   color;
    s32   textY;
    s32   prevId;
    s32   spriteX;
    s32   a;
    s32   b;
    s32   c;
    s32   aPrev;
    s32   bPrev;
    s32   cPrev;
    s32   textY2;
    u8*   text;
    {
        a   = (arg1 & 0x30) >> 4;
        b   = (arg1 & 0xC) >> 2;
        c   = arg1 & 3;
        raw = Gp_IdParamHi.rows[(((a * 3) + b) * 3) + c].value[arg5];
    }
    if (arg5 == 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
            raw = (raw * 4) / 5;
        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
            raw = (raw * 2) / 5;
        }
        width = arg0->panel.contentRight.signedValue;
    } else {
        width = arg0->panel.contentRight.signedValue;
    }
    x    = arg3;
    span = width - x;
    val  = raw & 0xFFFF;
    switch (arg5) {
        case ATTACHMENT_LEVEL_CAST_COST:
            max = 0x3C;
            break;

        case ATTACHMENT_LEVEL_ATP_LOSS:
            max = 0x5F;
            break;

        case ATTACHMENT_LEVEL_MP_BONUS:
            max  = 0x14;
            arg2 = 0;
            break;

        default:
            max = val;
            break;
    }

    if (arg2 == 1) {
        prevId = arg1 - 1;
        {
            aPrev   = (prevId & 0x30) >> 4;
            bPrev   = (prevId & 0xC) >> 2;
            cPrev   = prevId & 3;
            rawPrev = Gp_IdParamHi.rows[(((aPrev * 3) + bPrev) * 3) + cPrev].value[arg5];
        }
        if (arg5 == 0) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
                rawPrev = (rawPrev * 4) / 5;
            } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
                rawPrev = (rawPrev * 2) / 5;
            }
        }
        color              = 0x606060;
        p                  = gGpuPrimCursor;
        p->y0              = (arg0->panel.contentOriginY.unsignedValue + arg4) - 0xC;
        prev               = rawPrev & 0xFFFF;
        loc.req.x          = arg0->panel.contentOriginX.unsignedValue + x;
        textY              = arg0->panel.contentOriginY.unsignedValue - 6;
        loc.req.y          = textY + arg4;
        gGpuPrimCursor     = p + 1;
        loc.req.otIndex    = (arg0->panel.otIndex.signedValue) + 1;
        loc.req.colorRgb   = color;
        loc.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        loc.req.alignment  = TEXT_ALIGNMENT_LEFT;
        loc.req.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&loc.req, textItoaSigned(loc.buf, prev));
        if (prev < val) {
            s32 y;

            y = arg4 - 3;
            uiFillRectInterior(&(arg0)->panel, x, y, (span * prev) / max, 3, 0x1741FU);
            color = 0xD287F;
            uiDrawRaisedRect(&arg0->panel, x, y, ((span * val) / max), 3, 0x1A50FE);
            p->u0                          = 0xA0;
            GPU_PRIMITIVE_COLOR_WORD(p, 0) = color;
            p->y0                          = p->y0 - 1;
        } else {
            if (val < prev) {
                s32 y;

                y = arg4 - 3;
                uiFillRectInterior(&(arg0)->panel, x, y, (span * val) / max, 3, 0x1741FU);
                color = 0x1741F;
                uiDrawRaisedRect(&arg0->panel, x, y, ((span * prev) / max), 3, 1);
                p->u0                          = 0x30;
                GPU_PRIMITIVE_COLOR_WORD(p, 0) = color;
            } else {
                if (val > 0) {
                    uiDrawRaisedRect(&arg0->panel, x, (arg4 - 3), ((span * val) / max), 3, 0x1741F);
                }
                p->u0                          = 0x78;
                GPU_PRIMITIVE_COLOR_WORD(p, 0) = color;
            }
        }
        spriteX = arg0->panel.contentOriginX.unsignedValue + x;
        p->w    = 8;
        p->h    = 8;
        p->v0   = 0x60;
        p->clut = 0x3C09;
        setSprt(p);
        p->x0 = spriteX + 0x14;
        addPrim(gGpuCurrentOt + arg0->panel.otIndex.signedValue + 1, p);
        uiQueueTexturePage((arg0->panel.otIndex.signedValue) + 1, 0);
        loc.req2.x          = (arg0->panel.contentOriginX.unsignedValue + 0x1E) + x;
        textY               = arg0->panel.contentOriginY.unsignedValue - 6;
        loc.req2.y          = textY + arg4;
        loc.req2.otIndex    = (arg0->panel.otIndex.signedValue) + 1;
        loc.req2.colorRgb   = color;
        loc.req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        loc.req2.alignment  = TEXT_ALIGNMENT_LEFT;
        loc.req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&loc.req2, textItoaSigned(loc.buf, val));
    } else {
        if (arg5 == 1) {
            loc.req2.x          = arg0->panel.contentOriginX.unsignedValue + x;
            textY2              = arg0->panel.contentOriginY.unsignedValue - 6;
            loc.req2.y          = textY2 + arg4;
            loc.req2.otIndex    = (arg0->panel.otIndex.signedValue) + 1;
            loc.req2.colorRgb   = 0x606060;
            loc.req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            loc.req2.alignment  = TEXT_ALIGNMENT_LEFT;
            loc.req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            text                = textItoaSignPrefixed(loc.buf, val);
        } else {
            loc.req2.x          = arg0->panel.contentOriginX.unsignedValue + x;
            textY2              = arg0->panel.contentOriginY.unsignedValue - 6;
            loc.req2.y          = textY2 + arg4;
            loc.req2.otIndex    = (arg0->panel.otIndex.signedValue) + 1;
            loc.req2.colorRgb   = 0x606060;
            loc.req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            loc.req2.alignment  = TEXT_ALIGNMENT_LEFT;
            loc.req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            text                = textItoaSigned(loc.buf, val);
        }
        textDrawString(&loc.req2, text);
        if (val > 0) {
            uiDrawRaisedRect(&arg0->panel, x, (arg4 - 3), ((span * val) / max), 3, 0x1741F);
        }
    }
}

static void func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2)
{
    TextDrawReq req;
    TextDrawReq req2;
    TextDrawReq req3;
    s32         color;
    s32         color2;
    s32         x;
    s32         y;
    s32         mask;
    s32         line;
    s32         temp;
    u8*         text;

    if (arg2 == 1) {
        if ((arg1 & 3) == 0) {
            arg2 = 0;
        }
        arg1 += 1;
    }

    color = 0x606060;
    x     = arg0->panel.contentLeft.signedValue + 2;
    y     = arg0->panel.contentTop.signedValue + 0xF;
    mask  = arg1 & 3;
    Gp_DrawItemLabel(arg0, x, y, arg1, color, 0);
    if (mask != 0) {
        func_800C2538(arg0, x, y, mask, color);
    }

    text           = Gp_StrAreaEffect;
    x              = arg0->panel.contentLeft.signedValue + 2;
    line           = arg0->panel.contentTop.signedValue;
    y              = line + 0x21;
    req.x          = arg0->panel.contentOriginX.unsignedValue + x;
    req.y          = arg0->panel.contentOriginY.unsignedValue + line + 0x1C;
    req.otIndex    = arg0->panel.otIndex.signedValue + 1;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, text);

    if (CdCmd_IsIdle() & 0xFFFF) {
        func_800C7AE8(arg0, x, y, 0x200);
    } else {
        func_800C7AE8(arg0, x, y, 0x300);
    }

    color2          = 0x606060;
    text            = Gp_StrCastCost;
    x               = arg0->panel.contentLeft.signedValue + 0x54;
    temp            = arg0->panel.contentTop.signedValue;
    req2.x          = arg0->panel.contentOriginX.unsignedValue + 1 + x;
    req2.y          = arg0->panel.contentOriginY.unsignedValue + temp + 0x24;
    y               = temp + 0x36;
    req2.otIndex    = arg0->panel.otIndex.signedValue + 1;
    req2.colorRgb   = color2;
    req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req2.alignment  = TEXT_ALIGNMENT_LEFT;
    req2.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req2, text);
    func_800D3660(arg0, arg1, arg2, x, y, ATTACHMENT_LEVEL_CAST_COST);

    req3.x          = arg0->panel.contentOriginX.unsignedValue + 1 + x;
    req3.y          = arg0->panel.contentOriginY.unsignedValue + temp + 0x46;
    req3.otIndex    = arg0->panel.otIndex.signedValue + 1;
    req3.colorRgb   = color2;
    req3.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req3.alignment  = TEXT_ALIGNMENT_LEFT;
    req3.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req3, Gp_StrAtpLoss);
    y = temp + 0x58;
    func_800D3660(arg0, arg1, arg2, x, y, ATTACHMENT_LEVEL_ATP_LOSS);
}

void Gp_MapMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    UiObject* childObj;
    s32       x;
    s32       y;
    s32       flag;

    obj         = arg0->spawnArg2.pointer;
    menu        = &D_8010F81C;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        Ui_LayoutListPanel(menu, &(obj)->panel);
        x = 0x96 - ((s16)obj->panel.bounds.unsignedRect.x + (s16)obj->panel.bounds.unsignedRect.w);
        y = 0x6E - ((s16)obj->panel.bounds.unsignedRect.y + (s16)obj->panel.bounds.unsignedRect.h);
        if (x < 0) {
            obj->panel.bounds.unsignedRect.x += x;
        }
        if (y < 0) {
            obj->panel.bounds.unsignedRect.y += y;
        }
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        flag     = childObj->result;
        if ((flag == USER_INTERFACE_RESULT_CANCEL) || (flag == USER_INTERFACE_RESULT_CONFIRM)) {
            obj->result = childObj->result;
        }
    }
}

void Gp_MapScreenTask(Task* arg0)
{
    UiObject* obj;
    s32       one;

    obj = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        one                     = 1;
        obj                     = Ui_SpawnFromDesc(&D_8010F840, arg0->spawnArg1, one, one, NULL);
        arg0->spawnArg2.pointer = obj;
        if (obj != NULL) {
            obj->panel.bounds.unsignedRect.x = (u16)D_80114E8C;
            obj->panel.bounds.unsignedRect.y = (u16)D_80114E90;
        }
        Stage_InitPrimBufOnce();
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen = 1;
        D_80114E88           = 0;
        arg0->state          = arg0->state + 1;
    } else if (arg0->state == 1) {
        if ((obj->result == USER_INTERFACE_RESULT_CANCEL) || (obj->result == USER_INTERFACE_RESULT_CONFIRM)) {
            uiStartTreeClosing(obj, obj->owner);
            arg0->killCountdown = 0xA;
            arg0->state         = 2;
        }
    } else {
        arg0->killCountdown--;
        if (arg0->killCountdown <= 0) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gGameSession->uiOpen = 0;
            taskKill(arg0);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

static void func_800D4270(UiObject* obj, TmdSource* mesh, s32 mode, s32 dp)
{
    RECT                      tw;
    DR_MODE*                  dr;
    s32                       otz;
    u8*                       verts;
    _MenuMapAreaShapeScratch* scratch;
    u32*                      cur;
    s32                       type;
    u32                       word;
    s32                       count;
    s32                       stride;
    s16                       vz;
    s32                       minX;
    s32                       minY;

    otz            = obj->panel.otIndex.signedValue;
    verts          = (u8*)mesh->verts;
    cur            = mesh->stream;
    tw.y           = 0;
    tw.x           = 0;
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapAreaShapeScratch);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    tw.h           = 0xFF;
    tw.w           = 0xFF;
    setTexWindow(dr, &tw);
    addPrim(&gGpuCurrentOt[otz], dr);
    // Every shape is placed at the centre of the map picture.
    scratch->originX = 0;
    scratch->originY = 0;
    while (*cur != TMD_STREAM_GROUP_END) {
        type   = *cur;
        cur   += 2;
        word   = *cur;
        count  = word >> 16;
        stride = word & 0xFFFF;
        cur   += 1;
        if (type == 0x44) {
            if (count > 0) {
                do {
                    POLY_FT4* p4;
                    SVECTOR*  vert;

                    vert           = (SVECTOR*)(verts + (((u16*)cur)[0] & 0xFFF8));
                    p4             = gGpuPrimCursor;
                    gGpuPrimCursor = p4 + 1;
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);
                    p4->x0 = scratch->scaled.vx + scratch->originX;
                    vz     = scratch->scaled.vz;
                    p4->y0 = scratch->originY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[1] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);
                    p4->x1 = scratch->scaled.vx + scratch->originX;
                    vz     = scratch->scaled.vz;
                    p4->y1 = scratch->originY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[2] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);
                    p4->x2 = scratch->scaled.vx + scratch->originX;
                    vz     = scratch->scaled.vz;
                    p4->y2 = scratch->originY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[3] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);
                    p4->x3 = scratch->scaled.vx + scratch->originX;
                    vz     = scratch->scaled.vz;
                    p4->y3 = scratch->originY - vz;
                    if (mode == 0) {
                        minX = p4->x0;
                        if (p4->x1 < minX) {
                            minX = p4->x1;
                        }
                        if (p4->x2 < minX) {
                            minX = p4->x2;
                        }
                        if (p4->x3 < minX) {
                            minX = p4->x3;
                        }
                        minY = p4->y0;
                        if (p4->y1 < minY) {
                            minY = p4->y1;
                        }
                        if (p4->y2 < minY) {
                            minY = p4->y2;
                        }
                        if (p4->y3 < minY) {
                            minY = p4->y3;
                        }
                        p4->clut = 0x3FC0;
                        p4->u0   = (minX & 0x1F) + ((u8)p4->x0 - minX);
                        p4->v0   = (minY & 0x1F) + ((u8)p4->y0 - minY);
                        p4->u1   = (minX & 0x1F) + ((u8)p4->x1 - minX);
                        p4->v1   = (minY & 0x1F) + ((u8)p4->y1 - minY);
                        p4->u2   = (minX & 0x1F) + ((u8)p4->x2 - minX);
                        p4->v2   = (minY & 0x1F) + ((u8)p4->y2 - minY);
                        p4->u3   = (minX & 0x1F) + ((u8)p4->x3 - minX);
                        p4->v3   = (minY & 0x1F) + ((u8)p4->y3 - minY);
                    } else {
                        p4->u0 = p4->x0 - 0x80;
                        p4->u1 = p4->x1 - 0x80;
                        p4->u2 = p4->x2 - 0x80;
                        p4->u3 = p4->x3 - 0x80;
                        p4->v0 = p4->y0 - 0x78;
                        p4->v1 = p4->y1 - 0x78;
                        p4->v2 = p4->y2 - 0x78;
                        p4->v3 = p4->y3 - 0x78;
                        if (mode == 1) {
                            GPU_PRIMITIVE_COLOR_WORD(p4, 0) = GPU_PACK_COLOR_WORD(0x20, 0x20, 0x20, 0);
                        } else if (mode == 2) {
                            GPU_PRIMITIVE_COLOR_WORD(p4, 0) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0xff, 0);
                        } else {
                            GPU_PRIMITIVE_COLOR_WORD(p4, 0) = GPU_PACK_COLOR_WORD(0xff, 0x40, 0x40, 0);
                        }
                        p4->clut = 0x4000;
                    }
                    p4->tpage = 0xAE;
                    setlen(p4, 9);
                    setcode(p4, 0x2C);
                    if (mode == 0) {
                        setcode(p4, 0x2D);
                        addPrim(&gGpuCurrentOt[otz], p4);
                    } else {
                        addPrim(&gGpuCurrentOt[otz] + 1, p4);
                    }
                    cur += stride;
                    count--;
                } while (count > 0);
            }
        } else if (type == 4) {
            if (count > 0) {
                do {
                    POLY_FT3* p3;
                    SVECTOR*  vert;

                    vert           = (SVECTOR*)(verts + (((u16*)cur)[0] & 0xFFF8));
                    p3             = gGpuPrimCursor;
                    gGpuPrimCursor = p3 + 1;
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);

                    vert = (SVECTOR*)(verts + (((u16*)cur)[0] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);
                    p3->x0 = scratch->scaled.vx + scratch->originX;
                    vz     = scratch->scaled.vz;
                    p3->y0 = scratch->originY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[1] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);
                    p3->x1 = scratch->scaled.vx + scratch->originX;
                    vz     = scratch->scaled.vz;
                    p3->y1 = scratch->originY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[2] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);
                    p3->x2 = scratch->scaled.vx + scratch->originX;
                    vz     = scratch->scaled.vz;
                    p3->y2 = scratch->originY - vz;
                    if (mode == 0) {
                        minX = p3->x0;
                        if (p3->x1 < minX) {
                            minX = p3->x1;
                        }
                        if (p3->x2 < minX) {
                            minX = p3->x2;
                        }
                        minY = p3->y0;
                        if (p3->y1 < minY) {
                            minY = p3->y1;
                        }
                        if (p3->y2 < minY) {
                            minY = p3->y2;
                        }
                        p3->clut = 0x3FC0;
                        p3->u0   = (minX & 0x1F) + ((u8)p3->x0 - minX);
                        p3->v0   = (minY & 0x1F) + ((u8)p3->y0 - minY);
                        p3->u1   = (minX & 0x1F) + ((u8)p3->x1 - minX);
                        p3->v1   = (minY & 0x1F) + ((u8)p3->y1 - minY);
                        p3->u2   = (minX & 0x1F) + ((u8)p3->x2 - minX);
                        p3->v2   = (minY & 0x1F) + ((u8)p3->y2 - minY);
                    } else {
                        p3->u0 = p3->x0 - 0x80;
                        p3->u1 = p3->x1 - 0x80;
                        p3->u2 = p3->x2 - 0x80;
                        p3->v0 = p3->y0 - 0x78;
                        p3->v1 = p3->y1 - 0x78;
                        p3->v2 = p3->y2 - 0x78;
                        if (mode == 1) {
                            GPU_PRIMITIVE_COLOR_WORD(p3, 0) = GPU_PACK_COLOR_WORD(0x20, 0x20, 0x20, 0);
                        } else if (mode == 2) {
                            GPU_PRIMITIVE_COLOR_WORD(p3, 0) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0xff, 0);
                        } else {
                            GPU_PRIMITIVE_COLOR_WORD(p3, 0) = GPU_PACK_COLOR_WORD(0xff, 0x40, 0x40, 0);
                        }
                        p3->clut = 0x4000;
                    }
                    p3->tpage = 0xAE;
                    setlen(p3, 7);
                    setcode(p3, 0x24);
                    if (mode == 0) {
                        setcode(p3, 0x25);
                        addPrim(&gGpuCurrentOt[otz], p3);
                    } else {
                        addPrim(&gGpuCurrentOt[otz] + 1, p3);
                    }
                    cur += stride;
                    count--;
                } while (count > 0);
            }
        }
    }
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    tw.x           = 0;
    tw.y           = 0;
    tw.w           = 0x20;
    tw.h           = 0x20;
    setTexWindow(dr, &tw);
    addPrim(&gGpuCurrentOt[otz], dr);
    SCRATCH_STACK_RELEASE_BLOCK(_MenuMapAreaShapeScratch);
}

s32 func_800D4D2C(s32 arg0)
{
    s32 val;

    val                           = *(volatile s32*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
    *(volatile s32*)&Wip_UiHolder = 0;
    switch (val & ~0xFFFF) {
        case 0x1130000:
            Display_InitModeObj(&D_mist_parking_8018668C, arg0, 0, 0);
            break;
        case 0x21B0000:
            Display_InitModeObj(&D_dryfield_trailer_coach_80183F84, arg0, 0, 0);
            break;
        case 0x31B0000:
            Display_InitModeObj(&D_dryfield_night_trailer_coach_801846D0, arg0, 0, 0);
            break;
        case 0x3180000:
            Display_InitModeObj(&D_dryfield_night_garage_80181C2C, arg0, 0, 0);
            break;
        case 0x40D0000:
            Display_InitModeObj(&D_shelter_b1_armory_801824D0, arg0, 0, 0);
            break;
        case 0x4140000:
            Display_InitModeObj(&D_shelter_b1_underground_parking_801871F0.desc, arg0, 0, 0);
            break;
        case 0x5040000:
            Display_InitModeObj(&D_shelter_1f_heliport_80181188, arg0, 0, 0);
            break;
        default:
            return 0;
    }
    return 1;
}

UiObject* Gp_SpawnItemPrompt(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 one;

    one    = 1;
    arg3 <<= 16;
    return Ui_SpawnFromDesc(&D_8010F788, arg3 | arg1, one, one, arg0);
}

s32 func_800D4E78(s32 arg0, s32 arg1, s32 arg2)
{
    D_80114E94 = arg2;
    D_80114E8C = arg0;
    D_80114E90 = arg1;
    Display_InitModeObj(&D_8010F85C, arg2, 0, 0);
    return 1;
}

s32 func_800D4EC0(void)
{
    return D_80114E88;
}

void Gp_DrawUseAttachCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&req, Gp_StrUse2);

    status = arg1->panel.control.word;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            Ui_SetHolderParam(Gp_StrUseAttachHelp, 0, 0);
        }
    }

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg1->resultValue = 6;
            arg1->result      = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_DrawKeyItemCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&req, Gp_StrKeyItem2);

    status = arg1->panel.control.word;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            Ui_SetHolderParam(Gp_StrUseKeyHelp, 0, 0);
        }
    }

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg1->resultValue = 8;
            arg1->result      = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

s32 func_800D50D4(s32 arg0, s32 arg1)
{
    s32 a;
    s32 b;
    s32 c;
    s32 val;

    a   = (arg0 & 0x30) >> 4;
    b   = (arg0 & 0xC) >> 2;
    c   = arg0 & 3;
    val = Gp_IdParamHi.rows[(a * 3 + b) * 3 + c].value[arg1];
    if (arg1 == ATTACHMENT_LEVEL_EXP_COST) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
            val = (val * 4) / 5;
        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
            val = (val * 2) / 5;
        }
    }
    return val & 0xFFFF;
}

void Gp_DrawPeSlotCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrCancel2);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg1->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_DrawMapCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&req, Gp_StrMap);

    status = arg1->panel.control.word;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            Ui_SetHolderParam(Gp_StrCheckMap, 0, 0);
        }
    }

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            arg1->resultValue = 0x100;
            arg1->result      = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_DrawDiscardCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrDiscard2);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Ui_SpawnFromDesc(&D_8010F6FC, 0, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

static void Gp_DrawExamineCmd(UiObject* arg0, Task* arg1, u8* arg2, s32 arg3)
{
    s32 one;

    if (arg1->state == 0) {
        uiSizePanelForTextDefault(&(arg0)->panel, arg2);
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    one = 1;
    {
        s32 drawMode = one;

        textDrawUiLines(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0xF, arg2, arg3, drawMode, TEXT_ALIGNMENT_LEFT);
    }

    arg1->killCountdown--;
    if (arg0->panel.control.word == one) {
        if ((arg1->killCountdown <= 0) || (padCheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->result        = USER_INTERFACE_RESULT_CONFIRM;
            arg1->killCountdown = 0x7FFF;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            arg0->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

static void Gp_DrawPushCmd(UiObject* arg0, Task* arg1)
{
    s32 one;
    u8* text;
    s32 color;

    color = 0x606060;
    text  = Gp_NoticeTexts[(u16)arg1->spawnArg1.value];

    if (arg1->state == 0) {
        uiSizePanelForTextDefault(&(arg0)->panel, text);
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    one = 1;
    {
        s32 drawMode = one;

        textDrawUiLines(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0xF, text, color, drawMode, TEXT_ALIGNMENT_LEFT);
    }

    arg1->killCountdown--;
    if (arg0->panel.control.word == one) {
        if ((arg1->killCountdown <= 0) || (padCheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->result        = USER_INTERFACE_RESULT_CONFIRM;
            arg1->killCountdown = 0x7FFF;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            arg0->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

void Gp_DrawNextLevelCmd(Task* arg0)
{
    UiObject* obj;
    s32       spawnArg;
    s32       saved;
    s32       y;
    u8*       text;

    obj                     = arg0->spawnArg2.pointer;
    spawnArg                = arg0->spawnArg1.value;
    saved                   = obj->panel.control.word;
    obj->result             = USER_INTERFACE_RESULT_NONE;
    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrNextLevel);
    obj->panel.control.word = saved;
    func_800D3D98(obj, spawnArg, 1);
    y    = obj->panel.contentBottom.signedValue;
    text = Gp_GetItemText(spawnArg + 1, 1, 1);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, y - 0xF, text, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    text = Gp_GetItemText(spawnArg + 1, 2, 1);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, y, text, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

void func_800D573C(Task* arg0)
{
    UiObject* obj;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    Gp_UseHealItemPanel(obj, arg0, arg0->spawnArg1.value);
}

void Gp_DrawSpecsCmd(Task* arg0)
{
    UiObject* obj;
    s32       spawnArg;
    const u8* text;

    obj         = arg0->spawnArg2.pointer;
    spawnArg    = arg0->spawnArg1.value;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrSpecs2);
    if ((spawnArg & 3) == 0) {
        spawnArg += 1;
    }
    func_800D3D98(obj, spawnArg, 0);
    if (CdCmd_IsIdle() & 0xFFFF) {
        text = textSkipLines(Fs_GetChunkPayload(), 4);
        textDrawUiLines(obj, obj->panel.contentLeft.signedValue + 2, 0x14, text, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel | PAD_BUTTON_TRIANGLE) != 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

void Gp_DrawExaminePushCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    char*       text;
    s32         one;
    s32         confirm;

    text = Gp_StrExamine;
    one  = 1;
    if (arg1->owner->spawnArg1.value == one) {
        text = Gp_StrPush;
    }
    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = one;
    textDrawString(&req, text);
    confirm = arg0->rowInputEnabled;
    if (confirm == one) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            D_80114E88   = confirm;
            arg1->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_DrawItemCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrItem2);
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            Ui_SpawnFromDesc(&D_8010EFBC, 0, 1, 1, arg1);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiStartPanelHiding(arg1, arg1->owner);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void func_800D5A48(Task* arg0)
{
    UiObject* obj;
    s32       flags;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    flags       = 0;
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value == 0) {
            uiSetPanelContentSize(&(obj)->panel, 0x84, 0x64);
        } else {
            uiSetPanelContentSize(&(obj)->panel, 0x84, 0x83);
        }
        arg0->state = arg0->state + 1;
    }
    if (arg0->spawnArg1.value != 0) {
        flags |= 0x400;
    }
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        flags |= 0x100;
    }
    func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);
}
