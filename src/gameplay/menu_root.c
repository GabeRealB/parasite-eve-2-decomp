#include "gameplay/item_menu.h"
#include "rooms/mist_shooting_gallery.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "attachments.h"
#include "gameplay/captions.h"
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

#define D_8010EEA4 D_8010EAB4[36]

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

static void Gp_UiPromptUpdate(UiObject* arg0, Task* arg1);

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

TaskDesc D_8010E7E8 = { { { TASK_BODY_NONE, 32 } }, Gp_MenuExitCallback, { NULL } };

AnimationPlayRequest D_8010E7F4 = { { 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

UiListRowCallback Gp_MainMenuCmds[6] = { Gp_DrawUseAttachCmd, Gp_DrawKeyItemCmd, Gp_DrawPeEnergyCmd, Gp_DrawMapCmd, Gp_DrawOptionCmd, Gp_DrawExitCmd };

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

UiListRowCallback D_8010E90C[1] = { Gp_DrawItemDescLine };

UiList D_8010E910 = { D_8010E90C, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E934[1] = { Gp_DrawUseCmd };

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

UiListRowCallback D_8010E9A0[1] = { Gp_DrawAmmoRow };

UiList D_8010E9A4 = { D_8010E9A0, 25, { 25 }, 0, 14, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E9C8[1] = { Gp_DrawRemoveAmmoRow };

UiList D_8010E9CC = { D_8010E9C8, 3, { 3 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E9F0[1] = { Gp_DrawArmorSelectRow };

UiList D_8010E9F4 = { D_8010E9F0, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback Gp_ItemCmdFns[6] = {
    Gp_DrawUsePrompt,
    Gp_DrawMovePrompt,
    Gp_DrawMovePrompt,
    Gp_DrawMovePrompt,
    Gp_DrawDiscardCmd,
    Gp_DrawSortCmd,
};

UiList D_8010EA30 = { Gp_ItemCmdFns, 3, { 3 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

static s32 D_8010EA54[] = { 30, 60, 50, 100, 100, 200 };

UiListRowCallback Gp_DialogCmdFns[2] = {
    Gp_DrawOkCmd,
    Gp_DrawOkCmd,
};

UiList D_8010EA74 = { Gp_DialogCmdFns, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiObjectDesc D_8010EA98 = { 0, { 0, 0, 48, 32 }, 4, 0, TASK_BODY_NONE, 192, Gp_YesNoMenuTask, 0 };

/// Indexed by menu command ID; empty rows reserve unused commands.
UiObjectDesc D_8010EAB4[50] = {
    { 3, { -144, 64, 288, 40 }, 768, 0, TASK_BODY_NONE, 192, Gp_ItemMenuTask, 0 },
    { 3, { -144, -104, 72, 64 }, 28, 0, TASK_BODY_NONE, 192, Gp_StatusPanelTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 3, { -72, -104, 216, 65 }, 40, 0, TASK_BODY_NONE, 192, Gp_HpMpBarTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -39, 144, 72 }, 36, 0, TASK_BODY_NONE, 192, Gp_WeaponSummaryTask, 0 },
    { 0, { 0, 0, 144, 20 }, 48, 0, TASK_BODY_NONE, 192, Gp_ItemCountHeaderTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -104, 144, 165 }, 40, 0, TASK_BODY_NONE, 192, Gp_ItemDestCursorTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 144, 120 }, 40, 0, TASK_BODY_NONE, 192, Gp_KeyItemMenuTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -39, 144, 120 }, 512, 0, TASK_BODY_NONE, 192, Gp_ArmorStatsPanelTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, 33, 144, 48 }, 516, 0, TASK_BODY_NONE, 192, Gp_PeGridPanelTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 200, 24 }, 40, 0, TASK_BODY_NONE, 192, Gp_PeListPanelTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -152, -104, 144, 104 }, 816, 0, TASK_BODY_NONE, 192, Gp_EquipSummaryTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 8 }, 32, 0, TASK_BODY_NONE, 192, Gp_SelectArmorMenuTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 8 }, 32, 0, TASK_BODY_NONE, 192, Gp_SelectAmmoMenuTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 8 }, 32, 0, TASK_BODY_NONE, 192, Gp_SelectWeaponMenuTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 8 }, 32, 0, TASK_BODY_NONE, 192, Gp_EquipSelectMenuTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, -40, 144, 78 }, 8, 0, TASK_BODY_NONE, 192, Gp_AttachPromptTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 48, 1 }, 16, 0, TASK_BODY_NONE, 192, Gp_ItemCmdMenuTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -100, -80, 198, 158 }, 8, 0, TASK_BODY_NONE, 192, func_800CFA60, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 288, 144 }, 56, 0, TASK_BODY_NONE, 192, Ui_WaitCdThenOverlay, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { -100, -80, 144, 78 }, 12, 0, TASK_BODY_NONE, 192, Gp_AmmoListTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -72, -40, 144, 78 }, 8, 0, TASK_BODY_NONE, 192, Gp_ReloadPromptTask, 0 },
    { 0, { -100, -80, 144, 78 }, 12, 0, TASK_BODY_NONE, 192, Gp_AttachListTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, -40, 144, 78 }, 8, 0, TASK_BODY_NONE, 192, Gp_EquipPromptTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 48, 32 }, 16, 0, TASK_BODY_NONE, 192, Gp_KeyItemSubMenuTask, 0 },
    { (s32)(USER_INTERFACE_PANEL_NO_FRAME | USER_INTERFACE_PANEL_TITLE_STYLE), { -90, -40, 178, 78 }, 8, 0, TASK_BODY_NONE, 192, Gp_UseKeyItemRow, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 288, 208 }, 8, 0, TASK_BODY_NONE, 192, func_800C5F70, 0 },
    { 3, { -144, 64, 288, 40 }, 56, 0, TASK_BODY_NONE, 192, Gp_ItemMenuTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 144, 72 }, 960, 0, TASK_BODY_NONE, 192, Gp_WeaponMenuTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -32, 144, 91 }, 976, 0, TASK_BODY_NONE, 192, Gp_ArmorMenuTask, 0 },
    { (s32)USER_INTERFACE_PANEL_NO_FRAME, { -150, -80, 120, 70 }, 60, 0, TASK_BODY_NONE, 192, Gp_PickupTask, 0 },
};

void Gp_MenuRootTask(Task* arg0)
{
    switch (arg0->state) {
        case 0: {
            PlayerStatus* cfg;

            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            D_80114D88 = 0;
            SndEvt_EnqueueTypeD();
            Gp_ClearPreviewItems();
            D_80067634 = NULL;
            D_80114DE0 = -1;
            cfg        = &gPlayerStatus;
            D_80114DE8 = cfg->weapon;
            D_80114DE4 = cfg->weaponSlotItem;
            if (cfg->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                D_80114DE0 = Gp_GetItemSlot(cfg->weapon + 0x7F)->secondaryItemId;
            }
            Gp_AgeFlag119Void();
            arg0->killCountdown = 1;
            arg0->state         = 0xA;
            if ((arg0->spawnArg1.value == 0x42) || (arg0->spawnArg1.value == 0x44)) {
                arg0->killCountdown = 2;
                arg0->state         = 0xF;
            }
            return;
        }
        case 0xA:
            if (Stage_GetFadeStatus() != 1) {
                return;
            }
            Stage_RequestImageCapture();
            Stage_ResetFade();
            arg0->killCountdown = 2;
            arg0->state        += 5;
            return;
        case 0xF:
            arg0->killCountdown--;
            if (arg0->killCountdown > 0) {
                return;
            }
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            Stage_InitOtOnce();
            Stage_InitPrimBufOnce();
            arg0->state += 5;
            return;
        case 0x14: {
            RECT          rect;
            DisplayState* disp;
            UiObject*     obj;
            s32           arg;

            disp                         = &gDisplayState;
            disp->control.flags.flipMode = DISPLAY_FLIP_HOLD;
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
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
                CdCmd_EnqueueLoadFile(1, 0, 0);
                obj = uiSpawnObject(&D_8010EEA4, 1, 1, 2, 0);
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
            Stage_SetFadeMax(0xFF);
            Stage_SetFadeRate(0, 0, 0, 1);
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
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            {
                DisplayState* d;
                d = &gDisplayState;
                if (d->frameBuffer != d->otBuffer) {
                    return;
                }
                d->control.flags.flipMode = DISPLAY_FLIP_HOLD;
                Stage_ReleasePrimBuf();
            }
            memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
            if (Gp_IsStateF0Active() == 0) {
                Gp_EnqueueAttach7Cd();
            }
            if (D_80114D88 == 1) {
                Gp_LoadViewAndCd(1);
            }
            secondaryItemId = -1;
            memInitAuxHeap();
            cfg = &gPlayerStatus;
            Gp_SyncHeldRelated();
            if (cfg->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                secondaryItemId = Gp_GetItemSlot(cfg->weapon + 0x7F)->secondaryItemId;
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
            Gp_KillPlayerEffs();
            taskCallExitForPriority(list, 0x52);
            disp->immediateTaskFree = 0;
            cfg->weapon             = saved;
            taskSetActiveList(previousList);
            Gp_EnqueueHeldWeaponCd();
            break;
        }
        case 0x3C: {
            PlayerStatus* cfg;
            s32           secondaryItemId;
            TaskNode*     previousList;

            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            cfg             = &gPlayerStatus;
            secondaryItemId = -1;
            if (cfg->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                secondaryItemId = Gp_GetItemSlot(cfg->weapon + 0x7F)->secondaryItemId;
            }
            if ((D_80114DE8 != cfg->weapon) || (D_80114DE4 != cfg->weaponSlotItem) ||
                (D_80114DE0 != secondaryItemId)) {
                previousList = taskGetActiveList();
                taskSetActiveList(&gTaskDefaultList);
                // Rebuild the weapon bodies now; allocate their buffers after the view reload.
                gTaskDeferModelBufferAllocation = true;
                Gp_SpawnWeaponEff();
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    playerActorEnterAim(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 5);
                }
                if (arg0->spawnArg1.value == 0x44) {
                    Gp_PlayerWeaponId(&D_8010E7F4.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_8010E7F4, 0);
                }
                gTaskDeferModelBufferAllocation = false;
                taskSetActiveList(previousList);
            }
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gDisplayState.keepGraphics = 0;
            gGameSession->uiOpen       = 0;
            gpuResetAndInvalidateModelBuffers();
            if (Stage_GetModeByte12() == 0) {
                Stage_SetEndingFlag();
            } else {
                Stage_BeginTransitionKind7(gGameSession->location.loc.view);
            }
            Task_SpawnOnDefaultListA(FADE_DISPLAY_TASK_BANK, FADE_DISPLAY_TASK_TYPE, FADE_DISPLAY_REVEAL_WORLD, 0);
            if (taskSpawnFromTableOnDefaultList(&D_8010E7E8, 0, 0, 0) != NULL) {
                Display_AcquireRef();
            }
            Gp_MenuLockDelay = 8;
            func_800A7E4C();
            taskCallExit(arg0);
            SndEvt_EnqueueTypeE();
            break;
        }
        default:
            return;
    }
    arg0->state += 0xA;
}

static void Gp_UiPromptUpdate(UiObject* arg0, Task* arg1)
{
    Task*        childTask;
    const u8*    text;
    u32          textColorRgb;
    s32          one;
    TaskSpawnArg val;
    UiObject*    child;
    s32          flag;
    u8*          map;

    val = arg1->spawnArg1;
    map = (u8*)arg1->work;
    if (val.value != 0) {
        if (val.unsignedValue > 0xFFFF) {
            textColorRgb = uiGetTextColor(arg0, USER_INTERFACE_TEXT_COLOR_NORMAL);
            one          = 1;
            textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0xF, val.pointer, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
            text = textSkipLines(val.pointer, one);
            textDrawUiLine(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0x1E, text, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
        } else if ((u32)(val.value - 0x300) < 0x100U) {
            Gp_DrawCastCostLines(arg0, val.value);
        }
    }
    childTask = arg1->firstChild;
    if (childTask != NULL) {
        child = childTask->spawnArg2.pointer;
        flag  = child->result;
        if (flag == USER_INTERFACE_RESULT_CANCEL) {
            arg0->result = flag;
            Wip_UiHolder = NULL;
        } else if (flag == USER_INTERFACE_RESULT_CONFIRM) {
            arg0->resultValue = child->resultValue;
            uiStartTreeClosing(child, child->owner);
            uiStartPanelHiding(arg0, arg0->owner);
            arg1->killCountdown = 0x10;
            *map                = 0;
            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            arg1->state = arg1->state + 1;
            if (arg0->resultValue == 0x101) {
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            }
        }
    }
}

/// Three-entry dispatcher table indexed by `Task::state` (`Gp_ItemMenuTask`).
const UiObjectTaskFuncTable3 Gp_ItemMenuStates = { { Gp_ItemMenuInit, Gp_UiPromptUpdate, Gp_UiPromptDispatch } };

/// CLUT ids for the ten item-category icons drawn by `Gp_DrawItemIcon`,
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

/// Sets bit 0x100 in `flags`, which makes `func_800C7AE8` skip drawing the
/// item preview, while the CD queue is still busy loading it.
#define GP_HIDE_PREVIEW_WHILE_CD_BUSY(flags) \
    do {                                     \
        if (CdCmd_IsIdle() == 0) {           \
            (flags) |= 0x100;                \
        }                                    \
    } while (0)
