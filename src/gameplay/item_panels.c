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

void func_800CCDC8(Task* arg0);

void Gp_BuildItemCmdList(UiList* arg0, UiObject* arg1, s32 arg2, InventoryItemRow* arg3);

/// Body of `Gp_DrawQty`: prints the count `arg3` in colour `arg4` at row
/// position (`arg1`, `arg2`) of `arg0`, then lays out the box beside it.
/// Inlined where a caller draws a count without the call.
static inline void _gpDrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_PickupTitleTask(Task* arg0);

void Gp_PickupAskTask(Task* arg0);

void Gp_PickupFullTask(Task* arg0);

void Gp_ObtainedNoticeTask(Task* arg0);

static UiObject* func_800CD704(UiObject* arg0);

static UiObject* func_800CD78C(UiObject* arg0);

static inline void _gpSetPreviewItem(s32 itemId, u8 slot);

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
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                if ((arg3->qty - Gp_CountEquippedRelated(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg2)) > 0) {
                    Gp_ItemCmdFns[n++] = Gp_DrawLoadCmd;
                }
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else {
                Gp_ItemCmdFns[n++] = Gp_DrawUsePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            }
            break;
        case 1:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x80) < 0x20U) {
                    Gp_ItemCmdFns[n++] = Gp_DrawExchangeCmd;
                }
            }
            break;
        case 2:
            if (arg2 == 0) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeCmd;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeCmd;
            }
            break;
        case 3:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x60) < 0x20U) {
                    Gp_ItemCmdFns[n++] = Gp_DrawExchangeCmd;
                }
            }
            break;
        case 4:
            if (arg2 == 0) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeSlotCmd;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeSlotCmd;
                if ((arg2 != 0x92) && (arg2 != 0x95)) {
                    Gp_ItemCmdFns[n++] = Gp_DrawLoadCmd;
                }
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeSlotCmd;
                if ((arg3->qty - Gp_CountEquippedRelated(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg2)) > 0) {
                    Gp_ItemCmdFns[n++] = Gp_DrawLoadCmd;
                }
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeSlotCmd;
                Gp_ItemCmdFns[n++] = Gp_DrawUsePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            }
            break;
    }
    arg0->itemCount                     = n;
    arg0->visibleRowCount.unsignedValue = n;
}

UiObjectDesc D_8010F02C[3] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 144, 136 }, 56, 0, TASK_BODY_NONE, 192, Gp_PickupTitleTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, 32, 288, 48 }, 44, 0, TASK_BODY_NONE, 192, Gp_PickupAskTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, 32, 288, 48 }, 44, 0, TASK_BODY_NONE, 192, Gp_PickupFullTask, 0 },
};

UiObjectDesc D_8010F080 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -80, -40, 160, 80 }, 8, 0, TASK_BODY_NONE, 192, Gp_ObtainedNoticeTask, 0 };

UiObjectDesc D_8010F09C = { 0, { -144, 0, 90, 70 }, 52, 0, TASK_BODY_NONE, 192, func_800CCDC8, 0 };

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

UiObjectDesc D_8010F140 = { (s32)USER_INTERFACE_PANEL_NO_FRAME, { -145, -107, 290, 215 }, 56, 0, TASK_BODY_NONE, 192, Gp_MapTask, 0 };

UiObjectDesc D_8010F15C = { USER_INTERFACE_PANEL_TITLE_STYLE, { -140, 50, 280, 50 }, 20, 0, TASK_BODY_NONE, 192, Gp_HelpPanelTask, 0 };

UiObjectDesc D_8010F178 = { 3, { -140, -107, 170, 15 }, 24, 0, TASK_BODY_NONE, 192, Gp_DrawMapName, 0 };

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

UiListRowCallback D_8010F5C8[2] = { Gp_DrawUseAttachCmd, Gp_DrawKeyItemCmd };

UiList D_8010F5D0 = { D_8010F5C8, 2, { 2 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback Gp_PeCmdFns[2] = { Gp_DrawReviveCmd, Gp_DrawPeSlotCmd };

UiList D_8010F5FC = { Gp_PeCmdFns, 2, { 2 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010F620[1] = {
    Gp_DrawPeSlotRow,
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

UiObjectDesc D_8010F654[1] = { { 3, { -136, -50, 60, 80 }, 24, 0, TASK_BODY_NONE, 192, Gp_PeMenuListTask, 0 } };

UiObjectDesc D_8010F670 = { 3, { -136, -50, 70, 80 }, 20, 0, TASK_BODY_NONE, 192, Gp_PeCommandMenuTask, 0 };

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

void Gp_UseHealItemPanel(UiObject* arg0, Task* arg1, s32 arg2)
{
    PlayerStatus* cfg;
    HudHpMp*      hudHpMp;
    McSaveData*   save;
    s32           hp;
    s32           mp;
    s32           w;
    s32           h;

    cfg          = &gPlayerStatus;
    arg0->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(arg0)->panel, Gp_StrStatus);
    if (arg1->state == 0) {
        uiSetPanelContentSize(&(arg0)->panel, 0xB0, 0x2F);
        w                                 = (s16)arg0->panel.bounds.unsignedRect.w;
        h                                 = (s16)arg0->panel.bounds.unsignedRect.h;
        arg0->panel.bounds.unsignedRect.x = -(w >> 1);
        arg0->panel.bounds.unsignedRect.y = -(h >> 1) - 0x10;
        hp                                = cfg->hp;
        hudHpMp                           = &Gp_HpMpWork;
        hudHpMp->hp                       = hp;
        mp                                = cfg->mp;
        hudHpMp->mp                       = mp;
        if (arg2 < 0x100) {
            if (arg2 < 4) {
                if (hp < cfg->hpMax) {
                    inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
                }
                if (arg2 == 3) {
                    cfg->hp = cfg->hpMax;
                } else if (arg2 == 2) {
                    cfg->hp = cfg->hp + 0x64;
                } else {
                    cfg->hp = cfg->hp + 0x32;
                }
            } else if (arg2 == 5) {
                if ((mp < cfg->mpMax) || (hp < cfg->hpMax)) {
                    inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
                }
                cfg->mp = cfg->mp + 0x50;
                cfg->hp = cfg->hp + 0x14;
            } else if ((u32)(arg2 - 6) < 2U) {
                if (mp < cfg->mpMax) {
                    inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
                }
                if (arg2 == 7) {
                    cfg->mp = cfg->mpMax;
                } else {
                    cfg->mp = cfg->mp + 0x1E;
                }
            } else if (arg2 == 0x3D) {
                if ((mp < cfg->mpMax) || (hp < cfg->hpMax)) {
                    inventoryRemoveItemRow(0, Gp_SelItemRec, 1);
                }
                cfg->mp = cfg->mpMax;
                cfg->hp = cfg->hpMax;
            }
        } else if (hp < cfg->hpMax) {
            cfg->mp     = cfg->mp - func_800D50D4(arg2, ATTACHMENT_LEVEL_CAST_COST);
            hudHpMp->mp = cfg->mp;
            cfg->hp     = cfg->hp + func_800D50D4(arg2, ATTACHMENT_LEVEL_AMOUNT);
            save        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if ((s16)save->state.attachUseCounts[7] < 0x270F) {
                save->state.attachUseCounts[7] = save->state.attachUseCounts[7] + 1;
            }
        }
        if (cfg->hp > cfg->hpMax) {
            cfg->hp = cfg->hpMax;
        }
        if (cfg->mp > cfg->mpMax) {
            cfg->mp = cfg->mpMax;
        }
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }
    Gp_DrawHpMpStats(&(arg0)->panel, 0);
    if (arg0->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (Gp_HpMpWork.hp == cfg->hp) {
            if (Gp_HpMpWork.mp == cfg->mp) {
                arg1->killCountdown = arg1->killCountdown - 1;
            }
        }
        if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) || (arg1->killCountdown < 0)) {
            Gp_HpMpWork.hp      = cfg->hp;
            Gp_HpMpWork.mp      = cfg->mp;
            arg0->result        = USER_INTERFACE_RESULT_DISMISS;
            arg1->killCountdown = 0x7FFF;
        }
    }
}

void func_800CB6FC(UiObject* arg0, Task* arg1)
{
    struct {
        union {
            TextDrawReq               req;
            _ItemMenuM4a1VariantTable variantTable;
        } u;
    } sp20;
    s32                                 result;
    s32                                 bonus;
    register s32                        src;
    register s32                        extra;
    register s32                        item;
    register s32                        x;
    register s32                        color;
    register _ItemMenuWeaponCreateWork* work;
    s32                                 y;
    s32                                 i;
    s32                                 carried;
    s32                                 temp;
    s32                                 lines;
    s32                                 saved;
    s32                                 ten;
    s32                                 hiddenState;
    s32                                 textY;
    u16                                 cd;
    InventoryItemRange*                 scan;
    InventoryItemRange*                 scanInit;
    _ItemMenuWeaponCreateWork*          newWork;
    _ItemMenuWeaponVariant*             variants;
    EquipmentWeaponLoad*                slotSrc;
    EquipmentWeaponLoad*                slotDst;
    InventoryItemRow*                   rec;
    PlayerStatus*                       cfg;

    if (arg1->state == 0) {
        src          = 0;
        result       = 0;
        bonus        = 0;
        item         = arg1->spawnArg1.value;
        arg1->status = 0xFF;
        extra        = src;
        switch (item) {
            case 9:
                scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (Gp_SumScanQty(scan, 0x9F) != 0) {
                    arg1->status = 0x1A;
                } else if (Gp_SumScanQty(scan, 0x9E) != 0) {
                    src    = 0x9E;
                    result = 0x9F;
                } else if (Gp_SumScanQty(scan, 0x9D) != 0) {
                    src    = 0x9D;
                    result = 0x9E;
                } else {
                    arg1->status = 0x16;
                }
                break;
            case 0xC:
                scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (Gp_SumScanQty(scan, 0x80) != 0) {
                    arg1->status = 0x1A;
                } else if (Gp_SumScanQty(scan, 0x83) != 0) {
                    src    = 0x83;
                    result = 0x80;
                } else {
                    arg1->status = 0x19;
                }
                break;
            case 0xA:
            case 0x42:
            case 0x43:
            case 0x44:
            case 0x45:
            case 0x46:
                scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                if (Gp_SumScanQty(scan, 0x94) != 0) {
                    if (item == 0xA) {
                        arg1->status = 0x1A;
                    } else if (inventoryCanAddItem(scan, 0xA) != 0) {
                        bonus = 0xA;
                    } else {
                        arg1->status = 0x1B;
                    }
                }
                if (arg1->status == 0xFF) {
                    ten = 0xA;
                    // Copied into the slot the row text reuses once this scan is done.
                    variants            = sp20.u.variantTable.variants;
                    sp20.u.variantTable = D_80097184;
                    arg1->status        = 0x17;
                    // Find the variant being carried; the add-on it has mounted comes back to the inventory.
                    for (carried = 0; carried < ARRAY_SIZE(sp20.u.variantTable.variants); carried++) {
                        if (Gp_SumScanQty(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, variants[carried].weaponItemId) != 0) {
                            extra = variants[carried].mountedItemId;
                            if (item == ten && variants[carried].weaponItemId == 0x93) {
                                extra = 0;
                            }
                            if (extra != ten && extra == item) {
                                arg1->status = 0x1A;
                            } else {
                                // The weapon becomes the other variant that has the used item mounted.
                                src = variants[carried].weaponItemId;
                                for (i = 0; i < ARRAY_SIZE(sp20.u.variantTable.variants); i++) {
                                    if (item == variants[i].mountedItemId && src != variants[i].weaponItemId) {
                                        arg1->status = 0xFF;
                                        result       = variants[i].weaponItemId;
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
        if (arg1->status == 0xFF) {
            cfg        = &gPlayerStatus;
            slotSrc    = equipmentGetWeaponLoad(src);
            slotDst    = equipmentGetWeaponLoad(result);
            rec        = inventoryFindLastCarriedItemRow(src);
            newWork    = memCalloc(sizeof(_ItemMenuWeaponCreateWork), 0);
            scanInit   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            arg1->work = newWork;
            inventoryRemoveItemRow(scanInit, Gp_SelItemRec, 1);
            rec->itemId = result;
            equipmentClearSelectedRemovableLoads(result, EQUIPMENT_CLEAR_LOAD_BOTH);
            slotDst->primaryItemId = slotSrc->primaryItemId;
            Gp_EquipRelatedItem(scanInit, result, slotDst->primaryItemId, slotSrc->primaryQty);
            if ((extra == 0) && (slotDst->secondaryItemId == slotSrc->secondaryItemId)) {
                slotDst->secondaryQty = slotSrc->secondaryQty;
            }
            equipmentClearSelectedRemovableLoads(src, EQUIPMENT_CLEAR_LOAD_BOTH);
            if (cfg->weapon == (src - 0x7F)) {
                temp        = result;
                cfg->weapon = temp - 0x7F;
            }
            if (extra != 0) {
                inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, extra, INVENTORY_GIVE_ONE_PACK);
            }
            if (bonus != 0) {
                inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, bonus, INVENTORY_GIVE_ONE_PACK);
            }
            temp                          = result;
            newWork->createdWeaponItemId  = temp;
            newWork->previousWeaponItemId = src;
            newWork->returnedAddonItemId  = extra;
            lines                         = 5;
            newWork->returnedClipItemId   = bonus;
            newWork->usedAddonItemId      = item;
            if (extra != 0) {
                lines = 6;
            }
            if (bonus != 0) {
                lines += 1;
            }
            uiSetPanelContentSize(&(arg0)->panel, 0xA8, uiGetTextRowsHeight(lines) + 1);
            (&(arg0)->panel)->bounds.rect.x = (-(&(arg0)->panel)->bounds.rect.w) >> 1;
            (&(arg0)->panel)->bounds.rect.y = ((-(&(arg0)->panel)->bounds.rect.h) >> 1) - 0x14;
            arg1->killCountdown             = 0xBC;
            arg1->state                     = arg1->state + 1;
        }
    }
    if (arg1->status != 0xFF) {
        saved                 = arg1->spawnArg1.value;
        arg1->spawnArg1.value = arg1->status;
        Gp_NoticePanelTask(arg1);
        arg1->spawnArg1.value = saved;
        return;
    }
    color = 0x37A78;
    y     = arg0->panel.contentTop.signedValue + 0xF;
    work  = arg1->work;
    x     = arg0->panel.contentLeft.signedValue + 2;
    uiDrawPanelLabel(&(arg0)->panel, Gp_StrNotice);
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    item        = work->previousWeaponItemId;
    if (arg0->panel.state != hiddenState) {
        sp20.u.req.x          = arg0->panel.contentOriginX.unsignedValue + 0x11 + x;
        textY                 = arg0->panel.contentOriginY.unsignedValue - 6;
        sp20.u.req.y          = textY + y;
        sp20.u.req.otIndex    = arg0->panel.otIndex.signedValue + 1;
        sp20.u.req.colorRgb   = color;
        sp20.u.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        sp20.u.req.alignment  = TEXT_ALIGNMENT_LEFT;
        sp20.u.req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&sp20.u.req, itemGetText(item, ITEM_TEXT_NAME, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(arg0, x, y, item, 0);
    }
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    item        = work->usedAddonItemId;
    y          += 0xF;
    if (arg0->panel.state != hiddenState) {
        sp20.u.req.x          = arg0->panel.contentOriginX.unsignedValue + 0x11 + x;
        textY                 = arg0->panel.contentOriginY.unsignedValue - 6;
        sp20.u.req.y          = textY + y;
        sp20.u.req.otIndex    = arg0->panel.otIndex.signedValue + 1;
        sp20.u.req.colorRgb   = color;
        sp20.u.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        sp20.u.req.alignment  = TEXT_ALIGNMENT_LEFT;
        sp20.u.req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&sp20.u.req, itemGetText(item, ITEM_TEXT_NAME, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(arg0, x, y, item, 0);
    }
    y += 0xF;
    textDrawUiLine(arg0, x, y, (const u8*)Gp_StrUsedDot, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    hiddenState = USER_INTERFACE_PANEL_HIDDEN;
    item        = work->createdWeaponItemId;
    y          += 0xF;
    if (arg0->panel.state != hiddenState) {
        sp20.u.req.x          = arg0->panel.contentOriginX.unsignedValue + 0x11 + x;
        textY                 = arg0->panel.contentOriginY.unsignedValue - 6;
        sp20.u.req.y          = textY + y;
        sp20.u.req.otIndex    = arg0->panel.otIndex.signedValue + 1;
        sp20.u.req.colorRgb   = color;
        sp20.u.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        sp20.u.req.alignment  = TEXT_ALIGNMENT_LEFT;
        sp20.u.req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&sp20.u.req, itemGetText(item, ITEM_TEXT_NAME, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(arg0, x, y, item, 0);
    }
    item = work->returnedAddonItemId;
    if (item != 0) {
        y += 0xF;
        if (arg0->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
            sp20.u.req.x          = arg0->panel.contentOriginX.unsignedValue + 0x11 + x;
            textY                 = arg0->panel.contentOriginY.unsignedValue - 6;
            sp20.u.req.y          = textY + y;
            sp20.u.req.otIndex    = arg0->panel.otIndex.signedValue + 1;
            sp20.u.req.colorRgb   = color;
            sp20.u.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            sp20.u.req.alignment  = TEXT_ALIGNMENT_LEFT;
            sp20.u.req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&sp20.u.req, itemGetText(item, ITEM_TEXT_NAME, 0));
            temp = item - 0xF;
            if ((u32)temp < 0x24U) {
                func_800C2538(arg0, x, y, temp % 3 + 1, color);
            }
            Gp_DrawItemIcon(arg0, x, y, item, 0);
        }
        item = work->returnedClipItemId;
        if (item != 0) {
            y += 0xF;
            if (arg0->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
                sp20.u.req.x          = arg0->panel.contentOriginX.unsignedValue + 0x11 + x;
                textY                 = arg0->panel.contentOriginY.unsignedValue - 6;
                sp20.u.req.y          = textY + y;
                sp20.u.req.otIndex    = arg0->panel.otIndex.signedValue + 1;
                sp20.u.req.colorRgb   = color;
                sp20.u.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
                sp20.u.req.alignment  = TEXT_ALIGNMENT_LEFT;
                sp20.u.req.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&sp20.u.req, itemGetText(item, ITEM_TEXT_NAME, 0));
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    func_800C2538(arg0, x, y, temp % 3 + 1, color);
                }
                Gp_DrawItemIcon(arg0, x, y, item, 0);
            }
        }
    }
    textDrawUiLine(arg0, x, y + 0xF, (const u8*)Gp_StrCreatedDot, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (arg0->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        cd                  = arg1->killCountdown - 1;
        arg1->killCountdown = cd;
        if ((((s32)(cd << 0x10)) <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->result        = USER_INTERFACE_RESULT_DISMISS;
            arg1->killCountdown = 0x7FFF;
            return;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            arg0->result        = USER_INTERFACE_RESULT_CANCEL;
            arg1->killCountdown = 0x7FFF;
        }
    }
}

void Gp_InvokePeItemPanel(UiObject* arg0, Task* arg1, s32 arg2)
{
    const u8*     text;
    s32           width;
    s32           temp;
    PlayerStatus* cfg;
    McSaveData*   save;
    s32           n;
    s32           i;
    s32           row;
    s32           col;

    text = itemGetText(arg2, ITEM_TEXT_NAME, 0);
    if (arg1->state == 0) {
        width = textMeasureLineWidth(text) + 0xB;
        temp  = textMeasureLineWidth((const u8*)Gp_StrInvoked);
        if (width < temp) {
            width = temp;
        }
        uiSetPanelContentSize(&(arg0)->panel, width + 5, uiGetTextRowsHeight(2) + 1);
        (&(arg0)->panel)->bounds.rect.x = (-(&(arg0)->panel)->bounds.rect.w) >> 1;
        (&(arg0)->panel)->bounds.rect.y = ((-(&(arg0)->panel)->bounds.rect.h) >> 1) - 0x14;
        inventoryRemoveItemRow(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, Gp_SelItemRec, 1);

        i   = (arg2 - 0xF) / 3;
        n   = arg2 - 0xF;
        row = col = i / 3;
        col       = i - row * 3;
        save      = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        n         = n - i * 3 + 1;
        cfg       = &gPlayerStatus;
        if (save->state.attachLevels[col + row * 3] < n) {
            save->state.attachLevels[col + row * 3] = n;
        }
        Gp_RecalcMaxMp();
        cfg->mp             = cfg->mpMax;
        Gp_HpMpWork.mp      = cfg->mp;
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    uiDrawPanelLabel(&(arg0)->panel, Gp_StrInvoke);
    textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrInvoked, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    width = textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0x1E, text, 0x37A78, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(arg0, width, arg0->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    arg1->killCountdown--;
    if (arg0->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            arg0->result = USER_INTERFACE_RESULT_CANCEL;
        } else if ((arg1->killCountdown <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->result        = USER_INTERFACE_RESULT_DISMISS;
            arg1->killCountdown = 0x7FFF;
        }
    }
}

void func_800CC41C(UiObject* arg0, Task* arg1)
{
    McSaveData* save;
    McSaveData* save2;
    McSaveData* p;
    s32         idx;
    s32         slot;
    s32         temp;

    idx = arg1->spawnArg1.value - 0x36;
    if (arg1->state == 0) {
        save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        p    = (McSaveData*)&((u8*)&save->state.saveChecksum)[idx * 3];
        slot = p->state.attachLevels[0] > p->state.attachLevels[1];
        if (save->state.attachLevels[slot + idx * 3] >= 3) {
            slot = 2;
            if (save->state.attachLevels[idx * 3 + 2] >= 3) {
                slot = (idx * 3 + 2) * 3 + 0x11;
                goto store;
            }
        }
        save2 = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        temp  = slot + idx * 3;
        slot  = save2->state.attachLevels[temp] + temp * 3 + 0xF;
    store:
        arg1->extraState.value = slot;
    }
    Gp_InvokePeItemPanel(arg0, arg1, arg1->extraState.value);
}

void Gp_YesNoMenuTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    s32       mode;
    s32       sel;

    obj         = arg0->spawnArg2.pointer;
    menu        = &D_8010EA74;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        mode = arg0->spawnArg1.value & 0xF;
        switch (mode) {
            case 1:
                Gp_DialogCmdFns[0] = Gp_DrawOkCmd;
                menu->itemCount    = mode;
                break;
            case 2:
                Gp_DialogCmdFns[0] = Gp_DrawCancelCmd;
                menu->itemCount    = 1;
                break;
            case 3:
                Gp_DialogCmdFns[0] = Gp_DrawYesCmd;
                Gp_DialogCmdFns[1] = Gp_DrawNoCmd;
                menu->itemCount    = 2;
                break;
            default:
                Gp_DialogCmdFns[0] = Gp_DrawYesCmd;
                Gp_DialogCmdFns[1] = Gp_DrawNoCmd;
                menu->itemCount    = 2;
                break;
        }
        menu->visibleRowCount.unsignedValue = menu->itemCount;
        uiFitPanelToList(menu, &(obj)->panel);
        obj->panel.bounds.unsignedRect.y -= (s16)obj->panel.bounds.unsignedRect.h / 2;
        if (arg0->spawnArg1.value & 0x10) {
            uiSetListSystemCursorSound(menu, 1);
        } else {
            uiSetListSystemCursorSound(menu, 0);
        }
        if ((arg0->spawnArg1.value & 0xF) == 3) {
            menu->selectedItemIndex = 1;
        } else {
            menu->selectedItemIndex = 0;
        }
        arg0->state = arg0->state + 1;
    }
    uiUpdateList(menu, &obj->panel);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        sel = menu->actionResult;
        if (sel == USER_INTERFACE_RESULT_CONFIRM) {
            obj->result      = sel;
            obj->resultValue = menu->commandResult.unsignedValue;
        }
    }
}

void Gp_PeListPanelTask(Task* arg0)
{
    u8            buf[0x20];
    TextDrawReq   req;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    TextDrawReq   req5;
    TextDrawReq   req6;
    s32           xOff;
    UiObject*     obj;
    UiObjectDesc* desc;
    PlayerStatus* cfg;
    Task*         head;
    Task*         child;
    UiObject*     childObj;
    s32           color;
    s32           x;
    s32           y;
    s32           flag;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Gp_StrPeList);
    if (arg0->state == 0) {
        desc = D_8010F718;
        uiSpawnObject(desc, 0, 1, 1, obj);
        uiSpawnObject(desc + 1, 1, 0, 1, obj);
        uiSpawnObject(desc + 2, 2, 0, 1, obj);
        uiSpawnObject(desc + 3, 3, 0, 1, obj);
        arg0->state = arg0->state + 1;
    }
    color          = 0x606060;
    cfg            = &gPlayerStatus;
    xOff           = obj->panel.contentLeft.signedValue;
    x              = xOff + 0x22;
    y              = obj->panel.contentTop.signedValue + 8;
    req.x          = obj->panel.contentOriginX.unsignedValue + x;
    req.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, (const u8*)Gp_StrExp);
    req2.x          = obj->panel.contentOriginX.unsignedValue + 0xA + x;
    req2.y          = obj->panel.contentOriginY.unsignedValue + y;
    req2.otIndex    = obj->panel.otIndex.signedValue + 1;
    req2.colorRgb   = color;
    req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req2.alignment  = TEXT_ALIGNMENT_LEFT;
    req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req2, textItoaUnsigned(buf, cfg->exp));
    x               = xOff + 0x7A;
    req3.x          = obj->panel.contentOriginX.unsignedValue + x;
    req3.y          = obj->panel.contentOriginY.unsignedValue + (y - 2);
    req3.otIndex    = obj->panel.otIndex.signedValue + 1;
    req3.colorRgb   = color;
    req3.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req3.alignment  = TEXT_ALIGNMENT_RIGHT;
    req3.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req3, (const u8*)Gp_StrMp);
    req4.x          = obj->panel.contentOriginX.unsignedValue + 0xA + x;
    req4.y          = obj->panel.contentOriginY.unsignedValue + y;
    req4.otIndex    = obj->panel.otIndex.signedValue + 1;
    req4.colorRgb   = color;
    req4.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req4.alignment  = TEXT_ALIGNMENT_LEFT;
    req4.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req4, textItoaSigned(buf, cfg->mp));
    req5.x          = obj->panel.contentOriginX.unsignedValue + 0x25 + x;
    req5.y          = obj->panel.contentOriginY.unsignedValue + y;
    req5.otIndex    = obj->panel.otIndex.signedValue + 1;
    req5.colorRgb   = color;
    req5.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req5.alignment  = TEXT_ALIGNMENT_CENTER;
    req5.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req5, (const u8*)Gp_StrSlash);
    req6.x          = obj->panel.contentOriginX.unsignedValue + 0x2A + x;
    req6.y          = obj->panel.contentOriginY.unsignedValue + y;
    req6.otIndex    = obj->panel.otIndex.signedValue + 1;
    req6.colorRgb   = color;
    req6.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req6.alignment  = TEXT_ALIGNMENT_LEFT;
    req6.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req6, textItoaSigned(buf, cfg->mpMax));
    head = arg0->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->result;
            child    = child->nextSibling;
            switch (flag) {
                case USER_INTERFACE_RESULT_CONFIRM:
                    obj->resultValue = 1;
                    /* fallthrough */
                case USER_INTERFACE_RESULT_CANCEL:
                    obj->result = flag;
                    break;
            }
        } while (child != arg0->firstChild);
    }
}

void Gp_ItemCountHeaderTask(Task* arg0)
{
    u8                  buf[0x20];
    u8                  buf2[0x20];
    TextDrawReq         req;
    TextDrawReq         req2;
    UiObject*           obj;
    InventoryItemRange* scan;
    s32                 cur;
    s32                 cap;
    s32                 color;
    s32                 yOff;
    s32                 x;
    s32                 y;
    s32                 y2;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if ((Gp_ItemCountShow == 1) && (uiIsPanelHidingOrHidden(obj) == 0)) {
        uiStartPanelHiding(obj, obj->owner);
    } else if ((Gp_ItemCountShow == 0) && (uiIsPanelHidingOrHidden(obj) == 1)) {
        uiLimitHiddenDelayOrOpen(&(obj)->panel, obj->owner, 0x10);
    }
    yOff   = obj->panel.contentTop.signedValue + 0xD;
    buf[0] = D_800971A4;
    memset(&buf[1], 0, 0x1F);
    color = 0x606060;
    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    cur   = Gp_CountScanItems(scan);
    cap   = Gp_GetScanCount(scan);
    textItoaUnsigned(buf, cur);
    textAppendString(buf, (const u8*)Gp_StrSlash);
    textItoaUnsigned(buf2, cap);
    textAppendString(buf, buf2);
    x              = obj->panel.contentOriginX.unsignedValue - 2;
    req.x          = obj->panel.contentRight.unsignedValue + x;
    y              = obj->panel.contentOriginY.unsignedValue - 3;
    req.y          = y + yOff;
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req, buf);
    req2.x          = obj->panel.contentLeft.unsignedValue + (obj->panel.contentOriginX.unsignedValue + 2);
    y2              = obj->panel.contentOriginY.unsignedValue - 6;
    req2.y          = y2 + yOff;
    req2.otIndex    = obj->panel.otIndex.signedValue + 1;
    req2.colorRgb   = color;
    req2.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req2.alignment  = TEXT_ALIGNMENT_LEFT;
    req2.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req2, (const u8*)Gp_StrTotal2);
}

void Gp_PickupTask(Task* arg0)
{
    UiObject*     obj;
    UiObject*     spawned;
    UiObject*     childObj;
    UiObjectDesc* desc;
    Task*         head;
    Task*         child;
    Task*         next;
    s32           flag;
    s32           one;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        desc = D_8010F02C;
        uiSpawnObject(desc, 0, 0, 1, obj);
        if (arg0->spawnArg1.value != 0) {
            inventorySetCollectedBit(Gp_PubItemLoc);
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            one     = 1;
            spawned = uiSpawnObject(desc + 3, Gp_PubItemLoc | 0x10000, one, one, obj);
            if (spawned != NULL) {
                spawned->resultValue = 0x33;
            }
        } else if (inventoryCanAddItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, Gp_PubItemLoc) != 0) {
            one = 1;
            uiSpawnObject(desc + 1, 0, one, one, obj);
        } else {
            one = 1;
            uiSpawnObject(desc + 2, 0, one, one, obj);
        }
        arg0->state = arg0->state + 1;
    }
    head = arg0->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->result;
            next     = child->nextSibling;
            if (flag != USER_INTERFACE_RESULT_CANCEL) {
                if (flag == USER_INTERFACE_RESULT_CONFIRM) {
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                    uiStartTreeClosing(childObj, childObj->owner);
                }
            } else {
                obj->result      = flag;
                obj->resultValue = childObj->resultValue;
            }
            child = next;
        } while (child != arg0->firstChild);
    }
}

void func_800CCDC8(Task* arg0)
{
    UiObject*   obj;
    CdCmdQueue* queue;
    s32         item;
    s32         flags;
    s32*        table;
    s32         i;

    item        = Gp_PubItemLoc;
    obj         = arg0->spawnArg2.pointer;
    queue       = &gCdCmdQueue;
    obj->result = USER_INTERFACE_RESULT_NONE;
    flags       = 0x10;
    if (arg0->state == 0) {
        table    = Gp_PreviewItems;
        table[2] = -1;
        table[1] = -1;
        table[0] = -1;
        table[3] = -1;
        table[4] = -1;
        if (item != -1) {
            for (i = 0; i < 3; i++, table++) {
                if (i == 0) {
                    Gp_PreviewItems[0] = item;
                } else {
                    *table = -1;
                }
            }
            Gp_EnqueueItemPreviewCd(item, 0);
        }
        arg0->state = 2;
    }
    if (arg0->state == 2) {
        if ((queue->scenePayloadAvailable != 0) || (cdCmdIsIdle() & 0xFFFF)) {
            arg0->state = 1;
        }
    }
    if (arg0->state != 1) {
        flags |= 0x100;
    }
    func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, flags);
}

/// Body of `Gp_DrawQty`: prints the count `arg3` in colour `arg4` at row
/// position (`arg1`, `arg2`) of `arg0`, then lays out the box beside it.
/// Inlined where a caller draws a count without the call.
static inline void _gpDrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    u8          buf[0x20];
    TextDrawReq req;
    u16         y;

    req.x          = arg0->panel.contentOriginX.unsignedValue + 0x84 + arg1;
    y              = arg0->panel.contentOriginY.unsignedValue - 3;
    req.y          = y + arg2;
    req.otIndex    = arg0->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg4;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_FILL_ONLY;
    textDrawString(&req, textItoaSigned(buf, arg3));
    uiDrawRecessedRect(&arg0->panel, (arg1 + 0x69), (arg2 - 8), 0x1B, 7, 0x102010);
}

void Gp_PickupTitleTask(Task* arg0)
{
    TextDrawReq req;
    UiObject*   spawned;
    UiObject*   obj;
    s32         color;
    s32         x;
    s32         y;
    s32         temp;
    s32         textY;
    s32         item;

    item        = Gp_PubItemLoc;
    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (item < 0x100) {
        uiDrawTitle(&(obj)->panel, Gp_StrItemHdr);
    } else {
        uiDrawTitle(&(obj)->panel, Gp_StrKeyItem);
    }
    if (arg0->state == 0) {
        spawned = uiSpawnObject(&D_8010F09C, 0, 0, 1, obj);
        uiSetPanelContentSize(&(obj)->panel, 0, uiGetTextRowsHeight(1) + 1);
        if (spawned != NULL) {
            spawned->panel.bounds.unsignedRect.y = obj->panel.bounds.unsignedRect.y + obj->panel.bounds.unsignedRect.h;
        }
        arg0->state++;
    }
    color = 0x606060;
    x     = obj->panel.contentLeft.signedValue + 2;
    y     = obj->panel.contentTop.signedValue + 0xF;
    if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = obj->panel.contentOriginX.unsignedValue + 0x11 + x;
        textY          = obj->panel.contentOriginY.unsignedValue - 6;
        req.y          = textY + y;
        req.otIndex    = obj->panel.otIndex.signedValue + 1;
        req.colorRgb   = color;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, itemGetText(item, ITEM_TEXT_NAME, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }
    if ((u32)(item - 0xA0) < 0x20U) {
        _gpDrawQty(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, Gp_PubItemQty, 0x606060);
    }
}

void Gp_PickupAskTask(Task* arg0)
{
    UiObject*           obj;
    UiObject*           spawned;
    UiObject*           childObj;
    Task*               child;
    InventoryItemRange* scan;
    s32                 color;
    s32                 one;
    s32                 flag;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        spawned = uiSpawnObject(&D_8010EA98, 0, 1, 2, obj);
        if (spawned != NULL) {
            spawned->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 0xA) - spawned->panel.bounds.unsignedRect.w;
            spawned->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue;
            obj->resultValue                     = 0;
            obj->panel.control.word              = USER_INTERFACE_PANEL_INACTIVE;
        }
        arg0->state = arg0->state + 1;
    }
    uiDrawPanelLabelWithChildFocus(&(obj)->panel, Gp_StrMessage);
    color = 0x606060;
    one   = 1;
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrPickupAsk, color, one, TEXT_ALIGNMENT_LEFT);
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        flag     = childObj->result;
        if (flag != USER_INTERFACE_RESULT_CANCEL) {
            if (flag == USER_INTERFACE_RESULT_CONFIRM) {
                if (arg0->state == one) {
                    if (childObj->resultValue == 0x33) {
                        if (Gp_PubItemLoc < 0xC0U) {
                            scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
                            if (inventoryCanAddItem(scan, Gp_PubItemLoc) != 0) {
                                inventoryGiveItem(scan, Gp_PubItemLoc, Gp_PubItemQty);
                            } else {
                                childObj->resultValue = 0x34;
                            }
                        } else if (Gp_PubItemLoc < 0x200U) {
                            inventorySetCollectedBit(Gp_PubItemLoc);
                        }
                    }
                    obj->resultValue = childObj->resultValue;
                    if (obj->resultValue == 0x33) {
                        uiSpawnObject(&D_8010F080, (s32)(Gp_PubItemLoc), 1, 1, obj);
                        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                        uiStartTreeClosing(childObj, childObj->owner);
                        arg0->state = arg0->state + 1;
                    } else {
                        obj->result = USER_INTERFACE_RESULT_CANCEL;
                    }
                }
            }
        } else {
            obj->result = flag;
        }
    }
}

void Gp_PickupFullTask(Task* arg0)
{
    Task*     childTask;
    UiObject* obj;
    UiObject* spawned;
    UiObject* childObj;
    s32       color;
    s32       one;

    obj = arg0->spawnArg2.pointer;
    uiDrawPanelLabelWithChildFocus(&(obj)->panel, Gp_StrWarning);
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        spawned = uiSpawnObject(&D_8010EA98, 1, 1, 2, obj);
        if (spawned != NULL) {
            spawned->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 0xA) - spawned->panel.bounds.unsignedRect.w;
            spawned->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue;
            obj->resultValue                     = 0;
            obj->panel.control.word              = USER_INTERFACE_PANEL_INACTIVE;
        }
        arg0->state = arg0->state + 1;
    }
    color = 0x606060;
    one   = 1;
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrInvFull, color, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, (const u8*)D_8010E588, color, one, TEXT_ALIGNMENT_LEFT);
    childTask = arg0->firstChild;
    if (childTask != NULL) {
        childObj = childTask->spawnArg2.pointer;
        if (childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
            obj->result      = USER_INTERFACE_RESULT_CANCEL;
            obj->resultValue = 0x34;
        }
    }
}

void Gp_ObtainedNoticeTask(Task* arg0)
{
    UiObject* obj;
    s32       item;
    s32       width;
    s32       temp;
    s32       color;
    s32       one;
    const u8* text;

    obj         = arg0->spawnArg2.pointer;
    item        = (u16)arg0->spawnArg1.value;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Gp_StrNotice);
    if (arg0->state == 0) {
        arg0->killCountdown = 0xBC;
        width               = textMeasureLineWidth(itemGetText(item, ITEM_TEXT_NAME, 0)) + 0xB;
        temp                = textMeasureLineWidth((const u8*)Gp_StrObtained);
        if (width < temp) {
            width = temp;
        }
        uiSetPanelContentSize(&(obj)->panel, width + 5, uiGetTextRowsHeight(2) + 1);
        (&(obj)->panel)->bounds.rect.x = (-(&(obj)->panel)->bounds.rect.w) >> 1;
        arg0->state                    = arg0->state + 1;
    }
    color = 0x606060;
    one   = 1;
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)Gp_StrObtained, color, one, TEXT_ALIGNMENT_LEFT);
    text = itemGetText(item, ITEM_TEXT_NAME, 0);
    temp = textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, text, 0x37A78, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, temp, obj->panel.contentTop.signedValue + 0x1E, (const u8*)Gp_StrDot, color, one, TEXT_ALIGNMENT_LEFT);
    arg0->killCountdown--;
    if (obj->panel.control.word == one) {
        if ((arg0->killCountdown <= 0) ||
            (((arg0->spawnArg1.value & 0x10000) == 0) &&
             (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0))) {
            obj->result         = USER_INTERFACE_RESULT_CANCEL;
            arg0->killCountdown = 0x7FFF;
        }
    }
}

static UiObject* func_800CD704(UiObject* arg0)
{
    UiObject* obj;

    obj = uiSpawnObject(&D_8010EA98, 1, 1, 2, arg0);
    if (obj != NULL) {
        obj->panel.bounds.unsignedRect.x = (arg0->panel.contentOriginX.unsignedValue + arg0->panel.contentRight.unsignedValue + 0xA) - obj->panel.bounds.unsignedRect.w;
        obj->panel.bounds.unsignedRect.y = arg0->panel.contentOriginY.unsignedValue + arg0->panel.contentBottom.unsignedValue;
        arg0->resultValue                = 0;
        arg0->panel.control.word         = USER_INTERFACE_PANEL_INACTIVE;
    }
    return obj;
}

static UiObject* func_800CD78C(UiObject* arg0)
{
    UiObject* obj;

    obj = uiSpawnObject(&D_8010EA98, 2, 1, 2, arg0);
    if (obj != NULL) {
        obj->panel.bounds.unsignedRect.x = (arg0->panel.contentOriginX.unsignedValue + arg0->panel.contentRight.unsignedValue + 0xA) - obj->panel.bounds.unsignedRect.w;
        obj->panel.bounds.unsignedRect.y = arg0->panel.contentOriginY.unsignedValue + arg0->panel.contentBottom.unsignedValue;
        arg0->resultValue                = 0;
        arg0->panel.control.word         = USER_INTERFACE_PANEL_INACTIVE;
    }
    return obj;
}

UiObject* func_800CD814(UiObject* arg0)
{
    UiObject* obj;

    obj = uiSpawnObject(&D_8010EA98, 0, 1, 2, arg0);
    if (obj != NULL) {
        obj->panel.bounds.unsignedRect.x = (arg0->panel.contentOriginX.unsignedValue + arg0->panel.contentRight.unsignedValue + 0xA) - obj->panel.bounds.unsignedRect.w;
        obj->panel.bounds.unsignedRect.y = arg0->panel.contentOriginY.unsignedValue + arg0->panel.contentBottom.unsignedValue;
        arg0->resultValue                = 0;
        arg0->panel.control.word         = USER_INTERFACE_PANEL_INACTIVE;
    }
    return obj;
}

UiObject* func_800CD89C(UiObject* arg0)
{
    UiObject* obj;

    obj = uiSpawnObject(&D_8010EA98, 3, 1, 2, arg0);
    if (obj != NULL) {
        obj->panel.bounds.unsignedRect.x = (arg0->panel.contentOriginX.unsignedValue + arg0->panel.contentRight.unsignedValue + 0xA) - obj->panel.bounds.unsignedRect.w;
        obj->panel.bounds.unsignedRect.y = arg0->panel.contentOriginY.unsignedValue + arg0->panel.contentBottom.unsignedValue;
        arg0->resultValue                = 0;
        arg0->panel.control.word         = USER_INTERFACE_PANEL_INACTIVE;
    }
    return obj;
}

void Gp_DrawItemLabel(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    TextDrawReq req;
    s32         temp;
    s32         y;

    if (arg0->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = arg0->panel.contentOriginX.unsignedValue + 0x11 + arg1;
        y              = arg0->panel.contentOriginY.unsignedValue - 6;
        req.y          = y + arg2;
        req.otIndex    = arg0->panel.otIndex.signedValue + 1;
        req.colorRgb   = arg4;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, itemGetText(arg3, ITEM_TEXT_NAME, 0));
        if (arg5 != 0) {
            func_800C22D8(arg0, arg1, arg2, arg3, arg5);
        }
        temp = arg3 - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, arg1, arg2, temp % 3 + 1, arg4);
        }
        Gp_DrawItemIcon(arg0, arg1, arg2, arg3, 0);
    }
}

void Gp_DrawItemNameRow(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    TextDrawReq req;
    s32         temp;
    s32         y;

    if (arg3 == 0) {
        uiDrawRecessedRect(&arg0->panel, arg1, (arg2 - 0xE), 0xE, 0xE, 0x102010);
        return;
    }
    if (arg0->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x          = arg0->panel.contentOriginX.unsignedValue + 0x11 + arg1;
        y              = arg0->panel.contentOriginY.unsignedValue - 6;
        req.y          = y + arg2;
        req.otIndex    = arg0->panel.otIndex.signedValue + 1;
        req.colorRgb   = arg4;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, itemGetText(arg3, ITEM_TEXT_NAME, 0));
        if (arg5 != 0) {
            func_800C22D8(arg0, arg1, arg2, arg3, arg5);
        }
        temp = arg3 - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, arg1, arg2, temp % 3 + 1, arg4);
        }
        Gp_DrawItemIcon(arg0, arg1, arg2, arg3, 0);
    }
    uiDrawRecessedRect(&arg0->panel, arg1, (arg2 - 0xE), 0xE, 0xE, 0);
}

void Gp_DrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    _gpDrawQty(arg0, arg1, arg2, arg3, arg4);
}

void Gp_DrawStackLeft(UiObject* arg0, s32 arg1, s32 arg2, InventoryItemRow* arg3, s32 arg4, s32 arg5)
{
    u8          buf[0x20];
    TextDrawReq req;
    s32         y;
    s32         count;

    if (arg3 != NULL) {
        if ((u32)(arg3->itemId - 0xA0) < 0x20U) {
            count          = arg3->qty - Gp_CountEquippedRelated(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, arg3->itemId);
            req.x          = arg0->panel.contentOriginX.unsignedValue + 0x84 + arg1;
            y              = arg0->panel.contentOriginY.unsignedValue - 3;
            req.y          = y + arg2;
            req.otIndex    = arg0->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg4;
            req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            req.alignment  = TEXT_ALIGNMENT_RIGHT;
            req.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&req, textItoaSigned(buf, count));
            uiDrawRecessedRect(&arg0->panel, (arg1 + 0x69), (arg2 - 8), 0x1B, 7, 0x102010);
        }
    }
}

static inline void _gpSetPreviewItem(s32 itemId, u8 slot)
{
    s32 i;

    if (itemId != Gp_PreviewItems[slot]) {
        for (i = 0; i < 3; i++) {
            if (i == slot) {
                Gp_PreviewItems[i] = itemId;
            } else {
                Gp_PreviewItems[i] = -1;
            }
        }
        Gp_EnqueueItemPreviewCd(itemId, slot);
    }
}

void Gp_ItemRowSelect(UiList* arg0, UiObject* arg1, s32 arg2, s32 arg3)
{
    s32 flags;

    flags = arg3 + 0x10;
    if (arg2 != 0) {
        if (((arg1->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (arg1->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
            _gpSetPreviewItem(arg2, arg3);
        }
        if ((cdCmdIsIdle() & 0xFFFF) == 0) {
            flags |= 0x100;
        }
    } else {
        flags |= 0x100;
    }
    func_800C7AE8(arg1, arg1->panel.contentLeft.signedValue + 2, arg1->panel.contentTop.signedValue + 2, flags);
}

void Gp_SetPreviewItem(s32 arg0, s32 arg1)
{
    _gpSetPreviewItem(arg0, arg1);
}

void Gp_ClearPreviewItems(void)
{
    s32* p;
    s32  val;

    p    = Gp_PreviewItems;
    val  = -1;
    p[2] = val;
    p[1] = val;
    p[0] = val;
    p[3] = val;
    p[4] = val;
}

void Gp_CheckItemInfoButton(UiObject* arg0)
{
    s32 one;

    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) && (Gp_SelItemRec != NULL) && (Gp_SelItemRec->itemId != INVENTORY_ITEM_NONE)) {
        one = 1;
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        uiSpawnObject(&D_8010EFA0, (s32)Gp_SelItemRec->itemId, one, one, arg0);
        arg0->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
}

void Gp_SpawnPickupUiTask(Task* arg0)
{
    UiObjectDesc* desc;
    UiObject*     obj;

    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        switch (Gp_PubItemLoc >> 8) {
            case 0:
            case 1:
                desc = &D_8010F010;
                break;
            case 8:
                Gp_SavePlayerPos();
                desc                                               = &D_8010D348;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint = Gp_PubItemLoc;
                break;
            default:
                Stage_InitPrimBufOnce();
                desc = &D_8010D6D8;
                break;
        }
        obj = uiSpawnObject(desc, 0, 0, 0, NULL);
        if (obj != NULL) {
            arg0->spawnArg1.pointer = obj;
            gGameSession->uiOpen    = 1;
            arg0->state             = arg0->state + 1;
        }
    }
}

void Gp_PickupResultTask(Task* arg0)
{
    UiObject* obj;
    Enemy*    enemy;

    obj   = arg0->spawnArg1.pointer;
    enemy = arg0->spawnArg2.pointer;
    switch (obj->result) {
        case USER_INTERFACE_RESULT_CANCEL:
            if (obj->resultValue == 0x33) {
                enemy->workType = 0;
            }
        case USER_INTERFACE_RESULT_CONFIRM:
            switch (Gp_PubItemLoc >> 8) {
                case 0:
                case 1:
                    if (obj->resultValue == 0x33) {
                        if (areaGetCurrentObjectState((u8)enemy->placeKey) != 3) {
                            areaSetCurrentObjectState((u8)enemy->placeKey, 2);
                        }
                    }
                    break;
            }

            uiStartTreeClosing(obj, obj->owner);
            Wip_UiHolder         = NULL;
            gGameSession->uiOpen = 0;
            arg0->killCountdown  = 0xC;
            arg0->state          = arg0->state + 1;
            break;
    }
}

void func_800CE188(Task* arg0)
{
    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
        arg0->killCountdown = 1;
        arg0->state         = arg0->state + 1;
    }
}

void Gp_PickupExitTask(Task* arg0)
{
    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        taskKill(arg0);
        Stage_ReleasePrimBuf();
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
    Gp_MapFirstDrawTask,
    Gp_MapTaskState2,
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
