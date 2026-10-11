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

const u8** D_80114D80;

u16 gItemMenuEquipmentDetailOpen;

s32 gItemMenuRoomRestorePending;

s32 gItemMenuDestinationSelectionMode;

s32 gItemMenuWeaponLoadSelection;

UiObject* D_80114D98[2];

s32 Gp_AttachListIds[6];

extern UiListRowCallback Gp_MainMenuCmds[6];

extern UiListRowCallback D_8010E850[1];

extern UiListRowCallback Gp_WeaponSlotRows[3];

extern UiListRowCallback D_8010E8A8[1];

extern UiListRowCallback D_8010E8D0[1];

extern UiListRowCallback D_8010E934[1];

extern UiListRowCallback D_8010E95C[1];

/// Menu task priorities and the equipped-bank stationary aiming clip.
enum {
    MENU_PENDING_ITEM_USE_TASK_PRIORITY = 32,
    MENU_GALLERY_WEAPON_HOLD_ANIMATION  = 9,
    ITEM_MENU_PANEL_TASK_PRIORITY       = 192
};

static void _itemMenuUpdatePromptTask(UiObject* object, Task* task);

u8       gItemMenuAddonUsedText[6]                                           = "used.";
u8       gItemMenuAddonCreatedText[9]                                        = "created.";
const u8 gItemMenuKeyItemRefusedText[21] __attribute__((section(".data")))   = "No use for this now.";
const u8 gItemMenuKeyItemUsedText[5] __attribute__((section(".data")))       = "Used";
char     Gp_StrSelectDest[]                                                  = "Select destination.";
u8       gItemMenuEquippedText[9]                                            = "Equipped";
u8       gItemPickupObtainedText[9]                                          = "Obtained";
u8       gItemMenuLoadedText[7]                                              = "Loaded";
u8       gItemMenuRemovedText[8]                                             = "Removed";
u8       gItemMenuRemovedAmmunitionText[20]                                  = "Removed ammunition.";
u8       gItemMenuInvokedText[8]                                             = "Invoked";
char     Gp_StrAmmoNone[]                                                    = "Ammunition: None";
char     Gp_StrAttachNone[]                                                  = "Attachments: None";
char     Gp_StrUse[]                                                         = "Use";
char     Gp_StrMove[]                                                        = "Move";
const u8 gItemMenuRemoveAmmunitionText[18] __attribute__((section(".data"))) = "Remove ammunition";
char     Gp_StrLoad[]                                                        = "Load";
char     Gp_StrExchange[]                                                    = "Exchange";
const u8 gItemMenuDetachArmorText[18] __attribute__((section(".data")))      = "Remove from armor";
/// Retained NUL-terminated Equip command text with no current reader.
static const u8 _gItemMenuEquipText[6] __attribute__((section(".data"))) = "Equip";
u8              gItemMenuYesText[4]                                      = "Yes";
u8              gItemMenuNoText[3]                                       = "No";
u8              gItemMenuOkText[3]                                       = "OK";
u8              gItemMenuCancelText[7]                                   = "Cancel";
u8              gItemPickupQuestionText[19]                              = "Pick up this item?";
u8              gItemPickupInventoryFullText[16]                         = "Inventory full.";
u8              gItemPickupInventoryFullSecondLineText[1]                = "";
char            Gp_StrSort[]                                             = "Sort";
u8              gItemMenuAmmoLabelText[5]                                = "AMMO";
u8              gItemMenuPeriodText[2]                                   = ".";
/// NUL-only text for properties with no displayed label in the specifications panel.
static u8 _gItemMenuEmptyPropertyText[] = "";
/// Armor-feature label with the status name colored orange by an inline text command.
static u8 _gItemMenuResistSilenceText[] = "RESIST \\CoSILENCE";
/// Armor-feature label with the status name colored orange by an inline text command.
static u8 _gItemMenuResistParalysisText[] = "RESIST \\CoPARALYSIS";
/// Armor-feature label with the status name colored orange by an inline text command.
static u8 _gItemMenuResistPoisonText[] = "RESIST \\CoPOISON";
/// Armor-feature label with the status name colored orange by an inline text command.
static u8 _gItemMenuResistConfusionText[] = "RESIST \\CoCONFUSION";
/// Armor-feature label with the status name colored orange by an inline text command.
static u8 _gItemMenuResistImpactText[] = "RESIST \\CoIMPACT";
/// NUL-terminated specifications label for the armor's motion-detector feature.
static u8 _gItemMenuMotionDetectorText[] = "MOTION DETECTOR";
/// NUL-terminated specifications label for the armor's MP-generation feature.
static u8 _gItemMenuMpGenerationText[] = "MP GENERATION";
/// NUL-terminated specifications label for the armor's HP-recovery feature.
static u8 _gItemMenuHpRecoveryText[] = "HP RECOVERY";
/// NUL-terminated specifications label for the armor's quick-fire feature.
static u8 _gItemMenuQuickFireText[] = "QUICK FIRE";
/// NUL-terminated specifications label for the armor's medical-inspection feature.
static u8 _gItemMenuMedicalInspectionText[] = "MEDICAL INSPECTION";
/// NUL-terminated specifications label for the armor's MP-recovery feature.
static u8 _gItemMenuMpRecoveryText[]     = "MP RECOVERY";
const u8* gItemMenuArmorFeatureTexts[13] = {
    _gItemMenuMotionDetectorText,
    _gItemMenuResistPoisonText,
    _gItemMenuMpGenerationText,
    _gItemMenuQuickFireText,
    _gItemMenuResistImpactText,
    _gItemMenuResistSilenceText,
    _gItemMenuMedicalInspectionText,
    _gItemMenuMpRecoveryText,
    _gItemMenuHpRecoveryText,
    _gItemMenuResistConfusionText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuResistParalysisText,
};

/// 9mm specifications caption with inline column, font, Y-offset and outline commands.
static u8 _gItemMenuCaliber9mmText[] = "CALIBER \\B9\\Sm\\D2\\W0 9mm";
/// .40 Magnum specifications caption with inline column, font, Y-offset and outline commands.
static u8 _gItemMenuCaliber40MagnumText[] = "CALIBER \\B9\\Sm\\D2\\W0 .40Mag.";
/// .44 Magnum specifications caption with inline column, font, Y-offset and outline commands.
static u8 _gItemMenuCaliber44MagnumText[] = "CALIBER \\B9\\Sm\\D2\\W0 .44Mag.";
/// 40mm specifications caption with inline column, font, Y-offset and outline commands.
static u8 _gItemMenuCaliber40mmText[] = "CALIBER \\B9\\Sm\\D2\\W0 40mm";
/// 12-gauge specifications caption with inline column, font, Y-offset and outline commands.
static u8 _gItemMenuGauge12Text[] = "GAUGE \\B9\\Sm\\D2\\W0 12ga.";
/// 5.56mm specifications caption with inline column, font, Y-offset and outline commands.
static u8 _gItemMenuCaliber556mmText[] = "CALIBER \\B9\\Sm\\D2\\W0 5.56mm";
const u8* gItemMenuCaliberTexts[16]    = {
    _gItemMenuEmptyPropertyText,
    _gItemMenuCaliber9mmText,
    _gItemMenuCaliber40MagnumText,
    _gItemMenuCaliber44MagnumText,
    _gItemMenuCaliber40mmText,
    _gItemMenuGauge12Text,
    _gItemMenuCaliber556mmText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
};

/// NUL-terminated ammunition specifications label for the poison special attribute.
static u8 _gItemMenuPoisonText[] = "POISON";
/// NUL-terminated ammunition specifications label for the incendiary special attribute.
static u8 _gItemMenuIncendiaryText[] = "INCENDIARY";
/// NUL-terminated ammunition specifications label for the piercing special attribute.
static u8 _gItemMenuPiercingText[] = "PIERCING";
/// NUL-terminated ammunition specifications label for the burst special attribute.
static u8 _gItemMenuBurstText[] = "BURST";
/// NUL-terminated ammunition specifications label for the flash special attribute.
static u8 _gItemMenuFlashText[] = "FLASH";
/// NUL-terminated ammunition specifications label for the explosion special attribute.
static u8 _gItemMenuExplosionText[]              = "EXPLOSION";
const u8* gItemMenuAmmoSpecialAttributeTexts[10] = {
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuPoisonText,
    _gItemMenuBurstText,
    _gItemMenuPiercingText,
    _gItemMenuExplosionText,
    _gItemMenuIncendiaryText,
    _gItemMenuEmptyPropertyText,
    _gItemMenuFlashText,
};

/// Bodyless task that resumes deferred item actions after menu resource restoration.
///
/// Menu exit acquires a display hold only when spawning succeeds; the task
/// releases that hold after consuming the pending actions, then exits.
static TaskDesc _gMenuPendingItemUseTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = MENU_PENDING_ITEM_USE_TASK_PRIORITY } },
    .callback = menuApplyPendingItemUseTask,
};

/// Restarts the equipped weapon's holding pose after gallery-menu equipment changes.
///
/// The bank seed is overwritten with the current character/weapon bank before
/// synchronous dispatch. Clip 9 resets playback and disables world collision;
/// the receiver consumes this mutable request synchronously.
static AnimationPlayRequest _gMenuGalleryWeaponHoldAnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = MENU_GALLERY_WEAPON_HOLD_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

UiListRowCallback Gp_MainMenuCmds[6] = { itemMenuDrawUseAttachCommandRow, itemMenuDrawKeyItemCommandRow, itemMenuDrawPeCommandRow, itemMenuDrawMapCommandRow, itemMenuDrawOptionsCommandRow, itemMenuDrawExitCommandRow };

UiList D_8010E820 = { Gp_MainMenuCmds, 6, { 6 }, 1, 8, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

MenuParasiteEnergyCaption gItemMenuParasiteEnergyCaptions[4] = {
    { .u = 16, .v = 88, .xOffset = 0 },
    { .u = 40, .v = 88, .xOffset = 0 },
    { .u = 72, .v = 88, .xOffset = 0 },
    { .u = 120, .v = 80, .xOffset = 0 },
};

UiListRowCallback D_8010E850[1] = { itemMenuDrawReorderableItemRow };

UiList D_8010E854 = { D_8010E850, 10, { 10 }, 1, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback Gp_WeaponSlotRows[3] = { itemMenuDrawEquippedWeaponRow, itemMenuDrawWeaponLoadRow, itemMenuDrawWeaponLoadRow };

UiList D_8010E884 = { Gp_WeaponSlotRows, 3, { 3 }, 0, 16, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E8A8[1] = { itemMenuDrawArmorAttachmentRow };

UiList D_8010E8AC = { D_8010E8A8, 1, { 1 }, 0, 16, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E8D0[1] = { itemMenuDrawAttachmentCandidateRow };

UiList D_8010E8D4 = { D_8010E8D0, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

s32 gItemMenuPreviewItemIds[ITEM_MENU_PREVIEW_SLOT_COUNT] = {
    ITEM_MENU_PREVIEW_EMPTY,
    ITEM_MENU_PREVIEW_EMPTY,
    ITEM_MENU_PREVIEW_EMPTY,
    ITEM_MENU_PREVIEW_EMPTY,
    ITEM_MENU_PREVIEW_EMPTY,
};

/// Shared row drawer for every line of the scrolling item-description list.
///
/// The list borrows this single entry for the gameplay overlay's lifetime;
/// description setup enables `USER_INTERFACE_LIST_SHARED_ROW_CALLBACK`.
static UiListRowCallback _gItemMenuDescriptionRowCallbacks[1] = { itemMenuDrawDescriptionRow };

UiList gItemMenuDescriptionList = {
    .rowCallbacks    = _gItemMenuDescriptionRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .rowHeight       = 15,
};

UiListRowCallback D_8010E934[1] = { itemMenuDrawKeyItemUseRow };

UiList D_8010E938 = { D_8010E934, 1, { 1 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback D_8010E95C[1] = { itemMenuDrawCollectedItemRow };

UiList D_8010E960 = { D_8010E95C, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

const u8* D_8010E984[3] = {
    gItemMenuWeaponRangeLabel,
    gItemMenuWeaponRateLabel,
    gItemMenuWeaponWeightLabel,
};

const u8* D_8010E990[1] = {
    gItemMenuAmmoPowerLabel,
};

const u8* D_8010E994[3] = {
    gItemMenuArmorHpBonusLabel,
    gItemMenuArmorMpBonusLabel,
    gItemMenuArmorAttachmentCountLabel,
};

/// Single row drawer borrowed by the weapon choice list.
///
/// Setup enables `USER_INTERFACE_LIST_SHARED_ROW_CALLBACK` before drawing;
/// entry zero handles every item for the gameplay overlay's lifetime.
static UiListRowCallback _gItemMenuWeaponChoiceRowCallbacks[1] = { itemMenuDrawWeaponChoiceRow };

UiList gItemMenuWeaponChoiceList = {
    .rowCallbacks    = _gItemMenuWeaponChoiceRowCallbacks,
    .itemCount       = 25,
    .visibleRowCount = { .unsignedValue = 25 },
    .rowHeight       = 14,
};

/// Single row drawer borrowed by the consumable choice list.
///
/// Setup enables `USER_INTERFACE_LIST_SHARED_ROW_CALLBACK` before drawing;
/// entry zero handles every item for the gameplay overlay's lifetime.
static UiListRowCallback _gItemMenuConsumableChoiceRowCallbacks[1] = { itemMenuDrawConsumableChoiceRow };

UiList gItemMenuConsumableChoiceList = {
    .rowCallbacks    = _gItemMenuConsumableChoiceRowCallbacks,
    .itemCount       = 3,
    .visibleRowCount = { .unsignedValue = 3 },
    .rowHeight       = 15,
};

/// Single row drawer borrowed by the armor choice list.
///
/// Setup enables `USER_INTERFACE_LIST_SHARED_ROW_CALLBACK` before drawing;
/// entry zero handles every item for the gameplay overlay's lifetime.
static UiListRowCallback _gItemMenuArmorChoiceRowCallbacks[1] = { itemMenuDrawArmorChoiceRow };

UiList D_8010E9F4 = { _gItemMenuArmorChoiceRowCallbacks, 1, { 1 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

UiListRowCallback Gp_ItemCmdFns[6] = {
    itemMenuDrawUseRow,
    itemMenuDrawMoveRow,
    itemMenuDrawMoveRow,
    itemMenuDrawMoveRow,
    itemMenuDrawDiscardRow,
    itemMenuDrawSortRow,
};

UiList D_8010EA30 = { Gp_ItemCmdFns, 3, { 3 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };

/// Retained six-word data with no reader; its purpose and units are unproven.
static s32 D_8010EA54[] = { 30, 60, 50, 100, 100, 200 };

UiListRowCallback gItemMenuDialogRowCallbacks[2] = {
    itemMenuDrawOkRow,
    itemMenuDrawOkRow,
};

UiList gItemMenuDialogList = {
    .rowCallbacks    = gItemMenuDialogRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .wrapNavigation  = 0,
    .rowHeight       = 15,
};

UiObjectDesc gItemMenuDialogDesc = {
    .style           = 0,
    .bounds          = { 0, 0, 48, 32 },
    .otIndexSeed     = 4,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = itemMenuDialogTask,
};

UiObjectDesc gItemMenuPanelDescriptors[ITEM_MENU_DESCRIPTOR_COUNT] = {
    [ITEM_MENU_DESCRIPTOR_CAPTION] = {
        .style           = 3,
        .bounds          = { -144, 64, 288, 40 },
        .otIndexSeed     = 768,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuCaptionTask,
    },
    [ITEM_MENU_DESCRIPTOR_MAIN_COMMANDS] = {
        .style           = 3,
        .bounds          = { -144, -104, 72, 64 },
        .otIndexSeed     = 28,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuMainPanelTask,
    },
    [ITEM_MENU_DESCRIPTOR_PLAYER_SUMMARY] = {
        .style           = 3,
        .bounds          = { -72, -104, 216, 65 },
        .otIndexSeed     = 40,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuPlayerSummaryTask,
    },
    [ITEM_MENU_DESCRIPTOR_WEAPON_SUMMARY] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, -39, 144, 72 },
        .otIndexSeed     = 36,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuWeaponSummaryTask,
    },
    [ITEM_MENU_DESCRIPTOR_INVENTORY_COUNT] = {
        .bounds          = { 0, 0, 144, 20 },
        .otIndexSeed     = 48,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuInventoryCountTask,
    },
    [ITEM_MENU_DESCRIPTOR_INVENTORY] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { 0, -104, 144, 165 },
        .otIndexSeed     = 40,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuInventoryPanelTask,
    },
    [ITEM_MENU_DESCRIPTOR_COLLECTED_ITEMS] = {
        .style           = 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, -104, 144, 120 },
        .otIndexSeed     = 40,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuCollectedItemsTask,
    },
    [ITEM_MENU_DESCRIPTOR_ARMOR_SUMMARY] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { 0, -39, 144, 120 },
        .otIndexSeed     = 512,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuArmorSummaryTask,
    },
    [ITEM_MENU_DESCRIPTOR_PARASITE_ENERGY_SUMMARY] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, 33, 144, 48 },
        .otIndexSeed     = 516,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuParasiteEnergySummaryTask,
    },
    [ITEM_MENU_DESCRIPTOR_PARASITE_ENERGY_LIST] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, -104, 200, 24 },
        .otIndexSeed     = 40,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuParasiteEnergyListTask,
    },
    [ITEM_MENU_DESCRIPTOR_EQUIPMENT_DETAIL] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -152, -104, 144, 104 },
        .otIndexSeed     = 816,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuEquipmentDetailTask,
    },
    [ITEM_MENU_DESCRIPTOR_ARMOR_SELECTION] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -8, 0, 144, 8 },
        .otIndexSeed     = 32,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuArmorSelectionTask,
    },
    [ITEM_MENU_DESCRIPTOR_AMMO_SELECTION] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -8, 0, 144, 8 },
        .otIndexSeed     = 32,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuAmmoSelectionTask,
    },
    [ITEM_MENU_DESCRIPTOR_WEAPON_SELECTION] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -8, 0, 144, 8 },
        .otIndexSeed     = 32,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuWeaponSelectionTask,
    },
    [ITEM_MENU_DESCRIPTOR_ATTACHMENT_PICKER] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -8, 0, 144, 8 },
        .otIndexSeed     = 32,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuAttachmentItemPickerTask,
    },
    [ITEM_MENU_DESCRIPTOR_ATTACH_NOTICE] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -60, -40, 144, 78 },
        .otIndexSeed     = 8,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuAttachNoticeTask,
    },
    [ITEM_MENU_DESCRIPTOR_ITEM_COMMANDS] = {
        .bounds          = { 0, 0, 48, 1 },
        .otIndexSeed     = 16,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuCommandTask,
    },
    [ITEM_MENU_DESCRIPTOR_ITEM_USE] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -100, -80, 198, 158 },
        .otIndexSeed     = 8,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuUseItemTask,
    },
    [ITEM_MENU_DESCRIPTOR_OPTIONS] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, -104, 288, 144 },
        .otIndexSeed     = 56,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = uiUpdateOptionsAfterLoadTask,
    },
    [ITEM_MENU_DESCRIPTOR_WEAPON_CHOICES] = {
        .bounds          = { -100, -80, 144, 78 },
        .otIndexSeed     = 12,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuWeaponChoiceListTask,
    },
    [ITEM_MENU_DESCRIPTOR_RELOAD_NOTICE] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -72, -40, 144, 78 },
        .otIndexSeed     = 8,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuReloadNoticeTask,
    },
    [ITEM_MENU_DESCRIPTOR_CONSUMABLE_CHOICES] = {
        .bounds          = { -100, -80, 144, 78 },
        .otIndexSeed     = 12,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuConsumableChoiceListTask,
    },
    [ITEM_MENU_DESCRIPTOR_EQUIP_NOTICE] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -60, -40, 144, 78 },
        .otIndexSeed     = 8,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuEquipNoticeTask,
    },
    [ITEM_MENU_DESCRIPTOR_KEY_ITEM_COMMANDS] = {
        .bounds          = { 0, 0, 48, 32 },
        .otIndexSeed     = 16,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuKeyItemCommandTask,
    },
    [ITEM_MENU_DESCRIPTOR_KEY_ITEM_USE] = {
        .style           = (s32)(USER_INTERFACE_PANEL_NO_FRAME | USER_INTERFACE_PANEL_TITLE_STYLE),
        .bounds          = { -90, -40, 178, 78 },
        .otIndexSeed     = 8,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuUseKeyItemTask,
    },
    [ITEM_MENU_DESCRIPTOR_INFORMATION] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, -104, 288, 208 },
        .otIndexSeed     = 8,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuInfoTask,
    },
    [ITEM_MENU_DESCRIPTOR_HOTSPOT_ITEM_CAPTION] = {
        .style           = 3,
        .bounds          = { -144, 64, 288, 40 },
        .otIndexSeed     = 56,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuCaptionTask,
    },
    [ITEM_MENU_DESCRIPTOR_WEAPON_DESTINATION] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, -104, 144, 72 },
        .otIndexSeed     = 960,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuWeaponPanelTask,
    },
    [ITEM_MENU_DESCRIPTOR_ARMOR_DESTINATION] = {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, -32, 144, 91 },
        .otIndexSeed     = 976,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemMenuArmorPanelTask,
    },
    [ITEM_MENU_DESCRIPTOR_PICKUP] = {
        .style           = (s32)USER_INTERFACE_PANEL_NO_FRAME,
        .bounds          = { -150, -80, 120, 70 },
        .otIndexSeed     = 60,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = ITEM_MENU_PANEL_TASK_PRIORITY,
        .contentCallback = itemPickupPanelTask,
    },
};

/// Clears and synchronizes the 320x240 VRAM region opposite the task framebuffer selector.
///
/// Requires frameBuffer 0/1, the standard regions at Y=0/272 and held flipping.
/// Menu entry waits for frameBuffer to equal drawBuffer before this clear.
/// Framebuffer and environment indices do not identify the same VRAM region.
static inline void _menuClearAlternateFrameBuffer(const DisplayState* display)
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
            gItemMenuRoomRestorePending = ITEM_MENU_ROOM_RESTORE_CLEAR;
            sndEvtRequestScriptDuckAcquire();
            itemMenuClearPreviewItems();
            gItemMenuInfoPanel                   = NULL;
            gItemMenuPreviousSecondaryItemId     = ITEM_MENU_ROOT_SECONDARY_ITEM_ABSENT;
            player                               = &gPlayerStatus;
            gItemMenuPreviousWeaponSelection     = player->weapon;
            gItemMenuPreviousWeaponSlotSelection = player->weaponSlotItem;
            if (player->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                gItemMenuPreviousSecondaryItemId = equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId;
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
            // Clear the alternate VRAM region before reusing the auxiliary heap.
            _menuClearAlternateFrameBuffer(display);
            memInitAuxHeap();
            if (display->demoScene != DISPLAY_DEMO_NONE) {
                display->gameMode = DISPLAY_GAME_RESTART;
                break;
            }
            menuRequest = task->spawnArg1.value;
            if (menuRequest == ITEM_MENU_ROOT_REQUEST_OPTIONS) {
                gUiPromptObject = NULL;
                cdCmdEnqueueDisplayResource(1, 0, CD_COMMAND_DISPLAY_LOAD_MENU);
                rootObject = uiSpawnObject(&gItemMenuPanelDescriptors[ITEM_MENU_ROOT_OPTIONS_DESCRIPTOR], 1, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ROOT_OPTIONS_OPEN_DELAY_TICKS, NULL);
            } else if (menuRequest == ITEM_MENU_ROOT_REQUEST_GALLERY_WEAPON) {
                rootObject = uiSpawnObject(&gMistShootingGalleryWeaponSelectPanelDesc, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ROOT_OPEN_DELAY_TICKS, NULL);
            } else if (menuRequest == DISPLAY_MODE_MAP) {
                rootObject = uiSpawnObject(&D_8010F140, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ROOT_MAP_OPEN_DELAY_TICKS, NULL);
            } else if (menuRequest == ITEM_MENU_ROOT_REQUEST_ATTACHMENTS) {
                display->keepGraphics = 1;
                rootObject            = uiSpawnObject(&gItemMenuArmorAttachmentPanelDesc, 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_ROOT_OPEN_DELAY_TICKS, NULL);
            } else {
                display->keepGraphics = 1;
                rootObject            = uiSpawnObject(gItemMenuPanelDescriptors, 0, USER_INTERFACE_PANEL_INACTIVE, ITEM_MENU_ROOT_INVENTORY_OPEN_DELAY_TICKS, NULL);
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
            if (gItemMenuRoomRestorePending == ITEM_MENU_ROOM_RESTORE_PENDING) {
                loadingRestoreViewImageAndEnqueueResources(1);
            }
            secondaryItemId = ITEM_MENU_ROOT_SECONDARY_ITEM_ABSENT;
            memInitAuxHeap();
            player = &gPlayerStatus;
            equipmentSyncPrimaryAttackSelector();
            if (player->weapon != PLAYER_STATUS_EQUIPMENT_NONE) {
                secondaryItemId = equipmentGetWeaponLoad(player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId;
            }
            if ((gItemMenuPreviousWeaponSelection == player->weapon) && (gItemMenuPreviousWeaponSlotSelection == player->weaponSlotItem) &&
                (gItemMenuPreviousSecondaryItemId == secondaryItemId)) {
                break;
            }
            // Remove old weapon effects before admitting the replacement resources.
            previousList = taskGetActiveList();
            defaultList  = &gTaskDefaultList;
            taskSetActiveList(defaultList);
            selectedWeapon             = player->weapon;
            previousWeapon             = (u8)gItemMenuPreviousWeaponSelection;
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
            if ((gItemMenuPreviousWeaponSelection != player->weapon) || (gItemMenuPreviousWeaponSlotSelection != player->weaponSlotItem) ||
                (gItemMenuPreviousSecondaryItemId != secondaryItemId)) {
                previousList = taskGetActiveList();
                taskSetActiveList(&gTaskDefaultList);
                // Rebuild the weapon bodies now; allocate their buffers after the view reload.
                gTaskDeferModelBufferAllocation = true;
                playerActorRestoreEquipment();
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    playerActorEnterAim(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ITEM_MENU_ROOT_WEAPON_AIM_BLEND_FRAMES);
                }
                if (task->spawnArg1.value == ITEM_MENU_ROOT_REQUEST_GALLERY_WEAPON) {
                    playerActorWriteWeaponAnimationBankIndex(&_gMenuGalleryWeaponHoldAnimationRequest.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gMenuGalleryWeaponHoldAnimationRequest, 0);
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
            if (taskSpawnFromTableOnDefaultList(&_gMenuPendingItemUseTaskDesc, 0, 0, 0) != NULL) {
                displayAcquireMenuHold();
            }
            gPadInputMenuUnlockUpdates = ITEM_MENU_ROOT_INPUT_DELAY_UPDATES;
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
            object->result  = childResult;
            gUiPromptObject = NULL;
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

const u16 gItemMenuIconPaletteCluts[12] = {
    getClut(240, 242), // No current selector; role unproven.
    getClut(240, 242), // Medicine and key items.
    getClut(224, 242), // Unidentified items and ammunition fallback.
    getClut(208, 242), // 9mm ammunition and compatible equipment.
    getClut(192, 242), // .44 Magnum ammunition and compatible weapons.
    getClut(176, 242), // 12-gauge ammunition and compatible weapons.
    getClut(160, 242), // 5.56mm ammunition and compatible equipment.
    getClut(144, 242), // 40mm ammunition and compatible weapons.
    getClut(0, 242),   // Battery-powered weapons.
    getClut(48, 242),  // Armor and other ordinary items.
    0,                 // No current selector; role unproven.
    0,                 // No current selector; role unproven.
};
