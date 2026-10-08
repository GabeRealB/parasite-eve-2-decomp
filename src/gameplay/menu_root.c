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

UiListRowCallback D_8010E8D0[1] = { Gp_DrawRemoveArmorRow };

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

void Gp_MenuRootTask(Task* arg0)
{
    switch (arg0->state) {
        case 0: {
            PlayerStatus* cfg;

            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            D_80114D88 = 0;
            sndEvtRequestScriptDuckAcquire();
            itemMenuClearPreviewItems();
            D_80067634 = NULL;
            D_80114DE0 = -1;
            cfg        = &gPlayerStatus;
            D_80114DE8 = cfg->weapon;
            D_80114DE4 = cfg->weaponSlotItem;
            if (cfg->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                D_80114DE0 = equipmentGetWeaponLoad(cfg->weapon + 0x7F)->secondaryItemId;
            }
            inventoryUpdateIceBag();
            arg0->killCountdown = 1;
            arg0->state         = 0xA;
            if ((arg0->spawnArg1.value == 0x42) || (arg0->spawnArg1.value == 0x44)) {
                arg0->killCountdown = 2;
                arg0->state         = 0xF;
            }
            return;
        }
        case 0xA:
            if (stageGetFadeStatus() != STAGE_FADE_AT_MAX) {
                return;
            }
            stageRequestFrameCapture();
            stageResetFadeLevel();
            arg0->killCountdown = 2;
            arg0->state        += 5;
            return;
        case 0xF:
            arg0->killCountdown--;
            if (arg0->killCountdown > 0) {
                return;
            }
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            stageEnsureTaskOrderingTables();
            stageEnsureHeapTaskPrimitiveBuffer();
            arg0->state += 5;
            return;
        case 0x14: {
            RECT          rect;
            DisplayState* disp;
            UiObject*     obj;
            s32           arg;

            disp                         = &gDisplayState;
            disp->control.flags.flipMode = DISPLAY_FLIP_HOLD;
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            if (disp->frameBuffer != disp->drawBuffer) {
                return;
            }
            rect.y = (disp->frameBuffer ^ 1) * 0x110;
            rect.w = 0x140;
            rect.x = 0;
            rect.h = 0xF0;
            ClearImage(&rect, 0, 0, 0);
            DrawSync(0);
            memInitAuxHeap();
            if (disp->demoScene != DISPLAY_DEMO_NONE) {
                disp->gameMode = DISPLAY_GAME_RESTART;
                break;
            }
            arg = arg0->spawnArg1.value;
            if (arg == 0x45) {
                Wip_UiHolder = NULL;
                cdCmdEnqueueDisplayResource(1, 0, CD_COMMAND_DISPLAY_LOAD_MENU);
                obj = uiSpawnObject(&D_8010EAB4[36], 1, 1, 2, 0);
            } else if (arg == 0x44) {
                obj = uiSpawnObject(&D_mist_shooting_gallery_80184F70, 0, 1, 1, 0);
            } else if (arg == 0x43) {
                obj = uiSpawnObject(&D_8010F140, 0, 1, 8, 0);
            } else if (arg == 0x42) {
                disp->keepGraphics = 1;
                obj                = uiSpawnObject(&D_8010F898, 0, 1, 1, 0);
            } else {
                disp->keepGraphics = 1;
                obj                = uiSpawnObject(D_8010EAB4, 0, 0, 2, 0);
            }
            if (obj == NULL) {
                break;
            }
            arg0->spawnArg2.pointer = obj;
            gGameSession->uiOpen    = 1;
            if (arg0->spawnArg1.value != 0x44) {
                sndEvtRequestScriptStart(SOUND_MENU_OPEN, 0, 0);
            }
            break;
        }
        case 0x1E:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            arg0->state                         += 0xA;
        case 0x28: {
            UiObject* obj;

            obj = arg0->spawnArg2.pointer;
            if ((obj->result != USER_INTERFACE_RESULT_CONFIRM) && (obj->result != USER_INTERFACE_RESULT_CANCEL)) {
                return;
            }
            uiStartTreeClosing(obj, obj->owner);
            if ((arg0->spawnArg1.value != 0x44) && (arg0->spawnArg1.value != 0x42)) {
                sndEvtRequestScriptStart(SOUND_MENU_CLOSE, 0, 0);
            }
            arg0->killCountdown = 0xC;
            stageSetFadeMax(0xFF);
            stageConfigureFade(0, 0, 0, 1);
            arg0->state += 0xA;
            return;
        }
        case 0x32: {
            DisplayState* disp;
            PlayerStatus* cfg;
            s32           secondaryItemId;
            s32           old;
            TaskNode*     list;
            TaskNode*     previousList;
            s32           saved;

            arg0->killCountdown--;
            if (arg0->killCountdown > 0) {
                return;
            }
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            {
                DisplayState* d;
                d = &gDisplayState;
                if (d->frameBuffer != d->otBuffer) {
                    return;
                }
                d->control.flags.flipMode = DISPLAY_FLIP_HOLD;
                stageReleaseTaskPrimitiveBuffer();
            }
            memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
            if (sceneIsBattleActive() == 0) {
                attachmentEnqueueHealingSoundLoad();
            }
            if (D_80114D88 == 1) {
                loadingRestoreViewImageAndEnqueueResources(1);
            }
            secondaryItemId = -1;
            memInitAuxHeap();
            cfg = &gPlayerStatus;
            equipmentSyncPrimaryAttackSelector();
            if (cfg->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                secondaryItemId = equipmentGetWeaponLoad(cfg->weapon + 0x7F)->secondaryItemId;
            }
            if ((D_80114DE8 == cfg->weapon) && (D_80114DE4 == cfg->weaponSlotItem) &&
                (D_80114DE0 == secondaryItemId)) {
                break;
            }
            previousList = taskGetActiveList();
            list         = &gTaskDefaultList;
            taskSetActiveList(list);
            saved                   = cfg->weapon;
            old                     = (u8)D_80114DE8;
            disp                    = &gDisplayState;
            disp->immediateTaskFree = 1;
            cfg->weapon             = old;
            playerActorRemoveEquipment();
            taskCallExitForPriority(list, 0x52);
            disp->immediateTaskFree = 0;
            cfg->weapon             = saved;
            taskSetActiveList(previousList);
            loadingEnqueueEquippedWeaponResources();
            break;
        }
        case 0x3C: {
            PlayerStatus* cfg;
            s32           secondaryItemId;
            TaskNode*     previousList;

            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            cfg             = &gPlayerStatus;
            secondaryItemId = -1;
            if (cfg->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                secondaryItemId = equipmentGetWeaponLoad(cfg->weapon + 0x7F)->secondaryItemId;
            }
            if ((D_80114DE8 != cfg->weapon) || (D_80114DE4 != cfg->weaponSlotItem) ||
                (D_80114DE0 != secondaryItemId)) {
                previousList = taskGetActiveList();
                taskSetActiveList(&gTaskDefaultList);
                // Rebuild the weapon bodies now; allocate their buffers after the view reload.
                gTaskDeferModelBufferAllocation = true;
                playerActorRestoreEquipment();
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    playerActorEnterAim(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 5);
                }
                if (arg0->spawnArg1.value == 0x44) {
                    playerActorWriteWeaponAnimationBankIndex(&D_8010E7F4.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_8010E7F4, 0);
                }
                gTaskDeferModelBufferAllocation = false;
                taskSetActiveList(previousList);
            }
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
            Gp_MenuLockDelay = 8;
            hudDelayInputAfterMenu();
            taskCallExit(arg0);
            sndEvtRequestScriptDuckRelease();
            break;
        }
        default:
            return;
    }
    arg0->state += 0xA;
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
