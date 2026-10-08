#include "gameplay/item_menu.h"
#include "rooms/mist_shooting_gallery.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/display.h"
#include "hud_sprites.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/loading.h"
#include "loading.h"
#include "menu.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/player_actor.h"
#include "player_actor.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

u8** D_80114D80;

u16 Gp_ItemCountShow;

s32 D_80114D88;

s32 Gp_ItemOrderMode;

s32 Gp_ReloadMode;

UiObject* D_80114D98[2];

s32 Gp_AttachListIds[6];

static char D_8010E548[];

extern u8 D_8010E5A0[];

extern u8 Gp_StrResSilence[];

extern u8 Gp_StrResParalysis[];

extern u8 Gp_StrResPoison[];

extern u8 Gp_StrResConfusion[];

extern u8 Gp_StrResImpact[];

extern u8 Gp_StrMotionDet[];

extern u8 Gp_StrMpGen[];

extern u8 Gp_StrHpRecov[];

extern u8 Gp_StrQuickFire[];

extern u8 Gp_StrMedInspect[];

extern u8 Gp_StrMpRecov[];

extern u8 Gp_StrCal9mm[];

extern u8 Gp_StrCal40Mag[];

extern u8 Gp_StrCal44Mag[];

extern u8 Gp_StrCal40mm[];

extern u8 Gp_StrGauge12[];

extern u8 Gp_StrCal556[];

extern u8 Gp_StrPoison[];

extern u8 Gp_StrIncendiary[];

extern u8 Gp_StrPiercing[];

extern u8 Gp_StrBurst[];

extern u8 Gp_StrFlash[];

extern u8 Gp_StrExplosion[];

extern TaskDesc D_8010E7E8;

extern AnimationPlayRequest D_8010E7F4;

extern UiListRowCallback Gp_MainMenuCmds[6];

extern UiListRowCallback D_8010E850[1];

extern UiListRowCallback Gp_WeaponSlotRows[3];

extern UiListRowCallback D_8010E8A8[1];

extern UiListRowCallback D_8010E8D0[1];

extern UiListRowCallback D_8010E90C[1];

extern UiListRowCallback D_8010E934[1];

extern UiListRowCallback D_8010E95C[1];

extern UiListRowCallback D_8010E9A0[1];

extern UiListRowCallback D_8010E9C8[1];

extern UiListRowCallback D_8010E9F0[1];

static s32 D_8010EA54[];

static void _itemMenuUpdatePromptTask(UiObject* object, Task* task);

char        Gp_StrUsedDot[]      = "used.";
char        Gp_StrCreatedDot[]   = "created.";
char        Gp_StrNoUseNow[]     = "No use for this now.";
char        Gp_StrUsed[]         = "Used";
char        Gp_StrSelectDest[]   = "Select destination.";
char        Gp_StrEquipped[]     = "Equipped";
char        Gp_StrObtained[]     = "Obtained";
char        Gp_StrLoaded[]       = "Loaded";
char        Gp_StrRemoved[]      = "Removed";
char        Gp_StrRemovedAmmo[]  = "Removed ammunition.";
char        Gp_StrInvoked[]      = "Invoked";
char        Gp_StrAmmoNone[]     = "Ammunition: None";
char        Gp_StrAttachNone[]   = "Attachments: None";
char        Gp_StrUse[]          = "Use";
char        Gp_StrMove[]         = "Move";
char        Gp_StrRemoveAmmo[]   = "Remove ammunition";
char        Gp_StrLoad[]         = "Load";
char        Gp_StrExchange[]     = "Exchange";
char        Gp_StrRemoveArmor[]  = "Remove from armor";
static char D_8010E548[]         = "Equip";
char        Gp_StrYes[]          = "Yes";
char        Gp_StrNo[]           = "No";
char        Gp_StrOk[]           = "OK";
char        Gp_StrCancel[]       = "Cancel";
char        Gp_StrPickupAsk[]    = "Pick up this item?";
char        Gp_StrInvFull[]      = "Inventory full.";
char        D_8010E588[]         = "";
char        Gp_StrSort[]         = "Sort";
char        Gp_StrAmmoCaps[]     = "AMMO";
char        Gp_StrDot[]          = ".";
u8          D_8010E5A0[]         = "";
u8          Gp_StrResSilence[]   = "RESIST \\CoSILENCE";
u8          Gp_StrResParalysis[] = "RESIST \\CoPARALYSIS";
u8          Gp_StrResPoison[]    = "RESIST \\CoPOISON";
u8          Gp_StrResConfusion[] = "RESIST \\CoCONFUSION";
u8          Gp_StrResImpact[]    = "RESIST \\CoIMPACT";
u8          Gp_StrMotionDet[]    = "MOTION DETECTOR";
u8          Gp_StrMpGen[]        = "MP GENERATION";
u8          Gp_StrHpRecov[]      = "HP RECOVERY";
u8          Gp_StrQuickFire[]    = "QUICK FIRE";
u8          Gp_StrMedInspect[]   = "MEDICAL INSPECTION";
u8          Gp_StrMpRecov[]      = "MP RECOVERY";
u8*         Gp_FeatNameTbl[]     = {
    Gp_StrMotionDet,
    Gp_StrResPoison,
    Gp_StrMpGen,
    Gp_StrQuickFire,
    Gp_StrResImpact,
    Gp_StrResSilence,
    Gp_StrMedInspect,
    Gp_StrMpRecov,
    Gp_StrHpRecov,
    Gp_StrResConfusion,
    D_8010E5A0,
    D_8010E5A0,
    Gp_StrResParalysis,
};

u8  Gp_StrCal9mm[]      = "CALIBER \\B9\\Sm\\D2\\W0 9mm";
u8  Gp_StrCal40Mag[]    = "CALIBER \\B9\\Sm\\D2\\W0 .40Mag.";
u8  Gp_StrCal44Mag[]    = "CALIBER \\B9\\Sm\\D2\\W0 .44Mag.";
u8  Gp_StrCal40mm[]     = "CALIBER \\B9\\Sm\\D2\\W0 40mm";
u8  Gp_StrGauge12[]     = "GAUGE \\B9\\Sm\\D2\\W0 12ga.";
u8  Gp_StrCal556[]      = "CALIBER \\B9\\Sm\\D2\\W0 5.56mm";
u8* Gp_CaliberNameTbl[] = {
    D_8010E5A0,
    Gp_StrCal9mm,
    Gp_StrCal40Mag,
    Gp_StrCal44Mag,
    Gp_StrCal40mm,
    Gp_StrGauge12,
    Gp_StrCal556,
    D_8010E5A0,
    D_8010E5A0,
    D_8010E5A0,
    D_8010E5A0,
    D_8010E5A0,
    D_8010E5A0,
    D_8010E5A0,
    D_8010E5A0,
    D_8010E5A0,
};

u8  Gp_StrPoison[]     = "POISON";
u8  Gp_StrIncendiary[] = "INCENDIARY";
u8  Gp_StrPiercing[]   = "PIERCING";
u8  Gp_StrBurst[]      = "BURST";
u8  Gp_StrFlash[]      = "FLASH";
u8  Gp_StrExplosion[]  = "EXPLOSION";
u8* D_8010E7C0[]       = {
    D_8010E5A0,
    D_8010E5A0,
    D_8010E5A0,
    Gp_StrPoison,
    Gp_StrBurst,
    Gp_StrPiercing,
    Gp_StrExplosion,
    Gp_StrIncendiary,
    D_8010E5A0,
    Gp_StrFlash,
};

TaskDesc D_8010E7E8 = { { { TASK_BODY_NONE, 32 } }, menuApplyPendingItemUseTask, { NULL } };

AnimationPlayRequest D_8010E7F4 = { { 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

UiListRowCallback Gp_MainMenuCmds[6] = { itemMenuDrawUseAttachCommandRow, itemMenuDrawKeyItemCommandRow, itemMenuDrawPeCommandRow, itemMenuDrawMapCommandRow, itemMenuDrawOptionsCommandRow, itemMenuDrawExitCommandRow };

UiList D_8010E820 = { Gp_MainMenuCmds, 6, { 6 }, 1, 8, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

MenuParasiteEnergyCaption D_8010E844[4] = {
    { 16, 88, 0 },
    { 40, 88, 0 },
    { 72, 88, 0 },
    { 120, 80, 0 },
};

UiListRowCallback D_8010E850[1] = { Gp_DrawItemOrderRow };

UiList D_8010E854 = { D_8010E850, 10, { 10 }, 1, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback Gp_WeaponSlotRows[3] = { Gp_DrawWeaponSlotRow, Gp_DrawWeaponSlotRow2, Gp_DrawWeaponSlotRow2 };

UiList D_8010E884 = { Gp_WeaponSlotRows, 3, { 3 }, 0, 16, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E8A8[1] = { func_800C41A4 };

UiList D_8010E8AC = { D_8010E8A8, 1, { 1 }, 0, 16, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E8D0[1] = { itemMenuDrawAttachmentCandidateRow };

UiList D_8010E8D4 = { D_8010E8D0, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

s32 Gp_PreviewItems[5] = {
    -1,
    -1,
    -1,
    -1,
    -1,
};

UiListRowCallback D_8010E90C[1] = { itemMenuDrawDescriptionRow };

UiList D_8010E910 = { D_8010E90C, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E934[1] = { itemMenuDrawKeyItemUseRow };

UiList D_8010E938 = { D_8010E934, 1, { 1 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E95C[1] = { Gp_DrawCollectedRow };

UiList D_8010E960 = { D_8010E95C, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

u8* D_8010E984[3] = {
    gGpStrRange,
    gGpStrRate,
    gGpStrWeight,
};

u8* D_8010E990[1] = {
    gGpStrPower,
};

u8* D_8010E994[3] = {
    Gp_StrAddHp,
    Gp_StrAddMp,
    gGpStrAttachDot,
};

UiListRowCallback D_8010E9A0[1] = { itemMenuDrawWeaponChoiceRow };

UiList D_8010E9A4 = { D_8010E9A0, 25, { 25 }, 0, 14, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E9C8[1] = { itemMenuDrawConsumableChoiceRow };

UiList D_8010E9CC = { D_8010E9C8, 3, { 3 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E9F0[1] = { itemMenuDrawArmorChoiceRow };

UiList D_8010E9F4 = { D_8010E9F0, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback Gp_ItemCmdFns[6] = {
    itemMenuDrawUseRow,
    itemMenuDrawMoveRow,
    itemMenuDrawMoveRow,
    itemMenuDrawMoveRow,
    itemMenuDrawDiscardRow,
    itemMenuDrawSortRow,
};

UiList D_8010EA30 = { Gp_ItemCmdFns, 3, { 3 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

static s32 D_8010EA54[] = { 30, 60, 50, 100, 100, 200 };

UiListRowCallback Gp_DialogCmdFns[2] = {
    itemMenuDrawOkRow,
    itemMenuDrawOkRow,
};

UiList D_8010EA74 = { Gp_DialogCmdFns, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiObjectDesc D_8010EA98 = { 0, { 0, 0, 48, 32 }, 4, 0, TASK_BODY_NONE, 192, itemMenuDialogTask, 0 };

/// Indexed by menu command ID; empty rows reserve unused commands.
UiObjectDesc D_8010EAB4[50] = {
    { 3, { -144, 64, 288, 40 }, 768, 0, TASK_BODY_NONE, 192, itemMenuCaptionTask, 0 },
    { 3, { -144, -104, 72, 64 }, 28, 0, TASK_BODY_NONE, 192, itemMenuMainPanelTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 3, { -72, -104, 216, 65 }, 40, 0, TASK_BODY_NONE, 192, itemMenuPlayerSummaryTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -39, 144, 72 }, 36, 0, TASK_BODY_NONE, 192, itemMenuWeaponSummaryTask, 0 },
    { 0, { 0, 0, 144, 20 }, 48, 0, TASK_BODY_NONE, 192, itemMenuInventoryCountTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -104, 144, 165 }, 40, 0, TASK_BODY_NONE, 192, itemMenuInventoryPanelTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 144, 120 }, 40, 0, TASK_BODY_NONE, 192, Gp_KeyItemMenuTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -39, 144, 120 }, 512, 0, TASK_BODY_NONE, 192, itemMenuArmorSummaryTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, 33, 144, 48 }, 516, 0, TASK_BODY_NONE, 192, itemMenuParasiteEnergySummaryTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 200, 24 }, 40, 0, TASK_BODY_NONE, 192, itemMenuParasiteEnergyListTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -152, -104, 144, 104 }, 816, 0, TASK_BODY_NONE, 192, itemMenuEquipmentDetailTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 8 }, 32, 0, TASK_BODY_NONE, 192, itemMenuArmorSelectionTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 8 }, 32, 0, TASK_BODY_NONE, 192, itemMenuAmmoSelectionTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 8 }, 32, 0, TASK_BODY_NONE, 192, itemMenuWeaponSelectionTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 8 }, 32, 0, TASK_BODY_NONE, 192, Gp_EquipSelectMenuTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, -40, 144, 78 }, 8, 0, TASK_BODY_NONE, 192, itemMenuAttachNoticeTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 48, 1 }, 16, 0, TASK_BODY_NONE, 192, Gp_ItemCmdMenuTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -100, -80, 198, 158 }, 8, 0, TASK_BODY_NONE, 192, itemMenuUseItemTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 288, 144 }, 56, 0, TASK_BODY_NONE, 192, uiUpdateOptionsAfterLoadTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { -100, -80, 144, 78 }, 12, 0, TASK_BODY_NONE, 192, itemMenuWeaponChoiceListTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -72, -40, 144, 78 }, 8, 0, TASK_BODY_NONE, 192, itemMenuReloadNoticeTask, 0 },
    { 0, { -100, -80, 144, 78 }, 12, 0, TASK_BODY_NONE, 192, itemMenuConsumableChoiceListTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, -40, 144, 78 }, 8, 0, TASK_BODY_NONE, 192, itemMenuEquipNoticeTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 48, 32 }, 16, 0, TASK_BODY_NONE, 192, itemMenuKeyItemCommandTask, 0 },
    { (s32)(USER_INTERFACE_PANEL_NO_FRAME | USER_INTERFACE_PANEL_TITLE_STYLE), { -90, -40, 178, 78 }, 8, 0, TASK_BODY_NONE, 192, itemMenuUseKeyItemTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 288, 208 }, 8, 0, TASK_BODY_NONE, 192, itemMenuInfoTask, 0 },
    { 3, { -144, 64, 288, 40 }, 56, 0, TASK_BODY_NONE, 192, itemMenuCaptionTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 144, 72 }, 960, 0, TASK_BODY_NONE, 192, itemMenuWeaponPanelTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -32, 144, 91 }, 976, 0, TASK_BODY_NONE, 192, Gp_ArmorMenuTask, 0 },
    { (s32)USER_INTERFACE_PANEL_NO_FRAME, { -150, -80, 120, 70 }, 60, 0, TASK_BODY_NONE, 192, itemPickupPanelTask, 0 },
};

/// Clears and synchronizes the framebuffer opposite the currently presented image.
///
/// Requires a stable display-buffer index 0/1 and no pending draw into that image.
static inline void _menuClearUnpresentedFrameBuffer(const DisplayState* display)
{
    enum {
        ITEM_MENU_ROOT_CLEAR_WIDTH_PIXELS         = 320,
        ITEM_MENU_ROOT_CLEAR_HEIGHT_PIXELS        = 240,
        ITEM_MENU_ROOT_FRAME_BUFFER_STRIDE_PIXELS = 272
    };
    RECT clearRect;

    clearRect.y = (display->frameBuffer ^ 1) * ITEM_MENU_ROOT_FRAME_BUFFER_STRIDE_PIXELS;
    clearRect.w = ITEM_MENU_ROOT_CLEAR_WIDTH_PIXELS;
    clearRect.x = 0;
    clearRect.h = ITEM_MENU_ROOT_CLEAR_HEIGHT_PIXELS;
    ClearImage(&clearRect, 0, 0, 0);
    DrawSync(0);
}

void menuRootTask(Task* task)
{
    enum {
        ITEM_MENU_ROOT_INITIAL                    = 0,
        ITEM_MENU_ROOT_WAIT_CAPTURE_FADE          = 10,
        ITEM_MENU_ROOT_WAIT_CAPTURE_DELAY         = 15,
        ITEM_MENU_ROOT_OPEN_PANEL                 = 20,
        ITEM_MENU_ROOT_ACTIVATE_PANEL             = 30,
        ITEM_MENU_ROOT_WAIT_PANEL_RESULT          = 40,
        ITEM_MENU_ROOT_RESTORE_RESOURCES          = 50,
        ITEM_MENU_ROOT_REBUILD_EQUIPMENT          = 60,
        ITEM_MENU_ROOT_PHASE_STEP                 = 10,
        ITEM_MENU_ROOT_CAPTURE_PHASE_STEP         = 5,
        ITEM_MENU_ROOT_REQUEST_ATTACHMENTS        = 0x42,
        ITEM_MENU_ROOT_REQUEST_GALLERY_WEAPON     = 0x44,
        ITEM_MENU_ROOT_REQUEST_OPTIONS            = 0x45,
        ITEM_MENU_ROOT_OPTIONS_DESCRIPTOR         = 36,
        ITEM_MENU_ROOT_CAPTURE_DELAY_UPDATES      = 2,
        ITEM_MENU_ROOT_OPEN_DELAY_TICKS           = 1,
        ITEM_MENU_ROOT_OPTIONS_OPEN_DELAY_TICKS   = 2,
        ITEM_MENU_ROOT_INVENTORY_OPEN_DELAY_TICKS = 2,
        ITEM_MENU_ROOT_MAP_OPEN_DELAY_TICKS       = 8,
        ITEM_MENU_ROOT_CLOSE_DELAY_UPDATES        = 12,
        ITEM_MENU_ROOT_INPUT_DELAY_UPDATES        = 8,
        ITEM_MENU_ROOT_SECONDARY_ITEM_ABSENT      = -1,
        ITEM_MENU_ROOT_WEAPON_EFFECT_PRIORITY     = 0x52,
        ITEM_MENU_ROOT_FADE_MAX                   = 255,
        ITEM_MENU_ROOT_FADE_STEP                  = 1,
        ITEM_MENU_ROOT_WEAPON_AIM_BLEND_FRAMES    = 5
    };

    switch (task->state) {
        case ITEM_MENU_ROOT_INITIAL: {
            PlayerStatus* player;

            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            D_80114D88 = 0;
            sndEvtRequestScriptDuckAcquire();
            itemMenuClearPreviewItems();
            D_80067634 = NULL;
            D_80114DE0 = ITEM_MENU_ROOT_SECONDARY_ITEM_ABSENT;
            player     = &gPlayerStatus;
            D_80114DE8 = player->weapon;
            D_80114DE4 = player->weaponSlotItem;
            if (player->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                D_80114DE0 = equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId;
            }
            inventoryUpdateIceBag();
            task->killCountdown = 1;
            task->state         = ITEM_MENU_ROOT_WAIT_CAPTURE_FADE;
            if ((task->spawnArg1.value == ITEM_MENU_ROOT_REQUEST_ATTACHMENTS) || (task->spawnArg1.value == ITEM_MENU_ROOT_REQUEST_GALLERY_WEAPON)) {
                task->killCountdown = ITEM_MENU_ROOT_CAPTURE_DELAY_UPDATES;
                task->state         = ITEM_MENU_ROOT_WAIT_CAPTURE_DELAY;
            }
            return;
        }
        case ITEM_MENU_ROOT_WAIT_CAPTURE_FADE:
            if (stageGetFadeStatus() != STAGE_FADE_AT_MAX) {
                return;
            }
            stageRequestFrameCapture();
            stageResetFadeLevel();
            task->killCountdown = ITEM_MENU_ROOT_CAPTURE_DELAY_UPDATES;
            task->state        += ITEM_MENU_ROOT_CAPTURE_PHASE_STEP;
            return;
        case ITEM_MENU_ROOT_WAIT_CAPTURE_DELAY:
            task->killCountdown--;
            if (task->killCountdown > 0) {
                return;
            }
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            stageEnsureTaskOrderingTables();
            stageEnsureHeapTaskPrimitiveBuffer();
            task->state += ITEM_MENU_ROOT_CAPTURE_PHASE_STEP;
            return;
        case ITEM_MENU_ROOT_OPEN_PANEL: {
            DisplayState* display;
            UiObject*     rootObject;
            s32           menuRequest;

            display                         = &gDisplayState;
            display->control.flags.flipMode = DISPLAY_FLIP_HOLD;
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            if (display->frameBuffer != display->drawBuffer) {
                return;
            }
            // Clear the non-presented image before reusing the auxiliary heap.
            _menuClearUnpresentedFrameBuffer(display);
            memInitAuxHeap();
            if (display->demoScene != DISPLAY_DEMO_NONE) {
                display->gameMode = DISPLAY_GAME_RESTART;
                break;
            }
            menuRequest = task->spawnArg1.value;
            if (menuRequest == ITEM_MENU_ROOT_REQUEST_OPTIONS) {
                Wip_UiHolder = NULL;
                cdCmdEnqueueDisplayResource(1, 0, CD_COMMAND_DISPLAY_LOAD_MENU);
                rootObject = uiSpawnObject(&D_8010EAB4[ITEM_MENU_ROOT_OPTIONS_DESCRIPTOR], 1, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ROOT_OPTIONS_OPEN_DELAY_TICKS, NULL);
            } else if (menuRequest == ITEM_MENU_ROOT_REQUEST_GALLERY_WEAPON) {
                rootObject = uiSpawnObject(&D_mist_shooting_gallery_80184F70, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ROOT_OPEN_DELAY_TICKS, NULL);
            } else if (menuRequest == DISPLAY_MODE_MAP) {
                rootObject = uiSpawnObject(&D_8010F140, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ROOT_MAP_OPEN_DELAY_TICKS, NULL);
            } else if (menuRequest == ITEM_MENU_ROOT_REQUEST_ATTACHMENTS) {
                display->keepGraphics = 1;
                rootObject            = uiSpawnObject(&D_8010F898, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ROOT_OPEN_DELAY_TICKS, NULL);
            } else {
                display->keepGraphics = 1;
                rootObject            = uiSpawnObject(D_8010EAB4, 0, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_ROOT_INVENTORY_OPEN_DELAY_TICKS, NULL);
            }
            if (rootObject == NULL) {
                break;
            }
            task->spawnArg2.pointer = rootObject;
            gGameSession->uiOpen    = 1;
            if (task->spawnArg1.value != ITEM_MENU_ROOT_REQUEST_GALLERY_WEAPON) {
                sndEvtRequestScriptStart(SOUND_MENU_OPEN, 0, 0);
            }
            break;
        }
        case ITEM_MENU_ROOT_ACTIVATE_PANEL:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            task->state                         += ITEM_MENU_ROOT_PHASE_STEP;
        case ITEM_MENU_ROOT_WAIT_PANEL_RESULT: {
            UiObject* rootObject;

            rootObject = task->spawnArg2.pointer;
            if ((rootObject->result != USER_INTERFACE_RESULT_CONFIRM) && (rootObject->result != USER_INTERFACE_RESULT_CANCEL)) {
                return;
            }
            uiStartTreeClosing(rootObject, rootObject->owner);
            if ((task->spawnArg1.value != ITEM_MENU_ROOT_REQUEST_GALLERY_WEAPON) && (task->spawnArg1.value != ITEM_MENU_ROOT_REQUEST_ATTACHMENTS)) {
                sndEvtRequestScriptStart(SOUND_MENU_CLOSE, 0, 0);
            }
            task->killCountdown = ITEM_MENU_ROOT_CLOSE_DELAY_UPDATES;
            stageSetFadeMax(ITEM_MENU_ROOT_FADE_MAX);
            stageConfigureFade(0, 0, 0, ITEM_MENU_ROOT_FADE_STEP);
            task->state += ITEM_MENU_ROOT_PHASE_STEP;
            return;
        }
        case ITEM_MENU_ROOT_RESTORE_RESOURCES: {
            DisplayState* display;
            PlayerStatus* player;
            s32           secondaryItemId;
            s32           previousWeapon;
            TaskNode*     defaultList;
            TaskNode*     previousList;
            s32           selectedWeapon;

            task->killCountdown--;
            if (task->killCountdown > 0) {
                return;
            }
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            {
                DisplayState* closingDisplay;
                closingDisplay = &gDisplayState;
                if (closingDisplay->frameBuffer != closingDisplay->otBuffer) {
                    return;
                }
                closingDisplay->control.flags.flipMode = DISPLAY_FLIP_HOLD;
                stageReleaseTaskPrimitiveBuffer();
            }
            memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
            if (sceneIsBattleActive() == 0) {
                attachmentEnqueueHealingSoundLoad();
            }
            if (D_80114D88 == 1) {
                loadingRestoreViewImageAndEnqueueResources(1);
            }
            secondaryItemId = ITEM_MENU_ROOT_SECONDARY_ITEM_ABSENT;
            memInitAuxHeap();
            player = &gPlayerStatus;
            equipmentSyncPrimaryAttackSelector();
            if (player->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                secondaryItemId = equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId;
            }
            if ((D_80114DE8 == player->weapon) && (D_80114DE4 == player->weaponSlotItem) &&
                (D_80114DE0 == secondaryItemId)) {
                break;
            }
            // Remove old weapon effects before admitting the replacement resources.
            previousList = taskGetActiveList();
            defaultList  = &gTaskDefaultList;
            taskSetActiveList(defaultList);
            selectedWeapon             = player->weapon;
            previousWeapon             = (u8)D_80114DE8;
            display                    = &gDisplayState;
            display->immediateTaskFree = 1;
            player->weapon             = previousWeapon;
            playerActorRemoveEquipment();
            taskCallExitForPriority(defaultList, ITEM_MENU_ROOT_WEAPON_EFFECT_PRIORITY);
            display->immediateTaskFree = 0;
            player->weapon             = selectedWeapon;
            taskSetActiveList(previousList);
            loadingEnqueueEquippedWeaponResources();
            break;
        }
        case ITEM_MENU_ROOT_REBUILD_EQUIPMENT: {
            PlayerStatus* player;
            s32           secondaryItemId;
            TaskNode*     previousList;

            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            player          = &gPlayerStatus;
            secondaryItemId = ITEM_MENU_ROOT_SECONDARY_ITEM_ABSENT;
            if (player->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                secondaryItemId = equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId;
            }
            if ((D_80114DE8 != player->weapon) || (D_80114DE4 != player->weaponSlotItem) ||
                (D_80114DE0 != secondaryItemId)) {
                previousList = taskGetActiveList();
                taskSetActiveList(&gTaskDefaultList);
                // Rebuild the weapon bodies now; allocate their buffers after the view reload.
                gTaskDeferModelBufferAllocation = true;
                playerActorRestoreEquipment();
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    playerActorEnterAim(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ITEM_MENU_ROOT_WEAPON_AIM_BLEND_FRAMES);
                }
                if (task->spawnArg1.value == ITEM_MENU_ROOT_REQUEST_GALLERY_WEAPON) {
                    playerActorWriteWeaponAnimationBankIndex(&D_8010E7F4.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_8010E7F4, 0);
                }
                gTaskDeferModelBufferAllocation = false;
                taskSetActiveList(previousList);
            }
            // Return presentation and input to the restored room.
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gDisplayState.keepGraphics = 0;
            gGameSession->uiOpen       = 0;
            gpuResetAndInvalidateModelBuffers();
            if (stageGetLoadBuffersCleared() == 0) {
                stageRequestModeTaskExit();
            } else {
                stageRequestViewTransitionAndModeExit(gGameSession->location.loc.view);
            }
            taskSpawnOnDefaultList(FADE_DISPLAY_TASK_BANK, FADE_DISPLAY_TASK_TYPE, FADE_DISPLAY_REVEAL_WORLD, 0);
            if (taskSpawnFromTableOnDefaultList(&D_8010E7E8, 0, 0, 0) != NULL) {
                displayAcquireMenuHold();
            }
            Gp_MenuLockDelay = ITEM_MENU_ROOT_INPUT_DELAY_UPDATES;
            hudDelayInputAfterMenu();
            taskCallExit(task);
            sndEvtRequestScriptDuckRelease();
            break;
        }
        default:
            return;
    }
    task->state += ITEM_MENU_ROOT_PHASE_STEP;
}

/// Draws a borrowed caption payload under itemMenuDrawTaskPrompt's contracts.
///
/// object and promptPayload are side-effect-free values, evaluated repeatedly.
/// Arguments must not name secondLine, textColorRgb or outlinedMode. Captures no
/// caller locals. Use as a standalone block statement; no trailing else. Undefined
/// after the caption updater.
#define ITEM_MENU_DRAW_PROMPT_PAYLOAD(object, promptPayload)                                                                                                                                                                                                                   \
    {                                                                                                                                                                                                                                                                          \
        enum { ITEM_MENU_PROMPT_INLINE_VALUE_MAX  = 0xFFFF,                                                                                                                                                                                                                    \
               ITEM_MENU_PROMPT_ABILITY_ID_COUNT  = 0x100,                                                                                                                                                                                                                     \
               ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS = 15,                                                                                                                                                                                                                        \
               ITEM_MENU_PROMPT_LEFT_INSET_PIXELS = 2 };                                                                                                                                                                                                                       \
        const u8* secondLine;                                                                                                                                                                                                                                                  \
        u32       textColorRgb;                                                                                                                                                                                                                                                \
        s32       outlinedMode;                                                                                                                                                                                                                                                \
        if ((promptPayload).value != 0) {                                                                                                                                                                                                                                      \
            if ((promptPayload).unsignedValue > ITEM_MENU_PROMPT_INLINE_VALUE_MAX) {                                                                                                                                                                                           \
                textColorRgb = uiGetTextColor((object), USER_INTERFACE_TEXT_COLOR_NORMAL);                                                                                                                                                                                     \
                outlinedMode = TEXT_DRAW_OUTLINED;                                                                                                                                                                                                                             \
                textDrawUiLine((object), (object)->panel.contentLeft.signedValue + ITEM_MENU_PROMPT_LEFT_INSET_PIXELS, (object)->panel.contentTop.signedValue + ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS, (promptPayload).pointer, textColorRgb, outlinedMode, TEXT_ALIGNMENT_LEFT); \
                secondLine = textSkipLines((promptPayload).pointer, 1);                                                                                                                                                                                                        \
                textDrawUiLine((object), (object)->panel.contentLeft.signedValue + ITEM_MENU_PROMPT_LEFT_INSET_PIXELS, (object)->panel.contentTop.signedValue + 2 * ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS, secondLine, textColorRgb, outlinedMode, TEXT_ALIGNMENT_LEFT);          \
            } else if ((u32)((promptPayload).value - ITEM_TEXT_PACKED_ID_FIRST) < (u32)ITEM_MENU_PROMPT_ABILITY_ID_COUNT) {                                                                                                                                                    \
                itemMenuDrawAbilityDescription((object), (promptPayload).value);                                                                                                                                                                                               \
            }                                                                                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                                                      \
    }

/// Draws the menu caption and forwards the live child's cancel or selection result.
///
/// Uses itemMenuDrawTaskPrompt's borrowed payload and resource contract. The
/// first child must be a live UI task. Cancellation releases the registered
/// caption holder; confirmation closes the child, hides the caption and advances
/// to delayed command dispatch at every-VBlank timing. Clears the work block's
/// first byte before advancing; that byte's further role is unproven.
static void _itemMenuUpdatePromptTask(UiObject* object, Task* task)
{
    enum { ITEM_MENU_COMMAND_DISPATCH_DELAY_UPDATES = 16,
           ITEM_MENU_COMMAND_RETURN_FROM_MAP        = 0x101 };
    Task*        childTask;
    TaskSpawnArg promptPayload;
    UiObject*    childObject;
    s32          childResult;
    u8*          workByte;

    promptPayload = task->spawnArg1;
    workByte      = task->work;
    ITEM_MENU_DRAW_PROMPT_PAYLOAD(object, promptPayload);
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        if (childResult == USER_INTERFACE_RESULT_CANCEL) {
            object->result = childResult;
            Wip_UiHolder   = NULL;
        } else if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
            object->resultValue = childObject->resultValue;
            uiStartTreeClosing(childObject, childObject->owner);
            uiStartPanelHiding(object, object->owner);
            task->killCountdown = ITEM_MENU_COMMAND_DISPATCH_DELAY_UPDATES;
            *workByte           = 0;
            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            task->state = task->state + 1;
            if (object->resultValue == ITEM_MENU_COMMAND_RETURN_FROM_MAP) {
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            }
        }
    }
}

#undef ITEM_MENU_DRAW_PROMPT_PAYLOAD

/// Three-entry dispatcher table indexed by `Task::state` (`itemMenuCaptionTask`).
const UiObjectTaskFuncTable3 Gp_ItemMenuStates = { { itemMenuInitializeCaptionTask, _itemMenuUpdatePromptTask, itemMenuDispatchCommand } };

/// CLUT ids for the ten item-category icons drawn by `itemMenuDrawItemIcon`,
/// indexed by the icon index that function derives from the item id.
const u16 D_80096F88[12] = {
    0x3C8F,
    0x3C8F,
    0x3C8E,
    0x3C8D,
    0x3C8C,
    0x3C8B,
    0x3C8A,
    0x3C89,
    0x3C80,
    0x3C83,
    0x0000,
    0x0000,
};
