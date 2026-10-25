#include "stages/akropolis/rooms/mist_shooting_gallery.h"

#include "config.h"
#include "../../event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "mist_shooting_gallery_private.h"

#include "stages/akropolis/actors/shooting_gallery_pierce.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8018055c.h"
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the GTE flag word.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../../../shared/effects/glow_draw.h"
#include "../../../../shared/room/jukebox.h"

#define ROOM_PART declarations_mist_shooting_gallery
#include "assets/rooms/mist/shooting_gallery/declarations.inc"

// Relocated CAP file slots selected by commands 5..8 and 0x21..0x22.
enum {
    CAP_FILE_LOW_COMMANDS       = 1,
    CAP_FILE_HIGH_COMMANDS      = 3,
    CAP_TEXTURE_X_LOW_COMMANDS  = 0x300,
    CAP_TEXTURE_X_HIGH_COMMANDS = 0x2C0
};

// Key-item use requests carry an item ID and an unused second argument word.
// Colours of the mode-selection panels, packed with red in the low byte.
enum {
    MODE_TEXT_COLOR         = 0x606060,
    MODE_WARNING_TEXT_COLOR = 0x0D287F
};

static const char _gMistShootingGallerySupplyLevelCaption[];

static void _mistShootingGallerySetStoryBarrierLowered(s32 lowered);

static void _mistShootingGalleryJukeboxPanelTask(Task* task);
static void _mistShootingGalleryModeSplashTask(Task* task);
static void _mistShootingGalleryClearMovieTask(Task* task);
static void _mistShootingGalleryMovieSessionTask(Task* task);

static const char _gMistShootingGalleryJukeboxRow3HeavenSentKillerLabel[];
static const char _gMistShootingGalleryJukeboxRow2EagerForBloodLabel[];
static const char _gMistShootingGalleryJukeboxRow1CrazyKingLabel[];
static const char _gMistShootingGalleryJukeboxRow3CrawlingWasteEmperorLabel[];
static const char _gMistShootingGalleryJukeboxRow2PickUpTheGauntletLabel[];
static const char _gMistShootingGalleryJukeboxRow3HuntersMoonLabel[];
static const char _gMistShootingGalleryJukeboxRow2QuadrumanousLeaderLabel[];
static const char _gMistShootingGalleryJukeboxRow3GenicReactorLabel[];
static const char _gMistShootingGalleryJukeboxRow2RebelForcesLabel[];
static const char _gMistShootingGalleryJukeboxRow1YellowRainLabel[];
static const char _gMistShootingGalleryJukeboxRow3PickUpTheGauntletLabel[];
static const char _gMistShootingGalleryJukeboxRow2AmbushLabel[];
static const char _gMistShootingGalleryJukeboxRow4DarkVoiceOfTheHeartLabel[];
static const char _gMistShootingGalleryJukeboxRow3RequiemLabel[];
static const char _gMistShootingGalleryJukeboxRow2RockAndFireLabel[];
static const char _gMistShootingGalleryJukeboxRow1GhostTownLabel[];
static const char _gMistShootingGalleryJukeboxRow4SnooperLabel[];
static const char _gMistShootingGalleryJukeboxRow3WildHuntLabel[];
static const char _gMistShootingGalleryJukeboxRow2LightningOperationLabel[];
static const char _gMistShootingGalleryJukeboxRow1LAMazeLabel[];
static const char _gMistShootingGalleryJukeboxRow4GenicReactorLabel[];
static const char _gMistShootingGalleryJukeboxRow3MentalAgonyLabel[];
static const char _gMistShootingGalleryJukeboxRow2WishwashLabel[];
static const char _gMistShootingGalleryJukeboxRow1CurseYourFateLabel[];
static const char _gMistShootingGalleryJukeboxRow4KillingFieldLabel[];
static const char _gMistShootingGalleryJukeboxRow3FoolsParadiseLabel[];
static const char _gMistShootingGalleryJukeboxRow2GazingIntoTheVoidLabel[];
static const char _gMistShootingGalleryJukeboxRow1ManMadeNatureLabel[];
static const char _gMistShootingGalleryJukeboxRow4LazingAwayTheMorningLabel[];
static const char _gMistShootingGalleryJukeboxRow3OutOfPhase2Label[18];
static const char _gMistShootingGalleryJukeboxRow2TheVagrantsLabel[16];
static const char _gMistShootingGalleryJukeboxRow1TowerRendezvousLabel[20];
static s32        _mistShootingGalleryRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32        _mistShootingGalleryResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32        _mistShootingGalleryHandleRoomCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedSecondArg);
static s32        _mistShootingGalleryHandleRoomAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static void       _mistShootingGalleryResultPanelTask(Task* task);
static void       _mistShootingGalleryBonusPanelTask(Task* task);
static void       _mistShootingGalleryModeSelectPanelTask(Task* task);
static void       _mistShootingGalleryModeStatusPanelTask(Task* task);
static void       _mistShootingGalleryModeDataPanelTask(Task* task);
static void       _mistShootingGalleryCarryoverModeSessionTask(Task* task);
static void       _mistShootingGalleryModeRow(UiList* list, UiObject* object);
static void       _mistShootingGalleryModeHelpPanelTask(Task* task);
static void       _mistShootingGalleryCapPlaybackTask(Task* task);

static const char _gMistShootingGalleryRedTargetLabel[12];
static const char _gMistShootingGalleryBrownTargetLabel[16];
static const char _gMistShootingGalleryYellowTargetLabel[16];
static const char _gMistShootingGalleryWomanTargetLabel[8];
static const char _gMistShootingGalleryCrowTargetLabel[8];
static const char _gMistShootingGalleryBeeTargetLabel[4];
static const char _gMistShootingGallerySpiderTargetLabel[8];
static const char _gMistShootingGallerySnakeTargetLabel[8];
static const char _gMistShootingGalleryScorpionTargetLabel[12];
static const char _gMistShootingGalleryRatTargetLabel[4];
static const char _gMistShootingGalleryMonkeyTargetLabel[8];
static const char _gMistShootingGalleryBearTargetLabel[8];
static const char _gMistShootingGalleryBacteriumTargetLabel[12];
static void       _mistShootingGalleryWeaponRow(UiList* list, UiObject* object);
static void       _mistShootingGalleryWeaponSelectPanelTask(Task* task);

/// Three-line Replay description shown below the run-mode selector.
static char _gMistShootingGalleryReplayModeDescription[80] =
    "Replay mode\n"
    "Collect bonus items each time you\n"
    "clear the game in replay mode!";

/// Three-line Bounty description shown below the run-mode selector.
static char _gMistShootingGalleryBountyModeDescription[76] =
    "Bounty mode\n"
    "Find the hidden GOLEM soldiers\n"
    "and strive for a BP high score!";

/// Three-line Scavenger description shown below the run-mode selector.
static char _gMistShootingGalleryScavengerModeDescription[84] =
    "Scavenger mode\n"
    "Shops are bare, and pickings are\n"
    "slim. Use your wits, not your ammo!";

/// Three-line Nightmare description shown below the run-mode selector.
static char _gMistShootingGalleryNightmareModeDescription[84] =
    "Nightmare mode\n"
    "You start off sick and things get\n"
    "worse in this most difficult mode.";

/// NUL-terminated DATA-panel gauge text for rating level 1; each { glyph draws one mark.
static char _gMistShootingGalleryGaugeOneMarkText[4] = "{";

/// NUL-terminated DATA-panel gauge text for rating level 2; each { glyph draws one mark.
static char _gMistShootingGalleryGaugeTwoMarksText[4] = "{{";

/// NUL-terminated DATA-panel gauge text for rating level 3; each { glyph draws one mark.
static char _gMistShootingGalleryGaugeThreeMarksText[4] = "{{{";

/// NUL-terminated DATA-panel gauge text for rating level 4; each { glyph draws one mark.
static char _gMistShootingGalleryGaugeFourMarksText[8] = "{{{{";

/// NUL-terminated DATA-panel gauge text for rating level 5; each { glyph draws one mark.
static char _gMistShootingGalleryGaugeFiveMarksText[8] = "{{{{{";

// Inventory item IDs offered by this picker, rather than equipped-weapon selectors.
enum {
    WEAPON_ITEM_M93R     = 0x81,
    WEAPON_ITEM_M950     = 0x82,
    WEAPON_ITEM_MP5A5    = 0x9D,
    WEAPON_ITEM_M4A1     = 0x8F,
    WEAPON_ITEM_MONGOOSE = 0x88,
    WEAPON_ITEM_P229     = 0x84,
    WEAPON_ITEM_M249     = 0x90,
    WEAPON_ITEM_PA3      = 0x8C,
    WEAPON_ITEM_SP12     = 0x8D,
    WEAPON_ITEM_AS12     = 0x8E,
    WEAPON_COUNT         = 10,
};

/// Weapon-picker catalog in offer order, with no terminator.
static s16 _gMistShootingGalleryWeaponItemIds[WEAPON_COUNT] = {
    WEAPON_ITEM_M93R,
    WEAPON_ITEM_M950,
    WEAPON_ITEM_MP5A5,
    WEAPON_ITEM_M4A1,
    WEAPON_ITEM_MONGOOSE,
    WEAPON_ITEM_P229,
    WEAPON_ITEM_M249,
    WEAPON_ITEM_PA3,
    WEAPON_ITEM_SP12,
    WEAPON_ITEM_AS12,
};

/// Single row handler used for every owned weapon through shared-row dispatch.
static UiListRowCallback _gMistShootingGalleryWeaponRowCallbacks[1] = {
    _mistShootingGalleryWeaponRow,
};

/// Mutable singleton weapon-picker list with clamped navigation and 15-pixel rows.
static UiList _gMistShootingGalleryWeaponList = {
    .rowCallbacks    = _gMistShootingGalleryWeaponRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .rowHeight       = 15,
};

UiObjectDesc gMistShootingGalleryWeaponSelectPanelDesc = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -8, 0, 144, 64 },
    .otIndexSeed     = 32,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _mistShootingGalleryWeaponSelectPanelTask,
};

/// Bodyless priority-192 inventory-menu session used for the gallery weapon picker.
static TaskDesc _gMistShootingGalleryWeaponMenuTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = menuRootTask,
    .data     = { .value = 0 },
};

#include "assets/rooms/mist/shooting_gallery/scores.inc"
UiObjectDesc gMistShootingGalleryResultPanelDesc = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -104, -48, 208, 64 },
    .otIndexSeed     = 32,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _mistShootingGalleryResultPanelTask,
};

/// BONUS child-panel recipe for a course's total score and earned BP.
static UiObjectDesc _gMistShootingGalleryBonusPanelDesc = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -72, -48, 144, 56 },
    .otIndexSeed     = 24,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _mistShootingGalleryBonusPanelTask,
};

/// Single row handler shared by every run mode; initialization enables shared dispatch.
static UiListRowCallback _gMistShootingGalleryModeRowCallbacks[1] = {
    _mistShootingGalleryModeRow,
};

/// Mutable singleton list for carryover run modes 0..3, with wrapping navigation.
static UiList _gMistShootingGalleryModeList = {
    .rowCallbacks    = _gMistShootingGalleryModeRowCallbacks,
    .itemCount       = 4,
    .visibleRowCount = { .unsignedValue = 4 },
    .wrapNavigation  = 1,
    .rowHeight       = 15,
};

/// SELECT panel recipe for carryover run modes, with STATUS, help and DATA children.
static UiObjectDesc _gMistShootingGalleryModeSelectPanelDesc = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -96, 176, 64 },
    .otIndexSeed     = 32,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _mistShootingGalleryModeSelectPanelTask,
};

enum { MODE_HELP_UNTITLED_STYLE = 3 };

/// Carryover selector child recipes in STATUS, untitled help, DATA order.
static UiObjectDesc _gMistShootingGalleryModeChildPanelDescs[3] = {
    {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { 32, -96, 112, 128 },
        .otIndexSeed     = 28,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = 192,
        .contentCallback = _mistShootingGalleryModeStatusPanelTask,
    },
    {
        .style           = MODE_HELP_UNTITLED_STYLE,
        .bounds          = { -144, 32, 288, 48 },
        .otIndexSeed     = 24,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = 192,
        .contentCallback = _mistShootingGalleryModeHelpPanelTask,
    },
    {
        .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
        .bounds          = { -144, -22, 288, 73 },
        .otIndexSeed     = 20,
        .taskFlags       = TASK_BODY_NONE,
        .taskPriority    = 192,
        .contentCallback = _mistShootingGalleryModeDataPanelTask,
    },
};

/// Bodyless priority-192 display session for selecting and applying run-mode carryover.
static TaskDesc _gMistShootingGalleryCarryoverModeSessionDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _mistShootingGalleryCarryoverModeSessionTask,
    .data     = { .value = 0 },
};

/// Bodyless priority-192 task recipe for the gallery's room CAP commands.
static TaskDesc _gMistShootingGalleryCapPlaybackTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _mistShootingGalleryCapPlaybackTask,
    .data     = { .value = 0 },
};

/// Borrowed room-task message dispatch, terminated by TASK_MESSAGE_TABLE_END.
static TaskMessageEntry _gMistShootingGalleryRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mistShootingGalleryResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mistShootingGalleryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mistShootingGalleryHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _mistShootingGalleryHandleRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#define ROOM_PART _gMistShootingGalleryStoryBarrierCollisionTemplateNormals
#include "assets/rooms/mist/shooting_gallery/geometry.inc"
#define ROOM_PART _gMistShootingGalleryJukeboxTrainingReplayTracks
#include "assets/rooms/mist/shooting_gallery/jukebox.inc"
/// Single jukebox row handler; shared dispatch applies it to all three or four tracks.
static UiListRowCallback _gMistShootingGalleryJukeboxRowCallbacks[1] = {
    _jukeboxDrawRow,
};

/// Mutable singleton jukebox list with clamped navigation and 17-pixel rows.
static UiList _gMistShootingGalleryJukeboxList = {
    .rowCallbacks    = _gMistShootingGalleryJukeboxRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .rowHeight       = 17,
};

/// Recipe for this overlay's SELECT jukebox panel and track-picker task.
static UiObjectDesc _gJukeboxPanelDesc = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -112, -64, 224, 128 },
    .otIndexSeed     = 48,
    .field_E         = 0,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _mistShootingGalleryJukeboxPanelTask,
    .taskDataValue   = 0,
};

/// Bodyless priority-192 display session for this gallery's jukebox panel.
static TaskDesc _gMistShootingGalleryJukeboxSessionDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _jukeboxHostTask,
    .data     = { .value = 0 },
};

TaskDesc gMistShootingGalleryMovieTaskDescs[MIST_SHOOTING_GALLERY_MOVIE_TASK_COUNT] = {
    {
        .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
        .callback = _mistShootingGalleryMovieSessionTask,
        .data     = { .value = 0 },
    },
    {
        .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
        .callback = _mistShootingGalleryClearMovieTask,
        .data     = { .value = 0 },
    },
    {
        .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
        .callback = _mistShootingGalleryModeSplashTask,
        .data     = { .value = 0 },
    },
};
#include "assets/rooms/mist/shooting_gallery/header.inc"
#include "assets/rooms/mist/shooting_gallery/markers.inc"

static void _mistShootingGalleryInitializeRoomTask(Task* task);
static void _mistShootingGalleryUpdateRoomTask(Task* unusedTask);

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

void mistShootingGalleryPrepareTrainingLoadout(s32 courseLevel)
{
    enum {
        GPS_ITEM              = 0x40,
        RECOVERY1_ITEM        = 1,
        COLA_ITEM             = 5,
        MP_BOOST1_ITEM        = 6,
        COMBUSTION_INDEX      = 1,
        GPS_ATTACHMENT_SLOT   = 2,
        COLA_ATTACHMENT_SLOT1 = 3,
        COLA_ATTACHMENT_SLOT2 = 4
    };
    InventoryItemRange* carriedInventory;
    s32                 spellRow;
    s32                 spellColumn;

    carriedInventory = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    // Reset the twelve spell entries; the other six debug entries remain untouched.
    for (spellRow = 0; spellRow < ATTACHMENT_SPELL_COUNT / 3; spellRow++) {
        for (spellColumn = 0; spellColumn < 3; spellColumn++) {
            gAttachmentTrainingLevels[spellColumn + spellRow * 3] = 0;
        }
    }
    gAttachmentTrainingLevels[ATTACHMENT_INDEX_PYROKINESIS] = 1;

    switch (courseLevel) {
        case 1:
        case 2:
            break;
        case 3:
            itemSetIdentified(GPS_ITEM, 1);
            inventoryGiveItem(carriedInventory, GPS_ITEM, 1);
            break;
        case 4:
            itemSetIdentified(GPS_ITEM, 1);
            itemSetIdentified(COLA_ITEM, 1);
            inventoryGiveItem(carriedInventory, GPS_ITEM, 1)->attachSlot  = GPS_ATTACHMENT_SLOT;
            inventoryGiveItem(carriedInventory, COLA_ITEM, 1)->attachSlot = COLA_ATTACHMENT_SLOT1;
            inventoryGiveItem(carriedInventory, COLA_ITEM, 1)->attachSlot = COLA_ATTACHMENT_SLOT2;
            gAttachmentTrainingLevels[ATTACHMENT_INDEX_ENERGY_SHOT]       = 1;
            gAttachmentTrainingLevels[COMBUSTION_INDEX]                   = 1;
            break;
        case 5:
            itemSetIdentified(GPS_ITEM, 1);
            itemSetIdentified(RECOVERY1_ITEM, 1);
            itemSetIdentified(MP_BOOST1_ITEM, 1);
            inventoryGiveItem(carriedInventory, GPS_ITEM, 1);
            inventoryGiveItem(carriedInventory, RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MP_BOOST1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MP_BOOST1_ITEM, 1);
            gAttachmentTrainingLevels[ATTACHMENT_INDEX_ENERGY_SHOT] = 1;
            gAttachmentTrainingLevels[COMBUSTION_INDEX]             = 1;
            break;
    }
    equipmentRestoreHpMp();
    equipmentInitializeWeaponSupplies();
}
/// Draws an owned weapon row and handles its training-loadout and help commands.
static void _mistShootingGalleryWeaponRow(UiList* list, UiObject* object)
{
    enum {
        TRAINING_ARMOR_ITEM = 0x6C,
        TRAINING_AMMO_UNITS = 999,
        WEAPON_HELP_PANEL   = 45,
    };
    s32                               weaponItemId;
    s32                               weaponTableIndex;
    s32                               ownedRowsToSkip;
    s32                               panelControl;
    s32                               rowInputMode;
    s32                               ammoItemId;
    const EquipmentWeaponLoadOptions* loadOptions;
    u8*                               equippedWeapon;
    InventoryItemRange*               carriedInventory;

    // Map the displayed row through the owned entries of the weapon catalog.
    weaponItemId     = 0;
    ownedRowsToSkip  = list->currentItemIndex;
    weaponTableIndex = 0;
    do {
        if (inventoryIsItemLimitReached(_gMistShootingGalleryWeaponItemIds[weaponTableIndex]) != 0) {
            ownedRowsToSkip--;
            if (ownedRowsToSkip < 0) {
                weaponItemId = _gMistShootingGalleryWeaponItemIds[weaponTableIndex];
                break;
            }
        }
        weaponTableIndex++;
    } while (weaponTableIndex < (s32)ARRAY_SIZE(_gMistShootingGalleryWeaponItemIds));

    itemMenuDrawItemRow(object, list->rowTextX.signedValue, list->rowTextY.signedValue, weaponItemId, list->colorRgb, 0);
    panelControl = object->panel.control.word;
    if (((panelControl >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            itemMenuSetPreviewItem(weaponItemId, CD_COMMAND_DISPLAY_LOAD_MENU);
        }
    }
    rowInputMode = list->rowInputEnabled;
    if (rowInputMode == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
            carriedInventory = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            equippedWeapon   = &gPlayerStatus.weapon;
            loadOptions      = &gEquipmentWeaponPrimaryLoadOptions.rows[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
            ammoItemId       = loadOptions->acceptedItemIds[0];
            *equippedWeapon  = weaponItemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1);
            // Replace the training loadout and attach its ammunition in slot one.
            inventoryResetCarriedRange();
            inventoryClearItems(carriedInventory);
            inventoryGiveItem(carriedInventory, weaponItemId, 1);
            inventoryGiveItem(carriedInventory, TRAINING_ARMOR_ITEM, 1);
            equipmentEquipCarriedArmor(TRAINING_ARMOR_ITEM);
            inventoryGiveItem(carriedInventory, ammoItemId, TRAINING_AMMO_UNITS)->attachSlot = rowInputMode;
            equipmentLoadWeaponConsumable(carriedInventory, weaponItemId, ammoItemId, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY);
            equipmentRestoreHpMp();
            object->result = USER_INTERFACE_RESULT_CONFIRM;
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&gItemMenuPanelDescriptors[WEAPON_HELP_PANEL], weaponItemId, 1, 1, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}
/// NUL-terminated title of the gallery weapon picker, stored in eight rodata bytes.
static const char _gMistShootingGalleryWeaponSelectCaption[8] = "Select";
/// RESULT-panel label for red target kind 12 (600 points per kill).
static const char _gMistShootingGalleryRedTargetLabel[12] = "Red Target";
/// RESULT-panel label for brown target kind 11 (400 points per kill).
static const char _gMistShootingGalleryBrownTargetLabel[16] = "Brown Target";
/// RESULT-panel label for yellow target kind 10 (200 points per kill).
static const char _gMistShootingGalleryYellowTargetLabel[16] = "Yellow Target";
/// RESULT-panel label for civilian target kind 9 (3000-point penalty per kill).
static const char _gMistShootingGalleryWomanTargetLabel[8] = "Woman";
/// RESULT-panel label for crow target kind 8 (600 points per kill).
static const char _gMistShootingGalleryCrowTargetLabel[8] = "Crow";
/// RESULT-panel label for bee target kind 7 (3000 points per kill).
static const char _gMistShootingGalleryBeeTargetLabel[4] = "Bee";
/// RESULT-panel label for spider target kind 6 (1300 points per kill).
static const char _gMistShootingGallerySpiderTargetLabel[8] = "Spider";
/// RESULT-panel label for snake target kind 5 (500 points per kill).
static const char _gMistShootingGallerySnakeTargetLabel[8] = "Snake";
/// RESULT-panel label for scorpion target kind 4 (2400 points per kill).
static const char _gMistShootingGalleryScorpionTargetLabel[12] = "Scorpion";
/// RESULT-panel label for rat target kind 3 (500 points per kill).
static const char _gMistShootingGalleryRatTargetLabel[4] = "Rat";
/// RESULT-panel label for monkey target kind 2 (900 points per kill).
static const char _gMistShootingGalleryMonkeyTargetLabel[8] = "Monkey";
/// RESULT-panel label for bear target kind 1 (1600 points per kill).
static const char _gMistShootingGalleryBearTargetLabel[8] = "Bear";
/// RESULT-panel label for bacterium target kind 0 (600 points per kill).
static const char _gMistShootingGalleryBacteriumTargetLabel[12] = "Bacterium";

/// Updates the centered weapon picker and returns input from its item-help child.
static void _mistShootingGalleryWeaponSelectPanelTask(Task* task)
{
    enum { WEAPON_MAX_VISIBLE_ROWS = 10 };
    UiObject*  object;
    UiList*    list;
    Task*      child;
    UiObject*  childObject;
    const s16* weaponItem;
    s32        weaponIndex;
    s32        ownedWeaponCount;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    list           = &_gMistShootingGalleryWeaponList;
    uiDrawPanelLabel(&object->panel, _gMistShootingGalleryWeaponSelectCaption);
    if (task->state == 0) {
        ownedWeaponCount = 0;
        weaponIndex      = ownedWeaponCount;
        weaponItem       = _gMistShootingGalleryWeaponItemIds;
        do {
            if (inventoryIsItemLimitReached(*weaponItem) != 0) {
                itemSetIdentified(*weaponItem, 1);
                ownedWeaponCount += 1;
            }
            weaponIndex++;
            weaponItem++;
        } while (weaponIndex < (s32)ARRAY_SIZE(_gMistShootingGalleryWeaponItemIds));

        list->itemCount = ownedWeaponCount;
        if ((u8)ownedWeaponCount > WEAPON_MAX_VISIBLE_ROWS) {
            list->visibleRowCount.unsignedValue = WEAPON_MAX_VISIBLE_ROWS;
        } else {
            list->visibleRowCount.unsignedValue = ownedWeaponCount;
        }
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        uiFitPanelToList(list, &(object)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        object->panel.bounds.unsignedRect.x = -((s16)object->panel.bounds.unsignedRect.w / 2);
        object->panel.bounds.unsignedRect.y = -((s16)object->panel.bounds.unsignedRect.h / 2);
        task->state                        += 1;
    }
    uiUpdateList(list, &object->panel);
    child = task->firstChild;
    if (child != NULL) {
        childObject = child->spawnArg2.pointer;
        if (childObject->result == USER_INTERFACE_RESULT_CANCEL || childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(childObject, childObject->owner);
            object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
        }
    }
}
/// NUL-terminated text stream labeling the total in both RESULT and BONUS panels.
static const u8 _gMistShootingGalleryTotalScoreCaption[] = "TOTAL SCORE";

/// Draws course kill scores and records its first earned BP/prize result.
static void _mistShootingGalleryResultPanelTask(Task* task)
{
    enum {
        INIT       = 0,
        SUPPRESSED = 0x100,
        TEXT_COLOR = 0x606060,
    };
    u8                       numberText[0x20];
    MistShootingGalleryWork* work;
    s32                      rowCount;
    s32                      totalScore;
    UiObject*                object;
    s32                      targetKind;
    s32                      killCount;
    s32                      pointsPerKill;
    s32                      kindScore;
    s32                      contentInset;
    s32                      rowY;
    s32                      panelControl;
    s32                      panelPhase;
    s32                      bonusBp;
    s32                      prizeFlag;
    UiObject*                bonusObject;
    s32                      bonusResult;
    s32                      totalCaptionBaseline;
    s32                      totalValueBaseline;
    Task*                    bonusTask;

    /// Sets a result line's color, font and draw style above its owning panel.
#define SET_RESULT_TEXT_STYLE(request, panelObject, glyph, align, mode)      \
    {                                                                        \
        (request).otIndex    = (panelObject)->panel.otIndex.signedValue + 1; \
        (request).colorRgb   = TEXT_COLOR;                                   \
        (request).glyphTable = (glyph);                                      \
        (request).alignment  = (align);                                      \
        (request).drawMode   = (mode);                                       \
    }

    targetKind   = 0;
    rowCount     = 0;
    totalScore   = 0;
    object       = task->spawnArg2.pointer;
    work         = gMistShootingGalleryControllerTask->work;
    contentInset = object->panel.contentLeft.signedValue + 2;
    rowY         = object->panel.contentTop.signedValue + 0x17;
    {
        TextDrawReq nameText;
        TextDrawReq pointsText;
        TextDrawReq killsText;
        TextDrawReq subtotalText;

        // Only destroyed target kinds get rows; civilian kills deduct points.
        do {
            killCount = work->kills[targetKind];
            if (killCount > 0) {
                pointsPerKill = _gMistShootingGalleryTargetScores[targetKind].points;
                rowCount     += 1;

                nameText.x = object->panel.contentOriginX.unsignedValue + contentInset;
                nameText.y = object->panel.contentOriginY.unsignedValue + rowY;
                SET_RESULT_TEXT_STYLE(nameText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
                textDrawString(&nameText, _gMistShootingGalleryTargetScores[targetKind].name);

                pointsText.x = object->panel.contentOriginX.unsignedValue + 0x6E + contentInset;
                pointsText.y = object->panel.contentOriginY.unsignedValue + rowY;
                SET_RESULT_TEXT_STYLE(pointsText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
                textDrawString(&pointsText, textItoaSigned(numberText, pointsPerKill));

                killsText.x = object->panel.contentOriginX.unsignedValue + 0x91 + contentInset;
                killsText.y = object->panel.contentOriginY.unsignedValue + rowY;
                SET_RESULT_TEXT_STYLE(killsText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
                textDrawString(&killsText, textItoaSigned(numberText, killCount));
                kindScore = killCount * pointsPerKill;

                subtotalText.x = object->panel.contentOriginX.unsignedValue - 5 - contentInset;
                subtotalText.y = object->panel.contentOriginY.unsignedValue + rowY;
                SET_RESULT_TEXT_STYLE(subtotalText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
                textDrawString(&subtotalText, textItoaSigned(numberText, kindScore));
                totalScore += kindScore;
                rowY       += 0xB;
            }
            targetKind += 1;
        } while (targetKind < MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT);
    }

    {
        TextDrawReq totalCaption;
        TextDrawReq totalText;
        TextDrawReq nameHeading;
        TextDrawReq pointsHeading;
        TextDrawReq killsHeading;
        TextDrawReq subtotalHeading;

        uiDrawHorizontalSeparator(&object->panel, contentInset, -contentInset, object->panel.contentBottom.signedValue - 0xE);

        totalCaption.x       = object->panel.contentOriginX.unsignedValue + 0x78 + contentInset;
        totalCaptionBaseline = object->panel.contentOriginY.unsignedValue - 6;
        totalCaption.y       = object->panel.contentBottom.unsignedValue + totalCaptionBaseline;
        SET_RESULT_TEXT_STYLE(totalCaption, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
        textDrawString(&totalCaption, _gMistShootingGalleryTotalScoreCaption);

        totalText.x        = object->panel.contentOriginX.unsignedValue - 5 - contentInset;
        totalValueBaseline = object->panel.contentOriginY.unsignedValue - 4;
        totalText.y        = object->panel.contentBottom.unsignedValue + totalValueBaseline;
        SET_RESULT_TEXT_STYLE(totalText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&totalText, textItoaSigned(numberText, totalScore));

        uiDrawHorizontalSeparator(&object->panel, contentInset, -contentInset, object->panel.contentTop.signedValue + 0xA);

        rowY          = object->panel.contentTop.signedValue + 6;
        nameHeading.x = object->panel.contentOriginX.unsignedValue + 0x1E + contentInset;
        nameHeading.y = object->panel.contentOriginY.unsignedValue + rowY;
        SET_RESULT_TEXT_STYLE(nameHeading, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_CENTER, TEXT_DRAW_OUTLINED);
        textDrawString(&nameHeading, "NMC");

        pointsHeading.x = object->panel.contentOriginX.unsignedValue + 0x73 + contentInset;
        pointsHeading.y = object->panel.contentOriginY.unsignedValue + rowY;
        SET_RESULT_TEXT_STYLE(pointsHeading, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
        textDrawString(&pointsHeading, "SCORE");

        killsHeading.x = object->panel.contentOriginX.unsignedValue + 0x96 + contentInset;
        killsHeading.y = object->panel.contentOriginY.unsignedValue + rowY;
        SET_RESULT_TEXT_STYLE(killsHeading, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
        textDrawString(&killsHeading, "KILL");

        subtotalHeading.x = object->panel.contentOriginX.unsignedValue - contentInset;
        subtotalHeading.y = object->panel.contentOriginY.unsignedValue + rowY;
        SET_RESULT_TEXT_STYLE(subtotalHeading, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
        textDrawString(&subtotalHeading, "TOTAL");
    }

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, "Result");

    if (task->state == INIT) {
        if (gGameSession->battleResetPending == 1) {
            uiStartPanelHiding(object, object->owner);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
            task->state    = SUPPRESSED;
            return;
        }
        gGameSession->battleResetPending = 1;
        uiSetPanelContentSize(&object->panel, 0, (rowCount * 0xB) + 0x21);
        object->panel.bounds.unsignedRect.y = -((s16)object->panel.bounds.unsignedRect.h / 2);
        task->state                         = task->state + 1;
    }

    // Record a course reward once, then let the bonus child credit its BP.
    panelControl = object->panel.control.word;
    if ((panelControl == USER_INTERFACE_PANEL_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0)) {
        bonusBp    = mistShootingGalleryGetBonusBp(totalScore);
        panelPhase = task->state;
        if (panelPhase == panelControl) {
            if (bonusBp > 0) {
                prizeFlag = work->course + EVENT_FLAG_MIST_SHOOTING_GALLERY_PRIZE_FIRST;
                if (gameFlagGetNibble(prizeFlag) == 0) {
                    if (mistShootingGalleryQualifiesForPrize(bonusBp) == panelPhase) {
                        gameFlagSetNibble(prizeFlag, MIST_SHOOTING_GALLERY_PRIZE_WAITING);
                    } else {
                        gameFlagSetNibble(prizeFlag, MIST_SHOOTING_GALLERY_PRIZE_BP_ONLY);
                    }
                    uiSpawnObject(&_gMistShootingGalleryBonusPanelDesc, totalScore, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                    object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                } else {
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                }
            } else {
                object->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        } else {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }

    bonusTask = task->firstChild;
    if (bonusTask != NULL) {
        bonusObject = bonusTask->spawnArg2.pointer;
        bonusResult = bonusObject->result;
        if (bonusResult == USER_INTERFACE_RESULT_CONFIRM) {
            object->result = bonusResult;
        }
    }
#undef SET_RESULT_TEXT_STYLE
}

/// Credits the course's bonus BP once and draws its score and reward panel.
static void _mistShootingGalleryBonusPanelTask(Task* task)
{
    enum {
        INIT        = 0,
        BP_MAX      = 999999,
        TEXT_COLOR  = 0x606060,
        VALUE_COLOR = 0x037A78,
    };
    u8            numberText[0x20];
    TextDrawReq   scoreCaption;
    TextDrawReq   scoreText;
    TextDrawReq   bonusCaption;
    TextDrawReq   bonusText;
    UiObject*     object;
    s32           score;
    s32           bonusBp;
    s32           newBp;
    s32           contentInset;
    s32           contentTop;
    s32           rowY;
    s32           captionColor;
    PlayerStatus* player;

    object = task->spawnArg2.pointer;
    score  = task->spawnArg1.value;

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, "BONUS");
    // Credit the reward only on the first tick; later ticks redraw the amount.
    if (task->state == INIT) {
        bonusBp = mistShootingGalleryGetBonusBp(score);
        player  = &gPlayerStatus;
        if (bonusBp > 0) {
            newBp      = player->bp + bonusBp;
            player->bp = newBp;
            if (newBp > BP_MAX) {
                player->bp = BP_MAX;
            }
        }
        task->state = task->state + 1;
    }

    captionColor = TEXT_COLOR;
    contentInset = object->panel.contentLeft.signedValue + 2;
    contentTop   = object->panel.contentTop.signedValue;
    rowY         = contentTop + 0xB;

    scoreCaption.x          = object->panel.contentOriginX.unsignedValue + contentInset;
    scoreCaption.y          = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
    scoreCaption.otIndex    = object->panel.otIndex.signedValue + 1;
    scoreCaption.colorRgb   = captionColor;
    scoreCaption.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    scoreCaption.alignment  = TEXT_ALIGNMENT_LEFT;
    scoreCaption.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&scoreCaption, _gMistShootingGalleryTotalScoreCaption);

    scoreText.x          = object->panel.contentOriginX.unsignedValue - contentInset;
    scoreText.y          = object->panel.contentOriginY.unsignedValue + rowY;
    scoreText.otIndex    = object->panel.otIndex.signedValue + 1;
    scoreText.colorRgb   = captionColor;
    scoreText.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    scoreText.alignment  = TEXT_ALIGNMENT_RIGHT;
    scoreText.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&scoreText, textItoaSigned(numberText, score));

    uiDrawHorizontalSeparator(&object->panel, contentInset, -contentInset, contentTop + 0x1B);

    rowY                    = contentTop + 0x25;
    bonusCaption.x          = object->panel.contentOriginX.unsignedValue + contentInset;
    bonusCaption.y          = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
    bonusCaption.otIndex    = object->panel.otIndex.signedValue + 1;
    bonusCaption.colorRgb   = captionColor;
    bonusCaption.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    bonusCaption.alignment  = TEXT_ALIGNMENT_LEFT;
    bonusCaption.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&bonusCaption, "BONUS BP");

    bonusText.x          = object->panel.contentOriginX.unsignedValue - contentInset;
    bonusText.y          = object->panel.contentOriginY.unsignedValue + rowY;
    bonusText.otIndex    = object->panel.otIndex.signedValue + 1;
    bonusText.colorRgb   = VALUE_COLOR;
    bonusText.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    bonusText.alignment  = TEXT_ALIGNMENT_RIGHT;
    bonusText.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&bonusText, textItoaSigned(numberText, mistShootingGalleryGetBonusBp(score)));

    if ((object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0)) {
        object->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}
/// Replay label in the carryover run-mode selector.
static const char _gMistShootingGalleryReplayModeLabel[] = "Replay Mode";

/// Bounty label in the carryover run-mode selector.
static const char _gMistShootingGalleryBountyModeLabel[] = "Bounty Mode";

/// Scavenger label in the carryover run-mode selector.
static const char _gMistShootingGalleryScavengerModeLabel[] = "Scavenger Mode";

/// Nightmare label in the carryover run-mode selector.
static const char _gMistShootingGalleryNightmareModeLabel[] = "Nightmare Mode";

/// Menu labels indexed by run mode (0 Replay, 1 Bounty, 2 Scavenger, 3 Nightmare).
static const _MistShootingGalleryModeTexts _gMistShootingGalleryModeNames = { { _gMistShootingGalleryReplayModeLabel, _gMistShootingGalleryBountyModeLabel, _gMistShootingGalleryScavengerModeLabel, _gMistShootingGalleryNightmareModeLabel } };

/// Offers the unlocked carryover modes with status, help and difficulty children.
static void _mistShootingGalleryModeSelectPanelTask(Task* task)
{
    enum {
        SELECT_INIT           = 0,
        COUNT_BASE            = 2,
        COUNT_RANK_ONE        = 3,
        COUNT_ALL             = 4,
        UNLOCK_NIGHTMARE_RANK = 2,
        DEMO_SCENE            = 1,
        STATUS_PANEL          = 0,
        HELP_PANEL            = 1,
        DATA_PANEL            = 2,
    };
    UiObject* object = task->spawnArg2.pointer;
    UiList*   list   = &_gMistShootingGalleryModeList;

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, "SELECT");
    // Replay rank unlocks the extra rows; the demo exposes every mode.
    if (task->state == SELECT_INIT) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.replayRank == 0) {
            list->itemCount                     = COUNT_BASE;
            list->visibleRowCount.unsignedValue = COUNT_BASE;
        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.replayRank < UNLOCK_NIGHTMARE_RANK) {
            list->itemCount                     = COUNT_RANK_ONE;
            list->visibleRowCount.unsignedValue = COUNT_RANK_ONE;
        } else {
            list->itemCount                     = COUNT_ALL;
            list->visibleRowCount.unsignedValue = COUNT_ALL;
        }
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == DEMO_SCENE) {
            list->itemCount                     = COUNT_ALL;
            list->visibleRowCount.unsignedValue = COUNT_ALL;
        }
        list->selectedItemIndex = 0;
        uiFitPanelToList(list, &object->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSpawnObject(&_gMistShootingGalleryModeChildPanelDescs[STATUS_PANEL], 0, 0, 1, object);
        uiSpawnObject(&_gMistShootingGalleryModeChildPanelDescs[HELP_PANEL], 0, 0, 1, object);
        uiSpawnObject(&_gMistShootingGalleryModeChildPanelDescs[DATA_PANEL], 0, 0, 1, object);
        task->state = task->state + 1;
    }
    uiUpdateList(list, &object->panel);
    if ((object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        object->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}
/// Converts an accumulated EXP or BP total to the selected run mode's carryover.
static inline s32 _mistShootingGalleryScaleReward(s32 unscaledTotal)
{
    enum {
        REWARD_MODE_BOUNTY       = 1,
        REWARD_MODE_SCAVENGER    = 2,
        REWARD_MODE_NIGHTMARE    = 3,
        REPLAY_REWARD_DIVISOR    = 10,
        BOUNTY_REWARD_DIVISOR    = 20,
        SCAVENGER_REWARD_DIVISOR = 100,
        CARRYOVER_REWARD_MAX     = 999999
    };
    s32 reward;

    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
        case REWARD_MODE_NIGHTMARE:
            return 0;
        case REWARD_MODE_SCAVENGER:
            reward = unscaledTotal / SCAVENGER_REWARD_DIVISOR;
            break;
        case REWARD_MODE_BOUNTY:
            reward = unscaledTotal / BOUNTY_REWARD_DIVISOR;
            break;
        default:
            reward = unscaledTotal / REPLAY_REWARD_DIVISOR;
            break;
    }
    if (reward > CARRYOVER_REWARD_MAX) {
        reward = CARRYOVER_REWARD_MAX;
    }
    return reward;
}

/// Draws the selected run mode's starting HP/MP and scaled EXP/BP carryover.
static void _mistShootingGalleryModeStatusPanelTask(Task* task)
{
    enum { HP_WARNING_THRESHOLD = 100,
           MP_WARNING_THRESHOLD = 30 };
    u8          numberText[0x20];
    TextDrawReq hpCaption;
    TextDrawReq mpCaption;
    TextDrawReq expCaption;
    TextDrawReq bpCaption;
    UiObject*   object;
    s32         displayedValue;
    s32         valueColor;
    s32         captionX;
    s16         contentTop;
    s32         rowY;

    /// Draws one left-aligned outlined STATUS caption at the current row.
#define DRAW_STATUS_CAPTION(caption, captionText)                                                         \
    {                                                                                                     \
        enum { BASELINE_INSET = 8,                                                                        \
               TEXT_OT_BIAS   = 1 };                                                                        \
        (caption).x          = object->panel.contentOriginX.unsignedValue + captionX;                     \
        (caption).y          = (s16)(object->panel.contentOriginY.unsignedValue - BASELINE_INSET) + rowY; \
        (caption).otIndex    = object->panel.otIndex.signedValue + TEXT_OT_BIAS;                          \
        (caption).colorRgb   = MODE_TEXT_COLOR;                                                           \
        (caption).glyphTable = TEXT_GLYPH_TABLE_SMALL;                                                    \
        (caption).alignment  = TEXT_ALIGNMENT_LEFT;                                                       \
        (caption).drawMode   = TEXT_DRAW_OUTLINED;                                                        \
        textDrawString(&(caption), captionText);                                                          \
    }

    object = task->spawnArg2.pointer;
    uiDrawTitle(&(object)->panel, "STATUS");
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(4));
        task->state = task->state + 1;
    }

    valueColor     = MODE_TEXT_COLOR;
    contentTop     = object->panel.contentTop.signedValue;
    rowY           = contentTop + 0xF;
    displayedValue = gEquipmentModeBaseStats[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode].baseHp.hpWord;
    captionX       = object->panel.contentLeft.signedValue + 6;
    if (displayedValue < HP_WARNING_THRESHOLD) {
        valueColor = MODE_WARNING_TEXT_COLOR;
    }

    DRAW_STATUS_CAPTION(hpCaption, "HP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), valueColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY           = contentTop + 0x1E;
    displayedValue = gEquipmentModeBaseStats[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode].baseMp;
    valueColor     = MODE_TEXT_COLOR;
    if (displayedValue < MP_WARNING_THRESHOLD) {
        valueColor = MODE_WARNING_TEXT_COLOR;
    }

    DRAW_STATUS_CAPTION(mpCaption, "MP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), valueColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY           = contentTop + 0x2D;
    displayedValue = _mistShootingGalleryScaleReward(gMistShootingGalleryUnscaledExp);

    DRAW_STATUS_CAPTION(expCaption, "EXP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY          += 0xF;
    displayedValue = _mistShootingGalleryScaleReward(gMistShootingGalleryUnscaledBp);

    DRAW_STATUS_CAPTION(bpCaption, "BP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
#undef DRAW_STATUS_CAPTION
}
/// Three-line help text indexed by run mode (0 Replay, 1 Bounty, 2 Scavenger, 3 Nightmare).
static const _MistShootingGalleryModeTexts _gMistShootingGalleryModeDescriptions = { { _gMistShootingGalleryReplayModeDescription, _gMistShootingGalleryBountyModeDescription, _gMistShootingGalleryScavengerModeDescription, _gMistShootingGalleryNightmareModeDescription } };

/// Draws the selected run mode's mission, condition, enemy and supply ratings.
static void _mistShootingGalleryModeDataPanelTask(Task* task)
{
    UiObject* object = task->spawnArg2.pointer;
    // Each DATA row's rating for every run mode, indexed by the save's gameMode.
    _MistShootingGalleryRating missionLevels[4] = {
        { 2, "EASY" },
        { 3, "NORMAL" },
        { 4, "HARD" },
        { 5, "VERY HARD" },
    };
    _MistShootingGalleryRating conditions[4] = {
        { 5, "GOOD" },
        { 5, "GOOD" },
        { 4, "EXHAUSTED" },
        { 1, "SICK" },
    };
    _MistShootingGalleryRating enemyLevels[4] = {
        { 2, "EASY" },
        { 4, "STRONG" },
        { 3, "NORMAL" },
        { 5, "VERY STRONG" },
    };
    _MistShootingGalleryRating supplyLevels[4] = {
        { 4, "RICH" },
        { 3, "NORMAL" },
        { 1, "VERY POOR" },
        { 2, "POOR" },
    };
    // The gauge drawn for each rating level: entry n is n gauge marks. No
    // rating has level 0, whose entry repeats the single mark.
    const char* gaugeByLevel[6] = {
        _gMistShootingGalleryGaugeOneMarkText,
        _gMistShootingGalleryGaugeOneMarkText,
        _gMistShootingGalleryGaugeTwoMarksText,
        _gMistShootingGalleryGaugeThreeMarksText,
        _gMistShootingGalleryGaugeFourMarksText,
        _gMistShootingGalleryGaugeFiveMarksText,
    };
    TextDrawReq                       missionCaption;
    TextDrawReq                       missionRatingText;
    TextDrawReq                       conditionCaption;
    TextDrawReq                       conditionRatingText;
    TextDrawReq                       enemyCaption;
    TextDrawReq                       enemyRatingText;
    TextDrawReq                       supplyCaption;
    TextDrawReq                       supplyRatingText;
    const _MistShootingGalleryRating* rating;
    s32                               contentLeft;
    s32                               contentTop;
    s32                               captionX;
    s32                               rowY;

    /// Draws a positioned mode caption, its rating word and a level gauge.
#define DRAW_MODE_RATING(caption, ratingText, ratings, captionText)                                                                                          \
    {                                                                                                                                                        \
        enum { RIGHT_X        = 65,                                                                                                                          \
               BASELINE_INSET = 3,                                                                                                                           \
               GAUGE_X        = 70,                                                                                                                          \
               TEXT_OT_BIAS   = 1 };                                                                                                                           \
        (caption).otIndex    = object->panel.otIndex.signedValue + TEXT_OT_BIAS;                                                                             \
        rating               = &(ratings)[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode];                                                                \
        (caption).colorRgb   = MODE_TEXT_COLOR;                                                                                                              \
        (caption).glyphTable = TEXT_GLYPH_TABLE_SMALL;                                                                                                       \
        (caption).alignment  = TEXT_ALIGNMENT_LEFT;                                                                                                          \
        (caption).drawMode   = TEXT_DRAW_OUTLINED;                                                                                                           \
        textDrawString(&(caption), captionText);                                                                                                             \
                                                                                                                                                             \
        (ratingText).x          = object->panel.contentOriginX.unsignedValue + RIGHT_X;                                                                      \
        (ratingText).y          = (s16)(object->panel.contentOriginY.unsignedValue - BASELINE_INSET) + rowY;                                                 \
        (ratingText).otIndex    = object->panel.otIndex.signedValue + TEXT_OT_BIAS;                                                                          \
        (ratingText).colorRgb   = MODE_TEXT_COLOR;                                                                                                           \
        (ratingText).glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                                                                                   \
        (ratingText).alignment  = TEXT_ALIGNMENT_RIGHT;                                                                                                      \
        (ratingText).drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;                                                                                            \
        textDrawString(&(ratingText), rating->name);                                                                                                         \
        textDrawUiLine(object, GAUGE_X, rowY, (const u8*)gaugeByLevel[rating->level], MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT); \
    }

    uiDrawTitle(&(object)->panel, "DATA");

    contentLeft      = object->panel.contentLeft.signedValue;
    object->result   = USER_INTERFACE_RESULT_NONE;
    captionX         = contentLeft + 0xB;
    contentTop       = object->panel.contentTop.signedValue;
    missionCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    rowY             = contentTop + 0xB;
    missionCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    DRAW_MODE_RATING(missionCaption, missionRatingText, missionLevels, (const u8*)"MISSION LEVEL");
    uiDrawHorizontalSeparator(&(object)->panel, contentLeft + 6, -captionX + 5, contentTop + 0xD);

    rowY               = contentTop + 0x1E;
    conditionCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    conditionCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    DRAW_MODE_RATING(conditionCaption, conditionRatingText, conditions, (const u8*)"CONDITION");

    rowY           = contentTop + 0x2D;
    enemyCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    enemyCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    DRAW_MODE_RATING(enemyCaption, enemyRatingText, enemyLevels, (const u8*)"ENEMY LEVEL");

    rowY            = contentTop + 0x3C;
    supplyCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    supplyCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    DRAW_MODE_RATING(supplyCaption, supplyRatingText, supplyLevels, (const u8*)_gMistShootingGallerySupplyLevelCaption);
#undef DRAW_MODE_RATING
}
/// Runs carryover-mode selection and commits its previewed EXP/BP conversion.
static void _mistShootingGalleryCarryoverModeSessionTask(Task* task)
{
    enum {
        OPEN             = 0,
        CARRYOVER_SELECT = 1,
        CLOSE_COUNTDOWN  = 10,
    };
    UiObject*     object;
    PlayerStatus* player = &gPlayerStatus;

    if (task->state == OPEN) {
        object = uiSpawnObject(&_gMistShootingGalleryModeSelectPanelDesc, 0, 1, 1, NULL);
        if (object != NULL) {
            // Capture before the row callback starts changing the selected run mode.
            gMistShootingGalleryUnscaledBp  = player->bp;
            gMistShootingGalleryUnscaledExp = player->exp;
            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            task->spawnArg2.pointer = object;
            task->state             = task->state + 1;
        }
    } else if (task->state == CARRYOVER_SELECT) {
        object = task->spawnArg2.pointer;
        if (object->result == USER_INTERFACE_RESULT_CONFIRM) {
            task->killCountdown = CLOSE_COUNTDOWN;
            uiStartTreeClosing(object, object->owner);
            equipmentRecalculateMaxHp();
            equipmentRecalculateMaxMp();

            // Commit the same conversion the STATUS child has been previewing.
            player->exp = _mistShootingGalleryScaleReward(gMistShootingGalleryUnscaledExp);
            player->bp  = _mistShootingGalleryScaleReward(gMistShootingGalleryUnscaledBp);
            equipmentRestoreHpMp();
            task->state = task->state + 1;
        }
    } else {
        task->killCountdown = task->killCountdown - 1;
        if (task->killCountdown < 0) {
            taskCallExit(task);
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gUiPromptObject = NULL;
            stageRequestModeTaskExit();
        }
    }
}
s32 mistShootingGalleryOpenWeaponMenu(s32 unused)
{
    enum { WEAPON_MENU_REQUEST = 0x44 };

    displayQueueModeTask(&_gMistShootingGalleryWeaponMenuTaskDesc, WEAPON_MENU_REQUEST, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

/// Draws a run-mode row and selects its mode whenever the row has input.
static void _mistShootingGalleryModeRow(UiList* list, UiObject* object)
{
    _MistShootingGalleryModeTexts modeNames;

    modeNames = _gMistShootingGalleryModeNames;
    // The list has one row per run mode, so the row index is the mode.
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue - 1, (const u8*)modeNames.byMode[list->currentItemIndex], list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode = list->currentItemIndex;
    }
}
/// Retained out-of-line copy of the EXP/BP carryover conversion.
static s32 _mistShootingGalleryScaleRewardOutOfLine(s32 unscaledTotal)
{
    return _mistShootingGalleryScaleReward(unscaledTotal);
}
/// Draws the three-line description of the selected run mode below its menu.
static void _mistShootingGalleryModeHelpPanelTask(Task* task)
{
    UiObject*                     object           = task->spawnArg2.pointer;
    _MistShootingGalleryModeTexts modeDescriptions = _gMistShootingGalleryModeDescriptions;

    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(3) + 1);
        object->panel.bounds.unsignedRect.y = 0x68 - object->panel.bounds.unsignedRect.h;
        task->state                         = task->state + 1;
    }
    textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)modeDescriptions.byMode[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode], MODE_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}
void mistShootingGalleryOpenCarryoverModeMenu(void)
{
    enum { CARRYOVER_ENTRY_WARP = 7 };

    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) && (gGameSession->location.warp == CARRYOVER_ENTRY_WARP)) {
        displayQueueModeTask(&_gMistShootingGalleryCarryoverModeSessionDesc, 0, 0, STAGE_ENTRY_RELOAD);
    }
}
/// Registers the gallery room task and prepares captions, barriers and entry events.
static void _mistShootingGalleryInitializeRoomTask(Task* task)
{
    enum {
        CAPTION_TEXTURE_X      = 832,
        CAPTION_RESOURCE_INDEX = 2,
        DEMO_SCENE             = 7,
        CARRYOVER_WARP         = 7,
        TRAINING_WARP          = 6,
        STORY_ENTRY_COMMAND    = 22,
    };
    s32 storyBarrierLowered;

    task->msgTable = _gMistShootingGalleryRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    shootingGalleryPierceSelectCaptionResource(CAPTION_TEXTURE_X, 0, CAPTION_RESOURCE_INDEX);
    if (gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT) != 0) {
        sceneSetPlacedActorDrawMode(1, ACTOR_MESSAGE_DRAW_HIDE);
        storyBarrierLowered = 1;
    } else {
        storyBarrierLowered = 0;
    }
    _mistShootingGallerySetStoryBarrierLowered(storyBarrierLowered);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == DEMO_SCENE) {
        taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER, 0, 0);
    } else if (gGameSession->location.warp == CARRYOVER_WARP) {
        taskSpawnFromTable(gShootingGalleryPierceGalleryTaskDescs, SHOOTING_GALLERY_PIERCE_GALLERY_INTRO_TASK, 0, 0);
    }
    if ((gGameSession->location.warp == TRAINING_WARP) && (gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT) != 0)) {
        capRunCommandWithTransition(STORY_ENTRY_COMMAND);
    }
    gGameSession->flowFlags = GAME_SESSION_FLOW_SKIP_AREA_MUSIC;
    task->state             = task->state + 1;
}

/// Updates the gallery story actor visibility and checks training-exit input.
static void _mistShootingGalleryUpdateRoomTask(Task* unusedTask)
{
    enum {
        STORY_VARIANT      = 1,
        HIDE_ACTOR_VIEW_3  = 3,
        HIDE_ACTOR_VIEW_9  = 9,
        HIDE_ACTOR_VIEW_18 = 18,
    };
    u8 viewIndex;

    if ((gGameSession->location.variant == STORY_VARIANT) && (gGameSession->eventState == 0)) {
        viewIndex = gGameSession->location.view;
        if ((viewIndex == HIDE_ACTOR_VIEW_3) || (viewIndex == HIDE_ACTOR_VIEW_9) || (viewIndex == HIDE_ACTOR_VIEW_18)) {
            sceneSetPlacedActorDrawMode(1, ACTOR_MESSAGE_DRAW_HIDE);
        } else if (gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT) == 0) {
            sceneSetPlacedActorDrawMode(1, ACTOR_MESSAGE_DRAW_SHOW);
        }
    }
    shootingGalleryPierceCheckGalleryExitInput();
}
/// Plays one room CAP command, then restores player control and releases itself.
static void _mistShootingGalleryCapPlaybackTask(Task* task)
{
    enum {
        START = 0,
        WAIT  = 1,
        EXIT  = 2,
    };
    s16 texturePageX;

    switch (task->state) {
        case START:
            gCapFile = NULL;
            // Select the relocated CAP file and its VRAM texture-page origin.
            if (task->spawnArg2.value == CAP_FILE_HIGH_COMMANDS) {
                capSelectLoadedFile(CAP_FILE_HIGH_COMMANDS);
                texturePageX = CAP_TEXTURE_X_HIGH_COMMANDS;
            } else {
                capSelectLoadedFile(CAP_FILE_LOW_COMMANDS);
                texturePageX = CAP_TEXTURE_X_LOW_COMMANDS;
            }
            capSetTexturePage(texturePageX, 0);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state += 1;
            return;
        case WAIT:
            if (capIsBusy() != 0) {
                return;
            }
            task->state += 1;
            return;
        case EXIT:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
            taskKill(task);
            break;
    }
}

/// Caption for the DATA panel's supply rating.
static const char _gMistShootingGallerySupplyLevelCaption[16] = "SUPPLY LEVEL\0\xD0\x0E\xF0";

/// No key item can be used here.
static s32 _mistShootingGalleryRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// The doors: the parking lot's leads to its story state once the story has begun; the gallery's warp 5 waits for
/// Pierce's confirmation and restores the carried loadout, warp 6 hands out the training loadout.
static s32 _mistShootingGalleryResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        CARRIED_LOADOUT_WARP      = 5,
        TRAINING_LOADOUT_WARP     = 6,
        SCENE_CARRIED_LOADOUT     = 1,
        SCENE_TRAINING_LOADOUT    = 2,
        CARRIED_RESOURCE_VARIANT  = 3,
        TRAINING_RESOURCE_VARIANT = 4,
        PARKING_STORY_ROOM_OFFSET = 2,
    };
    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_MIST_PARKING && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) != EVENT_FLAG_STORY_CHAPTER_MIST_ARRIVAL) {
            reply->roomState += PARKING_STORY_ROOM_OFFSET;
        }
    }
    if (request->areaId == AKROPOLIS_ROOM_ID_MIST_SHOOTING_GALLERY) {
        if (reply->warp == CARRIED_LOADOUT_WARP && shootingGalleryPierceResolveGalleryExit(request) == SHOOTING_GALLERY_PIERCE_GALLERY_EXIT_DEFER) {
            return SHOOTING_GALLERY_PIERCE_GALLERY_EXIT_DEFER;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (reply->warp == TRAINING_LOADOUT_WARP) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = SCENE_TRAINING_LOADOUT;
                gPlayerStatus.resourceVariant                       = TRAINING_RESOURCE_VARIANT;
                gGameSession->hideHud                               = 1;
                inventoryInitializeShootingGalleryLoadout();
            }
            if (reply->warp == CARRIED_LOADOUT_WARP) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = SCENE_CARRIED_LOADOUT;
                gPlayerStatus.resourceVariant                       = CARRIED_RESOURCE_VARIANT;
                gGameSession->hideHud                               = 1;
                inventoryRestoreCarriedLoadout();
            }
        }
    }
    return SHOOTING_GALLERY_PIERCE_GALLERY_EXIT_ALLOW;
}

/// Room commands 5..8 and 33..34 play that CAP command, from file slot 1 or 3, with the player held.
static s32 _mistShootingGalleryHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        LOW_COMMAND_FIRST  = 5,
        LOW_COMMAND_END    = 9,
        HIGH_COMMAND_FIRST = 33,
        HIGH_COMMAND_END   = 35,
    };

    // Keep the nested bounds: their separate signed comparisons match dispatch.
    if (command >= LOW_COMMAND_FIRST) {
        if (command >= LOW_COMMAND_END) {
            if (command < HIGH_COMMAND_END) {
                if (command >= HIGH_COMMAND_FIRST) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(&_gMistShootingGalleryCapPlaybackTaskDesc, 0, command, CAP_FILE_HIGH_COMMANDS);
                }
            }
        } else {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&_gMistShootingGalleryCapPlaybackTaskDesc, 0, command, CAP_FILE_LOW_COMMANDS);
        }
    }
    return 0;
}

/// Room actions: 1 the course menu, 2 talking to Pierce, 3 the training point, 4 the control room's scene, once.
static s32 _mistShootingGalleryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ACTION_REARM_TICKS = 10,
    };
    if ((request->actionId == MIST_SHOOTING_GALLERY_ACTION_OPEN_COURSE_MENU) && (gShootingGalleryPierceGalleryTrainingActive == 0)) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(gShootingGalleryPierceGalleryTaskDescs, SHOOTING_GALLERY_PIERCE_GALLERY_MENU_TASK, 1, 0);
        gDirectionInteractionRearmUpdates = ACTION_REARM_TICKS;
    }
    if ((request->actionId == MIST_SHOOTING_GALLERY_ACTION_TALK_TO_PIERCE) && (gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT) == 0)) {
        shootingGalleryPierceStartPierceConversation();
    }
    if (request->actionId == MIST_SHOOTING_GALLERY_ACTION_TRAINING_POINT) {
        shootingGalleryPierceHandleGalleryAction();
    }
    if ((request->actionId == MIST_SHOOTING_GALLERY_ACTION_STORY_EVENT) && (gameFlagGetNibble(EVENT_FLAG_MIST_SHOOTING_GALLERY_CONTROL_ROOM_SCENE_SEEN) == 0)) {
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_MIST_SHOOTING_GALLERY_BRIEFED);
        gameFlagSetNibble(EVENT_FLAG_MIST_SHOOTING_GALLERY_CONTROL_ROOM_SCENE_SEEN, 1);
        evsStartScriptWithSkip(gShootingGalleryPiercePierceStoryScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gShootingGalleryPiercePierceStorySkipScript);
    }
    return 0;
}

/// Room task handlers in state order: initialization, running, then teardown.
static const TaskFuncTable3 _gMistShootingGalleryRoomStates = {
    { _mistShootingGalleryInitializeRoomTask, _mistShootingGalleryUpdateRoomTask, taskKill },
};

/// Numbered jukebox row 3 label for MIDI sequence 49.
static const char _gMistShootingGalleryJukeboxRow3HeavenSentKillerLabel[] = "3. Heaven-sent Killer";
/// Numbered jukebox row 2 label for MIDI sequence 23.
static const char _gMistShootingGalleryJukeboxRow2EagerForBloodLabel[] = "2. Eager For Blood";
/// Numbered jukebox row 1 label for MIDI sequence 20.
static const char _gMistShootingGalleryJukeboxRow1CrazyKingLabel[] = "1. Crazy King";
/// Numbered jukebox row 3 label for MIDI sequence 66.
static const char _gMistShootingGalleryJukeboxRow3CrawlingWasteEmperorLabel[] = "3. Crawling Waste Emperor";
/// Numbered jukebox row 2 label for MIDI sequence 60.
static const char _gMistShootingGalleryJukeboxRow2PickUpTheGauntletLabel[] = "2. Pick Up The Gauntlet";
/// Numbered jukebox row 3 label for MIDI sequence 74.
static const char _gMistShootingGalleryJukeboxRow3HuntersMoonLabel[] = "3. Hunter's Moon";
/// Numbered jukebox row 2 label for MIDI sequence 22.
static const char _gMistShootingGalleryJukeboxRow2QuadrumanousLeaderLabel[] = "2. Quadrumanous Leader";
/// Numbered jukebox row 3 label for MIDI sequence 93.
static const char _gMistShootingGalleryJukeboxRow3GenicReactorLabel[] = "3. Genic Reactor";
/// Numbered jukebox row 2 label for MIDI sequence 82.
static const char _gMistShootingGalleryJukeboxRow2RebelForcesLabel[] = "2. Rebel Forces";
/// Numbered jukebox row 1 label for MIDI sequence 67.
static const char _gMistShootingGalleryJukeboxRow1YellowRainLabel[] = "1. Yellow Rain";
/// Numbered jukebox row 3 label for MIDI sequence 60.
static const char _gMistShootingGalleryJukeboxRow3PickUpTheGauntletLabel[] = "3. Pick Up The Gauntlet";
/// Numbered jukebox row 2 label for MIDI sequence 21.
static const char _gMistShootingGalleryJukeboxRow2AmbushLabel[] = "2. Ambush!";
/// Numbered jukebox row 4 label for MIDI sequence 61.
static const char _gMistShootingGalleryJukeboxRow4DarkVoiceOfTheHeartLabel[] = "4. Dark Voice Of The Heart";
/// Numbered jukebox row 3 label for MIDI sequence 58.
static const char _gMistShootingGalleryJukeboxRow3RequiemLabel[] = "3. Requiem";
/// Numbered jukebox row 2 label for MIDI sequence 45.
static const char _gMistShootingGalleryJukeboxRow2RockAndFireLabel[] = "2. Rock And Fire";
/// Numbered jukebox row 1 label for MIDI sequence 41.
static const char _gMistShootingGalleryJukeboxRow1GhostTownLabel[] = "1. Ghost Town";
/// Numbered jukebox row 4 label for MIDI sequence 44.
static const char _gMistShootingGalleryJukeboxRow4SnooperLabel[] = "4. Snooper";
/// Numbered jukebox row 3 label for MIDI sequence 42.
static const char _gMistShootingGalleryJukeboxRow3WildHuntLabel[] = "3. Wild Hunt";
/// Numbered jukebox row 2 label for MIDI sequence 36.
static const char _gMistShootingGalleryJukeboxRow2LightningOperationLabel[] = "2. Lightning Operation";
/// Numbered jukebox row 1 label for MIDI sequence 35.
static const char _gMistShootingGalleryJukeboxRow1LAMazeLabel[] = "1. L.A. Maze";
/// Numbered jukebox row 4 label for MIDI sequence 93.
static const char _gMistShootingGalleryJukeboxRow4GenicReactorLabel[] = "4. Genic Reactor";
/// Numbered jukebox row 3 label for MIDI sequence 89.
static const char _gMistShootingGalleryJukeboxRow3MentalAgonyLabel[] = "3. Mental Agony";
/// Numbered jukebox row 2 label for MIDI sequence 59.
static const char _gMistShootingGalleryJukeboxRow2WishwashLabel[] = "2. Wishwash";
/// Numbered jukebox row 1 label for MIDI sequence 31.
static const char _gMistShootingGalleryJukeboxRow1CurseYourFateLabel[] = "1. Curse Your Fate!";
/// Numbered jukebox row 4 label for MIDI sequence 83.
static const char _gMistShootingGalleryJukeboxRow4KillingFieldLabel[] = "4. Killing Field";
/// Numbered jukebox row 3 label for MIDI sequence 78.
static const char _gMistShootingGalleryJukeboxRow3FoolsParadiseLabel[] = "3. Fool's Paradise";
/// Numbered jukebox row 2 label for MIDI sequence 77.
static const char _gMistShootingGalleryJukeboxRow2GazingIntoTheVoidLabel[] = "2. Gazing into The Void";
/// Numbered jukebox row 1 label for MIDI sequence 76.
static const char _gMistShootingGalleryJukeboxRow1ManMadeNatureLabel[] = "1. Man Made Nature";
/// Numbered jukebox row 4 label for MIDI sequence 37.
static const char _gMistShootingGalleryJukeboxRow4LazingAwayTheMorningLabel[] = "4. Lazing Away the Morning";
/// Numbered jukebox row 3 label for MIDI sequence 17.
static const char _gMistShootingGalleryJukeboxRow3OutOfPhase2Label[] = "3. Out Of Phase 2";
/// Numbered jukebox row 2 label for MIDI sequence 43.
static const char _gMistShootingGalleryJukeboxRow2TheVagrantsLabel[] = "2. The Vagrants";
/// Numbered jukebox row 1 label for MIDI sequence 9.
static const char _gMistShootingGalleryJukeboxRow1TowerRendezvousLabel[] = "1. Tower Rendezvous";

void mistShootingGalleryRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistShootingGalleryRoomStates;
    states.funcs[task->state](task);
}

/// Restores the story-controlled barrier, then lowers it by 3000 room Y units.
static void _mistShootingGallerySetStoryBarrierLowered(s32 lowered)
{
    enum { FACE_COUNT   = 3,
           VERTEX_COUNT = 8,
           LOWER_Y      = 3000 };
    WorldCollisionGrid*       liveGrid     = &gMistShootingGalleryCollisionGrid;
    const WorldCollisionGrid* templateGrid = &_gMistShootingGalleryStoryBarrierCollisionTemplate;
    SVECTOR                   offset;
    s32                       index;

    for (index = 0; index < FACE_COUNT; index++) {
        liveGrid->normals[index].vx = templateGrid->normals[index].vx;
        liveGrid->normals[index].vy = templateGrid->normals[index].vy;
        liveGrid->normals[index].vz = templateGrid->normals[index].vz;
        liveGrid->faces[index]      = templateGrid->faces[index];
    }
    for (index = 0; index < VERTEX_COUNT; index++) {
        liveGrid->vertices[index].vx = templateGrid->vertices[index].vx;
        liveGrid->vertices[index].vy = templateGrid->vertices[index].vy;
        liveGrid->vertices[index].vz = templateGrid->vertices[index].vz;
    }
    if (lowered == 0) {
        offset.vx = 0;
        offset.vy = 0;
    } else {
        offset.vx = 0;
        offset.vy = LOWER_Y;
    }
    offset.vz = 0;
    for (index = 0; index < VERTEX_COUNT; index++) {
        liveGrid->vertices[index].vx += offset.vx;
        liveGrid->vertices[index].vy += offset.vy;
        liveGrid->vertices[index].vz += offset.vz;
    }
}

void mistShootingGallerySetTrainingBarrierLowered(s32 lowered)
{
    enum { NORMAL_OFFSET = 3,
           VERTEX_OFFSET = 8,
           VERTEX_COUNT  = 4,
           OFFSET_COUNT  = 8,
           LOWER_Y       = 4000 };
    WorldCollisionGrid*       liveGrid     = &gMistShootingGalleryCollisionGrid;
    const WorldCollisionGrid* templateGrid = &_gMistShootingGalleryTrainingBarrierCollisionTemplate;
    WorldCollisionGridFace*   liveFace     = &gMistShootingGalleryCollisionGrid.faces[NORMAL_OFFSET];
    WorldCollisionGridFace*   templateFace = _gMistShootingGalleryTrainingBarrierCollisionTemplate.faces;
    SVECTOR                   offset;
    s32                       index;
    s32                       corner;

    for (index = 0; index < (s32)ARRAY_SIZE(_gMistShootingGalleryTrainingBarrierCollisionTemplateFaces); index++) {
        liveGrid->normals[index + NORMAL_OFFSET].vx = templateGrid->normals[index].vx;
        liveGrid->normals[index + NORMAL_OFFSET].vy = templateGrid->normals[index].vy;
        liveGrid->normals[index + NORMAL_OFFSET].vz = templateGrid->normals[index].vz;
        for (corner = 0; corner < ARRAY_SIZE(liveFace->vertexIndices); corner++) {
            liveFace->vertexIndices[corner] = templateFace->vertexIndices[corner] + VERTEX_OFFSET;
        }
        liveFace->normalIndex  = templateFace->normalIndex + NORMAL_OFFSET;
        liveFace->surfaceClass = templateFace->surfaceClass;
        liveFace++;
        templateFace++;
    }
    for (index = 0; index < VERTEX_COUNT; index++) {
        liveGrid->vertices[index + VERTEX_OFFSET].vx = templateGrid->vertices[index].vx;
        liveGrid->vertices[index + VERTEX_OFFSET].vy = templateGrid->vertices[index].vy;
        liveGrid->vertices[index + VERTEX_OFFSET].vz = templateGrid->vertices[index].vz;
    }
    if (lowered == 0) {
        offset.vx = 0;
        offset.vy = 0;
    } else {
        offset.vx = 0;
        offset.vy = LOWER_Y;
    }
    offset.vz = 0;
    // Preserve the eight-vertex offset pass: only the first four were restored.
    for (index = 0; index < OFFSET_COUNT; index++) {
        liveGrid->vertices[index + VERTEX_OFFSET].vx += offset.vx;
        liveGrid->vertices[index + VERTEX_OFFSET].vy += offset.vy;
        liveGrid->vertices[index + VERTEX_OFFSET].vz += offset.vz;
    }
}

#define ROOM_PART _gJukeboxTrackLists
#include "assets/rooms/mist/shooting_gallery/jukebox.inc"
/// SELECT title for the jukebox panel, ending at its first NUL.
static const char _gMistShootingGalleryJukeboxTitle[8] = "SELECT\0\xE1";

#include "../../../../shared/room/jukebox_row.inc.c"

/// Updates the gallery jukebox's track picker and asynchronous music change.
static void _mistShootingGalleryJukeboxPanelTask(Task* task)
{
    enum {
        INITIALIZE          = 0,
        WAIT_MIDI           = 1,
        TRACK_ID_LIMIT      = 0xF1,
        TRAINING_UNSELECTED = 0xFE,
        IDLE                = 0xFF,
        NO_ROW              = -1,
        MAX_VISIBLE_ROWS    = 10,
        FILE_GROUP          = 4,
        FILE_ID_HUNDREDS    = 1
    };
    u8        fileKey[4];
    u8        loadArgs[4];
    UiObject* object;
    UiList*   list;
    u8        sequenceId;
    s32       loadQueued;
    s32       phase;
    u8        sequenceStarted;

    object = task->spawnArg2.pointer;
    list   = &_gMistShootingGalleryJukeboxList;

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(object)->panel, _gMistShootingGalleryJukeboxTitle);
    if (task->state == INITIALIZE) {
        task->spawnArg1.value = NO_ROW;
        if (attachmentIsTrainingMode() == 0) {
            list->itemCount = ARRAY_SIZE(_gMistShootingGalleryJukeboxRegularReplayTracks);
        } else {
            list->itemCount = ARRAY_SIZE(_gMistShootingGalleryJukeboxTrainingReplayTracks);
        }
        if (list->itemCount > MAX_VISIBLE_ROWS) {
            list->visibleRowCount.unsignedValue = MAX_VISIBLE_ROWS;
        } else {
            list->visibleRowCount.unsignedValue = list->itemCount;
        }
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        uiFitPanelToList(list, &(object)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        object->panel.bounds.unsignedRect.x = -((s16)object->panel.bounds.unsignedRect.w / 2);
        object->panel.bounds.unsignedRect.y = -((s16)object->panel.bounds.unsignedRect.h / 2);
        if (attachmentIsTrainingMode() == 0) {
            task->status = IDLE;
        } else {
            task->status = TRAINING_UNSELECTED;
        }
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    sequenceId = task->status;
    if (sequenceId < TRACK_ID_LIMIT) {
        // Stop the old sequence before loading its replacement; start after CD idle.
        phase = task->state;
        if (phase == WAIT_MIDI) {
            if (midiIsSequenceBusy(0) == 0) {
                fileKey[3]  = 0;
                fileKey[2]  = FILE_GROUP;
                fileKey[0]  = sequenceId;
                loadArgs[0] = FILE_ID_HUNDREDS;
                loadArgs[3] = 0;
                loadArgs[2] = 0;
                loadArgs[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs);
                loadQueued = 1;
            } else {
                loadQueued = 0;
            }
            if (loadQueued == 1) {
                task->state += 1;
            }
        } else {
            if (cdCmdIsIdle() & 0xFFFF) {
                sndEvtRequestMidiStart(sequenceId, 0);
                sndEvtRequestMidiVolume(sequenceId, (u8)gMidiMusicVolume);
                sequenceStarted         = 1;
                gStageRoomMusicSequence = sequenceId;
            } else {
                sequenceStarted = 0;
            }
            if (sequenceStarted == 1) {
                task->state  = WAIT_MIDI;
                task->status = IDLE;
                if (attachmentIsTrainingMode() == 0) {
                    gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
                }
                if (object->panel.control.word != USER_INTERFACE_PANEL_ACTIVE) {
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                }
            }
        }
    }
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask | gPadCancelButtonMask) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
            if (task->status != TRAINING_UNSELECTED) {
                if (task->status == IDLE) {
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    uiStartPanelHiding(object, object->owner);
                    object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        }
    }
}

#include "../../../../shared/room/jukebox_host.inc.c"

s32 mistShootingGalleryOpenJukebox(s32 unused)
{
    displayQueueModeTask(&_gMistShootingGalleryJukeboxSessionDesc, 0, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

/// Queues a 320x240 subtractive tile for the mode splash's current ramp tick.
static inline void _mistShootingGalleryDrawSplashFade(const Task* task, s32 fadeIn)
{
    enum {
        SPLASH_FADE_TEXTURE_DEPTH_4BIT = 0,
    };
    TILE*     tile;
    DR_TPAGE* drawMode;
    u8        intensity;

    tile           = gGpuPrimCursor;
    intensity      = fadeIn ? ~(u8)task->killCountdown : (u8)task->killCountdown;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->r0 = intensity;
    tile->g0 = intensity;
    tile->b0 = intensity;
    tile->x0 = -DISPLAY_SCREEN_WIDTH / 2;
    tile->y0 = -DISPLAY_SCREEN_HEIGHT / 2;
    tile->w  = DISPLAY_SCREEN_WIDTH;
    tile->h  = DISPLAY_SCREEN_HEIGHT;

    addPrim(gGpuCurrentOt, tile);
    // Insertion prepends, so the mode queued last runs before the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(SPLASH_FADE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt, drawMode);
}

/// Displays the cleared run's mode splash with a fade in, hold and fade out.
static void _mistShootingGalleryModeSplashTask(Task* task)
{
    enum {
        QUEUE_FILE               = 0,
        WAIT_FILE                = 1,
        FADE_IN                  = 2,
        HOLD                     = 3,
        FADE_OUT                 = 4,
        FINISH                   = 5,
        REPLAY_FILE              = 41,
        BOUNTY_FILE              = 42,
        SCAVENGER_FILE           = 43,
        NIGHTMARE_FILE           = 44,
        INTENSITY_STEP           = 8,
        INTENSITY_END            = 256,
        DISPLAY_ENABLE_THRESHOLD = 17,
        HOLD_TICKS               = 151
    };
    u8 fileKey[4];
    u8 loadArgs[4];

    switch (task->state) {
        case QUEUE_FILE:
            fileKey[3]  = 0;
            fileKey[2]  = 0;
            loadArgs[0] = 0;
            loadArgs[1] = 0;
            loadArgs[2] = 0;
            loadArgs[3] = 0;
            switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
                case 0:
                    fileKey[0] = REPLAY_FILE;
                    break;
                case 1:
                    fileKey[0] = BOUNTY_FILE;
                    break;
                case 2:
                    fileKey[0] = SCAVENGER_FILE;
                    break;
                case 3:
                    fileKey[0] = NIGHTMARE_FILE;
                    break;
            }
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs);
            task->state++;
            return;

        case WAIT_FILE:
            if (cdCmdIsIdle() & 0xFFFF) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                task->killCountdown                     = 0;
                task->state++;
                return;
            }
            return;

        case FADE_IN: {
            // Keep display blank until the initial black overlay has started fading.
            _mistShootingGalleryDrawSplashFade(task, true);

            task->killCountdown += INTENSITY_STEP;
            if (task->killCountdown >= DISPLAY_ENABLE_THRESHOLD) {
                SetDispMask(1);
            }
            if (task->killCountdown < INTENSITY_END) {
                return;
            }
            task->killCountdown = 0;
            task->state++;
            return;
        }

        case HOLD:
            task->killCountdown += 1;
            if (task->killCountdown < HOLD_TICKS && padIsStartPressed() == 0) {
                return;
            }
            task->killCountdown = 0;
            task->state++;
            return;

        case FADE_OUT: {
            _mistShootingGalleryDrawSplashFade(task, false);

            task->killCountdown += INTENSITY_STEP;
            if (task->killCountdown < INTENSITY_END) {
                return;
            }
            memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
            SetDispMask(0);
            task->state++;
            return;
        }

        case FINISH:
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            taskKill(task);
            displayResumeGameLoop();
            return;

        default:
            return;
    }
}

/// Plays the gallery's post-clear movie and restores game graphics afterwards.
static void _mistShootingGalleryClearMovieTask(Task* task)
{
    enum {
        MOVIE_PREPARE    = 0,
        MOVIE_QUEUE      = 1,
        MOVIE_WAIT_READY = 2,
        MOVIE_PLAY       = 3,
        MOVIE_WAIT_STOP  = 4,
        MOVIE_RESTORE    = 5,
        CLEAR_MOVIE_VIEW = 100
    };
    u8              streamArgs[4];
    GameLocationKey movieLocation;
    s16             movieSlot;
    CdCmdQueue*     cdQueue;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case MOVIE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state = task->state + 1;
            break;
        case MOVIE_QUEUE:
            movieLocation      = gGameSession->location;
            movieLocation.view = CLEAR_MOVIE_VIEW;
            movieSlot          = streamFindMovieSlot(&movieLocation, 0, 0);
            streamArgs[0]      = movieSlot;
            // This opcode consumes only the slot byte; its other bytes are retained.
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state = task->state + 1;
            break;
        case MOVIE_WAIT_READY:
            if (cdQueue->movieReady == 0) {
                break;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case MOVIE_PLAY:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                break;
            }
            if (padIsStartPressed() == 0) {
                break;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            break;
        case MOVIE_WAIT_STOP:
            // Do not release the decoder workspace until playback/cancellation ends.
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            break;
        case MOVIE_RESTORE:
            if ((streamPollGameRestore(0, 1) & 0xFFFF) == 0) {
                break;
            }
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
            memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
            taskKill(task);
            displayResumeGameLoop();
            break;
    }
}

/// Sequences the optional cleared-run mode splash before the gallery movie.
static void _mistShootingGalleryMovieSessionTask(Task* task)
{
    enum {
        MOVIE_SESSION_START       = 0,
        MOVIE_SESSION_WAIT_SPLASH = 1,
        MOVIE_SESSION_PLAY        = 2,
        MOVIE_TASK_INDEX          = 1,
        MODE_SPLASH_TASK_INDEX    = 2,
    };
    switch (task->state) {
        case MOVIE_SESSION_START:
            SetDispMask(0);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount == 0) {
                task->state = MOVIE_SESSION_PLAY;
                return;
            }
            // The display child owns the splash before this session runs again.
            displaySpawnTaskFromTable(gMistShootingGalleryMovieTaskDescs, MODE_SPLASH_TASK_INDEX, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_FULL;
            viewQueueCurrentCameraAndPackets();
        case MOVIE_SESSION_WAIT_SPLASH:
            task->state = task->state + 1;
            return;
        case MOVIE_SESSION_PLAY:
            displaySpawnTaskFromTable(gMistShootingGalleryMovieTaskDescs, MOVIE_TASK_INDEX, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            viewQueueCurrentCameraAndPackets();
            taskKill(task);
            return;
    }
}

void mistShootingGallerySelectRoomLights(s16 useAlternate)
{
    if (useAlternate == 0) {
        gMistShootingGalleryRoomLightingTable[0].lights = &gMistShootingGalleryDefaultRoomLights;
        return;
    }
    gMistShootingGalleryRoomLightingTable[0].lights = &gMistShootingGalleryAlternateRoomLights;
}

void mistShootingGalleryDrawLightGlowsTask(Task* unused)
{
    enum {
        CAPSULE_RADIUS_SCALE = 0x200,
        CAPSULE_COLOR        = 0x222,
        DISC_RADIUS_SCALE    = 0x300,
        DISC_COLOR           = 0x111
    };
    u8 mappedView;

    // View mapping selects the visible strips and point glows in world space.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[0], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[8], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[10], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            break;
        case 3:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[4], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[6], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[14], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[16], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[18], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            break;
        case 7:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[14], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[16], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[18], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[20], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            break;
        case 8:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[24], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            break;
        case 9:
        case 18:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[20], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[22], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[26], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[28], CAPSULE_RADIUS_SCALE, CAPSULE_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[36], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[44], DISC_RADIUS_SCALE, DISC_COLOR);
            break;
        case 10:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[37], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[39], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[41], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[43], DISC_RADIUS_SCALE, DISC_COLOR);
            break;
        case 11:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[38], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[39], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[41], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[42], DISC_RADIUS_SCALE, DISC_COLOR);
            break;
        case 12:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[40], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[41], DISC_RADIUS_SCALE, DISC_COLOR);
            break;
        case 13:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[39], DISC_RADIUS_SCALE, DISC_COLOR);
            break;
        case 14:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[36], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[37], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[39], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[41], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[43], DISC_RADIUS_SCALE, DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[44], DISC_RADIUS_SCALE, DISC_COLOR);
            break;
    }
}

#include "../../../../shared/effects/glow_draw_capsule.inc.c"

#include "../../../../shared/effects/glow_draw_disc.inc.c"
