#include "gameplay/item_menu.h"
#include "rooms/mist_shooting_gallery.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "attachments.h"
#include "gameplay/captions.h"
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
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
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

extern GpAnimArg D_8010E7F4;

extern UiListItemFunc Gp_MainMenuCmds[6];

extern UiListItemFunc D_8010E850[1];

extern UiListItemFunc Gp_WeaponSlotRows[3];

extern UiListItemFunc D_8010E8A8[1];

extern UiListItemFunc D_8010E8D0[1];

extern UiListItemFunc D_8010E90C[1];

extern UiListItemFunc D_8010E934[1];

extern UiListItemFunc D_8010E95C[1];

extern UiListItemFunc D_8010E9A0[1];

extern UiListItemFunc D_8010E9C8[1];

extern UiListItemFunc D_8010E9F0[1];

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

TaskDesc D_8010E7E8 = { 0, 32, Gp_MenuExitCallback, { NULL } };

GpAnimArg D_8010E7F4 = { { 1 }, 9, 0, 0, 0 };

UiListItemFunc Gp_MainMenuCmds[6] = { Gp_DrawUseAttachCmd, Gp_DrawKeyItemCmd, Gp_DrawPeEnergyCmd, Gp_DrawMapCmd, Gp_DrawOptionCmd, Gp_DrawExitCmd };

UiList D_8010E820 = { Gp_MainMenuCmds, 6, { 6 }, 1, 8, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

GpEnergyIcon D_8010E844[4] = {
    { 16, 88, 0 },
    { 40, 88, 0 },
    { 72, 88, 0 },
    { 120, 80, 0 },
};

UiListItemFunc D_8010E850[1] = { Gp_DrawItemOrderRow };

UiList D_8010E854 = { D_8010E850, 10, { 10 }, 1, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListItemFunc Gp_WeaponSlotRows[3] = { Gp_DrawWeaponSlotRow, Gp_DrawWeaponSlotRow2, Gp_DrawWeaponSlotRow2 };

UiList D_8010E884 = { Gp_WeaponSlotRows, 3, { 3 }, 0, 16, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListItemFunc D_8010E8A8[1] = { func_800C41A4 };

UiList D_8010E8AC = { D_8010E8A8, 1, { 1 }, 0, 16, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListItemFunc D_8010E8D0[1] = { Gp_DrawRemoveArmorRow };

UiList D_8010E8D4 = { D_8010E8D0, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

s32 Gp_PreviewItems[5] = {
    -1,
    -1,
    -1,
    -1,
    -1,
};

UiListItemFunc D_8010E90C[1] = { Gp_DrawItemDescLine };

UiList D_8010E910 = { D_8010E90C, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListItemFunc D_8010E934[1] = { Gp_DrawUseCmd };

UiList D_8010E938 = { D_8010E934, 1, { 1 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListItemFunc D_8010E95C[1] = { Gp_DrawCollectedRow };

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

UiListItemFunc D_8010E9A0[1] = { Gp_DrawAmmoRow };

UiList D_8010E9A4 = { D_8010E9A0, 25, { 25 }, 0, 14, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListItemFunc D_8010E9C8[1] = { Gp_DrawRemoveAmmoRow };

UiList D_8010E9CC = { D_8010E9C8, 3, { 3 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListItemFunc D_8010E9F0[1] = { Gp_DrawArmorSelectRow };

UiList D_8010E9F4 = { D_8010E9F0, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListItemFunc Gp_ItemCmdFns[6] = {
    Gp_DrawUsePrompt,
    Gp_DrawMovePrompt,
    Gp_DrawMovePrompt,
    Gp_DrawMovePrompt,
    Gp_DrawDiscardCmd,
    Gp_DrawSortCmd,
};

UiList D_8010EA30 = { Gp_ItemCmdFns, 3, { 3 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

static s32 D_8010EA54[] = { 30, 60, 50, 100, 100, 200 };

UiListItemFunc Gp_DialogCmdFns[2] = {
    Gp_DrawOkCmd,
    Gp_DrawOkCmd,
};

UiList D_8010EA74 = { Gp_DialogCmdFns, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiObjectDesc D_8010EA98 = { 0, 0, 0, 48, 32, 4, 0, 0, 192, Gp_YesNoMenuTask, 0 };

/// Indexed by menu command ID; empty rows reserve unused commands.
UiObjectDesc D_8010EAB4[50] = {
    { 3, 65392, 64, 288, 40, 768, 0, 0, 192, Gp_ItemMenuTask, 0 },
    { 3, 65392, 65432, 72, 64, 28, 0, 0, 192, Gp_StatusPanelTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 3, 65464, 65432, 216, 65, 40, 0, 0, 192, Gp_HpMpBarTask, 0 },
    { 2, 65392, 65497, 144, 72, 36, 0, 0, 192, Gp_WeaponSummaryTask, 0 },
    { 0, 0, 0, 144, 20, 48, 0, 0, 192, Gp_ItemCountHeaderTask, 0 },
    { 2, 0, 65432, 144, 165, 40, 0, 0, 192, Gp_ItemDestCursorTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 524290, 65392, 65432, 144, 120, 40, 0, 0, 192, Gp_KeyItemMenuTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 2, 0, 65497, 144, 120, 512, 0, 0, 192, Gp_ArmorStatsPanelTask, 0 },
    { 2, 65392, 33, 144, 48, 516, 0, 0, 192, Gp_PeGridPanelTask, 0 },
    { 2, 65392, 65432, 200, 24, 40, 0, 0, 192, Gp_PeListPanelTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 2, 65384, 65432, 144, 104, 816, 0, 0, 192, Gp_EquipSummaryTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 2, 65528, 0, 144, 8, 32, 0, 0, 192, Gp_SelectArmorMenuTask, 0 },
    { 2, 65528, 0, 144, 8, 32, 0, 0, 192, Gp_SelectAmmoMenuTask, 0 },
    { 2, 65528, 0, 144, 8, 32, 0, 0, 192, Gp_SelectWeaponMenuTask, 0 },
    { 2, 65528, 0, 144, 8, 32, 0, 0, 192, Gp_EquipSelectMenuTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 2, 65476, 65496, 144, 78, 8, 0, 0, 192, Gp_AttachPromptTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 48, 1, 16, 0, 0, 192, Gp_ItemCmdMenuTask, 0 },
    { 2, 65436, 65456, 198, 158, 8, 0, 0, 192, func_800CFA60, 0 },
    { 2, 65392, 65432, 288, 144, 56, 0, 0, 192, Ui_WaitCdThenOverlay, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 65436, 65456, 144, 78, 12, 0, 0, 192, Gp_AmmoListTask, 0 },
    { 2, 65464, 65496, 144, 78, 8, 0, 0, 192, Gp_ReloadPromptTask, 0 },
    { 0, 65436, 65456, 144, 78, 12, 0, 0, 192, Gp_AttachListTask, 0 },
    { 2, 65476, 65496, 144, 78, 8, 0, 0, 192, Gp_EquipPromptTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 48, 32, 16, 0, 0, 192, Gp_KeyItemSubMenuTask, 0 },
    { -2147483646, 65446, 65496, 178, 78, 8, 0, 0, 192, Gp_UseKeyItemRow, 0 },
    { 2, 65392, 65432, 288, 208, 8, 0, 0, 192, func_800C5F70, 0 },
    { 3, 65392, 64, 288, 40, 56, 0, 0, 192, Gp_ItemMenuTask, 0 },
    { 2, 65392, 65432, 144, 72, 960, 0, 0, 192, Gp_WeaponMenuTask, 0 },
    { 2, 65392, 65504, 144, 91, 976, 0, 0, 192, Gp_ArmorMenuTask, 0 },
    { -2147483648, 65386, 65456, 120, 70, 60, 0, 0, 192, Gp_PickupTask, 0 },
};

void Gp_MenuRootTask(Task* arg0)
{
    switch (arg0->state) {
        case 0: {
            PlayerStatus* cfg;

            GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            D_80114D88 = 0;
            SndEvt_EnqueueTypeD();
            Gp_ClearPreviewItems();
            D_80067634 = NULL;
            D_80114DE0 = -1;
            cfg        = &Player_Status;
            D_80114DE8 = cfg->weapon;
            D_80114DE4 = cfg->weaponSlotItem;
            if (cfg->weapon != 0) {
                D_80114DE0 = Gp_GetItemSlot(cfg->weapon + 0x7F)->attachId;
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
            Mem_InitAux();
            if (disp->demoScene != DISPLAY_DEMO_NONE) {
                disp->gameMode = DISPLAY_GAME_RESTART;
                break;
            }
            arg = arg0->spawnArg1.value;
            if (arg == 0x45) {
                Wip_UiHolder = NULL;
                CdCmd_EnqueueLoadFile(1, 0, 0);
                obj = Ui_SpawnFromDesc(&D_8010EEA4, 1, 1, 2, 0);
            } else if (arg == 0x44) {
                obj = Ui_SpawnFromDesc(&D_mist_shooting_gallery_80184F70, 0, 1, 1, 0);
            } else if (arg == 0x43) {
                obj = Ui_SpawnFromDesc(&D_8010F140, 0, 1, 8, 0);
            } else if (arg == 0x42) {
                disp->keepGraphics = 1;
                obj                = Ui_SpawnFromDesc(&D_8010F898, 0, 1, 1, 0);
            } else {
                disp->keepGraphics = 1;
                obj                = Ui_SpawnFromDesc(D_8010EAB4, 0, 0, 2, 0);
            }
            if (obj == NULL) {
                break;
            }
            arg0->spawnArg2.pointer = obj;
            gGameSession->uiOpen    = 1;
            if (arg0->spawnArg1.value != 0x44) {
                SndEvt_EnqueueType6(1, 0, 0);
            }
            break;
        }
        case 0x1E:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            arg0->state                         += 0xA;
        case 0x28: {
            UiObject* obj;

            obj = arg0->spawnArg2.pointer;
            if ((obj->field_2E != 6) && (obj->field_2E != -1)) {
                return;
            }
            Ui_TeardownTree(obj, obj->owner);
            if ((arg0->spawnArg1.value != 0x44) && (arg0->spawnArg1.value != 0x42)) {
                SndEvt_EnqueueType6(5, 0, 0);
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
            s32           attach;
            s32           old;
            TaskNode*     list;
            TaskNode*     prev;
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
            Mem_ConfigureAuxHeap(gGameSession->at4.loc.stage, gGameSession->at4.loc.area);
            if (Gp_IsStateF0Active() == 0) {
                Gp_EnqueueAttach7Cd();
            }
            if (D_80114D88 == 1) {
                Gp_LoadViewAndCd(1);
            }
            attach = -1;
            Mem_InitAux();
            cfg = &Player_Status;
            Gp_SyncHeldRelated();
            if (cfg->weapon != 0) {
                attach = Gp_GetItemSlot(cfg->weapon + 0x7F)->attachId;
            }
            if ((D_80114DE8 == cfg->weapon) && (D_80114DE4 == cfg->weaponSlotItem) &&
                (D_80114DE0 == attach)) {
                break;
            }
            prev = Task_GetActiveList();
            list = &gTaskDefaultList;
            Task_SetActiveList(list);
            saved              = cfg->weapon;
            old                = (u8)D_80114DE8;
            disp               = &gDisplayState;
            disp->skipTeardown = 1;
            cfg->weapon        = old;
            Gp_KillPlayerEffs();
            Task_CallExitFiltered(list, 0x52);
            disp->skipTeardown = 0;
            cfg->weapon        = saved;
            Task_SetActiveList(prev);
            Gp_EnqueueHeldWeaponCd();
            break;
        }
        case 0x3C: {
            PlayerStatus* cfg;
            s32           attach;
            TaskNode*     prev;
            s32*          flag;

            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            cfg    = &Player_Status;
            attach = -1;
            if (cfg->weapon != 0) {
                attach = Gp_GetItemSlot(cfg->weapon + 0x7F)->attachId;
            }
            if ((D_80114DE8 != cfg->weapon) || (D_80114DE4 != cfg->weaponSlotItem) ||
                (D_80114DE0 != attach)) {
                prev = Task_GetActiveList();
                Task_SetActiveList(&gTaskDefaultList);
                flag  = &D_8005ED8C;
                *flag = 1;
                Gp_SpawnWeaponEff();
                if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                    func_8010870C(gameGetPtrSlot(3), 5);
                }
                if (arg0->spawnArg1.value == 0x44) {
                    Gp_PlayerWeaponId(&D_8010E7F4.animBlock.index);
                    Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &D_8010E7F4, 0);
                }
                *flag = 0;
                Task_SetActiveList(prev);
            }
            GameMain_SetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gDisplayState.keepGraphics = 0;
            gGameSession->uiOpen       = 0;
            Gpu_ResetGraphAndOt();
            if (Stage_GetModeByte12() == 0) {
                Stage_SetEndingFlag();
            } else {
                Stage_BeginTransitionKind7((u8)gGameSession->at4.loc.view);
            }
            Task_SpawnOnDefaultListA(1, 0x27, 2, 0);
            if (Task_SpawnOnDefaultList(&D_8010E7E8, 0, 0, 0) != NULL) {
                Display_AcquireRef();
            }
            Gp_MenuLockDelay = 8;
            func_800A7E4C();
            Task_CallExit(arg0);
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
    u8*          text;
    s32          color;
    s32          one;
    TaskSpawnArg val;
    UiObject*    child;
    s32          flag;
    u8*          map;

    val = arg1->spawnArg1;
    map = (u8*)arg1->work;
    if (val.value != 0) {
        if (val.unsignedValue > 0xFFFF) {
            color = Ui_LookupTable(arg0, 1);
            one   = 1;
            Text_DrawPrompt(arg0, arg0->panel.field_1C.s + 2, (s16)arg0->panel.field_18.u + 0xF, val.pointer, color, one, 0);
            text = Text_SkipLines(val.pointer, one);
            Text_DrawPrompt(arg0, arg0->panel.field_1C.s + 2, (s16)arg0->panel.field_18.u + 0x1E, text, color, one, 0);
        } else if ((u32)(val.value - 0x300) < 0x100U) {
            Gp_DrawCastCostLines(arg0, val.value);
        }
    }
    childTask = arg1->firstChild;
    if (childTask != NULL) {
        child = childTask->spawnArg2.pointer;
        flag  = child->field_2E;
        if (flag == -1) {
            arg0->field_2E = flag;
            Wip_UiHolder   = NULL;
        } else if (flag == 6) {
            arg0->field_2C = child->field_2C;
            Ui_TeardownTree(child, child->owner);
            Ui_SetState4(arg0, arg0->owner);
            arg1->killCountdown = 0x10;
            *map                = 0;
            GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            arg1->state = arg1->state + 1;
            if (arg0->field_2C == 0x101) {
                SndEvt_EnqueueType6(4, 0, 0);
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
