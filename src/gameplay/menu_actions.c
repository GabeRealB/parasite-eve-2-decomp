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
#include "gameplay/gameflag.h"
#include "cdcmd.h"
#include "direction_input.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "loading.h"
#include "menu_map.h"
#include "gameplay/map.h"
#include "gameplay/planar_reflection.h"

#include "main/areas.h"
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

extern char D_8010F8F0[];

extern char Gp_StrReturnGame[];

extern char D_8010F91C[];

extern char Gp_StrUseAttachHelp[];

extern char Gp_StrUseKeyHelp[];

extern char Gp_StrCheckMap[];

/// Exploration-word boundary and map-picture variants used by the map passes.
enum {
    MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA   = 33,
    MENU_MAP_SHELTER_RESERVOIR_PICTURE_OFFSET = 1,
    MENU_MAP_NEO_ARK_ALTAR_PICTURE_OFFSET     = 3
};

static inline s32 _gpIsItemRowFree(InventoryItemRow* arg0);

static InventoryItemRow* func_800CE980(InventoryItemRange* arg0, s32 arg1);

static s32 func_800CEA00(InventoryItemRange* arg0, s32 arg1);

static s32 _equipmentIsSelectedItem(s32 itemId);

static s32 func_800CEC5C(InventoryItemRow* arg0);

static InventoryItemRow* func_800CECC0(InventoryItemRange* arg0, s32 arg1);

static UiObject* Gp_OpenItemCmdMenu(UiList* arg0, UiObject* arg1, InventoryItemRow* arg2, s32 arg3);

static void func_800CEE5C(UiObject* arg0);

static s32 func_800CF204(CdCmdEntry* entry);

static s32 Gp_NthStockRelated(InventoryItemRange* arg0, s32 arg1, s32 arg2);

static void _itemMenuOpenUsePanel(UiList* unusedList, UiObject* parent);

static void _menuMapDrawPlayerCursor(const Task* mapTask);

static void _menuMapDrawPicture(Task* mapTask);

static void _menuMapDrawAreas(const Task* mapTask);

static void _menuMapDrawFlagMarkers(const Task* mapTask);

static s32 _menuMapDrawAreaIcons(const Task* task, u8 area, u8 unvisited);

static void _menuMapLoadPage(void);

static s8 _menuMapPageIsAvailable(u32 page, u8 flagId);

static void _menuMapDrawPageArrows(const Task* mapTask);

static void _menuMapPrepareClosing(Task* mapTask);

static u8 Gp_GetMapRoomId(void);

static void func_800D2020(u8 arg0);

static void _itemMenuDrawAbilityParameterBar(UiObject* object, s32 abilityId, s32 comparePreviousLevel, s32 barX, s32 valueY, s32 column);

static void _itemMenuDrawPeSpecifications(UiObject* object, s32 abilityId, s32 nextLevel);

/// Texture treatment for a map area's shape, independent of its visited flags.
typedef enum {
    MENU_MAP_AREA_FILL_PATTERN = 0,
    MENU_MAP_AREA_FILL_DIM     = 1,
    MENU_MAP_AREA_FILL_BLUE    = 2,
    MENU_MAP_AREA_FILL_RED     = 3
} _MenuMapAreaShapeFill;

static void _menuMapDrawAreaShape(UiObject* mapObject, const TmdSource* areaModel, _MenuMapAreaShapeFill fillMode, s32 scaleQ12);

static void Gp_DrawExamineCmd(UiObject* arg0, Task* arg1, u8* arg2, s32 arg3);

static void Gp_DrawPushCmd(UiObject* arg0, Task* arg1);

/// Bit fields of the packed PE menu id; higher catalogue bits do not select a row.
enum {
    ATTACHMENT_MENU_ELEMENT_MASK  = 0x30,
    ATTACHMENT_MENU_ELEMENT_SHIFT = 4,
    ATTACHMENT_MENU_ENERGY_MASK   = 0xC,
    ATTACHMENT_MENU_ENERGY_SHIFT  = 2,
    ATTACHMENT_MENU_LEVEL_MASK    = 3
};

/// Constants used by this translation unit's item-menu callbacks.
enum {
    ITEM_MENU_STATE_INITIAL                  = 0,
    ITEM_MENU_TEXT_COLOR_RGB                 = 0x606060,
    ITEM_MENU_PE_DISPLAY_CURRENT_LEVEL       = 0,
    ITEM_MENU_PE_DISPLAY_NEXT_LEVEL          = 1,
    ITEM_MENU_COMMAND_USE_ATTACH             = 6,
    ITEM_MENU_COMMAND_KEY_ITEMS              = 8,
    ITEM_MENU_NOTICE_NONE                    = -1,
    ITEM_MENU_DISCARD_ALLOWED                = 0x10,
    ITEM_MENU_NOTICE_CANNOT_DISCARD          = 1,
    ITEM_MENU_NOTICE_CANNOT_DISCARD_EQUIPPED = 2,
    ITEM_MENU_NOTICE_CANNOT_DISCARD_LOADED   = 3,
    ITEM_MENU_NOTICE_INSUFFICIENT_EXP        = 12,
    ITEM_MENU_NOTICE_MAX_PE_LEVEL            = 13,
    ITEM_MENU_PROMPT_CONFIRM_DISCARD         = 0,
    ITEM_MENU_ARMOR_ITEM_FIRST               = 0x60,
    ITEM_MENU_PE_MAX_LEVEL                   = 3,
    ITEM_MENU_PE_ABILITIES_PER_ELEMENT       = 3,
    ITEM_MENU_PE_ELEMENT_LOWER_ROW_BIT       = 1,
    ITEM_MENU_PE_ELEMENT_RIGHT_COLUMN_BIT    = 2,
    ITEM_MENU_PE_ROW_HEIGHT_PIXELS           = 15,
    ITEM_MENU_PE_PREVIEW_DESCRIPTION_LINE    = 4,
    ITEM_MENU_PE_COMMAND_ROW_COUNT           = 2
};

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

/// Fills a command-label request in list-row pixels relative to panel content.
///
/// The request is a writable lvalue; list/object are live pointers. Arguments
/// must have no side effects: request, list and object are evaluated repeatedly.
/// Colour, font and draw mode retain their destination widths. Uses the text
/// alignment constant from the text interface; no local identifiers are captured.
#define ITEM_MENU_PREPARE_COMMAND_TEXT(request, list, object, textColor, font, mode)                          \
    {                                                                                                         \
        (request).x          = (object)->panel.contentOriginX.unsignedValue + (list)->rowTextX.unsignedValue; \
        (request).y          = (object)->panel.contentOriginY.unsignedValue + (list)->rowTextY.unsignedValue; \
        (request).otIndex    = (object)->panel.otIndex.signedValue + 1;                                       \
        (request).colorRgb   = (textColor);                                                                   \
        (request).glyphTable = (font);                                                                        \
        (request).alignment  = TEXT_ALIGNMENT_LEFT;                                                           \
        (request).drawMode   = (mode);                                                                        \
    }

void itemMenuDrawPeCommandRow(UiList* list, UiObject* object)
{
    enum { ITEM_MENU_COMMAND_PARASITE_ENERGY = 12 };
    TextDrawReq request;
    u32         textColorRgb;
    s32         panelControl;

    textColorRgb = list->colorRgb;
    if (attachmentIsTrainingMode() != 0) {
        textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
    } else {
        panelControl = object->panel.control.word;
        if (((panelControl >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
            if (list->selectedItemIndex == list->currentItemIndex) {
                uiSetPromptText((const u8*)Gp_StrReleasePe, 0, 0);
            }
        }
    }

    ITEM_MENU_PREPARE_COMMAND_TEXT(request, list, object, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_DRAW_FILL_ONLY);
    textDrawString(&request, (const u8*)Gp_StrPEnergy);

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (attachmentIsTrainingMode() != 0) {
            list->actionResult = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            object->resultValue = ITEM_MENU_COMMAND_PARASITE_ENERGY;
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
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
            uiSetPromptText(Gp_StrCustomizeHelp, 0, 0);
            two = 2;
            if (arg1->owner->spawnArg1.value != two) {
                cdCmdDropQueuedTail();
                cdCmdEnqueueDisplayResource(1, 0, CD_COMMAND_DISPLAY_LOAD_MENU);
                itemMenuClearPreviewItems();
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

void itemMenuDrawExitCommandRow(UiList* list, UiObject* object)
{
    TextDrawReq request;
    s32         panelControl;

    ITEM_MENU_PREPARE_COMMAND_TEXT(request, list, object, list->colorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_DRAW_FILL_ONLY);
    textDrawString(&request, (const u8*)Gp_StrExit);

    panelControl = object->panel.control.word;

    if (((panelControl >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            uiSetPromptText((const u8*)Gp_StrReturnGame, 0, 0);
        }
    }

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

static InventoryItemRow* func_800CE980(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow* table;
    s32               i;
    s32               count;
    InventoryItemRow* rec;

    table = inventoryGetRangeTable(arg0);
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

    table = inventoryGetRangeTable(arg0);
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
        desc = &D_8010EAB4[10];
        uiSpawnObject(desc, 0, 0, 0, obj);
        uiSpawnObject(desc + 1, 0, 0, 0, obj);
        arg0->state = arg0->state + 1;
    }
    obj->result = USER_INTERFACE_RESULT_NONE;
    itemMenuDrawWeaponSummary(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, 0);
    uiDrawTitle(&(obj)->panel, Gp_StrWeaponTitle);
}

void itemMenuSetItemDescriptionPrompt(s32 itemId)
{
    if (itemId == INVENTORY_ITEM_NONE) {
        uiSetPromptText((const u8*)Gp_StrEmpty, 0, 0);
    } else {
        uiSetPromptText(itemGetText(itemId, ITEM_TEXT_DESCRIPTION_FIRST, 0), 0, 0);
    }
}

/// Tests whether an item is the selected weapon, armor or one of that weapon's consumables.
///
/// Item ids use weapon 0x80..0x9F, armor 0x60..0x7F and consumable 0xA0..0xBF
/// ranges. A configured consumable counts even with no remaining load; other
/// weapons' selections and armor attachments do not count. Returns 0 or 1.
static s32 _equipmentIsSelectedItem(s32 itemId)
{
    s32                 equipped;
    const PlayerStatus* player;

    equipped = 0;
    player   = &gPlayerStatus;
    if ((((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems)) && (player->weapon == itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1))) ||
        (((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus)) && (player->armor == itemId - (ITEM_MENU_ARMOR_ITEM_FIRST - 1))) ||
        (((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) && (player->weapon != PLAYER_STATUS_EQUIPMENT_NONE) &&
         ((equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->primaryItemId == itemId) ||
          (equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId == itemId)))) {
        equipped = 1;
    }
    return equipped;
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

    table = inventoryGetRangeTable(arg0);
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
        obj = uiSpawnObject(&D_8010EAB4[34], arg3, one, one, arg1);
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

void itemMenuDrawSortRow(UiList* list, UiObject* object)
{
    enum {
        ITEM_MENU_REORDER_SELECT_DESTINATION = 1
    };
    s32 panelControl;
    s32 activeMode;

    activeMode = USER_INTERFACE_PANEL_ACTIVE;
    if (Gp_ItemOrderMode == ITEM_MENU_REORDER_SELECT_DESTINATION) {
        list->colorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
    }
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, (const u8*)Gp_StrSort, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    panelControl = object->panel.control.word;
    if (((panelControl >> 16) == activeMode) || (panelControl == activeMode)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            if (Gp_ItemOrderMode == ITEM_MENU_REORDER_SELECT_DESTINATION) {
                list->actionResult    = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
                list->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
            } else {
                uiSetPromptText(Gp_StrChangeOrderHelp, 0, 0);
            }
        }
    }
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            inventorySortItems(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, 1);
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
    table = inventoryGetRangeTable(scan);
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
    return _cdCmdEnqueueEntry(entry);
}

s32 itemMenuGetPrimaryPreviewItem(void)
{
    return Gp_PreviewItems[0];
}

void itemMenuDrawDescriptionRow(UiList* list, UiObject* object)
{
    enum { ITEM_MENU_DESCRIPTION_CHUNK_HEADER_LINES = 5 };
    const u8* text;
    s8        lineIndex;
    s32       itemId;

    lineIndex = list->currentItemIndex;
    itemId    = (u16)object->owner->spawnArg1.value;
    if ((lineIndex < 2) && (itemId < ITEM_TEXT_KEY_ID_FIRST)) {
        text = itemGetText(itemId, lineIndex + ITEM_TEXT_DESCRIPTION_FIRST, 1);
        textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, text, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    } else {
        // The loaded description chunk has five header lines before its body.
        text = textSkipLines(fsGetChunkPayload(), list->currentItemIndex + ITEM_MENU_DESCRIPTION_CHUNK_HEADER_LINES);
        textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, text, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
}

void itemMenuInfoTaskExit(Task* task)
{
    UiObject* object;

    object = task->spawnArg2.pointer;
    if (object != NULL) {
        if (D_80067634 == object) {
            D_80067634 = NULL;
        }
    }
    uiObjectTaskExit(task);
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
            uiSpawnObject(&D_8010EAB4[44], 0, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

void equipmentEquipCarriedWeapon(s32 weaponItemId)
{
    PlayerStatus*     player;
    InventoryItemRow* weaponRow;
    InventoryItemRow* previousWeaponRow;
    u8                previousWeapon;

    player         = &gPlayerStatus;
    weaponRow      = inventoryFindLastCarriedItemRow(weaponItemId);
    previousWeapon = player->weapon;
    if (previousWeapon != weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
        // Preserve the attachment position or unload the weapon being replaced.
        if (previousWeapon != PLAYER_STATUS_EQUIPMENT_NONE) {
            previousWeaponRow = inventoryFindLastCarriedItemRow(previousWeapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1));
            if (weaponRow->attachSlot > INVENTORY_ATTACHMENT_NONE) {
                previousWeaponRow->attachSlot = weaponRow->attachSlot;
            } else {
                equipmentClearSelectedRemovableLoads(previousWeaponRow->itemId, EQUIPMENT_CLEAR_LOAD_BOTH);
            }
        }
        player->weapon = weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
        inventoryDetachItem(weaponRow);
        itemSetIdentified(weaponItemId, 1);
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
            qty  = inventoryGetConsumableStackQuantity(arg0, item);
            qty -= equipmentGetLoadedConsumableQuantity(arg0, item);
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
                qty  = inventoryGetConsumableStackQuantity(arg0, item);
                qty -= equipmentGetLoadedConsumableQuantity(arg0, item);
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

    width = textMeasureLineWidth(itemGetText(arg1, ITEM_TEXT_NAME, 0)) + 0xB;
    temp  = textMeasureLineWidth((const u8*)Gp_StrEquipped);
    if (width < temp) {
        width = temp;
    }
    uiSetPanelContentSize(arg0, width + 5, uiGetTextRowsHeight(2) + 1);
    arg0->bounds.rect.x = (-arg0->bounds.rect.w) >> 1;
}

void func_800CF6E8(UiObject* arg0, s32 arg1)
{
    const u8* text;
    s32       color;
    s32       one;
    s32       x;

    text  = itemGetText(arg1, ITEM_TEXT_NAME, 0);
    color = 0x606060;
    one   = 1;
    textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrEquipped, color, one, TEXT_ALIGNMENT_LEFT);
    x = textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0x1E, text, 0x37A78, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(arg0, x, arg0->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, color, one, TEXT_ALIGNMENT_LEFT);
}

void itemMenuDrawUseRow(UiList* list, UiObject* object)
{
    TextDrawReq request;

    request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    request.otIndex    = object->panel.otIndex.signedValue + 1;
    request.colorRgb   = list->colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&request, Gp_StrUse);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            _itemMenuOpenUsePanel(list, object);
            list->actionResult = USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED;
        }
    }
}

void itemMenuDrawMoveRow(UiList* list, UiObject* object)
{
    TextDrawReq request;

    request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    request.otIndex    = object->panel.otIndex.signedValue + 1;
    request.colorRgb   = list->colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&request, Gp_StrMove);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            list->actionResult = USER_INTERFACE_LIST_ACTION_MOVE;
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
            obj = uiSpawnObject(&D_8010EAB4[21], one, one, 0x10, arg1);
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

void itemMenuApplySelectedHealingItem(UiObject* object, Task* task)
{
    itemMenuApplyHealingPanel(object, task, Gp_SelItemRec->itemId);
}

void itemMenuUseItemTask(Task* task)
{
    UiObjectTaskFunc useHandler;
    UiObject*        object;

    useHandler = D_8010D3A0[task->spawnArg1.value];
    object     = task->spawnArg2.pointer;
    if (useHandler != NULL) {
        useHandler(object, object->owner);
    }
}

void itemMenuApplyParasiteEnergyItem(UiObject* object, Task* task)
{
    itemMenuInvokeParasiteEnergyItem(object, task, task->spawnArg1.value);
}

void itemMenuDrawOkRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, (const u8*)Gp_StrOk, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            list->actionResult              = USER_INTERFACE_RESULT_CONFIRM;
            list->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_OK;
        }
    }
}

void itemMenuDrawCancelRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, (const u8*)Gp_StrCancel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            list->actionResult              = USER_INTERFACE_RESULT_CONFIRM;
            list->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_CANCEL;
        }
    }
}

void itemMenuDrawYesRow(UiList* list, UiObject* object)
{
    s32 inputEnabled;

    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, (const u8*)Gp_StrYes, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    inputEnabled = list->rowInputEnabled;
    if (inputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            list->actionResult              = USER_INTERFACE_RESULT_CONFIRM;
            list->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_YES;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            list->navigationStep = inputEnabled;
            list->actionResult   = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
        }
    }
}

void itemMenuDrawNoRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, (const u8*)Gp_StrNo, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            list->actionResult              = USER_INTERFACE_RESULT_CONFIRM;
            list->commandResult.signedValue = USER_INTERFACE_LIST_COMMAND_NO;
        }
    }
}

void planarReflectionDispatchPlayerTask(Task* reflectionTask)
{
    enum { PLANAR_REFLECTION_DISPATCH_STATE_INIT = 0 };

    // Latch the room before its callback creates and reparents the reflection.
    if (reflectionTask->state == PLANAR_REFLECTION_DISPATCH_STATE_INIT) {
        D_80114DCC = GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
    }
    switch (D_80114DCC) {
        case GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SQUARE, 0, 0):
            acropolisSquarePlayerReflectionTask(reflectionTask);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL, 0, 0):
            acropolisEastElevatorHallPlayerReflectionTask(reflectionTask);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL, 0, 0):
            acropolisWestElevatorHallPlayerReflectionTask(reflectionTask);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_6, 0, 0):
            dryfieldMotelRoom6PlayerReflectionTask(reflectionTask);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, 0, 0):
            dryfieldNightMotelRoom6PlayerReflectionTask(reflectionTask);
            break;
        default:
            taskKill(reflectionTask);
            break;
    }
}

/// Opens a use panel for the selected ordinary item, or a cannot-use notice.
///
/// Borrows a stable selected carried row with item id 1..95 and a live parent.
/// The use task retains the id, while consuming handlers borrow the selected
/// row. Successful use-panel creation identifies the item. Either branch
/// suspends parent input even on allocation failure. `unusedList` is ignored.
static void _itemMenuOpenUsePanel(UiList* unusedList, UiObject* parent)
{
    enum { ITEM_MENU_USE_PANEL_DESCRIPTOR_INDEX = 35 };
    s32               itemId;
    s32               activeMode;
    UiObjectTaskFunc* useHandlerSlot;

    itemId         = Gp_SelItemRec->itemId;
    useHandlerSlot = &D_8010D3A0[itemId];
    if (*useHandlerSlot != NULL) {
        activeMode = USER_INTERFACE_PANEL_ACTIVE;
        if (uiSpawnObject(&D_8010EAB4[ITEM_MENU_USE_PANEL_DESCRIPTOR_INDEX], itemId, activeMode, activeMode, parent) != NULL) {
            itemSetIdentified(itemId, 1);
        }
        parent->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    } else {
        itemMenuSpawnNotice(parent, ITEM_MENU_NOTICE_NO_USE_NOW, 0, ITEM_MENU_NOTICE_RESULT_DISMISS);
        parent->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
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
    _menuMapDrawPlayerCursor(arg0);
    _menuMapDrawFlagMarkers(arg0);
    _menuMapDrawPicture(arg0);
    _menuMapDrawAreas(arg0);
    _menuMapDrawPageArrows(arg0);
    if (gDisplayState.keepGraphics != 0) {
        displaySetTaskDrawMode(DISPLAY_TASK_DRAW_ROOM);
    } else {
        displaySetTaskDrawMode(DISPLAY_TASK_DRAW_CLEAR);
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | PAD_BUTTON_SELECT) != 0) {
            obj->resultValue = 0x101;
            _menuMapPrepareClosing(arg0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
            if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                loadingRestoreViewImageAndEnqueueResources(1);
            }
            arg0->state = 3;
            return;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            _menuMapPrepareClosing(arg0);
            obj->result = USER_INTERFACE_RESULT_CANCEL;
            if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                loadingRestoreViewImageAndEnqueueResources(1);
            }
            arg0->state = 3;
            return;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            if (flags[Gp_MapRoomId] == 0xFF) {
                return;
            }
            for (room = Gp_MapRoomId + 1; room <= D_8010F130[gGameSession->location.loc.stage - 1]; room++) {
                if (_menuMapPageIsAvailable(room, flags[room]) == 1) {
                    if ((s8)Gp_MapRoomId != room) {
                        Gp_MapRoomId = room;
                        displaySetTaskDrawMode(DISPLAY_TASK_DRAW_HOLD);
                        sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
                        _menuMapLoadPage();
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
                if (_menuMapPageIsAvailable(room, flags[room]) == 1) {
                    if ((s8)Gp_MapRoomId != room) {
                        Gp_MapRoomId = room;
                        displaySetTaskDrawMode(DISPLAY_TASK_DRAW_HOLD);
                        sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
                        _menuMapLoadPage();
                        arg0->state = 1;
                    }
                    return;
                }
            }
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            D_8010F13D = gameFlagGetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010F15C, 0, 1, 1, obj);
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
            _menuMapPrepareClosing(arg0);
            obj->result = USER_INTERFACE_RESULT_CANCEL;
            if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                loadingRestoreViewImageAndEnqueueResources(1);
            }
            arg0->state = 3;
        }
    }
}

/// Selects one of eight cursor sprites for a yaw in 4096 units per turn.
///
/// Borrows a writable 16x16 sprite and changes only its UV coordinates. Requires
/// a yaw in 0..4095; sector zero straddles the turn boundary, and ties enter the
/// next sector. Tests retain 16-bit subtraction wrap; values outside that
/// domain can leave UV unchanged. All eight cells use the same texture row.
static inline void _menuMapSetPlayerCursorHeading(SPRT_16* cursor, s32 heading)
{
    enum {
        MENU_MAP_CURSOR_HALF_SECTOR_ANGLE     = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        MENU_MAP_CURSOR_SECTOR_ANGLE          = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        MENU_MAP_CURSOR_HEADING_ENCODING_MASK = 0xFFFF,
        MENU_MAP_CURSOR_TEXTURE_FIRST_U       = 0x40,
        MENU_MAP_CURSOR_TEXTURE_V             = 0x10,
        MENU_MAP_CURSOR_TEXTURE_STEP          = 16
    };
    if ((((heading - (ACTOR_TRANSFORM_ANGLE_TURN - MENU_MAP_CURSOR_HALF_SECTOR_ANGLE)) & MENU_MAP_CURSOR_HEADING_ENCODING_MASK) < (u32)MENU_MAP_CURSOR_HALF_SECTOR_ANGLE) ||
        (heading < (u32)MENU_MAP_CURSOR_HALF_SECTOR_ANGLE)) {
        cursor->u0 = MENU_MAP_CURSOR_TEXTURE_FIRST_U;
        cursor->v0 = MENU_MAP_CURSOR_TEXTURE_V;
    } else if (((heading - MENU_MAP_CURSOR_HALF_SECTOR_ANGLE) & MENU_MAP_CURSOR_HEADING_ENCODING_MASK) < (u32)MENU_MAP_CURSOR_SECTOR_ANGLE) {
        cursor->u0 = MENU_MAP_CURSOR_TEXTURE_FIRST_U + MENU_MAP_CURSOR_TEXTURE_STEP;
        cursor->v0 = MENU_MAP_CURSOR_TEXTURE_V;
    } else if (((heading - (3 * MENU_MAP_CURSOR_HALF_SECTOR_ANGLE)) & MENU_MAP_CURSOR_HEADING_ENCODING_MASK) < (u32)MENU_MAP_CURSOR_SECTOR_ANGLE) {
        cursor->u0 = MENU_MAP_CURSOR_TEXTURE_FIRST_U + 2 * MENU_MAP_CURSOR_TEXTURE_STEP;
        cursor->v0 = MENU_MAP_CURSOR_TEXTURE_V;
    } else if (((heading - (5 * MENU_MAP_CURSOR_HALF_SECTOR_ANGLE)) & MENU_MAP_CURSOR_HEADING_ENCODING_MASK) < (u32)MENU_MAP_CURSOR_SECTOR_ANGLE) {
        cursor->u0 = MENU_MAP_CURSOR_TEXTURE_FIRST_U + 3 * MENU_MAP_CURSOR_TEXTURE_STEP;
        cursor->v0 = MENU_MAP_CURSOR_TEXTURE_V;
    } else if (((heading - (7 * MENU_MAP_CURSOR_HALF_SECTOR_ANGLE)) & MENU_MAP_CURSOR_HEADING_ENCODING_MASK) < (u32)MENU_MAP_CURSOR_SECTOR_ANGLE) {
        cursor->u0 = MENU_MAP_CURSOR_TEXTURE_FIRST_U + 4 * MENU_MAP_CURSOR_TEXTURE_STEP;
        cursor->v0 = MENU_MAP_CURSOR_TEXTURE_V;
    } else if (((heading - (9 * MENU_MAP_CURSOR_HALF_SECTOR_ANGLE)) & MENU_MAP_CURSOR_HEADING_ENCODING_MASK) < (u32)MENU_MAP_CURSOR_SECTOR_ANGLE) {
        cursor->u0 = MENU_MAP_CURSOR_TEXTURE_FIRST_U + 5 * MENU_MAP_CURSOR_TEXTURE_STEP;
        cursor->v0 = MENU_MAP_CURSOR_TEXTURE_V;
    } else if (((heading - (11 * MENU_MAP_CURSOR_HALF_SECTOR_ANGLE)) & MENU_MAP_CURSOR_HEADING_ENCODING_MASK) < (u32)MENU_MAP_CURSOR_SECTOR_ANGLE) {
        cursor->u0 = MENU_MAP_CURSOR_TEXTURE_FIRST_U + 6 * MENU_MAP_CURSOR_TEXTURE_STEP;
        cursor->v0 = MENU_MAP_CURSOR_TEXTURE_V;
    } else if (((heading - (13 * MENU_MAP_CURSOR_HALF_SECTOR_ANGLE)) & MENU_MAP_CURSOR_HEADING_ENCODING_MASK) < (u32)MENU_MAP_CURSOR_SECTOR_ANGLE) {
        cursor->u0 = MENU_MAP_CURSOR_TEXTURE_FIRST_U + 7 * MENU_MAP_CURSOR_TEXTURE_STEP;
        cursor->v0 = MENU_MAP_CURSOR_TEXTURE_V;
    }
}

/// Queues the pulsing player-position cursor on the page containing the player.
///
/// Requires a live player task, coordinate matrix and loaded stage/area table.
/// The area's X/Z divisors must be nonzero; yaw is 0..4095 (4096 per turn).
/// Borrows the map object's OT index, which must admit a -28 offset. World X/Z
/// become pixels from the map picture's centre, narrowed to 16 bits. Scratch
/// storage is released before returning; packets belong to the live GPU buffer.
static void _menuMapDrawPlayerCursor(const Task* mapTask)
{
    enum {
        MENU_MAP_CURSOR_HALF_SIZE_PIXELS = 8,
        MENU_MAP_CURSOR_OT_OFFSET        = 28,
        MENU_MAP_CURSOR_SPRITE_CODE      = 0x7E // Modulated, semitransparent 16x16 sprite
    };
    UiObject*              mapObject;
    const GameActor*       playerActor;
    const MenuMapArea*     area;
    const PlayerStatus*    playerStatus;
    _MenuMapCentreScratch* centre;
    s32                    mapOffset;
    s32                    mapBase;
    SPRT_16*               cursor;
    DR_TPAGE*              texturePage;
    s32                    cursorValue;

    mapObject    = mapTask->spawnArg2.pointer;
    playerStatus = &gPlayerStatus;
    playerActor  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    area         = Gp_MapRecTables[gGameSession->location.loc.stage - 1];
    area         = area + gGameSession->location.loc.area;
    if (area->page != (s8)Gp_MapRoomId) {
        return;
    }

    // Project the live player X/Z into this area's map-screen coordinates.
    centre           = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapCentreScratch);
    centre->field_10 = centre->field_12 = centre->field_14 = 0;
    mapOffset                                              = (area->originX - playerStatus->coordMtx->t[0]) / area->scaleX;
    mapBase                                                = area->mapX;
    centre->x                                              = mapBase - mapOffset;
    mapOffset                                              = (area->originZ - playerStatus->coordMtx->t[2]) / area->scaleZ;
    mapBase                                                = area->mapY;
    centre->y                                              = mapBase + mapOffset;

    cursor         = gGpuPrimCursor;
    gGpuPrimCursor = cursor + 1;
    cursorValue    = (rsin(gDisplayState.loopCount << 6) + ONE) >> 5;
    if (cursorValue == 0x100) {
        cursorValue = 0xFF;
    }
    GPU_PRIMITIVE_COLOR_WORD(cursor, 0) = ((cursorValue & 0xFF) << 0x10) | ((cursorValue & 0xFF) << 8) | (cursorValue & 0xFF);
    setlen(cursor, 3);
    setcode(cursor, MENU_MAP_CURSOR_SPRITE_CODE);
    cursor->clut = GetClut(0, 0x101);

    cursorValue = (u16)playerActor->rotation.vy;
    _menuMapSetPlayerCursorHeading(cursor, cursorValue);

    cursor->x0 = centre->x - MENU_MAP_CURSOR_HALF_SIZE_PIXELS;
    cursor->y0 = centre->y - MENU_MAP_CURSOR_HALF_SIZE_PIXELS;
    addPrim(&gGpuCurrentOt[mapObject->panel.otIndex.signedValue - MENU_MAP_CURSOR_OT_OFFSET], cursor);
    texturePage    = gGpuPrimCursor;
    gGpuPrimCursor = texturePage + 1;
    setDrawTPage(texturePage, 0, 0, 0xE);
    addPrim(&gGpuCurrentOt[mapObject->panel.otIndex.signedValue - MENU_MAP_CURSOR_OT_OFFSET], texturePage);
    SCRATCH_STACK_RELEASE_BLOCK(_MenuMapCentreScratch);
}

/// Queues the centred 254x208 map picture and the fixed map-screen sprite.
///
/// Borrows the live `UiObject` in `mapTask->spawnArg2.pointer` and the loaded
/// eight-bit map texture. Coordinates are pixels from the picture's centre;
/// the panel's OT index must admit offsets +2 and -25. Packets belong to the
/// current primitive buffer; scratch storage is released before returning.
static void _menuMapDrawPicture(Task* mapTask)
{
    enum {
        MENU_MAP_PICTURE_HALF_WIDTH  = 127,
        MENU_MAP_PICTURE_HALF_HEIGHT = 104,
        MENU_MAP_FIXED_SPRITE_CODE   = 0x67 // Textured, semitransparent sprite without colour modulation
    };
    UiObject*              mapObject;
    _MenuMapCentreScratch* centre;
    POLY_FT4*              picture;
    SPRT*                  fixedSprite;
    DR_TPAGE*              spritePage;

    mapObject      = mapTask->spawnArg2.pointer;
    picture        = gGpuPrimCursor;
    centre         = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapCentreScratch);
    gGpuPrimCursor = picture + 1;
    centre->x = centre->y = centre->field_10 = centre->field_12 = centre->field_14 = 0;
    setPolyFT4(picture);
    setRGB0(picture, 0x80, 0x80, 0x80);
    picture->clut = getClut(0, 0x100);
    setSemiTrans(picture, 1);
    picture->tpage = GetTPage(1, 0, 0x380, 0x20);
    picture->u0 = picture->u2 = 1;
    picture->v0 = picture->v1 = 0x20;
    picture->u3 = picture->u1 = 0xFF;
    picture->v3 = picture->v2 = 0xF0;
    picture->x0 = picture->x2 = centre->x - MENU_MAP_PICTURE_HALF_WIDTH;
    picture->y0 = picture->y1 = centre->y - MENU_MAP_PICTURE_HALF_HEIGHT;
    picture->x1 = picture->x3 = centre->x + MENU_MAP_PICTURE_HALF_WIDTH;
    picture->y2 = picture->y3 = centre->y + MENU_MAP_PICTURE_HALF_HEIGHT;
    addPrim(&gGpuCurrentOt[mapObject->panel.otIndex.signedValue + 2], picture);
    SCRATCH_STACK_RELEASE_BLOCK(_MenuMapCentreScratch);

    fixedSprite    = gGpuPrimCursor;
    gGpuPrimCursor = fixedSprite + 1;
    setlen(fixedSprite, 4);
    setcode(fixedSprite, MENU_MAP_FIXED_SPRITE_CODE);
    fixedSprite->clut = GetClut(0x70, 0x101);
    fixedSprite->u0   = 0xC0;
    fixedSprite->w    = 0x20;
    fixedSprite->h    = 0x18;
    fixedSprite->x0   = 0x7E;
    fixedSprite->v0   = 0;
    fixedSprite->y0   = -0x64;
    addPrim(&gGpuCurrentOt[mapObject->panel.otIndex.signedValue - 0x19], fixedSprite);
    spritePage     = gGpuPrimCursor;
    gGpuPrimCursor = spritePage + 1;
    setDrawTPage(spritePage, 0, 0, 0xE);
    addPrim(&gGpuCurrentOt[mapObject->panel.otIndex.signedValue - 0x19], spritePage);
}

/// Draws the page's area shapes and icons from exploration, marked-area and objective state.
///
/// Requires stage 1..5, its loaded shape/icon tables and a live map object.
/// Counts include reserved record zero; numbered areas use visited bits for
/// area-1 in two 32-bit words. Model scaling is Q12, where 4096 means unity.
/// Neo Ark's alternate picture requires extra shape records 34 and 35.
static void _menuMapDrawAreas(const Task* mapTask)
{
    enum {
        MENU_MAP_AREA_DEFAULT_SCALE_Q12   = 0x5D7,
        MENU_MAP_AREA_ACROPOLIS_SCALE_Q12 = 0x83B,
        MENU_MAP_NEO_ARK_ALT_ISLAND_SHAPE = 34,
        MENU_MAP_NEO_ARK_ALT_BRIDGE_SHAPE = 35
    };
    GameSession*          session;
    GameFlagStageHeader** banks;
    GameFlagStageHeader*  bank;
    s32                   visited[2];
    u8*                   pageFlagIds;
    MenuMapAreaShape*     shapes;
    MenuMapAreaShape**    shapeTables;
    UiObject*             mapObject;
    u16                   scaleQ12;
    s32                   areaIndex;
    s32                   visitedWord;
    s32                   areaMask;
    s32                   shapeIndex;
    s32                   bitSeed;
    u8                    stage;
    s32                   stageIndex;

    scaleQ12    = MENU_MAP_AREA_DEFAULT_SCALE_Q12;
    session     = gGameSession;
    banks       = Gp_FlagBanks;
    shapeTables = Gp_MapMarkTables;
    stage       = session->location.loc.stage;
    mapObject   = mapTask->spawnArg2.pointer;
    stageIndex  = stage - 1;
    bank        = banks[stage];
    shapes      = shapeTables[stageIndex];
    pageFlagIds = Gp_MapFlagIds[stageIndex];
    if (stage == GAME_STAGE_ACROPOLIS) {
        scaleQ12 = MENU_MAP_AREA_ACROPOLIS_SCALE_Q12;
    }
    // Dryfield night shares exploration with the daytime area bank.
    visited[0] = bank->visitedAreas[0];
    visited[1] = bank->visitedAreas[1];
    if (session->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT) {
        bank        = banks[GAME_STAGE_DRYFIELD];
        visited[0] |= bank->visitedAreas[0];
        visited[1] |= bank->visitedAreas[1];
    }
    areaIndex = 0;
    if (Gp_MapMarkCounts[session->location.loc.stage - 1] != 0) {
        bitSeed = 1;
        do {
            if (shapes[(u8)areaIndex].page == (s8)Gp_MapRoomId) {
                if (shapes[(u8)areaIndex].model == NULL) {
                    _menuMapDrawAreaIcons(mapTask, (u8)areaIndex, 0);
                } else {
                    visitedWord = 0;
                    if ((u8)areaIndex >= (u32)MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA) {
                        visitedWord = 1;
                        areaMask    = bitSeed << ((u8)areaIndex - MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA);
                    } else {
                        areaMask = bitSeed << ((u8)areaIndex - 1);
                    }
                    // Paired bits retain the first area's word, even across the 32/33 boundary.
                    if (shapes[(u8)areaIndex].pairedArea != MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE) {
                        if (shapes[(u8)areaIndex].pairedArea >= (u32)MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA) {
                            areaMask |= bitSeed << (shapes[(u8)areaIndex].pairedArea - MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA);
                        } else {
                            areaMask |= bitSeed << (shapes[(u8)areaIndex].pairedArea - 1);
                        }
                    }
                    shapeIndex = areaIndex;
                    if (Gp_MapRoomOff == MENU_MAP_NEO_ARK_ALTAR_PICTURE_OFFSET) {
                        if ((u8)areaIndex == GAME_AREA_NEO_ARK_ISLAND) {
                            shapeIndex = MENU_MAP_NEO_ARK_ALT_ISLAND_SHAPE;
                        }
                        if ((u8)areaIndex == GAME_AREA_NEO_ARK_BRIDGE) {
                            shapeIndex = MENU_MAP_NEO_ARK_ALT_BRIDGE_SHAPE;
                        }
                    }
                    if (gameFlagGetNibble(pageFlagIds[Gp_MapRoomId]) == 0) {
                        if ((areaMask & visited[visitedWord]) == 0) {
                            if (_menuMapDrawAreaIcons(mapTask, (u8)areaIndex, 1) != 0) {
                                _menuMapDrawAreaShape(mapObject, shapes[(u8)shapeIndex].model, MENU_MAP_AREA_FILL_DIM, scaleQ12);
                            } else {
                                _menuMapDrawAreaShape(mapObject, shapes[(u8)shapeIndex].model, MENU_MAP_AREA_FILL_PATTERN, scaleQ12);
                            }
                        } else if ((areaMask & Gp_AreaIdBits[visitedWord]) != 0) {
                            _menuMapDrawAreaShape(mapObject, shapes[(u8)shapeIndex].model, MENU_MAP_AREA_FILL_RED, scaleQ12);
                            _menuMapDrawAreaIcons(mapTask, (u8)areaIndex, 0);
                        } else {
                            _menuMapDrawAreaIcons(mapTask, (u8)areaIndex, 0);
                        }
                    } else if ((areaMask & visited[visitedWord]) == 0) {
                        _menuMapDrawAreaShape(mapObject, shapes[(u8)shapeIndex].model, MENU_MAP_AREA_FILL_DIM, scaleQ12);
                        _menuMapDrawAreaIcons(mapTask, (u8)areaIndex, 1);
                    } else if ((areaMask & Gp_AreaIdBits[visitedWord]) != 0) {
                        _menuMapDrawAreaShape(mapObject, shapes[(u8)shapeIndex].model, MENU_MAP_AREA_FILL_RED, scaleQ12);
                        _menuMapDrawAreaIcons(mapTask, (u8)areaIndex, 0);
                    } else {
                        _menuMapDrawAreaIcons(mapTask, (u8)areaIndex, 0);
                    }
                }
            }
            areaIndex++;
        } while ((u8)areaIndex < Gp_MapMarkCounts[gGameSession->location.loc.stage - 1]);
    }
}

/// Queues the selected map page's markers whose game-flag state is visible.
///
/// Borrows the live UiObject in spawnArg2, current-stage visit bank and loaded
/// marker/flag tables for stage 1..5. The page-zero terminator must occur before
/// the byte index wraps; area gates are 1..64, ANY or NEVER. Coordinates are
/// signed pixels from the picture centre, staged as their 16-bit encoding.
/// Uses 16x16 raw semitransparent sprites at panel OT base -27; packets belong
/// to the current GPU buffer, and scratch storage is released for each marker.
static void _menuMapDrawFlagMarkers(const Task* mapTask)
{
    enum {
        MENU_MAP_MARKER_PAGE_END         = 0,
        MENU_MAP_MARKER_HALF_SIZE_PIXELS = 8,
        MENU_MAP_MARKER_OT_OFFSET        = 27,
        MENU_MAP_MARKER_TEXTURE_PAGE     = getTPage(0, 0, 0x380, 0),
        MENU_MAP_MARKER_PRIMARY_CLUT_X   = 0x30,
        MENU_MAP_MARKER_ALTERNATE_CLUT_X = 0x60,
        MENU_MAP_MARKER_CLUT_Y           = 0x101,
        MENU_MAP_MARKER_PRIMARY_U        = 0x60,
        MENU_MAP_MARKER_ALTERNATE_U      = 0x90,
        MENU_MAP_MARKER_TEXTURE_V        = 0,
        MENU_MAP_MARKER_SPRITE_CODE      = 0x7F // Raw, semitransparent 16x16 sprite
    };

    /// Queues one visible flag marker and its texture-page packet.
    ///
    /// object is live; marker is a loaded record lvalue and state selects one of
    /// the two visible pictures. centreBlock/sprite/pagePacket are writable
    /// pointer lvalues. Arguments must have no side effects: uses are repeated.
    /// Captures the MENU_MAP_MARKER constants, scratch stack and GPU cursor/OT;
    /// releases its scratch block after advancing the cursor by both packets.
#define MENU_MAP_QUEUE_FLAG_MARKER(object, marker, state, centreBlock, sprite, pagePacket)                      \
    {                                                                                                           \
        (centreBlock)           = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapCentreScratch);                           \
        (centreBlock)->field_10 = (centreBlock)->field_12 = (centreBlock)->field_14 = 0;                        \
        (centreBlock)->x                                                            = (marker).x;               \
        (sprite)                                                                    = gGpuPrimCursor;           \
        gGpuPrimCursor                                                              = (sprite) + 1;             \
        (centreBlock)->y                                                            = (marker).y;               \
        setlen((sprite), sizeof(*(sprite)) / sizeof(u32) - 1);                                                  \
        setcode((sprite), MENU_MAP_MARKER_SPRITE_CODE);                                                         \
        if ((state) == MENU_MAP_MARKER_STATE_VISIBLE) {                                                         \
            (sprite)->clut = GetClut(MENU_MAP_MARKER_PRIMARY_CLUT_X, MENU_MAP_MARKER_CLUT_Y);                   \
            (sprite)->u0   = MENU_MAP_MARKER_PRIMARY_U;                                                         \
            (sprite)->v0   = MENU_MAP_MARKER_TEXTURE_V;                                                         \
        } else if ((state) == (MENU_MAP_MARKER_STATE_VISIBLE + MENU_MAP_MARKER_ALTERNATE_PICTURE)) {            \
            (sprite)->clut = GetClut(MENU_MAP_MARKER_ALTERNATE_CLUT_X, MENU_MAP_MARKER_CLUT_Y);                 \
            (sprite)->u0   = MENU_MAP_MARKER_ALTERNATE_U;                                                       \
            (sprite)->v0   = MENU_MAP_MARKER_TEXTURE_V;                                                         \
        }                                                                                                       \
        (sprite)->x0 = (centreBlock)->x - MENU_MAP_MARKER_HALF_SIZE_PIXELS;                                     \
        (sprite)->y0 = (centreBlock)->y - MENU_MAP_MARKER_HALF_SIZE_PIXELS;                                     \
        addPrim(&gGpuCurrentOt[(object)->panel.otIndex.signedValue - MENU_MAP_MARKER_OT_OFFSET], (sprite));     \
        (pagePacket)   = gGpuPrimCursor;                                                                        \
        gGpuPrimCursor = (pagePacket) + 1;                                                                      \
        setDrawTPage((pagePacket), 0, 0, MENU_MAP_MARKER_TEXTURE_PAGE);                                         \
        addPrim(&gGpuCurrentOt[(object)->panel.otIndex.signedValue - MENU_MAP_MARKER_OT_OFFSET], (pagePacket)); \
        SCRATCH_STACK_RELEASE_BLOCK(_MenuMapCentreScratch);                                                     \
    }

    const UiObject*            mapObject;
    const MenuMapMarker*       markers;
    const GameFlagStageHeader* flagBank;
    _MenuMapCentreScratch*     centre;
    SPRT_16*                   markerSprite;
    DR_TPAGE*                  texturePage;
    s32                        visited[2];
    u8                         markerIndex;
    s16                        visitedWord;
    s32                        areaMask;
    u16                        markerState;
    u8                         stage;
    u8                         area;

    markerIndex = 0;
    stage       = gGameSession->location.loc.stage;
    mapObject   = mapTask->spawnArg2.pointer;
    markers     = D_8010F0E0[stage - 1];
    flagBank    = Gp_FlagBanks[stage];
    // Marker visibility uses this stage's visit bits without merging daytime visits.
    visited[0] = flagBank->visitedAreas[0];
    visited[1] = flagBank->visitedAreas[1];
    for (;;) {
        if (markers[markerIndex].page == MENU_MAP_MARKER_PAGE_END) {
            return;
        }
        area = markers[markerIndex].area;
        if (area == MENU_MAP_MARKER_AREA_NEVER) {
            markerIndex++;
            continue;
        }
        if (area != MENU_MAP_MARKER_AREA_ANY) {
            // Show the marker only once its area has been visited.
            if (area >= MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA) {
                areaMask    = 1 << (markers[markerIndex].area - MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA);
                visitedWord = 1;
            } else {
                areaMask    = 1 << (markers[markerIndex].area - 1);
                visitedWord = 0;
            }
            if (!(areaMask & visited[visitedWord])) {
                markerIndex++;
                continue;
            }
        }
        markerState = menuMapGetMarkerState(markerIndex);
        if (markers[markerIndex].page != (s8)Gp_MapRoomId) {
            markerIndex++;
            continue;
        }
        // A flag value of two enables one of the two marker pictures.
        if (markerState == MENU_MAP_MARKER_STATE_VISIBLE || markerState == (MENU_MAP_MARKER_STATE_VISIBLE + MENU_MAP_MARKER_ALTERNATE_PICTURE)) {
            MENU_MAP_QUEUE_FLAG_MARKER(mapObject, markers[markerIndex], markerState, centre, markerSprite, texturePage);
        }
        markerIndex++;
    }
}
#undef MENU_MAP_QUEUE_FLAG_MARKER

/// Draws an area's enabled icons on the selected map page and reports an objective marker.
///
/// Borrows the live task's UiObject and the current stage (1..5) icon table.
/// area is the map-area index; nonzero unvisited suppresses fixed icons.
/// Returns 1 if an objective marker was queued, otherwise 0. The page-zero
/// terminator must be reached within the byte-sized table index. Icons are
/// centered 16x16 sprites in signed map pixels, queued below the panel OT base;
/// the caller supplies packet space and valid OT indices. Scratch is released
/// per icon. Objective brightness pulses from 0 to 255 using the display clock.
static s32 _menuMapDrawAreaIcons(const Task* task, u8 area, u8 unvisited)
{
    enum {
        MENU_MAP_ICON_PAGE_END           = 0,
        MENU_MAP_ICON_FIXED_PICTURE_KIND = 0,
        MENU_MAP_ICON_SHADED_SPRITE_CODE = 0x7C,
        MENU_MAP_ICON_RAW_SPRITE_CODE    = 0x7D,
        MENU_MAP_ICON_SINE_ONE_Q12       = 4096,
        MENU_MAP_ICON_BRIGHTNESS_LIMIT   = 256,
        MENU_MAP_ICON_BRIGHTNESS_MAX     = 255,
        MENU_MAP_ICON_HALF_SIZE_PIXELS   = 8,
        MENU_MAP_ICON_FIXED_OT_OFFSET    = 27,
        MENU_MAP_ICON_MARKER_OT_OFFSET   = 28
    };
    UiObject*          object;
    const MenuMapIcon* icons;
    u8                 iconIndex;
    s32                drewObjective;
    u8                 otOffset;
    s32                brightness;

    otOffset      = 0;
    iconIndex     = 0;
    drewObjective = 0;
    object        = task->spawnArg2.pointer;
    icons         = D_8010F0CC[gGameSession->location.loc.stage - 1];
    brightness    = (rsin(gDisplayState.loopCount << 6) + MENU_MAP_ICON_SINE_ONE_Q12) >> 5;

    // Objective markers use the whole objective byte; fixed icons use nibble flags.
    for (;;) {
        _MenuMapIconCentreScratch* centre;
        SPRT_16*                   sprite;
        DR_TPAGE*                  texturePage;
        u16                        palette;

        if (icons[iconIndex].page == MENU_MAP_ICON_PAGE_END) {
            break;
        }
        if (icons[iconIndex].kind == MENU_MAP_ICON_KIND_OBJECTIVE) {
            if (icons[iconIndex].condition != gameFlagGetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE)) {
                iconIndex++;
                continue;
            }
        } else if (icons[iconIndex].condition != 0) {
            if (gameFlagGetNibble(icons[iconIndex].condition) == 0) {
                iconIndex++;
                continue;
            }
        }
        if ((unvisited != 0) && (icons[iconIndex].kind < MENU_MAP_ICON_KIND_OBJECTIVE)) {
            iconIndex++;
            continue;
        }
        if ((icons[iconIndex].page == (s8)Gp_MapRoomId) && (icons[iconIndex].area == area)) {
            centre          = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapIconCentreScratch);
            centre->field_4 = centre->field_6 = centre->field_8 = 0;
            centre->x                                           = icons[iconIndex].x;
            sprite                                              = gGpuPrimCursor;
            gGpuPrimCursor                                      = sprite + 1;
            centre->y                                           = icons[iconIndex].y;
            if (icons[iconIndex].kind == MENU_MAP_ICON_KIND_OBJECTIVE) {
                if (brightness == MENU_MAP_ICON_BRIGHTNESS_LIMIT) {
                    brightness = MENU_MAP_ICON_BRIGHTNESS_MAX;
                }
                GPU_PRIMITIVE_COLOR_WORD(sprite, 0) = ((brightness & 0xFF) << 0x10) | ((brightness & 0xFF) << 8) | (brightness & 0xFF);
            }
            setlen(sprite, sizeof(*sprite) / sizeof(u32) - 1);
            setcode(sprite, MENU_MAP_ICON_SHADED_SPRITE_CODE);
            if (icons[iconIndex].kind != MENU_MAP_ICON_KIND_OBJECTIVE) {
                setcode(sprite, MENU_MAP_ICON_RAW_SPRITE_CODE);
            }
            setSemiTrans(sprite, 1);
            switch (icons[iconIndex].kind) {
                case MENU_MAP_ICON_FIXED_PICTURE_KIND:
                    palette      = GetClut(0x20, 0x101);
                    otOffset     = MENU_MAP_ICON_FIXED_OT_OFFSET;
                    sprite->clut = palette;
                    sprite->u0   = 0x50;
                    sprite->v0   = 0;
                    break;
                case MENU_MAP_ICON_KIND_TELEPHONE:
                    palette      = GetClut(0x10, 0x101);
                    otOffset     = MENU_MAP_ICON_MARKER_OT_OFFSET;
                    sprite->clut = palette;
                    sprite->u0   = 0x40;
                    sprite->v0   = 0;
                    break;
                case MENU_MAP_ICON_KIND_OBJECTIVE:
                    palette       = GetClut(0x40, 0x101);
                    otOffset      = MENU_MAP_ICON_MARKER_OT_OFFSET;
                    drewObjective = 1;
                    sprite->clut  = palette;
                    sprite->u0    = 0x70;
                    sprite->v0    = 0;
                    break;
            }
            sprite->x0 = centre->x - MENU_MAP_ICON_HALF_SIZE_PIXELS;
            sprite->y0 = centre->y - MENU_MAP_ICON_HALF_SIZE_PIXELS;
            addPrim(&gGpuCurrentOt[object->panel.otIndex.signedValue - otOffset], sprite);
            texturePage    = gGpuPrimCursor;
            gGpuPrimCursor = texturePage + 1;
            setDrawTPage(texturePage, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[object->panel.otIndex.signedValue - otOffset], texturePage);
            SCRATCH_STACK_RELEASE_BLOCK(_MenuMapIconCentreScratch);
        }
        iconIndex++;
    }
    return drewObjective;
}

/// Queues the selected stage map picture and requests a later seek to the current view.
///
/// `Gp_MapRoomId` holds a one-based page, rather than a room id. Computes the
/// reservoir/altar picture variant, invalidates the loaded sound id and copies
/// the four-byte CD arguments into the request ring. Requires queue capacity;
/// returning does not mean the picture is ready to draw.
static void _menuMapLoadPage(void)
{
    enum {
        MENU_MAP_FILE_GROUP     = 3,
        MENU_MAP_RESERVOIR_PAGE = 6,
        MENU_MAP_ALTAR_PAGE     = 1
    };
    u8  fileKeyBytes[4];
    u8  commandArgs[4];
    s32 page;
    u8  stage;

    Gp_MapRoomOff             = 0;
    gGameSession->loadedSndId = 0;
    if ((gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER) && ((s8)Gp_MapRoomId == MENU_MAP_RESERVOIR_PAGE) && (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == 0)) {
        Gp_MapRoomOff = MENU_MAP_SHELTER_RESERVOIR_PICTURE_OFFSET;
    }
    if (gGameSession->location.loc.stage == GAME_STAGE_SHELTER_NEO_ARK) {
        page = (s8)Gp_MapRoomId;
        if ((page == MENU_MAP_ALTAR_PAGE) && (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) == page)) {
            Gp_MapRoomOff = MENU_MAP_NEO_ARK_ALTAR_PICTURE_OFFSET;
        }
    }
    // Global-library file 30000 + stage*100 + page + picture-variant offset.
    // Byte 1 of the file key is ignored; only four bytes of each block are read.
    fileKeyBytes[2] = MENU_MAP_FILE_GROUP;
    fileKeyBytes[3] = 0;
    fileKeyBytes[0] = Gp_MapRoomId + Gp_MapRoomOff;
    stage           = gGameSession->location.loc.stage;
    commandArgs[1]  = 0;
    commandArgs[3]  = 0;
    commandArgs[2]  = 0;
    commandArgs[0]  = stage;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, commandArgs);
    D_800626E8 = 1;
}

/// Returns 1 if a page is unlocked by its flag or by a visited area, otherwise 0.
///
/// Examines the low byte of `page`. Flag 0 checks exploration alone; 0x80 and
/// 0xFF reject the page. Neo Ark always returns 0 because its pages do not use
/// this navigation gate. Requires loaded stage tables; Dryfield night merges
/// daytime visits and retains its missing-terminator scan into adjacent data.
static s8 _menuMapPageIsAvailable(u32 page, u8 flagId)
{
    enum {
        MENU_MAP_PAGE_FLAG_END         = 0xFF,
        MENU_MAP_PAGE_FLAG_UNAVAILABLE = 0x80
    };
    GameFlagStageHeader* bank;
    MenuMapArea*         areas;
    s32                  visited[2];
    s32                  areaIndex;
    s32                  visitedWord;
    s32                  areaMask;
    s32                  bitSeed;
    s32                  noPage;

    bank = Gp_FlagBanks[gGameSession->location.loc.stage];
    if (gGameSession->location.loc.stage != GAME_STAGE_SHELTER_NEO_ARK) {
        if (flagId != MENU_MAP_PAGE_FLAG_END) {
            if (flagId == MENU_MAP_PAGE_FLAG_UNAVAILABLE) {
                return 0;
            }
            visitedWord = flagId != 0;
            if (visitedWord && (gameFlagGetNibble(flagId) != 0)) {
                return 1;
            }
            areas      = Gp_MapRecTables[gGameSession->location.loc.stage - 1];
            visited[0] = bank->visitedAreas[0];
            visited[1] = bank->visitedAreas[1];
            areaIndex  = 0;
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT) {
                bank        = Gp_FlagBanks[GAME_STAGE_DRYFIELD];
                visited[0] |= bank->visitedAreas[0];
                visited[1] |= bank->visitedAreas[1];
            }
            // Skip record zero; the data supplies an END record after the numbered areas.
            // Dryfield night lacks it and the original scan continues into adjacent data.
            if (areas->page != MENU_MAP_AREA_PAGE_END) {
                noPage  = MENU_MAP_AREA_PAGE_NONE;
                bitSeed = 1;
                do {
                    areas++;
                    areaIndex++;
                    if (areas->page != noPage) {
                        visitedWord = 0;
                        if ((u8)areaIndex >= (u32)MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA) {
                            visitedWord = 1;
                            areaMask    = bitSeed << ((u8)areaIndex - MENU_MAP_VISITED_SECOND_WORD_FIRST_AREA);
                        } else {
                            areaMask = bitSeed << ((u8)areaIndex - 1);
                        }
                        if (areaMask & visited[visitedWord]) {
                            if (areas->page == (u8)page) {
                                return 1;
                            }
                        }
                    }
                } while (areas->page != MENU_MAP_AREA_PAGE_END);
            }
        }
    }
    return 0;
}

/// Queues a pulsing arrow for each direction with an available map page.
///
/// Borrows the live UiObject in spawnArg2 and loaded stage/page tables. The
/// selected page is one-based and within the stage's declared maximum. Neo Ark
/// and a current-page 0xFF entry suppress both arrows; scans stop at 0 or 0xFF.
/// Candidate indices narrow to bytes before lookup. Each 8x16 arrow is placed
/// in picture-centred pixels and queued at panel OT base -28, with brightness
/// 0..255 from the Q12 sine clock. The caller supplies packet and OT capacity.
static void _menuMapDrawPageArrows(const Task* mapTask)
{
    enum {
        MENU_MAP_ARROW_PAGE_BOUNDARY     = 0,
        MENU_MAP_ARROW_PAGE_END          = 0xFF,
        MENU_MAP_ARROW_BRIGHTNESS_LIMIT  = 256,
        MENU_MAP_ARROW_BRIGHTNESS_MAX    = 255,
        MENU_MAP_ARROW_WIDTH_PIXELS      = 8,
        MENU_MAP_ARROW_HEIGHT_PIXELS     = 16,
        MENU_MAP_ARROW_LEFT_X_PIXELS     = -137,
        MENU_MAP_ARROW_RIGHT_X_PIXELS    = 130,
        MENU_MAP_ARROW_TOP_PIXELS        = -7,
        MENU_MAP_ARROW_OT_OFFSET         = 28,
        MENU_MAP_ARROW_TEXTURE_PAGE      = getTPage(0, 0, 0x380, 0),
        MENU_MAP_ARROW_CLUT_X            = 0x50,
        MENU_MAP_ARROW_CLUT_Y            = 0x101,
        MENU_MAP_ARROW_LEFT_TEXTURE_U    = 0x88,
        MENU_MAP_ARROW_RIGHT_TEXTURE_U   = 0x80,
        MENU_MAP_ARROW_TEXTURE_V         = 0,
        MENU_MAP_ARROW_PULSE_ANGLE_SHIFT = 5,
        MENU_MAP_ARROW_BRIGHTNESS_SHIFT  = 5,
        MENU_MAP_ARROW_SPRITE_CODE       = 0x66 // Modulated, semitransparent variable-size sprite
    };

    /// Queues one 8x16 page arrow and its texture-page packet in the live GPU buffer.
    ///
    /// sprite/texturePage/brightnessValue are writable caller-local lvalues;
    /// object is live and textureU/leftPixels are atlas/picture-centred pixels.
    /// Arguments must have no side effects because they are evaluated repeatedly.
    /// Captures this function's MENU_MAP_ARROW constants, gDisplayState's sine
    /// clock and the GPU primitive cursor/OT; advances the cursor by both packets.
#define MENU_MAP_QUEUE_PAGE_ARROW(object, sprite, texturePage, brightnessValue, textureU, leftPixels)                                                  \
    {                                                                                                                                                  \
        (sprite)          = gGpuPrimCursor;                                                                                                            \
        gGpuPrimCursor    = (sprite) + 1;                                                                                                              \
        (brightnessValue) = (rsin(gDisplayState.loopCount << MENU_MAP_ARROW_PULSE_ANGLE_SHIFT) + ONE) >> MENU_MAP_ARROW_BRIGHTNESS_SHIFT;              \
        if ((brightnessValue) == MENU_MAP_ARROW_BRIGHTNESS_LIMIT) {                                                                                    \
            (brightnessValue) = MENU_MAP_ARROW_BRIGHTNESS_MAX;                                                                                         \
        }                                                                                                                                              \
        GPU_PRIMITIVE_COLOR_WORD((sprite), 0) = (((brightnessValue) & 0xFF) << 0x10) | (((brightnessValue) & 0xFF) << 8) | ((brightnessValue) & 0xFF); \
        setlen((sprite), sizeof(*(sprite)) / sizeof(u32) - 1);                                                                                         \
        setcode((sprite), MENU_MAP_ARROW_SPRITE_CODE);                                                                                                 \
        (sprite)->clut = GetClut(MENU_MAP_ARROW_CLUT_X, MENU_MAP_ARROW_CLUT_Y);                                                                        \
        (sprite)->u0   = (textureU);                                                                                                                   \
        (sprite)->w    = MENU_MAP_ARROW_WIDTH_PIXELS;                                                                                                  \
        (sprite)->h    = MENU_MAP_ARROW_HEIGHT_PIXELS;                                                                                                 \
        (sprite)->x0   = (leftPixels);                                                                                                                 \
        (sprite)->v0   = MENU_MAP_ARROW_TEXTURE_V;                                                                                                     \
        (sprite)->y0   = MENU_MAP_ARROW_TOP_PIXELS;                                                                                                    \
        addPrim(&gGpuCurrentOt[(object)->panel.otIndex.signedValue - MENU_MAP_ARROW_OT_OFFSET], (sprite));                                             \
        (texturePage)  = gGpuPrimCursor;                                                                                                               \
        gGpuPrimCursor = (texturePage) + 1;                                                                                                            \
        setDrawTPage((texturePage), 0, 0, MENU_MAP_ARROW_TEXTURE_PAGE);                                                                                \
        addPrim(&gGpuCurrentOt[(object)->panel.otIndex.signedValue - MENU_MAP_ARROW_OT_OFFSET], (texturePage));                                        \
    }

    const UiObject* mapObject;
    const u8*       pageFlagIds;
    SPRT*           arrowSprite;
    DR_TPAGE*       leftTexturePage;
    DR_TPAGE*       rightTexturePage;
    s32             pageIndex;
    s32             brightness;
    s8              pageAvailable;
    u8              stage;

    stage       = gGameSession->location.loc.stage;
    mapObject   = mapTask->spawnArg2.pointer;
    pageFlagIds = Gp_MapFlagIds[stage - 1];
    if (stage == GAME_STAGE_SHELTER_NEO_ARK) {
        return;
    }
    if (pageFlagIds[Gp_MapRoomId] == MENU_MAP_ARROW_PAGE_END) {
        return;
    }

    // Each direction shows one arrow if any reachable page exists before a boundary.
    pageIndex = Gp_MapRoomId - 1;
    while ((u8)pageIndex != MENU_MAP_ARROW_PAGE_BOUNDARY) {
        if (pageFlagIds[(u8)pageIndex] == MENU_MAP_ARROW_PAGE_BOUNDARY) {
            break;
        }
        if (pageFlagIds[(u8)pageIndex] == MENU_MAP_ARROW_PAGE_END) {
            break;
        }
        pageAvailable = _menuMapPageIsAvailable((u8)pageIndex, pageFlagIds[(u8)pageIndex]);
        if (pageAvailable == 1) {
            MENU_MAP_QUEUE_PAGE_ARROW(mapObject, arrowSprite, leftTexturePage, brightness, MENU_MAP_ARROW_LEFT_TEXTURE_U, MENU_MAP_ARROW_LEFT_X_PIXELS);
            break;
        }
        pageIndex--;
    }

    pageIndex = Gp_MapRoomId + 1;
    while ((u8)pageIndex <= D_8010F130[gGameSession->location.loc.stage - 1]) {
        if (pageFlagIds[(u8)pageIndex] == MENU_MAP_ARROW_PAGE_BOUNDARY) {
            return;
        }
        if (pageFlagIds[(u8)pageIndex] == MENU_MAP_ARROW_PAGE_END) {
            return;
        }
        pageAvailable = _menuMapPageIsAvailable((u8)pageIndex, pageFlagIds[(u8)pageIndex]);
        if (pageAvailable == 1) {
            MENU_MAP_QUEUE_PAGE_ARROW(mapObject, arrowSprite, rightTexturePage, brightness, MENU_MAP_ARROW_RIGHT_TEXTURE_U, MENU_MAP_ARROW_RIGHT_X_PIXELS);
            return;
        }
        pageIndex++;
    }
}
#undef MENU_MAP_QUEUE_PAGE_ARROW

void menuMapHelpTask(Task* task)
{
    enum {
        MENU_MAP_HELP_STATE_LOAD       = 0,
        MENU_MAP_HELP_STATE_WAIT       = 1,
        MENU_MAP_HELP_STATE_INPUT      = 2,
        MENU_MAP_HELP_DISPLAY_RESOURCE = 8,
        MENU_MAP_HELP_TEXT_TOP_PIXELS  = 20
    };
    UiObject* object;
    s32       panelControl;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(object)->panel, Gp_StrHelp);
    switch (task->state) {
        case MENU_MAP_HELP_STATE_LOAD:
            cdCmdEnqueueDisplayResource(MENU_MAP_HELP_DISPLAY_RESOURCE, D_8010F13D, CD_COMMAND_DISPLAY_LOAD_MENU);
            task->state = task->state + 1;
            break;
        case MENU_MAP_HELP_STATE_WAIT:
            if (cdCmdIsIdle() & 0xFFFF) {
                uiSpawnObject(&D_8010F178, 0, 0, 1, object);
                task->state = task->state + 1;
            }
            break;
        case MENU_MAP_HELP_STATE_INPUT:
            textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + MENU_MAP_HELP_TEXT_TOP_PIXELS, fsGetChunkPayload(), ITEM_MENU_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
            panelControl = object->panel.control.word;
            if (panelControl == USER_INTERFACE_PANEL_ACTIVE) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | PAD_BUTTON_TRIANGLE) != 0) {
                    object->resultValue = panelControl;
                    object->result      = USER_INTERFACE_RESULT_CONFIRM;
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                    object->result = USER_INTERFACE_RESULT_CANCEL;
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                }
            }
            break;
    }
}

void menuMapAreaNameTask(Task* task)
{
    enum {
        MENU_MAP_AREA_NAME_COLOR_RGB                 = 0x806020,
        MENU_MAP_AREA_NAME_HORIZONTAL_PADDING_PIXELS = 4,
        MENU_MAP_AREA_NAME_LEFT_INSET_PIXELS         = 2,
        MENU_MAP_AREA_NAME_BASELINE_PIXELS           = 11
    };
    TextDrawReq      measureRequest;
    TextDrawReq      drawRequest;
    GameSession*     session;
    MenuMapAreaName* names;
    u8*              text;
    UiObject*        object;
    s32              width;

    session = gGameSession;
    names   = Gp_MapNameTables[session->location.loc.stage - 1];
    object  = task->spawnArg2.pointer;
    if (names != NULL) {
        text = names[session->location.loc.area - 1].text;
        if (task->state == ITEM_MENU_STATE_INITIAL) {
            // Right alignment at x=0 measures the name without queuing glyphs.
            measureRequest.x          = 0;
            measureRequest.y          = 0;
            measureRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            measureRequest.colorRgb   = 0;
            measureRequest.glyphTable = TEXT_GLYPH_TABLE_LARGE;
            measureRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
            measureRequest.drawMode   = TEXT_DRAW_FILL_ONLY;
            textAlignLine(&measureRequest, text);
            width = -measureRequest.x + MENU_MAP_AREA_NAME_HORIZONTAL_PADDING_PIXELS;
            uiSetPanelContentSize(&(object)->panel, width, uiGetTextRowsHeight(1));
            task->state = task->state + 1;
        }
        drawRequest.x          = object->panel.contentLeft.unsignedValue + (object->panel.contentOriginX.unsignedValue + MENU_MAP_AREA_NAME_LEFT_INSET_PIXELS);
        drawRequest.y          = object->panel.contentTop.unsignedValue + (object->panel.contentOriginY.unsignedValue + MENU_MAP_AREA_NAME_BASELINE_PIXELS);
        drawRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        drawRequest.colorRgb   = MENU_MAP_AREA_NAME_COLOR_RGB;
        drawRequest.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        drawRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        drawRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&drawRequest, text);
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
        displaySetTaskDrawMode(DISPLAY_TASK_DRAW_CLEAR);
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
    _menuMapLoadPage();
    arg0->state = arg0->state + 1;
}

void Gp_MapFirstDrawTask(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2.pointer;
    if (cdCmdIsIdle() & 0xFFFF) {
        obj->panel.animationTicks = 1;
        _menuMapDrawPlayerCursor(arg0);
        _menuMapDrawFlagMarkers(arg0);
        _menuMapDrawPicture(arg0);
        _menuMapDrawAreas(arg0);
        _menuMapDrawPageArrows(arg0);
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
        _menuMapDrawPlayerCursor(arg0);
        _menuMapDrawFlagMarkers(arg0);
        _menuMapDrawPicture(arg0);
        _menuMapDrawAreas(arg0);
    }
}

/// Arms four closing updates before the map task restores the saved view image.
///
/// Requires a live map task and its `UiObject` in `spawnArg2.pointer`. Restores
/// two-VBlank frame timing, resets panel animation ticks, and marks the saved
/// image restoration pending. The caller selects the closing task state.
static void _menuMapPrepareClosing(Task* mapTask)
{
    enum {
        MENU_MAP_CLOSING_UPDATES = 4,
        MENU_MAP_RESTORE_PENDING = 0
    };
    UiObject* mapObject;

    mapObject = mapTask->spawnArg2.pointer;
    displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
    mapTask->killCountdown          = MENU_MAP_CLOSING_UPDATES;
    mapObject->panel.animationTicks = 0;
    mapTask->spawnArg1.value        = MENU_MAP_RESTORE_PENDING;
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
        displaySetTaskDrawMode(DISPLAY_TASK_DRAW_CLEAR);
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
        uiFitPanelToList(menu, &(obj)->panel);
        owner->state = owner->state + 1;
    }
    uiUpdateList(menu, &obj->panel);
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

void itemMenuDrawPeUpgradeRow(UiList* list, UiObject* object)
{
    TextDrawReq request;
    s32         abilityId;
    s32         noticeId;

    abilityId = object->owner->spawnArg1.value;
    if (abilityId & ATTACHMENT_MENU_LEVEL_MASK) {
        request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
        request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
        request.otIndex    = object->panel.otIndex.signedValue + 1;
        request.colorRgb   = list->colorRgb;
        request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        request.alignment  = TEXT_ALIGNMENT_LEFT;
        request.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&request, (const u8*)Gp_StrStrengthen);
    } else {
        request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
        request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
        request.otIndex    = object->panel.otIndex.signedValue + 1;
        request.colorRgb   = list->colorRgb;
        request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        request.alignment  = TEXT_ALIGNMENT_LEFT;
        request.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&request, (const u8*)Gp_StrRevive);
    }
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            noticeId = ITEM_MENU_NOTICE_NONE;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if ((abilityId & ATTACHMENT_MENU_LEVEL_MASK) == ITEM_MENU_PE_MAX_LEVEL) {
                noticeId = ITEM_MENU_NOTICE_MAX_PE_LEVEL;
            }
            if (noticeId >= 0) {
                itemMenuSpawnNotice(object, noticeId, 0, ITEM_MENU_NOTICE_RESULT_DISMISS);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                uiSpawnObject(&D_8010F7A4, abilityId, USER_INTERFACE_PANEL_ACTIVE, 0xA, object);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
    }
}

void itemMenuPeCommandTask(Task* task)
{
    UiObject*          object;
    UiList*            list;
    Task*              ownerTask;
    Task*              child;
    Task*              nextChild;
    Task*              firstChild;
    UiObject*          childObject;
    UiListRowCallback* callbacks;
    s32                childResult;
    s32                rowCount;

    list   = &D_8010F5FC;
    object = task->spawnArg2.pointer;
    if (task->state == ITEM_MENU_STATE_INITIAL) {
        callbacks                           = list->rowCallbacks;
        callbacks[0]                        = itemMenuDrawPeUpgradeRow;
        callbacks[1]                        = itemMenuDrawPeCancelRow;
        rowCount                            = ITEM_MENU_PE_COMMAND_ROW_COUNT;
        list->visibleRowCount.unsignedValue = rowCount;
        list->itemCount                     = rowCount;
        if ((task->spawnArg1.value & ATTACHMENT_MENU_LEVEL_MASK) != ITEM_MENU_PE_MAX_LEVEL) {
            itemMenuSetPreviewItem(task->spawnArg1.value + 1, CD_COMMAND_DISPLAY_LOAD_MENU);
        }
    }
    ownerTask      = object->owner;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (ownerTask->state == ITEM_MENU_STATE_INITIAL) {
        uiFitPanelToList(list, &(object)->panel);
        ownerTask->state = ownerTask->state + 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
    // Child acceptance restores input; a finished upgrade completes this command menu.
    firstChild = ownerTask->firstChild;
    if (firstChild != NULL) {
        child = firstChild;
        do {
            childObject = child->spawnArg2.pointer;
            childResult = childObject->result;
            nextChild   = child->nextSibling;
            switch (childResult) {
                case USER_INTERFACE_RESULT_CONFIRM:
                    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    uiStartTreeClosing(childObject, childObject->owner);
                    break;
                case USER_INTERFACE_RESULT_DISMISS:
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                    break;
                case USER_INTERFACE_RESULT_CANCEL:
                    object->result = childResult;
                    break;
            }
            child = nextChild;
        } while (child != ownerTask->firstChild);
    }
}

/// Clears a discarded consumable from every live-save weapon's two selections.
///
/// `itemId` is 0xA0..0xBF. Both the selected id and remaining quantity are reset;
/// other selections and weapon metadata are retained.
static inline void _itemMenuClearDiscardedConsumableSelections(s32 itemId)
{
    s32                  weaponItemId;
    EquipmentWeaponLoad* weaponLoad;

    weaponItemId = EQUIPMENT_WEAPON_ITEM_FIRST;
    do {
        weaponLoad = equipmentGetWeaponLoad(weaponItemId);
        if (weaponLoad->primaryItemId == itemId) {
            weaponLoad->primaryItemId = INVENTORY_ITEM_NONE;
            weaponLoad->primaryQty    = 0;
        }
        if (weaponLoad->secondaryItemId == itemId) {
            weaponLoad->secondaryItemId = INVENTORY_ITEM_NONE;
            weaponLoad->secondaryQty    = 0;
        }
        weaponItemId += 1;
    } while (weaponItemId < EQUIPMENT_WEAPON_ITEM_FIRST + (s32)ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems));
}

void itemMenuDiscardTask(Task* task)
{
    InventoryItemRow* selectedRow;
    s32               itemId;
    Task*             child;
    UiObject*         answerObject;
    UiObject*         commandObject;
    UiObject*         object;
    s32               restrictionNotice;
    const u8*         promptText;
    UiObject*         confirmationMenu;

    selectedRow = Gp_SelItemRec;
    itemId      = selectedRow->itemId;

    object            = task->spawnArg2.pointer;
    object->result    = USER_INTERFACE_RESULT_NONE;
    restrictionNotice = ITEM_MENU_DISCARD_ALLOWED;
    if (Gp_ItemDescs[itemId].flags & ITEM_FLAG_NO_DISCARD) {
        restrictionNotice = ITEM_MENU_NOTICE_CANNOT_DISCARD;
    } else if (((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) && (equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId) > 0)) {
        restrictionNotice = ITEM_MENU_NOTICE_CANNOT_DISCARD_LOADED;
    } else if (_equipmentIsSelectedItem(itemId) != 0) {
        restrictionNotice = ITEM_MENU_NOTICE_CANNOT_DISCARD_EQUIPPED;
    }
    if (restrictionNotice != ITEM_MENU_DISCARD_ALLOWED) {
        task->spawnArg1.value = restrictionNotice;
        itemMenuNoticeTask(task);
        return;
    }
    promptText = Gp_PromptTexts[ITEM_MENU_PROMPT_CONFIRM_DISCARD];
    if (task->state == ITEM_MENU_STATE_INITIAL) {
        uiSizePanelForTextWide(&(object)->panel, promptText);
        confirmationMenu = itemMenuSpawnYesNoMenuDefaultNo(object);
        if (confirmationMenu != NULL) {
            confirmationMenu->panel.bounds.unsignedRect.x = (object->panel.bounds.unsignedRect.x + object->panel.bounds.unsignedRect.w) - 0x18;
        }
        task->state += 1;
    }
    uiDrawPanelLabelWithChildFocus(&(object)->panel, Gp_StrAttention2);
    textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, promptText, ITEM_MENU_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);

    // A Yes answer removes the whole stack after clearing its equipment selections.
    child = task->firstChild;
    if (child != NULL) {
        answerObject = child->spawnArg2.pointer;
        if (answerObject->result == USER_INTERFACE_RESULT_CONFIRM) {
            commandObject = task->parent->spawnArg2.pointer;
            if (answerObject->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems)) {
                    EquipmentWeaponLoad* weaponLoad;
                    PlayerStatus*        player;

                    weaponLoad = equipmentGetWeaponLoad(itemId);
                    player     = &gPlayerStatus;
                    equipmentClearRemovableLoads(itemId);
                    weaponLoad->field_4 = 0;
                    if (player->weapon == (itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1))) {
                        player->weapon = PLAYER_STATUS_EQUIPMENT_NONE;
                    }
                } else if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
                    _itemMenuClearDiscardedConsumableSelections(itemId);
                } else if ((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus)) {
                    PlayerStatus* player;

                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus[itemId - ITEM_MENU_ARMOR_ITEM_FIRST] = 0;
                    player                                                                                       = &gPlayerStatus;
                    if (player->armor == (itemId - (ITEM_MENU_ARMOR_ITEM_FIRST - 1))) {
                        player->armor = PLAYER_STATUS_EQUIPMENT_NONE;
                    }
                }
                inventoryRemoveItemRow(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, selectedRow, INVENTORY_REMOVE_WHOLE_STACK);
            }
            commandObject->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void itemMenuDrawPeAbilityRow(UiList* list, UiObject* object)
{
    s32       level;
    s32       abilityId;
    s32       elementIndex;
    s32       energyIndex;
    s32       elementIdBits;
    s32       energyIdBase;
    s32       panelControl;
    s32       activeMode;
    UiObject* childObject;

    elementIndex  = object->owner->spawnArg1.value;
    energyIndex   = list->currentItemIndex;
    level         = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[energyIndex + elementIndex * ITEM_MENU_PE_ABILITIES_PER_ELEMENT];
    elementIdBits = elementIndex * (1 << ATTACHMENT_MENU_ELEMENT_SHIFT);
    energyIdBase  = energyIndex * (1 << ATTACHMENT_MENU_ENERGY_SHIFT) + ITEM_TEXT_PACKED_ID_FIRST;
    abilityId     = elementIdBits + energyIdBase + level;
    if (level == 0) {
        list->colorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
    }
    itemMenuDrawItemRow(object, list->rowTextX.signedValue, list->rowTextY.signedValue, abilityId, list->colorRgb, 0);
    if (level != 0) {
        itemMenuDrawParasiteEnergyLevel(object, list->rowTextX.signedValue, list->rowTextY.signedValue, level, list->colorRgb);
    }
    panelControl = object->panel.control.word;
    activeMode   = USER_INTERFACE_PANEL_ACTIVE;
    if (((panelControl >> 16) == activeMode) || (panelControl == activeMode)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            uiSetPromptPeItem(abilityId, 0, 0);
        }
    }
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        itemMenuSetPreviewItem(abilityId, CD_COMMAND_DISPLAY_LOAD_MENU);
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            activeMode  = USER_INTERFACE_PANEL_ACTIVE;
            childObject = uiSpawnObject(&D_8010F670, abilityId, activeMode, activeMode, object);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            if (childObject != NULL) {
                uiPositionRowDialog(&(childObject)->panel, list, &(object)->panel);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            activeMode = USER_INTERFACE_PANEL_ACTIVE;
            uiSpawnObject(&D_8010F7F8, abilityId, activeMode, activeMode, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

/// Sets the list's two or three visible PE abilities from one element's saved levels.
///
/// Borrows three levels in 0..3. The third ability is available when learned
/// or when both preceding abilities reach level 3; item and visible counts agree.
static inline void _itemMenuSetPeVisibleAbilityCount(UiList* list, const u8* elementLevels)
{
    enum { ITEM_MENU_PE_LEVEL_UNLEARNED = 0 };
    if (elementLevels[ITEM_MENU_PE_ABILITIES_PER_ELEMENT - 1] != ITEM_MENU_PE_LEVEL_UNLEARNED || (elementLevels[0] == ITEM_MENU_PE_MAX_LEVEL && elementLevels[1] == elementLevels[0])) {
        list->itemCount = list->visibleRowCount.unsignedValue = ITEM_MENU_PE_ABILITIES_PER_ELEMENT;
    } else {
        list->itemCount = list->visibleRowCount.unsignedValue = ITEM_MENU_PE_ABILITIES_PER_ELEMENT - 1;
    }
}

void itemMenuPeElementTask(Task* task)
{
    UiObject* object;
    UiList*   list;
    Task*     child;
    UiObject* childObject;
    const u8* elementLevels;
    s32       childResult;
    s32       lastItemIndex;
    s32       captionIndex;

    object       = task->spawnArg2.pointer;
    captionIndex = task->spawnArg1.value;
    task->status = 0;
    list         = &D_80114DF8[task->spawnArg1.value];
    uiDrawPanelLabel(&(object)->panel, (const char*)D_8010F644[captionIndex]);
    if (task->state == ITEM_MENU_STATE_INITIAL) {
        list->rowCallbacks                        = D_8010F620;
        list->itemCount                           = ITEM_MENU_PE_ABILITIES_PER_ELEMENT;
        list->visibleRowCount.unsignedValue       = ITEM_MENU_PE_ABILITIES_PER_ELEMENT;
        list->wrapNavigation                      = 0;
        list->rowHeight                           = ITEM_MENU_PE_ROW_HEIGHT_PIXELS;
        list->currentItemIndex                    = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        uiFitPanelToList(list, &(object)->panel);
        elementLevels = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[task->spawnArg1.value * ITEM_MENU_PE_ABILITIES_PER_ELEMENT];
        _itemMenuSetPeVisibleAbilityCount(list, elementLevels);
        list->firstVisibleItemIndex.unsignedValue = 0;
        list->selectedItemIndex                   = 0;
        list->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        if (task->spawnArg1.value & ITEM_MENU_PE_ELEMENT_LOWER_ROW_BIT) {
            object->panel.bounds.unsignedRect.y = object->panel.bounds.unsignedRect.h - 0x50;
        }
        task->state++;
    }
    elementLevels = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[task->spawnArg1.value * ITEM_MENU_PE_ABILITIES_PER_ELEMENT];
    _itemMenuSetPeVisibleAbilityCount(list, elementLevels);
    // Transfer focus around the four siblings, clamping horizontal row selection.
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (list->actionResult == USER_INTERFACE_LIST_ACTION_AT_END && !(task->spawnArg1.value & ITEM_MENU_PE_ELEMENT_LOWER_ROW_BIT)) {
            UiObject* verticalObject;
            UiList*   verticalList;

            verticalObject = task->nextSibling->spawnArg2.pointer;
            verticalList   = &D_80114DF8[task->spawnArg1.value] + 1;
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            verticalObject->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
            verticalList->selectedItemIndex    = 0;
            object->panel.control.word         = USER_INTERFACE_PANEL_INACTIVE;
        } else if (list->actionResult == USER_INTERFACE_LIST_ACTION_AT_START && (task->spawnArg1.value & ITEM_MENU_PE_ELEMENT_LOWER_ROW_BIT)) {
            UiObject* verticalObject;
            UiList*   verticalList;

            verticalObject = task->nextSibling->nextSibling->nextSibling->spawnArg2.pointer;
            verticalList   = &D_80114DF8[task->spawnArg1.value] - 1;
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            verticalObject->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
            verticalList->selectedItemIndex    = verticalList->itemCount - 1;
            object->panel.control.word         = USER_INTERFACE_PANEL_INACTIVE;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
            if (!(task->spawnArg1.value & ITEM_MENU_PE_ELEMENT_RIGHT_COLUMN_BIT)) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                    UiObject* horizontalObject;
                    UiList*   horizontalList;

                    horizontalObject = task->nextSibling->nextSibling->spawnArg2.pointer;
                    horizontalList   = &D_80114DF8[task->spawnArg1.value] + 2;
                    sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                    horizontalObject->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
                    lastItemIndex                        = horizontalList->itemCount - 1;
                    horizontalList->selectedItemIndex    = list->selectedItemIndex;
                    if (lastItemIndex < horizontalList->selectedItemIndex) {
                        horizontalList->selectedItemIndex = lastItemIndex;
                    }
                    object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
                UiObject* horizontalObject;
                UiList*   horizontalList;

                horizontalObject = task->nextSibling->nextSibling->spawnArg2.pointer;
                horizontalList   = &D_80114DF8[task->spawnArg1.value] - 2;
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                horizontalObject->panel.control.word = USER_INTERFACE_PANEL_FOCUS_TRANSFER;
                lastItemIndex                        = horizontalList->itemCount - 1;
                horizontalList->selectedItemIndex    = list->selectedItemIndex;
                if (lastItemIndex < horizontalList->selectedItemIndex) {
                    horizontalList->selectedItemIndex = lastItemIndex;
                }
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
    }
    child = task->firstChild;
    if (child != NULL) {
        childObject = child->spawnArg2.pointer;
        childResult = childObject->result;
        if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(childObject, childObject->owner);
            object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
        } else if (childResult == USER_INTERFACE_RESULT_CANCEL) {
            object->result = childResult;
        }
    }
    if (object->panel.control.word == USER_INTERFACE_PANEL_FOCUS_TRANSFER) {
        object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }
}

void itemMenuDrawAbilityDescription(UiObject* object, s32 abilityId)
{
    TextDrawReq request;
    s16         contentTop;
    s32         level;
    s32         descriptionY;
    s32         textColorRgb;
    const u8*   text;

    contentTop   = object->panel.contentTop.unsignedValue;
    level        = abilityId & ATTACHMENT_MENU_LEVEL_MASK;
    descriptionY = contentTop + 0xF;
    text         = itemGetText(abilityId, ITEM_TEXT_DESCRIPTION_FIRST, 1);
    textColorRgb = 0x606060;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, descriptionY, text, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    text = itemGetText(abilityId, ITEM_TEXT_DESCRIPTION_SECOND, 1);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, contentTop + 0x1E, text, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    contentTop   = object->panel.contentTop.unsignedValue;
    descriptionY = contentTop + 0xF;
    uiDrawVerticalSeparator(&(object)->panel, contentTop, object->panel.contentBottom.signedValue, 0x2F);
    if (level) {
        request.x          = object->panel.contentOriginX.unsignedValue + 0x34;
        request.y          = (s16)(object->panel.contentOriginY.unsignedValue - 6) + descriptionY;
        request.otIndex    = object->panel.otIndex.signedValue + 1;
        request.colorRgb   = textColorRgb;
        request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        request.alignment  = TEXT_ALIGNMENT_LEFT;
        request.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&request, Gp_StrCastCost);
        _itemMenuDrawAbilityParameterBar(object, abilityId, 0, 0x34, contentTop + 0x1A, ATTACHMENT_LEVEL_CAST_COST);
    }
}

void itemMenuNoticeTask(Task* task)
{
    enum {
        ITEM_MENU_NOTICE_STATE_INIT     = 0,
        ITEM_MENU_NOTICE_TIMEOUT_TICKS  = 188,
        ITEM_MENU_NOTICE_ACCEPTED_TICKS = 0x7FFF
    };
    UiObject* object;
    const u8* text;
    s32       textColorRgb;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(object)->panel, Gp_StrNotice3);

    textColorRgb = 0x606060;
    text         = Gp_NoticeTexts[(u16)task->spawnArg1.value];

    if (task->state == ITEM_MENU_NOTICE_STATE_INIT) {
        uiSizePanelForTextDefault(&(object)->panel, text);
        task->killCountdown = ITEM_MENU_NOTICE_TIMEOUT_TICKS;
        task->state         = task->state + 1;
    }

    textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, text, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);

    task->killCountdown--;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if ((task->killCountdown <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
            task->killCountdown = ITEM_MENU_NOTICE_ACCEPTED_TICKS;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }

    if ((s16)(task->spawnArg1.value >> 16) == 0) {
        if (object->result == USER_INTERFACE_RESULT_CONFIRM) {
            object->result = USER_INTERFACE_RESULT_DISMISS;
        }
    }
}

void itemMenuPeUpgradeTask(Task* task)
{
/// Discounts the purchase panel's s32 cost lvalue in place.
///
/// Reads live gameMode first, then clearCount only for a nonpositive mode.
/// Positive modes pay 4/5; cleared normal games pay 2/5. Signed division rounds
/// toward zero and the caller later keeps the low 16 bits. costValue must be
/// a side-effect-free s32 lvalue: the selected branch reads and writes it.
/// Expands to a standalone block and is defined only for this purchase task.
#define ITEM_MENU_DISCOUNT_PE_UPGRADE_COST(costValue)                         \
    {                                                                         \
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {          \
            (costValue) = ((costValue) * 4) / 5;                              \
        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) { \
            (costValue) = ((costValue) * 2) / 5;                              \
        }                                                                     \
    }

    u8            numberText[0x20];
    TextDrawReq   expLabelRequest;
    TextDrawReq   costLabelRequest;
    TextDrawReq   bonusLabelRequest;
    TextDrawReq   mpLabelRequest;
    UiObject*     object;
    UiObject*     confirmationMenu;
    UiObject*     childObject;
    Task*         child;
    Task*         nextChild;
    s32           abilityId;
    s32           bonusElementIndex;
    s32           bonusEnergyIndex;
    s32           bonusLevel;
    s32           purchaseElementIndex;
    s32           purchaseEnergyIndex;
    s32           purchaseLevel;
    s32           elementIndex;
    s32           energyIndex;
    s32           nextLevel;
    s32           lineY;
    s32           bonusColumn;
    s32           labelX;
    s32           displayCost;
    PlayerStatus* player;
    s32           purchaseCost;

    object         = task->spawnArg2.pointer;
    abilityId      = task->spawnArg1.value;
    object->result = USER_INTERFACE_RESULT_NONE;

    if (task->state == ITEM_MENU_STATE_INITIAL) {
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(2) + 1);
        confirmationMenu = itemMenuSpawnYesNoMenu(object);
        if (confirmationMenu != NULL) {
            confirmationMenu->panel.animationTicks         = object->panel.animationTicks - 8;
            confirmationMenu->panel.bounds.unsignedRect.y += 4;
        }
        uiSpawnObject(D_8010F7C0, abilityId, 0, 1, object);
        task->state = task->state + 1;
    }

    labelX = 0x20;
    lineY  = object->panel.contentTop.signedValue + 0xF;

    expLabelRequest.x          = object->panel.contentOriginX.unsignedValue + labelX;
    expLabelRequest.y          = (s16)(object->panel.contentOriginY.unsignedValue - 8) + lineY;
    expLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    expLabelRequest.colorRgb   = ITEM_MENU_TEXT_COLOR_RGB;
    expLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    expLabelRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    expLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&expLabelRequest, (const u8*)D_8009720C);

    costLabelRequest.x          = object->panel.contentOriginX.unsignedValue + labelX;
    costLabelRequest.y          = (s16)(object->panel.contentOriginY.unsignedValue - 2) + lineY;
    costLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    costLabelRequest.colorRgb   = ITEM_MENU_TEXT_COLOR_RGB;
    costLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    costLabelRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    costLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&costLabelRequest, (const u8*)Gp_StrCost);

    elementIndex = ((abilityId + 1) & ATTACHMENT_MENU_ELEMENT_MASK) >> ATTACHMENT_MENU_ELEMENT_SHIFT;
    energyIndex  = ((abilityId + 1) & ATTACHMENT_MENU_ENERGY_MASK) >> ATTACHMENT_MENU_ENERGY_SHIFT;
    nextLevel    = (abilityId + 1) & ATTACHMENT_MENU_LEVEL_MASK;
    displayCost  = Gp_IdParamHi.rows[(elementIndex * ITEM_MENU_PE_ABILITIES_PER_ELEMENT + energyIndex) * ITEM_MENU_PE_ABILITIES_PER_ELEMENT + nextLevel].column.expCost;
    ITEM_MENU_DISCOUNT_PE_UPGRADE_COST(displayCost);
    textDrawUiLine(object, labelX + 0x30, lineY, textItoaSigned(numberText, displayCost & 0xFFFF), ITEM_MENU_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    lineY += 0xF;

    bonusLabelRequest.x          = object->panel.contentOriginX.unsignedValue + labelX;
    bonusLabelRequest.y          = (s16)(object->panel.contentOriginY.unsignedValue - 8) + lineY;
    bonusLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    bonusLabelRequest.colorRgb   = ITEM_MENU_TEXT_COLOR_RGB;
    bonusLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    bonusLabelRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    bonusLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&bonusLabelRequest, (const u8*)Gp_StrBonus);

    mpLabelRequest.x          = object->panel.contentOriginX.unsignedValue + labelX;
    mpLabelRequest.y          = (s16)(object->panel.contentOriginY.unsignedValue - 2) + lineY;
    mpLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    mpLabelRequest.colorRgb   = ITEM_MENU_TEXT_COLOR_RGB;
    mpLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    mpLabelRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    mpLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&mpLabelRequest, (const u8*)D_80097220);

    bonusColumn       = ATTACHMENT_LEVEL_MP_BONUS;
    bonusElementIndex = ((abilityId + 1) & ATTACHMENT_MENU_ELEMENT_MASK) >> ATTACHMENT_MENU_ELEMENT_SHIFT;
    bonusEnergyIndex  = ((abilityId + 1) & ATTACHMENT_MENU_ENERGY_MASK) >> ATTACHMENT_MENU_ENERGY_SHIFT;
    bonusLevel        = (abilityId + 1) & ATTACHMENT_MENU_LEVEL_MASK;
    textDrawUiLine(object, labelX + 0x30, lineY, textItoaSigned(numberText, Gp_IdParamHi.rows[(bonusElementIndex * ITEM_MENU_PE_ABILITIES_PER_ELEMENT + bonusEnergyIndex) * ITEM_MENU_PE_ABILITIES_PER_ELEMENT + bonusLevel].value[bonusColumn]),
                   ITEM_MENU_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    // Recheck affordability on Yes, then purchase the level and restore the new MP maximum.
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObject          = child->spawnArg2.pointer;
            nextChild            = child->nextSibling;
            purchaseElementIndex = ((abilityId + 1) & ATTACHMENT_MENU_ELEMENT_MASK) >> ATTACHMENT_MENU_ELEMENT_SHIFT;
            purchaseEnergyIndex  = ((abilityId + 1) & ATTACHMENT_MENU_ENERGY_MASK) >> ATTACHMENT_MENU_ENERGY_SHIFT;
            purchaseLevel        = (abilityId + 1) & ATTACHMENT_MENU_LEVEL_MASK;
            if (childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
                if (childObject->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                    player       = &gPlayerStatus;
                    purchaseCost = Gp_IdParamHi.rows[(purchaseElementIndex * ITEM_MENU_PE_ABILITIES_PER_ELEMENT + purchaseEnergyIndex) * ITEM_MENU_PE_ABILITIES_PER_ELEMENT + purchaseLevel].column.expCost;
                    ITEM_MENU_DISCOUNT_PE_UPGRADE_COST(purchaseCost);
                    if (player->exp < (purchaseCost & 0xFFFF)) {
                        uiSpawnObject(&D_8010F788, ITEM_MENU_NOTICE_INSUFFICIENT_EXP, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                        uiStartTreeClosing(childObject, childObject->owner);
                    } else {
                        purchaseCost = Gp_IdParamHi.rows[(purchaseElementIndex * ITEM_MENU_PE_ABILITIES_PER_ELEMENT + purchaseEnergyIndex) * ITEM_MENU_PE_ABILITIES_PER_ELEMENT + purchaseLevel].column.expCost;
                        ITEM_MENU_DISCOUNT_PE_UPGRADE_COST(purchaseCost);
                        player->exp                                                                                                                                                                                                                                            -= purchaseCost & 0xFFFF;
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[((abilityId & ATTACHMENT_MENU_ENERGY_MASK) >> ATTACHMENT_MENU_ENERGY_SHIFT) + ((abilityId & ATTACHMENT_MENU_ELEMENT_MASK) >> ATTACHMENT_MENU_ELEMENT_SHIFT) * ITEM_MENU_PE_ABILITIES_PER_ELEMENT] = (abilityId & ATTACHMENT_MENU_LEVEL_MASK) + 1;
                        equipmentRecalculateMaxMp();
                        player->mp     = player->mpMax;
                        Gp_HpMpWork.mp = player->mp;
                        object->result = USER_INTERFACE_RESULT_DISMISS;
                    }
                } else {
                    object->result = USER_INTERFACE_RESULT_DISMISS;
                }
            } else if (childObject->result == USER_INTERFACE_RESULT_DISMISS) {
                object->result = USER_INTERFACE_RESULT_DISMISS;
            }
            child = nextChild;
        } while (child != task->firstChild);
    }
#undef ITEM_MENU_DISCOUNT_PE_UPGRADE_COST
}

/// Draws a PE level parameter as a number and bar, optionally beside its previous level.
///
/// `column` selects 0..7; `abilityId` uses the low-six-bit menu encoding.
/// `barX` and `valueY` are pixels relative to the panel content origin; the
/// bar ends at the content's right edge and lies three pixels above valueY.
/// Exactly 1 enables previous-level comparison; callers supply level 1..3
/// after an upgrade. MP bonus always suppresses comparison and uses a signed
/// prefix. EXP cost applies the live save's mode/clear discount.
/// Casting cost, ATP loss (cast frames), and MP bonus use maxima 60, 95, and
/// 20; other columns scale to the current value, which must be nonzero when
/// comparing. Bars are not clamped to their maxima. Queues packets on panel OT + 1.
static void _itemMenuDrawAbilityParameterBar(UiObject* object, s32 abilityId, s32 comparePreviousLevel, s32 barX, s32 valueY, s32 column)
{
    enum {
        ITEM_MENU_ABILITY_CAST_COST_BAR_MAX = 60,
        ITEM_MENU_ABILITY_ATP_LOSS_BAR_MAX  = 95,
        ITEM_MENU_ABILITY_MP_BONUS_BAR_MAX  = 20
    };
/// Prepares medium translucent outlined value text at an absolute screen X.
///
/// Used only by this parameter bar. `requestValue` is a TextDrawReq lvalue and
/// `baseYValue` a separate s32 lvalue; `objectValue` is a live panel object. All arguments must
/// be side-effect-free because request/object expressions are evaluated repeatedly.
/// `valueY` is panel-relative pixels; the supplied base-Y lvalue receives the
/// screen origin minus the font baseline adjustment. Expands to a standalone block.
#define ITEM_MENU_PREPARE_ABILITY_VALUE_TEXT(requestValue, objectValue, screenX, valueY, colorValue, baseYValue) \
    {                                                                                                            \
        (requestValue).x          = (screenX);                                                                   \
        (baseYValue)              = (objectValue)->panel.contentOriginY.unsignedValue - 6;                       \
        (requestValue).y          = (baseYValue) + (valueY);                                                     \
        (requestValue).otIndex    = (objectValue)->panel.otIndex.signedValue + 1;                                \
        (requestValue).colorRgb   = (colorValue);                                                                \
        (requestValue).glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                                     \
        (requestValue).alignment  = TEXT_ALIGNMENT_LEFT;                                                         \
        (requestValue).drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;                                              \
    }

    struct
    {
        u8          numberText[0x20]; // Decimal value text reused after each draw
        TextDrawReq previousRequest;  // Previous level's value, left of the change marker
        TextDrawReq currentRequest;   // Current value, alone or right of the change marker
    } draw;
    s32   barWidth;
    s32   contentRight;
    s32   rawValue;
    s32   value;
    s32   barMaximum;
    s32   barLeft;
    SPRT* changeSprite;
    s32   rawPreviousValue;
    s32   previousValue;
    s32   colorRgb;
    s32   textBaseY;
    s32   previousAbilityId;
    s32   spriteBaseX;
    s32   elementIndex;
    s32   energyIndex;
    s32   level;
    s32   previousElementIndex;
    s32   previousEnergyIndex;
    s32   previousLevel;
    s32   singleValueBaseY;
    u8*   valueText;
    {
        elementIndex = (abilityId & ATTACHMENT_MENU_ELEMENT_MASK) >> ATTACHMENT_MENU_ELEMENT_SHIFT;
        energyIndex  = (abilityId & ATTACHMENT_MENU_ENERGY_MASK) >> ATTACHMENT_MENU_ENERGY_SHIFT;
        level        = abilityId & ATTACHMENT_MENU_LEVEL_MASK;
        rawValue     = Gp_IdParamHi.rows[(((elementIndex * 3) + energyIndex) * 3) + level].value[column];
    }
    if (column == ATTACHMENT_LEVEL_EXP_COST) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
            rawValue = (rawValue * 4) / 5;
        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
            rawValue = (rawValue * 2) / 5;
        }
        contentRight = object->panel.contentRight.signedValue;
    } else {
        contentRight = object->panel.contentRight.signedValue;
    }
    barLeft  = barX;
    barWidth = contentRight - barLeft;
    value    = rawValue & 0xFFFF;
    switch (column) {
        case ATTACHMENT_LEVEL_CAST_COST:
            barMaximum = ITEM_MENU_ABILITY_CAST_COST_BAR_MAX;
            break;

        case ATTACHMENT_LEVEL_ATP_LOSS:
            barMaximum = ITEM_MENU_ABILITY_ATP_LOSS_BAR_MAX;
            break;

        case ATTACHMENT_LEVEL_MP_BONUS:
            barMaximum           = ITEM_MENU_ABILITY_MP_BONUS_BAR_MAX;
            comparePreviousLevel = 0;
            break;

        default:
            barMaximum = value;
            break;
    }

    if (comparePreviousLevel == 1) {
        // Compare the preceding level and queue its increase/decrease marker.
        previousAbilityId = abilityId - 1;
        {
            previousElementIndex = (previousAbilityId & ATTACHMENT_MENU_ELEMENT_MASK) >> ATTACHMENT_MENU_ELEMENT_SHIFT;
            previousEnergyIndex  = (previousAbilityId & ATTACHMENT_MENU_ENERGY_MASK) >> ATTACHMENT_MENU_ENERGY_SHIFT;
            previousLevel        = previousAbilityId & ATTACHMENT_MENU_LEVEL_MASK;
            rawPreviousValue     = Gp_IdParamHi.rows[(((previousElementIndex * 3) + previousEnergyIndex) * 3) + previousLevel].value[column];
        }
        if (column == ATTACHMENT_LEVEL_EXP_COST) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
                rawPreviousValue = (rawPreviousValue * 4) / 5;
            } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
                rawPreviousValue = (rawPreviousValue * 2) / 5;
            }
        }
        colorRgb                        = 0x606060;
        changeSprite                    = gGpuPrimCursor;
        changeSprite->y0                = (object->panel.contentOriginY.unsignedValue + valueY) - 0xC;
        previousValue                   = rawPreviousValue & 0xFFFF;
        draw.previousRequest.x          = object->panel.contentOriginX.unsignedValue + barLeft;
        textBaseY                       = object->panel.contentOriginY.unsignedValue - 6;
        draw.previousRequest.y          = textBaseY + valueY;
        gGpuPrimCursor                  = changeSprite + 1;
        draw.previousRequest.otIndex    = (object->panel.otIndex.signedValue) + 1;
        draw.previousRequest.colorRgb   = colorRgb;
        draw.previousRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        draw.previousRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        draw.previousRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&draw.previousRequest, textItoaSigned(draw.numberText, previousValue));
        if (previousValue < value) {
            s32 barY;

            barY = valueY - 3;
            uiFillRectInterior(&(object)->panel, barLeft, barY, (barWidth * previousValue) / barMaximum, 3, 0x1741FU);
            colorRgb = 0xD287F;
            uiDrawRaisedRect(&object->panel, barLeft, barY, ((barWidth * value) / barMaximum), 3, 0x1A50FE);
            changeSprite->u0                          = 0xA0;
            GPU_PRIMITIVE_COLOR_WORD(changeSprite, 0) = colorRgb;
            changeSprite->y0                          = changeSprite->y0 - 1;
        } else {
            if (value < previousValue) {
                s32 barY;

                barY = valueY - 3;
                uiFillRectInterior(&(object)->panel, barLeft, barY, (barWidth * value) / barMaximum, 3, 0x1741FU);
                colorRgb = 0x1741F;
                uiDrawRaisedRect(&object->panel, barLeft, barY, ((barWidth * previousValue) / barMaximum), 3, 1);
                changeSprite->u0                          = 0x30;
                GPU_PRIMITIVE_COLOR_WORD(changeSprite, 0) = colorRgb;
            } else {
                if (value > 0) {
                    uiDrawRaisedRect(&object->panel, barLeft, (valueY - 3), ((barWidth * value) / barMaximum), 3, 0x1741F);
                }
                changeSprite->u0                          = 0x78;
                GPU_PRIMITIVE_COLOR_WORD(changeSprite, 0) = colorRgb;
            }
        }
        spriteBaseX        = object->panel.contentOriginX.unsignedValue + barLeft;
        changeSprite->w    = 8;
        changeSprite->h    = 8;
        changeSprite->v0   = 0x60;
        changeSprite->clut = 0x3C09;
        setSprt(changeSprite);
        changeSprite->x0 = spriteBaseX + 0x14;
        addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, changeSprite);
        uiQueueTexturePage((object->panel.otIndex.signedValue) + 1, 0);
        ITEM_MENU_PREPARE_ABILITY_VALUE_TEXT(draw.currentRequest, object, (object->panel.contentOriginX.unsignedValue + 0x1E) + barLeft, valueY, colorRgb, textBaseY);
        textDrawString(&draw.currentRequest, textItoaSigned(draw.numberText, value));
    } else {
        if (column == ATTACHMENT_LEVEL_MP_BONUS) {
            ITEM_MENU_PREPARE_ABILITY_VALUE_TEXT(draw.currentRequest, object, object->panel.contentOriginX.unsignedValue + barLeft, valueY, 0x606060, singleValueBaseY);
            valueText = textItoaSignPrefixed(draw.numberText, value);
        } else {
            ITEM_MENU_PREPARE_ABILITY_VALUE_TEXT(draw.currentRequest, object, object->panel.contentOriginX.unsignedValue + barLeft, valueY, 0x606060, singleValueBaseY);
            valueText = textItoaSigned(draw.numberText, value);
        }
        textDrawString(&draw.currentRequest, valueText);
        if (value > 0) {
            uiDrawRaisedRect(&object->panel, barLeft, (valueY - 3), ((barWidth * value) / barMaximum), 3, 0x1741F);
        }
    }
#undef ITEM_MENU_PREPARE_ABILITY_VALUE_TEXT
}

/// Draws a PE ability's name, level, area preview, casting cost and ATP-loss bars.
///
/// Borrows the live object and a packed PE id with element 0..3, ability 0..2,
/// and level 0..3. Exactly 1 for nextLevel selects level+1 and compares against
/// the previous level, except level zero has no comparison. That mode requires
/// a starting level below 3. Preview packets remain hidden until CD resources
/// are ready; coordinates are pixels relative to the panel content origin.
static void _itemMenuDrawPeSpecifications(UiObject* object, s32 abilityId, s32 nextLevel)
{
    TextDrawReq areaLabelRequest;
    TextDrawReq costLabelRequest;
    TextDrawReq atpLabelRequest;
    s32         textColorRgb;
    s32         parameterColorRgb;
    s32         contentX;
    s32         contentY;
    s32         level;
    s32         contentTop;
    s32         parameterTop;
    const u8*   labelText;

    // Level zero has no previous learned level to compare against.
    if (nextLevel == ITEM_MENU_PE_DISPLAY_NEXT_LEVEL) {
        if ((abilityId & ATTACHMENT_MENU_LEVEL_MASK) == 0) {
            nextLevel = ITEM_MENU_PE_DISPLAY_CURRENT_LEVEL;
        }
        abilityId += 1;
    }

    textColorRgb = ITEM_MENU_TEXT_COLOR_RGB;
    contentX     = object->panel.contentLeft.signedValue + 2;
    contentY     = object->panel.contentTop.signedValue + 0xF;
    level        = abilityId & ATTACHMENT_MENU_LEVEL_MASK;
    itemMenuDrawItemRow(object, contentX, contentY, abilityId, textColorRgb, 0);
    if (level != 0) {
        itemMenuDrawParasiteEnergyLevel(object, contentX, contentY, level, textColorRgb);
    }

    labelText                   = (const u8*)Gp_StrAreaEffect;
    contentX                    = object->panel.contentLeft.signedValue + 2;
    contentTop                  = object->panel.contentTop.signedValue;
    contentY                    = contentTop + 0x21;
    areaLabelRequest.x          = object->panel.contentOriginX.unsignedValue + contentX;
    areaLabelRequest.y          = object->panel.contentOriginY.unsignedValue + contentTop + 0x1C;
    areaLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    areaLabelRequest.colorRgb   = textColorRgb;
    areaLabelRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    areaLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    areaLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&areaLabelRequest, labelText);

    if (cdCmdIsIdle() & 0xFFFF) {
        itemMenuDrawPreview(object, contentX, contentY, ITEM_MENU_PREVIEW_SMALL);
    } else {
        itemMenuDrawPreview(object, contentX, contentY, ITEM_MENU_PREVIEW_SMALL | ITEM_MENU_PREVIEW_HIDDEN);
    }

    parameterColorRgb           = ITEM_MENU_TEXT_COLOR_RGB;
    labelText                   = (const u8*)Gp_StrCastCost;
    contentX                    = object->panel.contentLeft.signedValue + 0x54;
    parameterTop                = object->panel.contentTop.signedValue;
    costLabelRequest.x          = object->panel.contentOriginX.unsignedValue + 1 + contentX;
    costLabelRequest.y          = object->panel.contentOriginY.unsignedValue + parameterTop + 0x24;
    contentY                    = parameterTop + 0x36;
    costLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    costLabelRequest.colorRgb   = parameterColorRgb;
    costLabelRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    costLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    costLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&costLabelRequest, labelText);
    _itemMenuDrawAbilityParameterBar(object, abilityId, nextLevel, contentX, contentY, ATTACHMENT_LEVEL_CAST_COST);

    atpLabelRequest.x          = object->panel.contentOriginX.unsignedValue + 1 + contentX;
    atpLabelRequest.y          = object->panel.contentOriginY.unsignedValue + parameterTop + 0x46;
    atpLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    atpLabelRequest.colorRgb   = parameterColorRgb;
    atpLabelRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    atpLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    atpLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&atpLabelRequest, (const u8*)Gp_StrAtpLoss);
    contentY = parameterTop + 0x58;
    _itemMenuDrawAbilityParameterBar(object, abilityId, nextLevel, contentX, contentY, ATTACHMENT_LEVEL_ATP_LOSS);
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
        uiFitPanelToList(menu, &(obj)->panel);
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
    uiUpdateList(menu, &obj->panel);
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
        obj                     = uiSpawnObject(&D_8010F840, arg0->spawnArg1, one, one, NULL);
        arg0->spawnArg2.pointer = obj;
        if (obj != NULL) {
            obj->panel.bounds.unsignedRect.x = (u16)D_80114E8C;
            obj->panel.bounds.unsignedRect.y = (u16)D_80114E90;
        }
        stageEnsureHeapTaskPrimitiveBuffer();
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
            stageReleaseTaskPrimitiveBuffer();
            stageRequestModeTaskExit();
        }
    }
}

/// Scales one model vertex and places a map corner about the scratch origin.
///
/// Borrows a complete `SVECTOR` and the live scratch block; `scaleQ12` uses 4096
/// for 1.0. Output halfwords receive X and inverted Z in map-screen pixels.
/// The scaled Z load precedes the origin-Y load; output pointers must name
/// separate corner coordinates outside the scratch block.
static inline void _menuMapPlaceAreaShapeVertex(const SVECTOR* vertex, _MenuMapAreaShapeScratch* scratch, s32 scaleQ12, s16* x, s16* y)
{
    s16 scaledZ;

    gte_lddp(scaleQ12);
    gte_ldsv(vertex);
    gte_gpf12();
    gte_stsv(&scratch->scaled);
    *x      = scratch->scaled.vx + scratch->originX;
    scaledZ = scratch->scaled.vz;
    *y      = scratch->originY - scaledZ;
}

/// Draws a flat area model over the map picture with the selected fill treatment.
///
/// Borrows a live `mapObject` and `areaModel`. Only the first stream group is read:
/// each record must have opcode 0x04 (textured triangle) or 0x44 (textured quad),
/// a positive 16-bit element count, and a word stride covering at least its three or
/// four packed u16 vertex references. `TMD_STREAM_GROUP_END` closes the group.
/// Each reference's low three bits are ignored; its remaining byte offset
/// must address a complete eight-byte `SVECTOR` in the source vertex pool.
/// The stream carries no checked length and this routine has no fallback for
/// other opcodes, empty records, missing terminators or out-of-range references.
///
/// `scaleQ12` is the GTE IR0 multiplier (4096 = 1.0). Model X/Z become map X/-Y;
/// scaled coordinates saturate to signed halfwords before packet coordinates
/// and texture coordinates truncate to 16 and 8 bits. The OT must admit the
/// panel index and the next entry. Reserves and releases its scratch block;
/// queued packets live in the current primitive buffer, with room for two
/// texture-window commands and every triangle/quad in the first group.
static void _menuMapDrawAreaShape(UiObject* mapObject, const TmdSource* areaModel, _MenuMapAreaShapeFill fillMode, s32 scaleQ12)
{
    enum {
        MENU_MAP_AREA_TRIANGLE_OPCODE    = 0x04,
        MENU_MAP_AREA_QUAD_OPCODE        = 0x44,
        MENU_MAP_AREA_VERTEX_OFFSET_MASK = 0xFFF8,
        MENU_MAP_AREA_PATTERN_MASK       = 0x1F,
        MENU_MAP_AREA_PATTERN_SIZE       = 0x20,
        MENU_MAP_AREA_TEXTURE_ORIGIN_X   = 0x80,
        MENU_MAP_AREA_TEXTURE_ORIGIN_Y   = 0x78,
        MENU_MAP_AREA_RAW_QUAD_CODE      = 0x2D, // Textured quad without colour modulation
        MENU_MAP_AREA_RAW_TRIANGLE_CODE  = 0x25  // Textured triangle without colour modulation
    };
    RECT                      textureWindow;
    DR_MODE*                  windowPacket;
    s32                       otIndex;
    const u8*                 vertexBytes;
    _MenuMapAreaShapeScratch* scratch;
    const u32*                command;
    s32                       opcode;
    u32                       dimensions;
    s32                       elementCount;
    s32                       elementStrideWords;
    s32                       minX;
    s32                       minY;

    otIndex         = mapObject->panel.otIndex.signedValue;
    vertexBytes     = (const u8*)areaModel->verts;
    command         = areaModel->stream;
    textureWindow.y = 0;
    textureWindow.x = 0;
    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_MenuMapAreaShapeScratch);
    windowPacket    = gGpuPrimCursor;
    gGpuPrimCursor  = windowPacket + 1;
    textureWindow.h = 0xFF;
    textureWindow.w = 0xFF;
    setTexWindow(windowPacket, &textureWindow);
    addPrim(&gGpuCurrentOt[otIndex], windowPacket);
    // Every shape is placed at the centre of the map picture.
    scratch->originX = 0;
    scratch->originY = 0;
    // Headers and payload strides are u32 words; vertex offsets are bytes.
    while (*command != TMD_STREAM_GROUP_END) {
        opcode             = *command;
        command           += 2; // Skip the opcode and its unused draw-handler slot.
        dimensions         = *command;
        elementCount       = dimensions >> 16;
        elementStrideWords = dimensions & 0xFFFF;
        command           += 1;
        if (opcode == MENU_MAP_AREA_QUAD_OPCODE) {
            if (elementCount > 0) {
                do {
                    POLY_FT4*      quad;
                    const SVECTOR* vertex;

                    vertex         = (const SVECTOR*)(vertexBytes + (((const u16*)command)[0] & MENU_MAP_AREA_VERTEX_OFFSET_MASK));
                    quad           = gGpuPrimCursor;
                    gGpuPrimCursor = quad + 1;
                    _menuMapPlaceAreaShapeVertex(vertex, scratch, scaleQ12, &quad->x0, &quad->y0);

                    vertex = (const SVECTOR*)(vertexBytes + (((const u16*)command)[1] & MENU_MAP_AREA_VERTEX_OFFSET_MASK));
                    _menuMapPlaceAreaShapeVertex(vertex, scratch, scaleQ12, &quad->x1, &quad->y1);

                    vertex = (const SVECTOR*)(vertexBytes + (((const u16*)command)[2] & MENU_MAP_AREA_VERTEX_OFFSET_MASK));
                    _menuMapPlaceAreaShapeVertex(vertex, scratch, scaleQ12, &quad->x2, &quad->y2);

                    vertex = (const SVECTOR*)(vertexBytes + (((const u16*)command)[3] & MENU_MAP_AREA_VERTEX_OFFSET_MASK));
                    _menuMapPlaceAreaShapeVertex(vertex, scratch, scaleQ12, &quad->x3, &quad->y3);
                    // Pattern UVs wrap in 32 pixels; tinted fills sample the map picture.
                    if (fillMode == MENU_MAP_AREA_FILL_PATTERN) {
                        minX = quad->x0;
                        if (quad->x1 < minX) {
                            minX = quad->x1;
                        }
                        if (quad->x2 < minX) {
                            minX = quad->x2;
                        }
                        if (quad->x3 < minX) {
                            minX = quad->x3;
                        }
                        minY = quad->y0;
                        if (quad->y1 < minY) {
                            minY = quad->y1;
                        }
                        if (quad->y2 < minY) {
                            minY = quad->y2;
                        }
                        if (quad->y3 < minY) {
                            minY = quad->y3;
                        }
                        quad->clut = getClut(0, 0xFF);
                        quad->u0   = (minX & MENU_MAP_AREA_PATTERN_MASK) + ((u8)quad->x0 - minX);
                        quad->v0   = (minY & MENU_MAP_AREA_PATTERN_MASK) + ((u8)quad->y0 - minY);
                        quad->u1   = (minX & MENU_MAP_AREA_PATTERN_MASK) + ((u8)quad->x1 - minX);
                        quad->v1   = (minY & MENU_MAP_AREA_PATTERN_MASK) + ((u8)quad->y1 - minY);
                        quad->u2   = (minX & MENU_MAP_AREA_PATTERN_MASK) + ((u8)quad->x2 - minX);
                        quad->v2   = (minY & MENU_MAP_AREA_PATTERN_MASK) + ((u8)quad->y2 - minY);
                        quad->u3   = (minX & MENU_MAP_AREA_PATTERN_MASK) + ((u8)quad->x3 - minX);
                        quad->v3   = (minY & MENU_MAP_AREA_PATTERN_MASK) + ((u8)quad->y3 - minY);
                    } else {
                        quad->u0 = quad->x0 - MENU_MAP_AREA_TEXTURE_ORIGIN_X;
                        quad->u1 = quad->x1 - MENU_MAP_AREA_TEXTURE_ORIGIN_X;
                        quad->u2 = quad->x2 - MENU_MAP_AREA_TEXTURE_ORIGIN_X;
                        quad->u3 = quad->x3 - MENU_MAP_AREA_TEXTURE_ORIGIN_X;
                        quad->v0 = quad->y0 - MENU_MAP_AREA_TEXTURE_ORIGIN_Y;
                        quad->v1 = quad->y1 - MENU_MAP_AREA_TEXTURE_ORIGIN_Y;
                        quad->v2 = quad->y2 - MENU_MAP_AREA_TEXTURE_ORIGIN_Y;
                        quad->v3 = quad->y3 - MENU_MAP_AREA_TEXTURE_ORIGIN_Y;
                        if (fillMode == MENU_MAP_AREA_FILL_DIM) {
                            GPU_PRIMITIVE_COLOR_WORD(quad, 0) = GPU_PACK_COLOR_WORD(0x20, 0x20, 0x20, 0);
                        } else if (fillMode == MENU_MAP_AREA_FILL_BLUE) {
                            GPU_PRIMITIVE_COLOR_WORD(quad, 0) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0xff, 0);
                        } else {
                            GPU_PRIMITIVE_COLOR_WORD(quad, 0) = GPU_PACK_COLOR_WORD(0xff, 0x40, 0x40, 0);
                        }
                        quad->clut = getClut(0, 0x100);
                    }
                    quad->tpage = getTPage(1, 1, 0x380, 0);
                    setPolyFT4(quad);
                    if (fillMode == MENU_MAP_AREA_FILL_PATTERN) {
                        setcode(quad, MENU_MAP_AREA_RAW_QUAD_CODE);
                        addPrim(&gGpuCurrentOt[otIndex], quad);
                    } else {
                        addPrim(&gGpuCurrentOt[otIndex] + 1, quad);
                    }
                    command += elementStrideWords;
                    elementCount--;
                } while (elementCount > 0);
            }
        } else if (opcode == MENU_MAP_AREA_TRIANGLE_OPCODE) {
            if (elementCount > 0) {
                do {
                    POLY_FT3*      triangle;
                    const SVECTOR* vertex;

                    vertex         = (const SVECTOR*)(vertexBytes + (((const u16*)command)[0] & MENU_MAP_AREA_VERTEX_OFFSET_MASK));
                    triangle       = gGpuPrimCursor;
                    gGpuPrimCursor = triangle + 1;
                    gte_lddp(scaleQ12);
                    gte_ldsv(vertex);
                    gte_gpf12();
                    gte_stsv(&scratch->scaled);

                    // Retain the stream drawer's second scale of triangle vertex zero.
                    vertex = (const SVECTOR*)(vertexBytes + (((const u16*)command)[0] & MENU_MAP_AREA_VERTEX_OFFSET_MASK));
                    _menuMapPlaceAreaShapeVertex(vertex, scratch, scaleQ12, &triangle->x0, &triangle->y0);

                    vertex = (const SVECTOR*)(vertexBytes + (((const u16*)command)[1] & MENU_MAP_AREA_VERTEX_OFFSET_MASK));
                    _menuMapPlaceAreaShapeVertex(vertex, scratch, scaleQ12, &triangle->x1, &triangle->y1);

                    vertex = (const SVECTOR*)(vertexBytes + (((const u16*)command)[2] & MENU_MAP_AREA_VERTEX_OFFSET_MASK));
                    _menuMapPlaceAreaShapeVertex(vertex, scratch, scaleQ12, &triangle->x2, &triangle->y2);
                    if (fillMode == MENU_MAP_AREA_FILL_PATTERN) {
                        minX = triangle->x0;
                        if (triangle->x1 < minX) {
                            minX = triangle->x1;
                        }
                        if (triangle->x2 < minX) {
                            minX = triangle->x2;
                        }
                        minY = triangle->y0;
                        if (triangle->y1 < minY) {
                            minY = triangle->y1;
                        }
                        if (triangle->y2 < minY) {
                            minY = triangle->y2;
                        }
                        triangle->clut = getClut(0, 0xFF);
                        triangle->u0   = (minX & MENU_MAP_AREA_PATTERN_MASK) + ((u8)triangle->x0 - minX);
                        triangle->v0   = (minY & MENU_MAP_AREA_PATTERN_MASK) + ((u8)triangle->y0 - minY);
                        triangle->u1   = (minX & MENU_MAP_AREA_PATTERN_MASK) + ((u8)triangle->x1 - minX);
                        triangle->v1   = (minY & MENU_MAP_AREA_PATTERN_MASK) + ((u8)triangle->y1 - minY);
                        triangle->u2   = (minX & MENU_MAP_AREA_PATTERN_MASK) + ((u8)triangle->x2 - minX);
                        triangle->v2   = (minY & MENU_MAP_AREA_PATTERN_MASK) + ((u8)triangle->y2 - minY);
                    } else {
                        triangle->u0 = triangle->x0 - MENU_MAP_AREA_TEXTURE_ORIGIN_X;
                        triangle->u1 = triangle->x1 - MENU_MAP_AREA_TEXTURE_ORIGIN_X;
                        triangle->u2 = triangle->x2 - MENU_MAP_AREA_TEXTURE_ORIGIN_X;
                        triangle->v0 = triangle->y0 - MENU_MAP_AREA_TEXTURE_ORIGIN_Y;
                        triangle->v1 = triangle->y1 - MENU_MAP_AREA_TEXTURE_ORIGIN_Y;
                        triangle->v2 = triangle->y2 - MENU_MAP_AREA_TEXTURE_ORIGIN_Y;
                        if (fillMode == MENU_MAP_AREA_FILL_DIM) {
                            GPU_PRIMITIVE_COLOR_WORD(triangle, 0) = GPU_PACK_COLOR_WORD(0x20, 0x20, 0x20, 0);
                        } else if (fillMode == MENU_MAP_AREA_FILL_BLUE) {
                            GPU_PRIMITIVE_COLOR_WORD(triangle, 0) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0xff, 0);
                        } else {
                            GPU_PRIMITIVE_COLOR_WORD(triangle, 0) = GPU_PACK_COLOR_WORD(0xff, 0x40, 0x40, 0);
                        }
                        triangle->clut = getClut(0, 0x100);
                    }
                    triangle->tpage = getTPage(1, 1, 0x380, 0);
                    setPolyFT3(triangle);
                    if (fillMode == MENU_MAP_AREA_FILL_PATTERN) {
                        setcode(triangle, MENU_MAP_AREA_RAW_TRIANGLE_CODE);
                        addPrim(&gGpuCurrentOt[otIndex], triangle);
                    } else {
                        addPrim(&gGpuCurrentOt[otIndex] + 1, triangle);
                    }
                    command += elementStrideWords;
                    elementCount--;
                } while (elementCount > 0);
            }
        }
    }
    // OT links prepend: this window precedes pattern primitives, then the no-mask window follows.
    windowPacket    = gGpuPrimCursor;
    gGpuPrimCursor  = windowPacket + 1;
    textureWindow.x = 0;
    textureWindow.y = 0;
    textureWindow.w = MENU_MAP_AREA_PATTERN_SIZE;
    textureWindow.h = MENU_MAP_AREA_PATTERN_SIZE;
    setTexWindow(windowPacket, &textureWindow);
    addPrim(&gGpuCurrentOt[otIndex], windowPacket);
    SCRATCH_STACK_RELEASE_BLOCK(_MenuMapAreaShapeScratch);
}

s32 shopOpenSession(s32 stockSelector)
{
    s32 locationWord;

    // Use the saved room key: the display transition may replace live room state.
    // Preserve the single volatile prefix read before clearing the holder.
    locationWord                        = *(const volatile s32*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
    *(UiObject* volatile*)&Wip_UiHolder = NULL;
    switch (locationWord & (s32)GAME_LOCATION_STAGE_AREA_MASK) {
        case GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_PARKING, 0, 0):
            displayQueueModeTask(&D_mist_parking_8018668C, stockSelector, 0, STAGE_ENTRY_RELOAD);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 0, 0):
            displayQueueModeTask(&D_dryfield_trailer_coach_80183F84, stockSelector, 0, STAGE_ENTRY_RELOAD);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 0, 0):
            displayQueueModeTask(&D_dryfield_night_trailer_coach_801846D0, stockSelector, 0, STAGE_ENTRY_RELOAD);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GARAGE, 0, 0):
            displayQueueModeTask(&D_dryfield_night_garage_80181C2C, stockSelector, 0, STAGE_ENTRY_RELOAD);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, 0, 0):
            displayQueueModeTask(&D_shelter_b1_armory_801824D0, stockSelector, 0, STAGE_ENTRY_RELOAD);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0, 0):
            displayQueueModeTask(&D_shelter_b1_underground_parking_801871F0.desc, stockSelector, 0, STAGE_ENTRY_RELOAD);
            break;
        case GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT, 0, 0):
            displayQueueModeTask(&D_shelter_1f_heliport_80181188, stockSelector, 0, STAGE_ENTRY_RELOAD);
            break;
        default:
            return 0;
    }
    return 1;
}

UiObject* itemMenuSpawnNotice(UiObject* parent, s32 noticeId, s32 unused, s32 returnConfirmation)
{
    returnConfirmation <<= 16;
    return uiSpawnObject(&D_8010F788, returnConfirmation | noticeId, USER_INTERFACE_PANEL_ACTIVE, 1, parent);
}

s32 itemMenuOpenHotspotCommands(s32 screenX, s32 screenY, s32 actionKind)
{
    D_80114E94 = actionKind;
    D_80114E8C = screenX;
    D_80114E90 = screenY;
    displayQueueModeTask(&D_8010F85C, actionKind, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

s32 itemMenuIsHotspotActionConfirmed(void)
{
    return D_80114E88;
}

void itemMenuDrawUseAttachCommandRow(UiList* list, UiObject* object)
{
    TextDrawReq request;
    s32         panelControl;
    s32         activeMode;

    request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    request.otIndex    = object->panel.otIndex.signedValue + 1;
    request.colorRgb   = list->colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&request, (const u8*)Gp_StrUse2);

    panelControl = object->panel.control.word;
    activeMode   = USER_INTERFACE_PANEL_ACTIVE;
    if (((panelControl >> 16) == activeMode) || (panelControl == activeMode)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            uiSetPromptText((const u8*)Gp_StrUseAttachHelp, 0, 0);
        }
    }

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            object->resultValue = ITEM_MENU_COMMAND_USE_ATTACH;
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void itemMenuDrawKeyItemCommandRow(UiList* list, UiObject* object)
{
    TextDrawReq request;
    s32         panelControl;
    s32         activeMode;

    request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    request.otIndex    = object->panel.otIndex.signedValue + 1;
    request.colorRgb   = list->colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&request, (const u8*)Gp_StrKeyItem2);

    panelControl = object->panel.control.word;
    activeMode   = USER_INTERFACE_PANEL_ACTIVE;
    if (((panelControl >> 16) == activeMode) || (panelControl == activeMode)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            uiSetPromptText((const u8*)Gp_StrUseKeyHelp, 0, 0);
        }
    }

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            object->resultValue = ITEM_MENU_COMMAND_KEY_ITEMS;
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

s32 attachmentGetPackedLevelValue(s32 abilityId, s32 column)
{
    s32 elementIndex;
    s32 energyIndex;
    s32 level;
    s32 value;

    elementIndex = (abilityId & ATTACHMENT_MENU_ELEMENT_MASK) >> ATTACHMENT_MENU_ELEMENT_SHIFT;
    energyIndex  = (abilityId & ATTACHMENT_MENU_ENERGY_MASK) >> ATTACHMENT_MENU_ENERGY_SHIFT;
    level        = abilityId & ATTACHMENT_MENU_LEVEL_MASK;
    value        = Gp_IdParamHi.rows[(elementIndex * 3 + energyIndex) * 3 + level].value[column];
    if (column == ATTACHMENT_LEVEL_EXP_COST) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
            value = (value * 4) / 5;
        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
            value = (value * 2) / 5;
        }
    }
    return value & 0xFFFF;
}

void itemMenuDrawPeCancelRow(UiList* list, UiObject* object)
{
    TextDrawReq request;

    request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    request.otIndex    = object->panel.otIndex.signedValue + 1;
    request.colorRgb   = list->colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&request, Gp_StrCancel2);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void itemMenuDrawMapCommandRow(UiList* list, UiObject* object)
{
    enum { ITEM_MENU_COMMAND_MAP = 0x100 };
    TextDrawReq request;
    s32         panelControl;

    ITEM_MENU_PREPARE_COMMAND_TEXT(request, list, object, list->colorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_DRAW_FILL_ONLY);
    textDrawString(&request, (const u8*)Gp_StrMap);

    panelControl = object->panel.control.word;

    if (((panelControl >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            uiSetPromptText((const u8*)Gp_StrCheckMap, 0, 0);
        }
    }

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            object->resultValue = ITEM_MENU_COMMAND_MAP;
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void itemMenuDrawDiscardRow(UiList* list, UiObject* object)
{
    TextDrawReq request;

    request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    request.otIndex    = object->panel.otIndex.signedValue + 1;
    request.colorRgb   = list->colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&request, Gp_StrDiscard2);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010F6FC, 0, USER_INTERFACE_PANEL_ACTIVE, 1, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
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

void itemMenuPeNextLevelTask(Task* task)
{
    UiObject* object;
    s32       abilityId;
    s32       savedPanelControl;
    s32       descriptionBottomY;
    const u8* text;

    object                     = task->spawnArg2.pointer;
    abilityId                  = task->spawnArg1.value;
    savedPanelControl          = object->panel.control.word;
    object->result             = USER_INTERFACE_RESULT_NONE;
    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    uiDrawPanelLabel(&(object)->panel, Gp_StrNextLevel);
    object->panel.control.word = savedPanelControl;
    _itemMenuDrawPeSpecifications(object, abilityId, ITEM_MENU_PE_DISPLAY_NEXT_LEVEL);
    descriptionBottomY = object->panel.contentBottom.signedValue;
    text               = itemGetText(abilityId + 1, ITEM_TEXT_DESCRIPTION_FIRST, 1);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, descriptionBottomY - 0xF, text, ITEM_MENU_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    text = itemGetText(abilityId + 1, ITEM_TEXT_DESCRIPTION_SECOND, 1);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, descriptionBottomY, text, ITEM_MENU_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

void itemMenuHealingTask(Task* task)
{
    UiObject* object;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    itemMenuApplyHealingPanel(object, task, task->spawnArg1.value);
}

void itemMenuPeSpecificationsTask(Task* task)
{
    UiObject* object;
    s32       abilityId;
    const u8* text;

    object         = task->spawnArg2.pointer;
    abilityId      = task->spawnArg1.value;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(object)->panel, Gp_StrSpecs2);
    if ((abilityId & ATTACHMENT_MENU_LEVEL_MASK) == 0) {
        abilityId += 1;
    }
    _itemMenuDrawPeSpecifications(object, abilityId, ITEM_MENU_PE_DISPLAY_CURRENT_LEVEL);
    if (cdCmdIsIdle() & 0xFFFF) {
        text = textSkipLines(fsGetChunkPayload(), ITEM_MENU_PE_PREVIEW_DESCRIPTION_LINE);
        textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, 0x14, text, ITEM_MENU_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel | PAD_BUTTON_TRIANGLE) != 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

void itemMenuDrawHotspotActionRow(UiList* list, UiObject* object)
{
    enum { ACTION_PROMPT_COMMAND_PUSH = 1 };
    TextDrawReq request;
    const u8*   label;
    s32         rowInputEnabled;

    label = (const u8*)Gp_StrExamine;
    if (object->owner->spawnArg1.value == ACTION_PROMPT_COMMAND_PUSH) {
        label = (const u8*)Gp_StrPush;
    }
    ITEM_MENU_PREPARE_COMMAND_TEXT(request, list, object, list->colorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_DRAW_OUTLINED);
    textDrawString(&request, label);
    rowInputEnabled = list->rowInputEnabled;
    if (rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            D_80114E88     = rowInputEnabled;
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

#undef ITEM_MENU_PREPARE_COMMAND_TEXT

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
            uiSpawnObject(&D_8010EAB4[46], 0, 1, 1, arg1);
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
        flags |= ITEM_MENU_PREVIEW_TALL;
    }
    if ((cdCmdIsIdle() & 0xFFFF) == 0) {
        flags |= ITEM_MENU_PREVIEW_HIDDEN;
    }
    itemMenuDrawPreview(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);
}
