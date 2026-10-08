#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/memory.h>

#include "common.h"

#include "attachments.h"
#include "gameplay/attachment_state.h"
#include "gameplay/enemy.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/map.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

/// One variant of a weapon that takes add-ons: the weapon item it is carried
/// as, and the add-on that item has mounted.
///
/// Using an add-on swaps the carried weapon item for the variant that has it
/// mounted; the add-on the old variant held goes back to the inventory.
typedef struct {
    u8 weaponItemId;  // Weapon item id of this variant (0x80..0x9F).
    u8 mountedItemId; // Add-on item mounted on it (0 none).
} _ItemMenuWeaponVariant;
STATIC_ASSERT_SIZEOF(_ItemMenuWeaponVariant, 2);

/// The eight M4A1 weapon variants, in the order an add-on use scans them.
///
/// The first carried row is the weapon the player has. The first other row
/// whose mounted add-on is the item just used is the weapon it becomes.
/// M4A1(+1) is listed before M4A1(+2); both mount the Rifle Clip Holder, so
/// that add-on steps the base rifle to (+1) and (+1) to (+2).
typedef struct {
    _ItemMenuWeaponVariant variants[8]; // One row per M4A1 variant, in scan order.
} _ItemMenuM4a1VariantTable;
STATIC_ASSERT_SIZEOF(_ItemMenuM4a1VariantTable, 0x10);

/// Task work for the panel that shows a weapon created by using an add-on.
///
/// The add-on is consumed and one carried weapon item is replaced by the
/// weapon that add-on produces. The panel lists the previous weapon and the
/// add-on above "used.", then the new weapon and any add-ons returned to the
/// inventory above "created."
typedef struct {
    s32 usedAddonItemId;      // Add-on item that was used.
    s32 previousWeaponItemId; // Weapon item carried before the swap (0x80..0x9F).
    s32 createdWeaponItemId;  // Weapon item carried after the swap (0x80..0x9F).
    s32 returnedAddonItemId;  // Add-on taken off the old variant (0 none).
    s32 returnedClipItemId;   // Extra Rifle Clip Holder returned while M4A1(+2) is carried (0 none).
} _ItemMenuWeaponCreateWork;
STATIC_ASSERT_SIZEOF(_ItemMenuWeaponCreateWork, 0x14);

/// Item-panel colors, ordinary P.E. item ids and update-count delays.
enum {
    ITEM_MENU_PANEL_TEXT_COLOR           = 0x606060,
    ITEM_MENU_RECESSED_FILL_COLOR        = 0x102010,
    ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST = 15,
    ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT = ATTACHMENT_SPELL_COUNT * ATTACHMENT_AREA_LEVEL_COUNT,
    ITEM_MENU_DIALOG_OPEN_DELAY_TICKS    = 2,
    ITEM_MENU_COMPLETED_COUNTDOWN        = 0x7FFF,
};

/// Item-menu result-panel accents, display duration and elemental slot grouping.
enum {
    ITEM_MENU_RESULT_ITEM_COLOR            = 0x037A78,
    ITEM_MENU_RESULT_NOTICE_UPDATES        = 188,
    ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT = 3,
    ITEM_MENU_INVOKE_MAX_LEVEL             = 3
};

/// Draws one visible unmarked item row into a reusable text request.
///
/// Statement block; the caller checks visibility. Inputs must be free of side
/// effects because object, coordinates, item and color are evaluated repeatedly.
/// request is a TextDrawReq lvalue; baseY and levelIndex are s32 scratch lvalues
/// overwritten by the operation. Uses `itemMenuDrawItemRow`'s resource contract.
#define ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, request, rowX, rowY, itemId, rowColorRgb, baseY, levelIndex)                     \
    {                                                                                                                                 \
        (request).x          = (object)->panel.contentOriginX.unsignedValue + 0x11 + (rowX);                                          \
        (baseY)              = (object)->panel.contentOriginY.unsignedValue - 6;                                                      \
        (request).y          = (baseY) + (rowY);                                                                                      \
        (request).otIndex    = (object)->panel.otIndex.signedValue + 1;                                                               \
        (request).colorRgb   = (rowColorRgb);                                                                                         \
        (request).glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                                                               \
        (request).alignment  = TEXT_ALIGNMENT_LEFT;                                                                                   \
        (request).drawMode   = TEXT_DRAW_OUTLINED;                                                                                    \
        textDrawString(&(request), itemGetText((itemId), ITEM_TEXT_NAME, 0));                                                         \
        (levelIndex) = (itemId) - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;                                                               \
        if ((u32)(levelIndex) < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {                                                          \
            itemMenuDrawParasiteEnergyLevel((object), (rowX), (rowY), (levelIndex) % ATTACHMENT_AREA_LEVEL_COUNT + 1, (rowColorRgb)); \
        }                                                                                                                             \
        itemMenuDrawItemIcon((object), (rowX), (rowY), (itemId), ITEM_MENU_ICON_DEFAULT);                                             \
    }

u8 Gp_MapRoomId;

u8 Gp_MapRoomOff;

UiList D_80114DF8[4];

s32 D_80114E88;

s32 D_80114E8C;

s32 D_80114E90;

s32 D_80114E94;

extern UiObjectDesc D_8010F02C[3];

extern UiObjectDesc D_8010F080;

extern UiObjectDesc D_8010F09C;

static u8 D_8010F194[];

extern u8 Gp_StrWrongAmmo[];

extern u8 Gp_StrCannotDiscard[];

extern u8 Gp_StrCannotDiscardEq[];

extern u8 Gp_StrCannotDiscardAmmo[];

extern u8 Gp_StrCannotMoveHere[];

extern u8 Gp_StrDestFull[];

extern u8 Gp_StrCannotMoveEq[];

extern u8 Gp_StrCannotSwitchAmmo[];

extern u8 Gp_StrCannotSwitchEq[];

extern u8 Gp_StrCannotSwitchEq2[];

extern u8 Gp_StrCannotMoveAmmo[];

extern u8 Gp_StrNeedExp[];

extern u8 Gp_StrMaxLevel[];

extern u8 Gp_StrNeedMp[];

extern u8 Gp_StrSaveCancel[];

extern u8 Gp_StrReallyDiscard[];

extern u8 Gp_StrSaveDone[];

extern u8 Gp_StrNoUseNow2[];

extern u8 Gp_StrDontKnowUse[];

extern u8 Gp_StrNoOtherWpn[];

extern u8 Gp_StrNoOtherArmor[];

extern u8 Gp_StrNeedMp5[];

extern u8 Gp_StrNeedM4[];

extern u8 Gp_StrNeedAs12[];

extern u8 Gp_StrNeedP08[];

extern u8 Gp_StrNoMoreMods[];

extern u8 Gp_StrNeedEmptySlot[];

extern u8 Gp_StrMaxHpUp[];

extern u8 Gp_StrMaxMpUp[];

extern u8 Gp_StrCannotMoveItem[];

extern u8 Gp_StrCannotSwitchItem[];

extern u8 Gp_StrCannotSwitchWith[];

extern UiListRowCallback D_8010F5C8[2];

extern UiListRowCallback Gp_PeCmdFns[2];

extern u8 Gp_StrFire[];

extern u8 Gp_StrWind[];

extern u8 Gp_StrWater[];

extern u8 Gp_StrEarth[];

extern UiObjectDesc D_8010F654[1];

extern const char Gp_StrStatus[];

extern const char Gp_StrInvoke[];

extern const char Gp_StrPeList[];

extern const u8 D_800971A4;

extern const char Gp_StrTotal2[];

extern const char Gp_StrMessage[];

extern const char Gp_StrWarning[];

static void _itemPickupPreviewTask(Task* task);

void Gp_BuildItemCmdList(UiList* arg0, UiObject* arg1, s32 arg2, InventoryItemRow* arg3);

static void _itemPickupTitleTask(Task* task);

static void _itemPickupAskTask(Task* task);

static void _itemPickupInventoryFullTask(Task* task);

static void _itemPickupObtainedNoticeTask(Task* task);

static inline void _itemMenuSetPreviewItem(s32 itemId, u8 loadProfile);

void Gp_BuildItemCmdList(UiList* arg0, UiObject* arg1, s32 arg2, InventoryItemRow* arg3)
{
    s32 n;
    s32 mode;

    n                                 = 0;
    mode                              = arg1->owner->spawnArg1.value;
    arg1->panel.bounds.unsignedRect.w = 0x60;
    switch (mode) {
        case 0:
            if (arg2 == 0) {
                Gp_ItemCmdFns[n++] = itemMenuDrawMoveRow;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                Gp_ItemCmdFns[n++] = itemMenuDrawMoveRow;
                Gp_ItemCmdFns[n++] = itemMenuDrawDiscardRow;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
                Gp_ItemCmdFns[n++] = itemMenuDrawMoveRow;
                Gp_ItemCmdFns[n++] = itemMenuDrawDiscardRow;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                if ((arg3->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg2)) > 0) {
                    Gp_ItemCmdFns[n++] = itemMenuDrawLoadRow;
                }
                Gp_ItemCmdFns[n++] = itemMenuDrawMoveRow;
                Gp_ItemCmdFns[n++] = itemMenuDrawDiscardRow;
            } else {
                Gp_ItemCmdFns[n++] = itemMenuDrawUseRow;
                Gp_ItemCmdFns[n++] = itemMenuDrawMoveRow;
                Gp_ItemCmdFns[n++] = itemMenuDrawDiscardRow;
            }
            break;
        case 1:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x80) < 0x20U) {
                    Gp_ItemCmdFns[n++] = itemMenuDrawExchangeRow;
                }
            }
            break;
        case 2:
            if (arg2 == 0) {
                Gp_ItemCmdFns[n++] = itemMenuDrawExchangeRow;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                Gp_ItemCmdFns[n++] = itemMenuDrawExchangeRow;
            }
            break;
        case 3:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x60) < 0x20U) {
                    Gp_ItemCmdFns[n++] = itemMenuDrawExchangeRow;
                }
            }
            break;
        case 4:
            if (arg2 == 0) {
                Gp_ItemCmdFns[n++] = itemMenuDrawAttachmentExchangeRow;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                Gp_ItemCmdFns[n++] = itemMenuDrawAttachmentExchangeRow;
                if ((arg2 != 0x92) && (arg2 != 0x95)) {
                    Gp_ItemCmdFns[n++] = itemMenuDrawLoadRow;
                }
                Gp_ItemCmdFns[n++] = itemMenuDrawDiscardRow;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                Gp_ItemCmdFns[n++] = itemMenuDrawAttachmentExchangeRow;
                if ((arg3->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg2)) > 0) {
                    Gp_ItemCmdFns[n++] = itemMenuDrawLoadRow;
                }
                Gp_ItemCmdFns[n++] = itemMenuDrawDiscardRow;
            } else {
                Gp_ItemCmdFns[n++] = itemMenuDrawAttachmentExchangeRow;
                Gp_ItemCmdFns[n++] = itemMenuDrawUseRow;
                Gp_ItemCmdFns[n++] = itemMenuDrawDiscardRow;
            }
            break;
    }
    arg0->itemCount                     = n;
    arg0->visibleRowCount.unsignedValue = n;
}

UiObjectDesc D_8010F02C[3] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 144, 136 }, 56, 0, TASK_BODY_NONE, 192, _itemPickupTitleTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, 32, 288, 48 }, 44, 0, TASK_BODY_NONE, 192, _itemPickupAskTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, 32, 288, 48 }, 44, 0, TASK_BODY_NONE, 192, _itemPickupInventoryFullTask, 0 },
};

UiObjectDesc D_8010F080 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -80, -40, 160, 80 }, 8, 0, TASK_BODY_NONE, 192, _itemPickupObtainedNoticeTask, 0 };

UiObjectDesc D_8010F09C = { 0, { -144, 0, 90, 70 }, 52, 0, TASK_BODY_NONE, 192, _itemPickupPreviewTask, 0 };

MenuMapAreaName* Gp_MapNameTables[5] = {
    D_map_akropolis_8017A3BC,
    D_map_dryfield_8017A044,
    D_map_dryfield_full_80179F4C,
    D_map_shelter_8017A268,
    D_map_neo_ark_8017A28C,
};

MenuMapIcon* D_8010F0CC[5] = {
    D_map_akropolis_8017A38C,
    D_map_dryfield_80179FEC,
    D_map_dryfield_full_80179F04,
    D_map_shelter_8017A1F0,
    D_map_neo_ark_8017A26C,
};

MenuMapMarker* D_8010F0E0[5] = {
    D_map_akropolis_8017A330,
    D_map_dryfield_80179F38,
    D_map_dryfield_full_80179E48,
    D_map_shelter_8017A134,
    D_map_neo_ark_8017A230,
};

MenuMapArea* Gp_MapRecTables[5] = {
    D_map_akropolis_8017A154,
    D_map_dryfield_80179BD0,
    D_map_dryfield_full_80179AE0,
    D_map_shelter_80179CD8,
    D_map_neo_ark_80179F24,
};

u8* Gp_MapFlagIds[5] = {
    D_map_akropolis_8017A14C,
    D_map_dryfield_80179BCC,
    D_map_dryfield_full_80179ADC,
    D_map_shelter_80179CD0,
    D_map_neo_ark_80179F1C,
};

MenuMapAreaShape* Gp_MapMarkTables[5] = {
    D_map_akropolis_8017A288,
    D_map_dryfield_80179E00,
    D_map_dryfield_full_80179D10,
    D_map_shelter_80179FA4,
    D_map_neo_ark_8017A110,
};

u8 D_8010F130[5] = { 2, 3, 3, 6, 3 };

u8 Gp_MapMarkCounts[5] = {
    21,
    39,
    39,
    50,
    34,
};

u8 D_8010F13D = 1;

UiObjectDesc D_8010F140 = { (s32)USER_INTERFACE_PANEL_NO_FRAME, { -145, -107, 290, 215 }, 56, 0, TASK_BODY_NONE, 192, menuMapTask, 0 };

UiObjectDesc D_8010F15C = { USER_INTERFACE_PANEL_TITLE_STYLE, { -140, 50, 280, 50 }, 20, 0, TASK_BODY_NONE, 192, menuMapHelpTask, 0 };

UiObjectDesc D_8010F178 = { 3, { -140, -107, 170, 15 }, 24, 0, TASK_BODY_NONE, 192, menuMapAreaNameTask, 0 };

static u8 D_8010F194[] = { 142, 204, 130, 196, 130, 233, 0, 0 };

char Gp_StrDiscard2[]          = "Discard";
char Gp_StrItem2[]             = "Item";
char Gp_StrExamine[]           = "Examine";
char Gp_StrPush[]              = "Push";
char Gp_StrRevive[]            = "Revive";
char Gp_StrStrengthen[]        = "Strengthen";
char Gp_StrCancel2[0xC]        = "Cancel\000\000Use";
u8   Gp_StrWrongAmmo[]         = "You do not habe the correct ammo.";
u8   Gp_StrCannotDiscard[]     = "You cannot discard this item.";
u8   Gp_StrCannotDiscardEq[]   = "You cannot discard equipped items.";
u8   Gp_StrCannotDiscardAmmo[] = "You cannot discard loaded ammo.";
u8   Gp_StrNoWeaponEq[]        = "No applicable weapon equipped.";
u8   Gp_StrCannotMoveHere[]    = "Cannot move this item here.";
u8   Gp_StrDestFull[]          = "Destination full.";
u8   Gp_StrCannotMoveEq[]      = "Cannot move equipped items.";
u8   Gp_StrCannotSwitchAmmo[]  = "Cannot switch with ammunition.";
u8   Gp_StrCannotSwitchEq[]    = "Cannot switch equiped items.";
u8   Gp_StrCannotSwitchEq2[]   = "Cannot switch with equipped items.";
u8   Gp_StrCannotMoveAmmo[]    = "Cannot move loaded ammunition.";
u8   Gp_StrNeedExp[]           = "Insufficient EXP.";
u8   Gp_StrMaxLevel[]          = "Max level.";
u8   Gp_StrNeedMp[]            = "Insufficient MP.";
u8   Gp_StrSaveCancel[]        = "Save cancelled.";
u8   Gp_StrReallyDiscard[]     = "Really discard?";
u8   Gp_StrSaveDone[]          = "Save complete.";
u8   Gp_StrNoUseNow2[]         = "No use for this now.";
u8   Gp_StrDontKnowUse[]       = "I don't no how to use this.";
u8   Gp_StrNoOtherWpn[]        = "I have no other weapons.";
u8   Gp_StrNoOtherArmor[]      = "I have no other armor.";
u8   Gp_StrNeedMp5[]           = "MP5A5 required.";
u8   Gp_StrNeedM4[]            = "M4A1 required.";
u8   Gp_StrNeedAs12[]          = "AS12 required.";
u8   Gp_StrNeedP08[]           = "P08 required.";
u8   Gp_StrNoMoreMods[]        = "No further modifications.";
u8   Gp_StrNeedEmptySlot[]     = "Empty slot required.";
u8   Gp_StrMaxHpUp[]           = "Maximum HP increased.";
u8   Gp_StrMaxMpUp[]           = "Maximum MP increased.";
u8   Gp_StrCannotMoveItem[]    = "Cannot move this item.";
u8   Gp_StrCannotSwitchItem[]  = "Cannot switch this item.";
u8   Gp_StrCannotSwitchWith[]  = "Cannot switch with this item.";
char Gp_StrAreaEffect[]        = "Area of Effect";
char Gp_StrCastCost[]          = "Casting Cost";
char Gp_StrAtpLoss[]           = "ATP Loss";
u8*  Gp_NoticeTexts[]          = {
    Gp_StrWrongAmmo,
    Gp_StrCannotDiscard,
    Gp_StrCannotDiscardEq,
    Gp_StrCannotDiscardAmmo,
    Gp_StrNoWeaponEq,
    Gp_StrCannotMoveHere,
    Gp_StrDestFull,
    Gp_StrCannotMoveEq,
    Gp_StrCannotSwitchAmmo,
    Gp_StrCannotSwitchEq,
    Gp_StrCannotSwitchEq2,
    Gp_StrCannotMoveAmmo,
    Gp_StrNeedExp,
    Gp_StrMaxLevel,
    Gp_StrNeedMp,
    Gp_StrSaveCancel,
};

u8* Gp_PromptTexts[] = {
    Gp_StrReallyDiscard,
    Gp_StrSaveDone,
    Gp_StrNoUseNow2,
    Gp_StrDontKnowUse,
    Gp_StrNoOtherWpn,
    Gp_StrNoOtherArmor,
    Gp_StrNeedMp5,
    Gp_StrNeedM4,
    Gp_StrNeedAs12,
    Gp_StrNeedP08,
    Gp_StrNoMoreMods,
    Gp_StrNeedEmptySlot,
    Gp_StrMaxHpUp,
    Gp_StrMaxMpUp,
    Gp_StrCannotMoveItem,
    Gp_StrCannotSwitchItem,
    Gp_StrCannotSwitchWith,
};

UiListRowCallback D_8010F5C8[2] = { itemMenuDrawUseAttachCommandRow, itemMenuDrawKeyItemCommandRow };

UiList D_8010F5D0 = { D_8010F5C8, 2, { 2 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback Gp_PeCmdFns[2] = { itemMenuDrawPeUpgradeRow, itemMenuDrawPeCancelRow };

UiList D_8010F5FC = { Gp_PeCmdFns, 2, { 2 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010F620[1] = {
    itemMenuDrawPeAbilityRow,
};

u8 Gp_StrFire[] = "Fire";

u8 Gp_StrWind[] = "Wind";

u8 Gp_StrWater[] = "Water";

u8 Gp_StrEarth[] = "Earth";

u8* D_8010F644[4] = {
    Gp_StrFire,
    Gp_StrWind,
    Gp_StrWater,
    Gp_StrEarth,
};

UiObjectDesc D_8010F654[1] = { { 3, { -136, -50, 60, 80 }, 24, 0, TASK_BODY_NONE, 192, itemMenuItemCommandTask, 0 } };

UiObjectDesc D_8010F670 = { 3, { -136, -50, 70, 80 }, 20, 0, TASK_BODY_NONE, 192, itemMenuPeCommandTask, 0 };

// "EXP"
// "MP"

const char Gp_StrStatus[];
const char Gp_StrInvoke[];
const char Gp_StrPeList[];
const u8   D_800971A4;
const char Gp_StrTotal2[];
const char Gp_StrMessage[];
const char Gp_StrWarning[];
const char Gp_StrHelp[];
const char Gp_StrUse2[];
const char Gp_StrKeyItem2[];
const char Gp_StrMap[];
const char Gp_StrAttention2[];
const char Gp_StrNotice3[];
const char Gp_StrNextLevel[];
const char D_8009720C[];
const char Gp_StrCost[];
const char Gp_StrBonus[];
const char D_80097220[];
const char Gp_StrSpecs2[];

const _ItemMenuM4a1VariantTable D_80097184;
const TaskFuncTable4            Gp_MapTaskStates;

void Gp_ItemCmdMenuTask(Task* arg0)
{
    Task*             childTask;
    UiObject*         obj;
    UiList*           menu;
    UiObject*         child;
    s32               flag;
    s32               y;
    InventoryItemRow* ptr;
    s32               val;
    s32               sel;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    menu        = &D_8010EA30;
    if (arg0->state == 0) {
        ptr = Gp_SelItemRec;
        val = 0;
        if (ptr != NULL) {
            val = ptr->itemId;
        }
        Gp_BuildItemCmdList(menu, obj, val, ptr);
        uiFitPanelToList(menu, &(obj)->panel);
        y = 0x46 - ((s16)obj->panel.bounds.unsignedRect.y + (s16)obj->panel.bounds.unsignedRect.h);
        if (y < 0) {
            obj->panel.bounds.unsignedRect.y += y;
        }
        arg0->state = arg0->state + 1;
    } else {
        uiUpdateList(menu, &obj->panel);
        if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
            sel = menu->actionResult;
            if (sel != USER_INTERFACE_LIST_ACTION_INPUT_CONSUMED) {
                if (sel == USER_INTERFACE_LIST_ACTION_MOVE) {
                    obj->result = sel;
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                    obj->result = USER_INTERFACE_RESULT_CANCEL;
                } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                    obj->result = USER_INTERFACE_RESULT_CONFIRM;
                }
            }
        }
        childTask = arg0->firstChild;
        if (childTask != NULL) {
            child = childTask->spawnArg2.pointer;
            flag  = child->result;
            switch (flag) {
                case USER_INTERFACE_RESULT_CANCEL:
                    obj->result = flag;
                    break;
                case USER_INTERFACE_RESULT_CONFIRM:
                    uiStartTreeClosing(child, child->owner);
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    break;
                case USER_INTERFACE_RESULT_DISMISS:
                    obj->result = USER_INTERFACE_RESULT_CONFIRM;
                    break;
            }
        }
    }
}

/// Caps the healing panel's stored HP and MP at their respective maxima.
///
/// Borrows the live writable status record after the effect has been applied.
/// Comparisons use the stored signed 16-bit values; negative values are retained.
static inline void _itemMenuClampHealedStats(PlayerStatus* player)
{
    if (player->hp > player->hpMax) {
        player->hp = player->hpMax;
    }
    if (player->mp > player->mpMax) {
        player->mp = player->mpMax;
    }
}

void itemMenuApplyHealingPanel(UiObject* object, Task* task, s32 healingId)
{
    enum {
        ITEM_MENU_HEALING_STATE_INIT       = 0,
        ITEM_MENU_HEALING_DISPLAY_UPDATES  = 188,
        ITEM_MENU_HEALING_RECOVERY_2       = 2,
        ITEM_MENU_HEALING_RECOVERY_3       = 3,
        ITEM_MENU_HEALING_COLA             = 5,
        ITEM_MENU_HEALING_MP_BOOST_1       = 6,
        ITEM_MENU_HEALING_MP_BOOST_2       = 7,
        ITEM_MENU_HEALING_MP_BOOST_COUNT   = 2,
        ITEM_MENU_HEALING_RINGERS_SOLUTION = 61,
        ITEM_MENU_HEALING_USE_COUNT_MAX    = 9999
    };
    PlayerStatus* player;
    HudHpMp*      displayedStats;
    McSaveData*   save;
    s32           previousHp;
    s32           previousMp;
    s32           panelWidth;
    s32           panelHeight;

    player         = &gPlayerStatus;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, Gp_StrStatus);
    // Apply the effect once, retaining the old values for the rising stat display.
    if (task->state == ITEM_MENU_HEALING_STATE_INIT) {
        uiSetPanelContentSize(&object->panel, 0xB0, 0x2F);
        panelWidth                          = (s16)object->panel.bounds.unsignedRect.w;
        panelHeight                         = (s16)object->panel.bounds.unsignedRect.h;
        object->panel.bounds.unsignedRect.x = -(panelWidth >> 1);
        object->panel.bounds.unsignedRect.y = -(panelHeight >> 1) - 0x10;
        previousHp                          = player->hp;
        displayedStats                      = &Gp_HpMpWork;
        displayedStats->hp                  = previousHp;
        previousMp                          = player->mp;
        displayedStats->mp                  = previousMp;
        if (healingId < ITEM_TEXT_KEY_ID_FIRST) {
            if (healingId < ITEM_MENU_HEALING_RECOVERY_3 + 1) {
                if (previousHp < player->hpMax) {
                    inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
                }
                if (healingId == ITEM_MENU_HEALING_RECOVERY_3) {
                    player->hp = player->hpMax;
                } else if (healingId == ITEM_MENU_HEALING_RECOVERY_2) {
                    player->hp = player->hp + 0x64;
                } else {
                    player->hp = player->hp + 0x32;
                }
            } else if (healingId == ITEM_MENU_HEALING_COLA) {
                if ((previousMp < player->mpMax) || (previousHp < player->hpMax)) {
                    inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
                }
                player->mp = player->mp + 0x50;
                player->hp = player->hp + 0x14;
            } else if ((u32)(healingId - ITEM_MENU_HEALING_MP_BOOST_1) < (u32)ITEM_MENU_HEALING_MP_BOOST_COUNT) {
                if (previousMp < player->mpMax) {
                    inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
                }
                if (healingId == ITEM_MENU_HEALING_MP_BOOST_2) {
                    player->mp = player->mpMax;
                } else {
                    player->mp = player->mp + 0x1E;
                }
            } else if (healingId == ITEM_MENU_HEALING_RINGERS_SOLUTION) {
                if ((previousMp < player->mpMax) || (previousHp < player->hpMax)) {
                    inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
                }
                player->mp = player->mpMax;
                player->hp = player->hpMax;
            }
        } else if (previousHp < player->hpMax) {
            player->mp         = player->mp - attachmentGetPackedLevelValue(healingId, ATTACHMENT_LEVEL_CAST_COST);
            displayedStats->mp = player->mp;
            player->hp         = player->hp + attachmentGetPackedLevelValue(healingId, ATTACHMENT_LEVEL_AMOUNT);
            save               = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if (save->state.attachUseCounts[ATTACHMENT_INDEX_HEALING] < ITEM_MENU_HEALING_USE_COUNT_MAX) {
                save->state.attachUseCounts[ATTACHMENT_INDEX_HEALING] = save->state.attachUseCounts[ATTACHMENT_INDEX_HEALING] + 1;
            }
        }
        _itemMenuClampHealedStats(player);
        task->killCountdown = ITEM_MENU_HEALING_DISPLAY_UPDATES;
        task->state         = task->state + 1;
    }
    // Start the dismissal countdown only after both displayed values catch up.
    itemMenuDrawPlayerStats(&object->panel, 0);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (Gp_HpMpWork.hp == player->hp) {
            if (Gp_HpMpWork.mp == player->mp) {
                task->killCountdown = task->killCountdown - 1;
            }
        }
        if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) || (task->killCountdown < 0)) {
            Gp_HpMpWork.hp      = player->hp;
            Gp_HpMpWork.mp      = player->mp;
            object->result      = USER_INTERFACE_RESULT_DISMISS;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
        }
    }
}

void itemMenuApplyWeaponAddonPanel(UiObject* object, Task* task)
{
    enum {
        ITEM_MENU_WEAPON_ADDON_STATE_INIT            = 0,
        ITEM_MENU_WEAPON_ADDON_SMG_CLIP_HOLDER       = 9,
        ITEM_MENU_WEAPON_ADDON_ACCEPTED              = 0xFF,
        ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS = 0x1A,
        ITEM_MENU_WEAPON_ADDON_NEEDS_MP5A5           = 0x16,
        ITEM_MENU_WEAPON_ADDON_NEEDS_P08             = 0x19,
        ITEM_MENU_WEAPON_ADDON_NEEDS_EMPTY_SLOT      = 0x1B,
        ITEM_MENU_WEAPON_ADDON_NEEDS_M4A1            = 0x17,
        ITEM_MENU_WEAPON_ADDON_MP5A5_TWO_HOLDERS     = 0x9F,
        ITEM_MENU_WEAPON_ADDON_MP5A5_ONE_HOLDER      = 0x9E,
        ITEM_MENU_WEAPON_ADDON_MP5A5                 = 0x9D,
        ITEM_MENU_WEAPON_ADDON_P08_SNAIL             = 0x80,
        ITEM_MENU_WEAPON_ADDON_P08                   = 0x83,
        ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER     = 0xA,
        ITEM_MENU_WEAPON_ADDON_SNAIL_MAGAZINE        = 0xC,
        ITEM_MENU_WEAPON_ADDON_HAMMER                = 0x42,
        ITEM_MENU_WEAPON_ADDON_PYKE                  = 0x43,
        ITEM_MENU_WEAPON_ADDON_JAVELIN               = 0x44,
        ITEM_MENU_WEAPON_ADDON_M203                  = 0x45,
        ITEM_MENU_WEAPON_ADDON_M9                    = 0x46,
        ITEM_MENU_WEAPON_ADDON_M4A1_ONE_HOLDER       = 0x93,
        ITEM_MENU_WEAPON_ADDON_M4A1_TWO_HOLDERS      = 0x94,
    };
    union {
        TextDrawReq               textRequest;
        _ItemMenuM4a1VariantTable variantTable;
    } scratch;
    s32                                 createdWeaponItemId;
    s32                                 returnedClipItemId;
    register s32                        previousWeaponItemId;
    register s32                        returnedAddonItemId;
    register s32                        itemId;
    register s32                        x;
    register s32                        itemColorRgb;
    register _ItemMenuWeaponCreateWork* work;
    s32                                 y;
    s32                                 replacementVariantIndex;
    s32                                 carriedVariantIndex;
    s32                                 energyLevelIndex;
    s32                                 rowCount;
    s32                                 savedAddonItemId;
    s32                                 clipHolderItemId;
    s32                                 hiddenState;
    s32                                 textBaseY;
    u16                                 remainingUpdates;
    InventoryItemRange*                 carriedItems;
    InventoryItemRange*                 carriedItemsForSwap;
    _ItemMenuWeaponCreateWork*          newWork;
    _ItemMenuWeaponVariant*             variants;
    EquipmentWeaponLoad*                previousWeaponLoad;
    EquipmentWeaponLoad*                createdWeaponLoad;
    InventoryItemRow*                   weaponRow;
    PlayerStatus*                       player;

    if (task->state == ITEM_MENU_WEAPON_ADDON_STATE_INIT) {
        previousWeaponItemId = 0;
        createdWeaponItemId  = 0;
        returnedClipItemId   = 0;
        itemId               = task->spawnArg1.value;
        task->status         = ITEM_MENU_WEAPON_ADDON_ACCEPTED;
        returnedAddonItemId  = previousWeaponItemId;
        switch (itemId) {
            case ITEM_MENU_WEAPON_ADDON_SMG_CLIP_HOLDER:
                carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_MP5A5_TWO_HOLDERS) != 0) {
                    task->status = ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS;
                } else if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_MP5A5_ONE_HOLDER) != 0) {
                    previousWeaponItemId = ITEM_MENU_WEAPON_ADDON_MP5A5_ONE_HOLDER;
                    createdWeaponItemId  = ITEM_MENU_WEAPON_ADDON_MP5A5_TWO_HOLDERS;
                } else if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_MP5A5) != 0) {
                    previousWeaponItemId = ITEM_MENU_WEAPON_ADDON_MP5A5;
                    createdWeaponItemId  = ITEM_MENU_WEAPON_ADDON_MP5A5_ONE_HOLDER;
                } else {
                    task->status = ITEM_MENU_WEAPON_ADDON_NEEDS_MP5A5;
                }
                break;
            case ITEM_MENU_WEAPON_ADDON_SNAIL_MAGAZINE:
                carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_P08_SNAIL) != 0) {
                    task->status = ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS;
                } else if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_P08) != 0) {
                    previousWeaponItemId = ITEM_MENU_WEAPON_ADDON_P08;
                    createdWeaponItemId  = ITEM_MENU_WEAPON_ADDON_P08_SNAIL;
                } else {
                    task->status = ITEM_MENU_WEAPON_ADDON_NEEDS_P08;
                }
                break;
            case ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER:
            case ITEM_MENU_WEAPON_ADDON_HAMMER:
            case ITEM_MENU_WEAPON_ADDON_PYKE:
            case ITEM_MENU_WEAPON_ADDON_JAVELIN:
            case ITEM_MENU_WEAPON_ADDON_M203:
            case ITEM_MENU_WEAPON_ADDON_M9:
                carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (inventoryGetItemQuantity(carriedItems, ITEM_MENU_WEAPON_ADDON_M4A1_TWO_HOLDERS) != 0) {
                    if (itemId == ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER) {
                        task->status = ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS;
                    } else if (inventoryCanAddItem(carriedItems, ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER) != 0) {
                        returnedClipItemId = ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER;
                    } else {
                        task->status = ITEM_MENU_WEAPON_ADDON_NEEDS_EMPTY_SLOT;
                    }
                }
                if (task->status == ITEM_MENU_WEAPON_ADDON_ACCEPTED) {
                    clipHolderItemId = ITEM_MENU_WEAPON_ADDON_RIFLE_CLIP_HOLDER;
                    // Copied into the slot the row text reuses once this carriedItems is done.
                    variants             = scratch.variantTable.variants;
                    scratch.variantTable = D_80097184;
                    task->status         = ITEM_MENU_WEAPON_ADDON_NEEDS_M4A1;
                    // Find the variant being carriedVariantIndex; the add-on it has mounted comes back to the inventory.
                    for (carriedVariantIndex = 0; carriedVariantIndex < ARRAY_SIZE(scratch.variantTable.variants); carriedVariantIndex++) {
                        if (inventoryGetItemQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, variants[carriedVariantIndex].weaponItemId) != 0) {
                            returnedAddonItemId = variants[carriedVariantIndex].mountedItemId;
                            if (itemId == clipHolderItemId && variants[carriedVariantIndex].weaponItemId == ITEM_MENU_WEAPON_ADDON_M4A1_ONE_HOLDER) {
                                returnedAddonItemId = 0;
                            }
                            if (returnedAddonItemId != clipHolderItemId && returnedAddonItemId == itemId) {
                                task->status = ITEM_MENU_WEAPON_ADDON_NO_MORE_MODIFICATIONS;
                            } else {
                                // The weapon becomes the other variant that has the used itemId mounted.
                                previousWeaponItemId = variants[carriedVariantIndex].weaponItemId;
                                for (replacementVariantIndex = 0; replacementVariantIndex < ARRAY_SIZE(scratch.variantTable.variants); replacementVariantIndex++) {
                                    if (itemId == variants[replacementVariantIndex].mountedItemId && previousWeaponItemId != variants[replacementVariantIndex].weaponItemId) {
                                        task->status        = ITEM_MENU_WEAPON_ADDON_ACCEPTED;
                                        createdWeaponItemId = variants[replacementVariantIndex].weaponItemId;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                break;
        }
        if (task->status == ITEM_MENU_WEAPON_ADDON_ACCEPTED) {
            // Replace the carried variant while transferring ammunition and returning displaced add-ons.
            player              = &gPlayerStatus;
            previousWeaponLoad  = equipmentGetWeaponLoad(previousWeaponItemId);
            createdWeaponLoad   = equipmentGetWeaponLoad(createdWeaponItemId);
            weaponRow           = inventoryFindLastCarriedItemRow(previousWeaponItemId);
            newWork             = memCalloc(sizeof(_ItemMenuWeaponCreateWork), 0);
            carriedItemsForSwap = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            task->work          = newWork;
            inventoryRemoveItemRow(carriedItemsForSwap, Gp_SelItemRec, 1);
            weaponRow->itemId = createdWeaponItemId;
            equipmentClearSelectedRemovableLoads(createdWeaponItemId, EQUIPMENT_CLEAR_LOAD_BOTH);
            createdWeaponLoad->primaryItemId = previousWeaponLoad->primaryItemId;
            equipmentLoadWeaponConsumable(carriedItemsForSwap, createdWeaponItemId, createdWeaponLoad->primaryItemId, previousWeaponLoad->primaryQty);
            if ((returnedAddonItemId == 0) && (createdWeaponLoad->secondaryItemId == previousWeaponLoad->secondaryItemId)) {
                createdWeaponLoad->secondaryQty = previousWeaponLoad->secondaryQty;
            }
            equipmentClearSelectedRemovableLoads(previousWeaponItemId, EQUIPMENT_CLEAR_LOAD_BOTH);
            if (player->weapon == (previousWeaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1))) {
                player->weapon = createdWeaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
            }
            if (returnedAddonItemId != 0) {
                inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, returnedAddonItemId, INVENTORY_GIVE_ONE_PACK);
            }
            if (returnedClipItemId != 0) {
                inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, returnedClipItemId, INVENTORY_GIVE_ONE_PACK);
            }
            newWork->createdWeaponItemId  = createdWeaponItemId;
            newWork->previousWeaponItemId = previousWeaponItemId;
            newWork->returnedAddonItemId  = returnedAddonItemId;
            rowCount                      = 5;
            newWork->returnedClipItemId   = returnedClipItemId;
            newWork->usedAddonItemId      = itemId;
            if (returnedAddonItemId != 0) {
                rowCount = 6;
            }
            if (returnedClipItemId != 0) {
                rowCount += 1;
            }
            uiSetPanelContentSize(&(object)->panel, 0xA8, uiGetTextRowsHeight(rowCount) + 1);
            (&(object)->panel)->bounds.rect.x = (-(&(object)->panel)->bounds.rect.w) >> 1;
            (&(object)->panel)->bounds.rect.y = ((-(&(object)->panel)->bounds.rect.h) >> 1) - 0x14;
            task->killCountdown               = ITEM_MENU_RESULT_NOTICE_UPDATES;
            task->state                       = task->state + 1;
        }
    }
    if (task->status != ITEM_MENU_WEAPON_ADDON_ACCEPTED) {
        savedAddonItemId      = task->spawnArg1.value;
        task->spawnArg1.value = task->status;
        itemMenuNoticeTask(task);
        task->spawnArg1.value = savedAddonItemId;
        return;
    }
    itemColorRgb = ITEM_MENU_RESULT_ITEM_COLOR;
    y            = object->panel.contentTop.signedValue + 0xF;
    work         = task->work;
    x            = object->panel.contentLeft.signedValue + 2;
    uiDrawPanelLabel(&(object)->panel, Gp_StrNotice);
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    itemId      = work->previousWeaponItemId;
    if (object->panel.state != hiddenState) {
        ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
    }
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    itemId      = work->usedAddonItemId;
    y          += 0xF;
    if (object->panel.state != hiddenState) {
        ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
    }
    y += 0xF;
    textDrawUiLine(object, x, y, (const u8*)Gp_StrUsedDot, ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    itemId      = work->createdWeaponItemId;
    y          += 0xF;
    if (object->panel.state != hiddenState) {
        ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
    }
    itemId = work->returnedAddonItemId;
    // The clip row remains conditional on a returned add-on, even if a clip was returned independently.
    if (itemId != 0) {
        y += 0xF;
        if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
            ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
        }
        itemId = work->returnedClipItemId;
        if (itemId != 0) {
            y += 0xF;
            if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, scratch.textRequest, x, y, itemId, itemColorRgb, textBaseY, energyLevelIndex);
            }
        }
    }
    textDrawUiLine(object, x, y + 0xF, (const u8*)Gp_StrCreatedDot, ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        remainingUpdates    = task->killCountdown - 1;
        task->killCountdown = remainingUpdates;
        if ((((s32)(remainingUpdates << 0x10)) <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            object->result      = USER_INTERFACE_RESULT_DISMISS;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
            return;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result      = USER_INTERFACE_RESULT_CANCEL;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
        }
    }
}

/// Fits and centers the two-line notice for invoking a Parasite Energy item.
///
/// Requires initialized panel bounds, style and content coordinates, retaining
/// its frame insets. Borrows read-only itemName under `textMeasureLineWidth`'s
/// contract. Width covers "Invoked" or the item
/// name with room for its trailing period, plus five content-margin pixels.
/// Height covers two fifteen-pixel rows plus one margin pixel. The outer bounds
/// are centered twenty pixels above screen center; halving negative signed
/// extents rounds toward negative infinity before the halfword stores.
static inline void _itemMenuSizeInvocationPanel(UiPanel* panel, const u8* itemName)
{
    enum {
        ITEM_MENU_INVOCATION_SUFFIX_ALLOWANCE_PIXELS = 11,
        ITEM_MENU_INVOCATION_TEXT_ROWS               = 2,
        ITEM_MENU_INVOCATION_CENTER_Y_PIXELS         = -20
    };
    s32 contentWidth;
    s32 invokedTextWidth;

    contentWidth     = textMeasureLineWidth(itemName) + ITEM_MENU_INVOCATION_SUFFIX_ALLOWANCE_PIXELS;
    invokedTextWidth = textMeasureLineWidth((const u8*)Gp_StrInvoked);
    if (contentWidth < invokedTextWidth) {
        contentWidth = invokedTextWidth;
    }
    uiSetPanelContentSize(panel, contentWidth + 5, uiGetTextRowsHeight(ITEM_MENU_INVOCATION_TEXT_ROWS) + 1);
    panel->bounds.rect.x = (-panel->bounds.rect.w) >> 1;
    panel->bounds.rect.y = ((-panel->bounds.rect.h) >> 1) + ITEM_MENU_INVOCATION_CENTER_Y_PIXELS;
}

void itemMenuInvokeParasiteEnergyItem(UiObject* object, Task* task, s32 itemId)
{
    enum {
        ITEM_MENU_INVOKE_STATE_INIT = 0
    };
    const u8*     itemName;
    s32           nameEndX;
    PlayerStatus* player;
    McSaveData*   save;
    s32           level;
    s32           abilityIndex;
    s32           elementIndex;
    s32           elementAbilityIndex;

    itemName = itemGetText(itemId, ITEM_TEXT_NAME, 0);
    if (task->state == ITEM_MENU_INVOKE_STATE_INIT) {
        _itemMenuSizeInvocationPanel(&object->panel, itemName);
        inventoryRemoveItemRow(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, Gp_SelItemRec, 1);

        // Decode the ordinary item into its element, ability slot and one-based level.
        abilityIndex = (itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST) / ATTACHMENT_AREA_LEVEL_COUNT;
        level        = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        elementIndex = elementAbilityIndex = abilityIndex / ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT;
        elementAbilityIndex                = abilityIndex - elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT;
        save                               = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        level                              = level - abilityIndex * ATTACHMENT_AREA_LEVEL_COUNT + 1;
        player                             = &gPlayerStatus;
        if (save->state.attachLevels[elementAbilityIndex + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT] < level) {
            save->state.attachLevels[elementAbilityIndex + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT] = level;
        }
        equipmentRecalculateMaxMp();
        player->mp          = player->mpMax;
        Gp_HpMpWork.mp      = player->mp;
        task->killCountdown = ITEM_MENU_RESULT_NOTICE_UPDATES;
        task->state         = task->state + 1;
    }

    uiDrawPanelLabel(&(object)->panel, Gp_StrInvoke);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrInvoked, ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    nameEndX = textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0x1E, itemName, ITEM_MENU_RESULT_ITEM_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(object, nameEndX, object->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, ITEM_MENU_PANEL_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    // Invocation notices advance their timeout even while the panel is inactive.
    task->killCountdown--;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if ((task->killCountdown <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            object->result      = USER_INTERFACE_RESULT_DISMISS;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
        }
    }
}

void itemMenuInvokeElementBoostPanel(UiObject* object, Task* task)
{
    enum {
        ITEM_MENU_ELEMENT_BOOST_STATE_INIT = 0,
        ITEM_MENU_ELEMENT_BOOST_ITEM_FIRST = 0x36
    };
    McSaveData* save;
    McSaveData* levelSave;
    u8*         saveBytesForElement;
    s32         elementIndex;
    s32         abilitySelection;
    s32         abilityIndex;

    elementIndex = task->spawnArg1.value - ITEM_MENU_ELEMENT_BOOST_ITEM_FIRST;
    if (task->state == ITEM_MENU_ELEMENT_BOOST_STATE_INIT) {
        // Keep the grouped reads inside the save's byte representation, with named member offsets.
        save                = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        saveBytesForElement = (u8*)save + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT;
        abilitySelection    = saveBytesForElement[OFFSET_OF(McSaveData, state.attachLevels)] > saveBytesForElement[OFFSET_OF(McSaveData, state.attachLevels) + 1];
        if (save->state.attachLevels[abilitySelection + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT] >= ITEM_MENU_INVOKE_MAX_LEVEL) {
            abilitySelection = 2;
            if (save->state.attachLevels[elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT + 2] >= ITEM_MENU_INVOKE_MAX_LEVEL) {
                abilitySelection = (elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT + 2) * ATTACHMENT_AREA_LEVEL_COUNT + ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST + ITEM_MENU_INVOKE_MAX_LEVEL - 1;
                goto storeInvocation;
            }
        }
        // Reuse the selection value for the ordinary P.E. item id after choosing its group slot.
        levelSave        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        abilityIndex     = abilitySelection + elementIndex * ITEM_MENU_INVOKE_ABILITIES_PER_ELEMENT;
        abilitySelection = levelSave->state.attachLevels[abilityIndex] + abilityIndex * ATTACHMENT_AREA_LEVEL_COUNT + ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
    storeInvocation:
        task->extraState.value = abilitySelection;
    }
    itemMenuInvokeParasiteEnergyItem(object, task, task->extraState.value);
}

void itemMenuDialogTask(Task* task)
{
    enum { ITEM_MENU_DIALOG_STATE_INIT = 0 };
    UiObject* object;
    UiList*   dialogList;
    s32       commandMode;
    s32       actionResult;

    object         = task->spawnArg2.pointer;
    dialogList     = &D_8010EA74;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_MENU_DIALOG_STATE_INIT) {
        commandMode = task->spawnArg1.value & ITEM_MENU_DIALOG_LAYOUT_MASK;
        switch (commandMode) {
            case ITEM_MENU_DIALOG_OK:
                Gp_DialogCmdFns[0]    = itemMenuDrawOkRow;
                dialogList->itemCount = commandMode;
                break;
            case ITEM_MENU_DIALOG_CANCEL:
                Gp_DialogCmdFns[0]    = itemMenuDrawCancelRow;
                dialogList->itemCount = 1;
                break;
            case ITEM_MENU_DIALOG_NO_SELECTED:
                Gp_DialogCmdFns[0]    = itemMenuDrawYesRow;
                Gp_DialogCmdFns[1]    = itemMenuDrawNoRow;
                dialogList->itemCount = ARRAY_SIZE(Gp_DialogCmdFns);
                break;
            default:
                Gp_DialogCmdFns[0]    = itemMenuDrawYesRow;
                Gp_DialogCmdFns[1]    = itemMenuDrawNoRow;
                dialogList->itemCount = ARRAY_SIZE(Gp_DialogCmdFns);
                break;
        }
        dialogList->visibleRowCount.unsignedValue = dialogList->itemCount;
        uiFitPanelToList(dialogList, &object->panel);
        object->panel.bounds.unsignedRect.y -= (s16)object->panel.bounds.unsignedRect.h / 2;
        if (task->spawnArg1.value & ITEM_MENU_DIALOG_SYSTEM_CURSOR_SOUND) {
            uiSetListSystemCursorSound(dialogList, 1);
        } else {
            uiSetListSystemCursorSound(dialogList, 0);
        }
        if ((task->spawnArg1.value & ITEM_MENU_DIALOG_LAYOUT_MASK) == ITEM_MENU_DIALOG_NO_SELECTED) {
            dialogList->selectedItemIndex = 1;
        } else {
            dialogList->selectedItemIndex = 0;
        }
        task->state = task->state + 1;
    }
    uiUpdateList(dialogList, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        actionResult = dialogList->actionResult;
        if (actionResult == USER_INTERFACE_RESULT_CONFIRM) {
            object->result      = actionResult;
            object->resultValue = dialogList->commandResult.unsignedValue;
        }
    }
}

/// Draws the available EXP and current/maximum MP above the elemental P.E. lists.
///
/// Borrows panel coordinates and live player stats for this draw. Small labels
/// anchor the medium decimal values in content pixels, translated with the
/// unsigned origin views before signed-halfword narrowing. EXP is formatted as
/// unsigned; MP and maximum MP retain their signed values. Each formatter needs
/// at most ten bytes, fitting the reusable 32-byte buffer. Drawing requires
/// loaded font textures/palettes and primitive capacity under `textDrawString`;
/// the header uses panel OT base + 1 and base + 2, without testing visibility.
static inline void _itemMenuDrawParasiteEnergyHeader(const UiPanel* panel)
{
    u8                  numberText[32];
    TextDrawReq         expLabelRequest;
    TextDrawReq         expValueRequest;
    TextDrawReq         mpLabelRequest;
    TextDrawReq         mpValueRequest;
    TextDrawReq         slashRequest;
    TextDrawReq         maxMpRequest;
    s32                 contentLeft;
    const PlayerStatus* player;
    s32                 textColorRgb;
    s32                 labelX;
    s32                 valueY;

    /// Sets depth, color and glyph style without changing the request's position.
    ///
    /// request is a simple writable TextDrawReq lvalue, evaluated five times;
    /// other arguments are each evaluated once and must be free of side effects.
    /// panel supplies the signed OT base; selectors narrow to request bytes.
#define ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(request, panel, textColor, glyphFace, textAlignment, textDrawMode) \
    {                                                                                                                      \
        (request).otIndex    = (panel)->otIndex.signedValue + 1;                                                           \
        (request).colorRgb   = (textColor);                                                                                \
        (request).glyphTable = (glyphFace);                                                                                \
        (request).alignment  = (textAlignment);                                                                            \
        (request).drawMode   = (textDrawMode);                                                                             \
    }

    textColorRgb      = ITEM_MENU_PANEL_TEXT_COLOR;
    player            = &gPlayerStatus;
    contentLeft       = panel->contentLeft.signedValue;
    labelX            = contentLeft + 34;
    valueY            = panel->contentTop.signedValue + 8;
    expLabelRequest.x = panel->contentOriginX.unsignedValue + labelX;
    expLabelRequest.y = panel->contentOriginY.unsignedValue + (valueY - 2);
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(expLabelRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
    textDrawString(&expLabelRequest, (const u8*)Gp_StrExp);
    expValueRequest.x = panel->contentOriginX.unsignedValue + 10 + labelX;
    expValueRequest.y = panel->contentOriginY.unsignedValue + valueY;
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(expValueRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&expValueRequest, textItoaUnsigned(numberText, player->exp));
    labelX           = contentLeft + 122;
    mpLabelRequest.x = panel->contentOriginX.unsignedValue + labelX;
    mpLabelRequest.y = panel->contentOriginY.unsignedValue + (valueY - 2);
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(mpLabelRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
    textDrawString(&mpLabelRequest, (const u8*)Gp_StrMp);
    mpValueRequest.x = panel->contentOriginX.unsignedValue + 10 + labelX;
    mpValueRequest.y = panel->contentOriginY.unsignedValue + valueY;
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(mpValueRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&mpValueRequest, textItoaSigned(numberText, player->mp));
    slashRequest.x = panel->contentOriginX.unsignedValue + 37 + labelX;
    slashRequest.y = panel->contentOriginY.unsignedValue + valueY;
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(slashRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_CENTER, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&slashRequest, (const u8*)Gp_StrSlash);
    maxMpRequest.x = panel->contentOriginX.unsignedValue + 42 + labelX;
    maxMpRequest.y = panel->contentOriginY.unsignedValue + valueY;
    ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE(maxMpRequest, panel, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&maxMpRequest, textItoaSigned(numberText, player->mpMax));
#undef ITEM_MENU_SET_PARASITE_ENERGY_HEADER_TEXT_STYLE
}

void itemMenuParasiteEnergyListTask(Task* task)
{
    enum {
        ITEM_MENU_PE_LIST_STATE_INIT    = 0,
        ITEM_MENU_PE_LIST_CONFIRMED     = 1,
        ITEM_MENU_PE_LIST_ELEMENT_FIRE  = 0,
        ITEM_MENU_PE_LIST_ELEMENT_WIND  = 1,
        ITEM_MENU_PE_LIST_ELEMENT_WATER = 2,
        ITEM_MENU_PE_LIST_ELEMENT_EARTH = 3,
        ITEM_MENU_PE_LIST_OPEN_DELAY    = 1
    };
    UiObject*     object;
    UiObjectDesc* elementPanelDescs;
    Task*         firstChild;
    Task*         childTask;
    UiObject*     childObject;
    s32           childResult;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(object)->panel, Gp_StrPeList);
    if (task->state == ITEM_MENU_PE_LIST_STATE_INIT) {
        elementPanelDescs = D_8010F718;
        uiSpawnObject(elementPanelDescs, ITEM_MENU_PE_LIST_ELEMENT_FIRE, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_PE_LIST_OPEN_DELAY, object);
        uiSpawnObject(elementPanelDescs + 1, ITEM_MENU_PE_LIST_ELEMENT_WIND, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_PE_LIST_OPEN_DELAY, object);
        uiSpawnObject(elementPanelDescs + 2, ITEM_MENU_PE_LIST_ELEMENT_WATER, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_PE_LIST_OPEN_DELAY, object);
        uiSpawnObject(elementPanelDescs + 3, ITEM_MENU_PE_LIST_ELEMENT_EARTH, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_PE_LIST_OPEN_DELAY, object);
        task->state = task->state + 1;
    }
    _itemMenuDrawParasiteEnergyHeader(&object->panel);
    firstChild = task->firstChild;
    if (firstChild != NULL) {
        childTask = firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            childResult = childObject->result;
            childTask   = childTask->nextSibling;
            switch (childResult) {
                case USER_INTERFACE_RESULT_CONFIRM:
                    object->resultValue = ITEM_MENU_PE_LIST_CONFIRMED;
                    /* fallthrough */
                case USER_INTERFACE_RESULT_CANCEL:
                    object->result = childResult;
                    break;
            }
        } while (childTask != task->firstChild);
    }
}

void itemMenuInventoryCountTask(Task* task)
{
    enum {
        ITEM_MENU_INVENTORY_COUNT_SHOW       = 0,
        ITEM_MENU_INVENTORY_COUNT_HIDE       = 1,
        ITEM_MENU_INVENTORY_COUNT_OPEN_DELAY = 16
    };
    u8                  countText[0x20];
    u8                  capacityText[0x20];
    TextDrawReq         countRequest;
    TextDrawReq         labelRequest;
    UiObject*           object;
    InventoryItemRange* carriedItems;
    s32                 occupiedRows;
    s32                 rowCapacity;
    s32                 textColorRgb;
    s32                 rowY;
    s32                 countBaseX;
    s32                 countBaseY;
    s32                 labelBaseY;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if ((Gp_ItemCountShow == ITEM_MENU_INVENTORY_COUNT_HIDE) && (uiIsPanelHidingOrHidden(object) == 0)) {
        uiStartPanelHiding(object, object->owner);
    } else if ((Gp_ItemCountShow == ITEM_MENU_INVENTORY_COUNT_SHOW) && (uiIsPanelHidingOrHidden(object) == 1)) {
        uiLimitHiddenDelayOrOpen(&(object)->panel, object->owner, ITEM_MENU_INVENTORY_COUNT_OPEN_DELAY);
    }
    rowY         = object->panel.contentTop.signedValue + 0xD;
    countText[0] = D_800971A4;
    memset(&countText[1], 0, sizeof(countText) - sizeof(countText[0]));
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    occupiedRows = inventoryCountOccupiedRows(carriedItems);
    rowCapacity  = inventoryGetRangeCapacity(carriedItems);
    textItoaUnsigned(countText, occupiedRows);
    textAppendString(countText, (const u8*)Gp_StrSlash);
    textItoaUnsigned(capacityText, rowCapacity);
    textAppendString(countText, capacityText);
    countBaseX              = object->panel.contentOriginX.unsignedValue - 2;
    countRequest.x          = object->panel.contentRight.unsignedValue + countBaseX;
    countBaseY              = object->panel.contentOriginY.unsignedValue - 3;
    countRequest.y          = countBaseY + rowY;
    countRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    countRequest.colorRgb   = textColorRgb;
    countRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    countRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    countRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&countRequest, countText);
    labelRequest.x          = object->panel.contentLeft.unsignedValue + (object->panel.contentOriginX.unsignedValue + 2);
    labelBaseY              = object->panel.contentOriginY.unsignedValue - 6;
    labelRequest.y          = labelBaseY + rowY;
    labelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    labelRequest.colorRgb   = textColorRgb;
    labelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    labelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    labelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&labelRequest, (const u8*)Gp_StrTotal2);
}

void itemPickupPanelTask(Task* task)
{
    enum {
        ITEM_PICKUP_PANEL_STATE_INIT   = 0,
        ITEM_PICKUP_PANEL_TIMEOUT_ONLY = 0x10000,
        ITEM_PICKUP_PANEL_OPEN_DELAY   = 1
    };
    UiObject*     object;
    UiObject*     noticeObject;
    UiObject*     childObject;
    UiObjectDesc* pickupPanelDescs;
    Task*         firstChild;
    Task*         childTask;
    Task*         nextChild;
    s32           childResult;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_PICKUP_PANEL_STATE_INIT) {
        pickupPanelDescs = D_8010F02C;
        uiSpawnObject(pickupPanelDescs, 0, 0, 1, object);
        if (task->spawnArg1.value != 0) {
            inventorySetCollectedBit(Gp_PubItemLoc);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            // The separate obtained recipe immediately follows the three-entry table.
            noticeObject = uiSpawnObject(pickupPanelDescs + ARRAY_SIZE(D_8010F02C), Gp_PubItemLoc | ITEM_PICKUP_PANEL_TIMEOUT_ONLY, USER_INTERFACE_PANEL_ACTIVE, ITEM_PICKUP_PANEL_OPEN_DELAY, object);
            if (noticeObject != NULL) {
                noticeObject->resultValue = USER_INTERFACE_LIST_COMMAND_YES;
            }
        } else if (inventoryCanAddItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, Gp_PubItemLoc) != 0) {
            uiSpawnObject(pickupPanelDescs + 1, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_PICKUP_PANEL_OPEN_DELAY, object);
        } else {
            uiSpawnObject(pickupPanelDescs + 2, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_PICKUP_PANEL_OPEN_DELAY, object);
        }
        task->state = task->state + 1;
    }
    firstChild = task->firstChild;
    if (firstChild != NULL) {
        childTask = firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            childResult = childObject->result;
            nextChild   = childTask->nextSibling;
            if (childResult != USER_INTERFACE_RESULT_CANCEL) {
                if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
                    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    uiStartTreeClosing(childObject, childObject->owner);
                }
            } else {
                object->result      = childResult;
                object->resultValue = childObject->resultValue;
            }
            childTask = nextChild;
        } while (childTask != task->firstChild);
    }
}

/// Replaces the menu previews with a pickup and requests its display resources.
///
/// Clears all five shared slots, including the two beyond the three load
/// profiles, before publishing itemId in `CD_COMMAND_DISPLAY_LOAD_MENU` (0).
/// -1 clears the ids without requesting resources; other values are published
/// even if `itemMenuEnqueuePreviewLoad` rejects the id or suppresses the load.
/// No resource is released or waited for here; the menu resource system retains
/// ownership and the caller waits for readiness before drawing the preview.
static inline void _itemPickupRequestPreview(s32 itemId)
{
    enum {
        ITEM_PICKUP_PREVIEW_EMPTY         = -1,
        ITEM_PICKUP_PREVIEW_PROFILE_COUNT = CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW + 1
    };
    s32* previewItemIds;
    s32  profileIndex;

    previewItemIds    = Gp_PreviewItems;
    previewItemIds[2] = ITEM_PICKUP_PREVIEW_EMPTY;
    previewItemIds[1] = ITEM_PICKUP_PREVIEW_EMPTY;
    previewItemIds[0] = ITEM_PICKUP_PREVIEW_EMPTY;
    previewItemIds[3] = ITEM_PICKUP_PREVIEW_EMPTY;
    previewItemIds[4] = ITEM_PICKUP_PREVIEW_EMPTY;
    if (itemId != ITEM_PICKUP_PREVIEW_EMPTY) {
        // Republish all load-profile slots after the full reset.
        for (profileIndex = 0; profileIndex < ITEM_PICKUP_PREVIEW_PROFILE_COUNT; profileIndex++, previewItemIds++) {
            if (profileIndex == CD_COMMAND_DISPLAY_LOAD_MENU) {
                Gp_PreviewItems[CD_COMMAND_DISPLAY_LOAD_MENU] = itemId;
            } else {
                *previewItemIds = ITEM_PICKUP_PREVIEW_EMPTY;
            }
        }
        itemMenuEnqueuePreviewLoad(itemId, CD_COMMAND_DISPLAY_LOAD_MENU);
    }
}

/// Shows the published pickup's equipment-scale preview after its resources are ready.
///
/// spawnArg2 borrows the live task-owned UiObject; the published catalogue id
/// must remain stable. State 0 resets the shared request slots and queues menu
/// profile 0. Waiting ends when a scene payload is available or the CD queue
/// is idle. Until then only the recessed preview frame is drawn. The loaded
/// preview remains owned by the menu resource system through task teardown.
static void _itemPickupPreviewTask(Task* task)
{
    enum {
        ITEM_PICKUP_PREVIEW_STATE_INIT      = 0,
        ITEM_PICKUP_PREVIEW_STATE_READY     = 1,
        ITEM_PICKUP_PREVIEW_STATE_WAIT_LOAD = 2
    };
    UiObject*   object;
    CdCmdQueue* cdQueue;
    s32         itemId;
    s32         previewFlags;

    itemId         = Gp_PubItemLoc;
    object         = task->spawnArg2.pointer;
    cdQueue        = &gCdCmdQueue;
    object->result = USER_INTERFACE_RESULT_NONE;
    previewFlags   = ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
    if (task->state == ITEM_PICKUP_PREVIEW_STATE_INIT) {
        _itemPickupRequestPreview(itemId);
        task->state = ITEM_PICKUP_PREVIEW_STATE_WAIT_LOAD;
    }
    if (task->state == ITEM_PICKUP_PREVIEW_STATE_WAIT_LOAD) {
        if ((cdQueue->scenePayloadAvailable != 0) || cdCmdIsIdle()) {
            task->state = ITEM_PICKUP_PREVIEW_STATE_READY;
        }
    }
    if (task->state != ITEM_PICKUP_PREVIEW_STATE_READY) {
        previewFlags |= ITEM_MENU_PREVIEW_HIDDEN;
    }
    itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, previewFlags);
}

/// Draws the quantity and its recessed row box using a wrapped 16-bit Y origin.
///
/// Uses `itemMenuDrawQuantity`'s drawing contract. The u16 intermediate preserves
/// the panel-coordinate wrap before the signed row offset is added.
static inline void _itemMenuDrawQuantity(const UiObject* object, s32 x, s32 y, s32 quantity, s32 colorRgb)
{
    u8          quantityText[0x20];
    TextDrawReq textRequest;
    u16         textBaseY;

    textRequest.x          = object->panel.contentOriginX.unsignedValue + 0x84 + x;
    textBaseY              = object->panel.contentOriginY.unsignedValue - 3;
    textRequest.y          = textBaseY + y;
    textRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    textRequest.colorRgb   = colorRgb;
    textRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    textRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    textRequest.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&textRequest, textItoaSigned(quantityText, quantity));
    uiDrawRecessedRect(&object->panel, (x + 0x69), (y - 8), 0x1B, 7, ITEM_MENU_RECESSED_FILL_COLOR);
}

/// Draws the published pickup's name, icon and stack quantity above its preview.
///
/// spawnArg2 borrows the live task-owned UiObject; published id/quantity remain
/// stable for its lifetime. State 0 spawns the preview child and places it
/// below the one-row title panel. Ids below 0x100 use Item, others Key Item;
/// only ordinary P.E. ids show levels, and only 0xA0..0xBF show quantity.
/// Hidden panels suppress name/icon drawing but still draw a consumable's
/// quantity box. Text and pictures use the item-menu resource contracts.
static void _itemPickupTitleTask(Task* task)
{
    enum {
        ITEM_PICKUP_TITLE_STATE_INIT = 0,
        ITEM_PICKUP_KEY_ITEM_FIRST   = 0x100
    };
    TextDrawReq nameRequest;
    UiObject*   previewObject;
    UiObject*   object;
    s32         textColorRgb;
    s32         x;
    s32         y;
    s32         energyLevelIndex;
    s32         textBaseY;
    s32         itemId;

    itemId         = Gp_PubItemLoc;
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (itemId < ITEM_PICKUP_KEY_ITEM_FIRST) {
        uiDrawTitle(&(object)->panel, Gp_StrItemHdr);
    } else {
        uiDrawTitle(&(object)->panel, Gp_StrKeyItem);
    }
    if (task->state == ITEM_PICKUP_TITLE_STATE_INIT) {
        previewObject = uiSpawnObject(&D_8010F09C, 0, 0, 1, object);
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(1) + 1);
        if (previewObject != NULL) {
            previewObject->panel.bounds.unsignedRect.y = object->panel.bounds.unsignedRect.y + object->panel.bounds.unsignedRect.h;
        }
        task->state++;
    }
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    x            = object->panel.contentLeft.signedValue + 2;
    y            = object->panel.contentTop.signedValue + 0xF;
    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS(object, nameRequest, x, y, itemId, textColorRgb, textBaseY, energyLevelIndex);
    }
    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
        _itemMenuDrawQuantity(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, Gp_PubItemQty, ITEM_MENU_PANEL_TEXT_COLOR);
    }
}

#undef ITEM_MENU_DRAW_UNMARKED_ROW_CONTENTS

/// Creates a child choice dialog anchored at the parent's lower-right content corner.
///
/// parent must be a live task-owned object with current content bounds.
/// dialogOptions carries an `ITEM_MENU_DIALOG_*` layout in its low nibble and
/// optionally `ITEM_MENU_DIALOG_SYSTEM_CURSOR_SOUND`. The child's task owns it
/// until teardown. It starts active with a two-tick opening delay; its initial
/// right edge is ten pixels past the parent's content edge. Coordinates narrow
/// to 16 bits. Success clears the parent's command value and makes it inactive;
/// allocation failure returns NULL with parent unchanged.
static inline UiObject* _itemMenuSpawnDialog(UiObject* parent, s32 dialogOptions)
{
    UiObject* dialog;

    dialog = uiSpawnObject(&D_8010EA98, dialogOptions, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_DIALOG_OPEN_DELAY_TICKS, parent);
    if (dialog != NULL) {
        dialog->panel.bounds.unsignedRect.x = (parent->panel.contentOriginX.unsignedValue + parent->panel.contentRight.unsignedValue + 0xA) - dialog->panel.bounds.unsignedRect.w;
        dialog->panel.bounds.unsignedRect.y = parent->panel.contentOriginY.unsignedValue + parent->panel.contentBottom.unsignedValue;
        parent->resultValue                 = USER_INTERFACE_LIST_COMMAND_NONE;
        parent->panel.control.word          = USER_INTERFACE_PANEL_INACTIVE;
    }
    return dialog;
}

/// Asks whether to collect the published item and shows its obtained notice.
///
/// spawnArg2 is the live UiObject. State 0 attempts one Yes-selected child menu;
/// state 1 accepts its answer. Ordinary ids below 0xC0 use the carried inventory,
/// with a failed capacity check changed to No; ids 0xC0..0x1FF set a collection
/// bit. A Yes answer waits for the obtained child to return CANCEL, then passes
/// that result to the root while retaining the Yes answer in resultValue.
static void _itemPickupAskTask(Task* task)
{
    enum {
        ITEM_PICKUP_ASK_STATE_INIT        = 0,
        ITEM_PICKUP_ASK_STATE_WAIT_ANSWER = 1,
        ITEM_PICKUP_COLLECTION_ID_LIMIT   = 0x200
    };
    UiObject*           object;
    UiObject*           childObject;
    Task*               childTask;
    InventoryItemRange* carriedItems;
    s32                 textColorRgb;
    s32                 drawMode;
    s32                 childResult;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_PICKUP_ASK_STATE_INIT) {
        _itemMenuSpawnDialog(object, ITEM_MENU_DIALOG_YES_SELECTED);
        task->state = task->state + 1;
    }
    uiDrawPanelLabelWithChildFocus(&object->panel, Gp_StrMessage);
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    drawMode     = TEXT_DRAW_OUTLINED;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrPickupAsk, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        if (childResult != USER_INTERFACE_RESULT_CANCEL) {
            if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
                if (task->state == ITEM_PICKUP_ASK_STATE_WAIT_ANSWER) {
                    // Recheck capacity before granting the published item.
                    if (childObject->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                        if (Gp_PubItemLoc < (u32)(INVENTORY_CONSUMABLE_ITEM_FIRST + INVENTORY_CONSUMABLE_ITEM_COUNT)) {
                            carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                            if (inventoryCanAddItem(carriedItems, Gp_PubItemLoc) != 0) {
                                inventoryGiveItem(carriedItems, Gp_PubItemLoc, Gp_PubItemQty);
                            } else {
                                childObject->resultValue = USER_INTERFACE_LIST_COMMAND_NO;
                            }
                        } else if (Gp_PubItemLoc < (u32)ITEM_PICKUP_COLLECTION_ID_LIMIT) {
                            inventorySetCollectedBit(Gp_PubItemLoc);
                        }
                    }
                    object->resultValue = childObject->resultValue;
                    if (object->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                        uiSpawnObject(&D_8010F080, (s32)Gp_PubItemLoc, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                        uiStartTreeClosing(childObject, childObject->owner);
                        task->state = task->state + 1;
                    } else {
                        object->result = USER_INTERFACE_RESULT_CANCEL;
                    }
                }
            }
        } else {
            object->result = childResult;
        }
    }
}

/// Shows an inventory-full warning and returns a No answer when OK is accepted.
///
/// spawnArg2 is the live UiObject. The warning attempts one OK child menu during
/// initialization; acknowledgment reports CANCEL with command result No.
static void _itemPickupInventoryFullTask(Task* task)
{
    enum { ITEM_PICKUP_FULL_STATE_INIT = 0 };
    Task*     childTask;
    UiObject* object;
    UiObject* childObject;
    s32       textColorRgb;
    s32       drawMode;

    object = task->spawnArg2.pointer;
    uiDrawPanelLabelWithChildFocus(&object->panel, Gp_StrWarning);
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == ITEM_PICKUP_FULL_STATE_INIT) {
        _itemMenuSpawnDialog(object, ITEM_MENU_DIALOG_OK);
        task->state = task->state + 1;
    }
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    drawMode     = TEXT_DRAW_OUTLINED;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrInvFull, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0x1E, (const u8*)D_8010E588, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        if (childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
            object->result      = USER_INTERFACE_RESULT_CANCEL;
            object->resultValue = USER_INTERFACE_LIST_COMMAND_NO;
        }
    }
}

/// Shows the obtained item's name until timeout or permitted acknowledgment.
///
/// spawnArg2 is the live UiObject. spawnArg1's low 16 bits hold a valid catalogue
/// item id; bit 16 disables Confirm/Cancel acknowledgment. The countdown advances
/// every update, including while opening, and reports CANCEL only while active.
static void _itemPickupObtainedNoticeTask(Task* task)
{
    enum {
        ITEM_PICKUP_OBTAINED_STATE_INIT     = 0,
        ITEM_PICKUP_OBTAINED_NOTICE_UPDATES = 188,
        ITEM_PICKUP_OBTAINED_ITEM_COLOR     = 0x037A78,
        ITEM_PICKUP_OBTAINED_TIMEOUT_ONLY   = 0x10000
    };
    UiObject* object;
    s32       itemId;
    s32       contentWidth;
    s32       noticeTextWidth;
    s32       nameEndX;
    s32       textColorRgb;
    s32       drawMode;
    const u8* itemName;

    object         = task->spawnArg2.pointer;
    itemId         = (u16)task->spawnArg1.value;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, Gp_StrNotice);
    if (task->state == ITEM_PICKUP_OBTAINED_STATE_INIT) {
        task->killCountdown = ITEM_PICKUP_OBTAINED_NOTICE_UPDATES;
        contentWidth        = textMeasureLineWidth(itemGetText(itemId, ITEM_TEXT_NAME, 0)) + 0xB;
        noticeTextWidth     = textMeasureLineWidth((const u8*)Gp_StrObtained);
        if (contentWidth < noticeTextWidth) {
            contentWidth = noticeTextWidth;
        }
        uiSetPanelContentSize(&object->panel, contentWidth + 5, uiGetTextRowsHeight(2) + 1);
        object->panel.bounds.rect.x = (-object->panel.bounds.rect.w) >> 1;
        task->state                 = task->state + 1;
    }
    textColorRgb = ITEM_MENU_PANEL_TEXT_COLOR;
    drawMode     = TEXT_DRAW_OUTLINED;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrObtained, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    itemName = itemGetText(itemId, ITEM_TEXT_NAME, 0);
    nameEndX = textDrawUiLine(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0x1E, itemName, ITEM_PICKUP_OBTAINED_ITEM_COLOR, drawMode, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(object, nameEndX, object->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, textColorRgb, drawMode, TEXT_ALIGNMENT_LEFT);
    task->killCountdown--;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if ((task->killCountdown <= 0) ||
            (((task->spawnArg1.value & ITEM_PICKUP_OBTAINED_TIMEOUT_ONLY) == 0) &&
             (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0))) {
            object->result      = USER_INTERFACE_RESULT_CANCEL;
            task->killCountdown = ITEM_MENU_COMPLETED_COUNTDOWN;
        }
    }
}

/// Opens an OK child menu below a live parent and transfers input on success.
///
/// Returns a task-owned UiObject, or NULL with parent unchanged on failure.
static UiObject* _itemMenuSpawnOkMenu(UiObject* parent)
{
    return _itemMenuSpawnDialog(parent, ITEM_MENU_DIALOG_OK);
}

/// Opens a Cancel child menu below a live parent and transfers input on success.
///
/// Returns a task-owned UiObject, or NULL with parent unchanged on failure.
static UiObject* _itemMenuSpawnCancelMenu(UiObject* parent)
{
    return _itemMenuSpawnDialog(parent, ITEM_MENU_DIALOG_CANCEL);
}

UiObject* itemMenuSpawnYesNoMenu(UiObject* parent)
{
    return _itemMenuSpawnDialog(parent, ITEM_MENU_DIALOG_YES_SELECTED);
}

UiObject* itemMenuSpawnYesNoMenuDefaultNo(UiObject* parent)
{
    return _itemMenuSpawnDialog(parent, ITEM_MENU_DIALOG_NO_SELECTED);
}

/// Draws a visible item row's name and icon with optional status and P.E. marks.
///
/// Borrows object for this draw, using `itemMenuDrawItemRow`'s panel-relative
/// pixels and catalogue/GPU-storage contract. colorRgb is packed 24-bit RGB.
/// Attachment state 0 omits status; 1 enables E/L, and 2 also allows A.
/// Only ordinary P.E. item ids 15..50 draw a level. Hidden panels draw nothing.
static inline void _itemMenuDrawItemRowContents(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState)
{
    TextDrawReq nameRequest;
    s32         energyLevelIndex;
    s32         textBaseY;

    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        nameRequest.x          = object->panel.contentOriginX.unsignedValue + 0x11 + x;
        textBaseY              = object->panel.contentOriginY.unsignedValue - 6;
        nameRequest.y          = textBaseY + y;
        nameRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        nameRequest.colorRgb   = colorRgb;
        nameRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        nameRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        nameRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&nameRequest, itemGetText(itemId, ITEM_TEXT_NAME, 0));
        if (attachmentState != ITEM_MENU_ATTACHMENT_MARK_AUTOMATIC) {
            itemMenuDrawEquipmentMarker(object, x, y, itemId, attachmentState);
        }
        energyLevelIndex = itemId - ITEM_MENU_PARASITE_ENERGY_ITEM_FIRST;
        if ((u32)energyLevelIndex < (u32)ITEM_MENU_PARASITE_ENERGY_ITEM_COUNT) {
            itemMenuDrawParasiteEnergyLevel(object, x, y, energyLevelIndex % ATTACHMENT_AREA_LEVEL_COUNT + 1, colorRgb);
        }
        itemMenuDrawItemIcon(object, x, y, itemId, ITEM_MENU_ICON_DEFAULT);
    }
}

void itemMenuDrawItemRow(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState)
{
    _itemMenuDrawItemRowContents(object, x, y, itemId, colorRgb, attachmentState);
}

void itemMenuDrawItemSlotRow(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState)
{
    if (itemId == INVENTORY_ITEM_NONE) {
        uiDrawRecessedRect(&object->panel, x, (y - 0xE), 0xE, 0xE, ITEM_MENU_RECESSED_FILL_COLOR);
        return;
    }
    _itemMenuDrawItemRowContents(object, x, y, itemId, colorRgb, attachmentState);
    uiDrawRecessedRect(&object->panel, x, (y - 0xE), 0xE, 0xE, 0);
}

void itemMenuDrawQuantity(const UiObject* object, s32 x, s32 y, s32 quantity, s32 colorRgb)
{
    _itemMenuDrawQuantity(object, x, y, quantity, colorRgb);
}

void itemMenuDrawUnloadedConsumableQuantity(const UiObject* object, s32 x, s32 y, const InventoryItemRow* row, s32 colorRgb, s32 unused)
{
    u8          quantityText[0x20];
    TextDrawReq quantityRequest;
    s32         textBaseY;
    s32         unloadedQuantity;

    if (row != NULL) {
        if ((u32)(row->itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
            unloadedQuantity           = row->qty - equipmentGetLoadedConsumableQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, row->itemId);
            quantityRequest.x          = object->panel.contentOriginX.unsignedValue + 0x84 + x;
            textBaseY                  = object->panel.contentOriginY.unsignedValue - 3;
            quantityRequest.y          = textBaseY + y;
            quantityRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            quantityRequest.colorRgb   = colorRgb;
            quantityRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            quantityRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
            quantityRequest.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&quantityRequest, textItoaSigned(quantityText, unloadedQuantity));
            uiDrawRecessedRect(&object->panel, (x + 0x69), (y - 8), 0x1B, 7, ITEM_MENU_RECESSED_FILL_COLOR);
        }
    }
}

/// Replaces one of the three requested display-profile ids and queues its load.
///
/// loadProfile must be 0..2. The byte parameter also supplies the public
/// setter's low-byte narrowing. Uses `itemMenuSetPreviewItem`'s request and
/// readiness contract, publishing all three ids before calling the loader.
static inline void _itemMenuSetPreviewItem(s32 itemId, u8 loadProfile)
{
    enum {
        ITEM_MENU_PREVIEW_PROFILE_COUNT = 3,
        ITEM_MENU_PREVIEW_EMPTY         = -1
    };
    s32 profileIndex;

    if (itemId != Gp_PreviewItems[loadProfile]) {
        // Publish the selection before requesting resources, even if no load is queued.
        for (profileIndex = 0; profileIndex < ITEM_MENU_PREVIEW_PROFILE_COUNT; profileIndex++) {
            if (profileIndex == loadProfile) {
                Gp_PreviewItems[profileIndex] = itemId;
            } else {
                Gp_PreviewItems[profileIndex] = ITEM_MENU_PREVIEW_EMPTY;
            }
        }
        itemMenuEnqueuePreviewLoad(itemId, loadProfile);
    }
}

void itemMenuUpdateSelectionPreview(UiList* unusedList, const UiObject* object, s32 itemId, s32 loadProfile)
{
    s32 previewFlags;

    previewFlags = loadProfile + ITEM_MENU_PREVIEW_SCALE_EQUIPMENT;
    if (itemId != INVENTORY_ITEM_NONE) {
        if (((object->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
            _itemMenuSetPreviewItem(itemId, loadProfile);
        }
        if (cdCmdIsIdle() == 0) {
            previewFlags |= ITEM_MENU_PREVIEW_HIDDEN;
        }
    } else {
        previewFlags |= ITEM_MENU_PREVIEW_HIDDEN;
    }
    itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, previewFlags);
}

void itemMenuSetPreviewItem(s32 itemId, s32 loadProfile)
{
    _itemMenuSetPreviewItem(itemId, loadProfile);
}

void itemMenuClearPreviewItems(void)
{
    enum { ITEM_MENU_PREVIEW_EMPTY = -1 };
    s32* previewItemIds;
    s32  emptyItemId;

    previewItemIds    = Gp_PreviewItems;
    emptyItemId       = ITEM_MENU_PREVIEW_EMPTY;
    previewItemIds[2] = emptyItemId;
    previewItemIds[1] = emptyItemId;
    previewItemIds[0] = emptyItemId;
    previewItemIds[3] = emptyItemId;
    previewItemIds[4] = emptyItemId;
}

void Gp_CheckItemInfoButton(UiObject* arg0)
{
    s32 one;

    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) && (Gp_SelItemRec != NULL) && (Gp_SelItemRec->itemId != INVENTORY_ITEM_NONE)) {
        one = 1;
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        uiSpawnObject(&D_8010EAB4[45], (s32)Gp_SelItemRec->itemId, one, one, arg0);
        arg0->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
}

void itemPickupOpenPromptTask(Task* task)
{
    const UiObjectDesc* descriptor;
    UiObject*           rootObject;

    task->killCountdown--;
    if (task->killCountdown <= 0) {
        switch (Gp_PubItemLoc >> 8) {
            case ITEM_PICKUP_PLACE_BANK_ITEM:
            case ITEM_PICKUP_PLACE_BANK_KEY_ITEM:
                descriptor = &D_8010EAB4[ITEM_PICKUP_PANEL_DESCRIPTOR];
                break;
            case ITEM_PICKUP_PLACE_BANK_SAVE_POINT:
                playerCaptureSaveState();
                descriptor                                         = &D_8010D348;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint = Gp_PubItemLoc;
                break;
            default:
                stageEnsureHeapTaskPrimitiveBuffer();
                descriptor = &D_8010D6D8;
                break;
        }
        rootObject = uiSpawnObject(descriptor, 0, USER_INTERFACE_PANEL_INACTIVE, 0, NULL);
        if (rootObject != NULL) {
            task->spawnArg1.pointer = rootObject;
            gGameSession->uiOpen    = 1;
            task->state             = task->state + 1;
        }
    }
}

void itemPickupHandleResultTask(Task* task)
{
    enum {
        ITEM_PICKUP_BANK_ITEM           = 0,
        ITEM_PICKUP_BANK_KEY_ITEM       = 1,
        ITEM_PICKUP_PLACE_COLLECTED     = 2,
        ITEM_PICKUP_PLACE_REPLENISHABLE = 3,
        ITEM_PICKUP_CLOSE_DELAY_UPDATES = 12
    };
    UiObject* object;
    Enemy*    enemy;

    object = task->spawnArg1.pointer;
    enemy  = task->spawnArg2.pointer;
    switch (object->result) {
        case USER_INTERFACE_RESULT_CANCEL:
            // The obtained notice closes with CANCEL while retaining the Yes answer.
            if (object->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                enemy->workType = INVENTORY_ITEM_NONE;
            }
            // Fall through to the common prompt-closing path.
        case USER_INTERFACE_RESULT_CONFIRM:
            switch (Gp_PubItemLoc >> 8) {
                case ITEM_PICKUP_BANK_ITEM:
                case ITEM_PICKUP_BANK_KEY_ITEM:
                    if (object->resultValue == USER_INTERFACE_LIST_COMMAND_YES) {
                        if (areaGetCurrentObjectState((u8)enemy->placeKey) != ITEM_PICKUP_PLACE_REPLENISHABLE) {
                            areaSetCurrentObjectState((u8)enemy->placeKey, ITEM_PICKUP_PLACE_COLLECTED);
                        }
                    }
                    break;
            }

            uiStartTreeClosing(object, object->owner);
            Wip_UiHolder         = NULL;
            gGameSession->uiOpen = 0;
            task->killCountdown  = ITEM_PICKUP_CLOSE_DELAY_UPDATES;
            task->state          = task->state + 1;
            break;
    }
}

void itemPickupRestoreFrameTimingTask(Task* task)
{
    task->killCountdown--;
    if (task->killCountdown <= 0) {
        displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
        task->killCountdown = 1;
        task->state         = task->state + 1;
    }
}

void itemPickupExitTask(Task* task)
{
    task->killCountdown--;
    if (task->killCountdown <= 0) {
        taskKill(task);
        stageReleaseTaskPrimitiveBuffer();
        stageRequestModeTaskExit();
    }
}

const char                      Gp_StrStatus[]  = "Status";
const _ItemMenuM4a1VariantTable D_80097184      = { {
    { 0x8F, 0x00 },
    { 0x93, 0x0A },
    { 0x94, 0x0A },
    { 0x98, 0x42 },
    { 0x9A, 0x45 },
    { 0x99, 0x46 },
    { 0x9B, 0x43 },
    { 0x9C, 0x44 },
} };
const char                      Gp_StrInvoke[]  = "Invoke";
const char                      Gp_StrPeList[]  = "PE LIST";
const u8                        D_800971A4      = 0;
const char                      Gp_StrTotal2[]  = "TOTAL";
const char                      Gp_StrMessage[] = "Message";
const char                      Gp_StrWarning[] = "Warning";

const TaskFuncTable4 Gp_MapTaskStates = { {
    Gp_MapPanelInit,
    menuMapWaitForPageTask,
    menuMapNavigateTask,
    Gp_MapDrawTask,
} };

/// "Help". The three bytes after the terminator are not zero: the original
/// toolchain left them in the alignment gap.
const char Gp_StrHelp[8]      = "Help\0a~\xC6";
const char Gp_StrUse2[]       = "Use";
const char Gp_StrKeyItem2[]   = "Key Item";
const char Gp_StrMap[]        = "Map";
const char Gp_StrAttention2[] = "Attention";
const char Gp_StrNotice3[]    = "Notice";
const char Gp_StrNextLevel[]  = "Next Level";
const char D_8009720C[]       = "EXP";
const char Gp_StrCost[]       = "COST";
const char Gp_StrBonus[]      = "BONUS";
const char D_80097220[]       = "MP";
/// "Specifications". The byte after the terminator is not zero: the original
/// toolchain left it in the alignment gap.
const char Gp_StrSpecs2[16] = "Specifications\0\"";
