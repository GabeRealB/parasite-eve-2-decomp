#include "rooms/mist_shooting_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "mist_shooting_gallery_private.h"

#include "actors/actor_215100.h"

#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/loadui.h"
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
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the
/// GTE flag word. Defined before `glow_draw.h`, which otherwise selects
/// `GlowCentreScratch`.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/jukebox.h"

#define ROOM_PART declarations_mist_shooting_gallery
#include "assets/rooms/mist/shooting_gallery/declarations.inc"

// Relocated CAP file slots selected by commands 5..8 and 0x21..0x22.
enum {
    MIST_SHOOTING_GALLERY_CAP_FILE_LOW_COMMANDS       = 1,
    MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS      = 3,
    MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_LOW_COMMANDS  = 0x300,
    MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_HIGH_COMMANDS = 0x2C0
};

// Key-item use requests carry an item ID and an unused second argument word.
enum { MIST_SHOOTING_GALLERY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

// Colours of the mode-selection panels, packed with red in the low byte.
enum {
    MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR         = 0x606060,
    MIST_SHOOTING_GALLERY_MODE_WARNING_TEXT_COLOR = 0x0D287F
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
static s32        _mistShootingGalleryRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32        _mistShootingGalleryResolveRoomEvent(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32        _mistShootingGalleryHandleCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedSecondArg);
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
///
/// The unused level-zero table entry also points here.
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
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_M93R     = 0x81,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_M950     = 0x82,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_MP5A5    = 0x9D,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_M4A1     = 0x8F,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_MONGOOSE = 0x88,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_P229     = 0x84,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_M249     = 0x90,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_PA3      = 0x8C,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_SP12     = 0x8D,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_AS12     = 0x8E,
    MIST_SHOOTING_GALLERY_WEAPON_COUNT         = 10,
};

/// Weapon-picker catalog in offer order, with no terminator.
///
/// Signed halfwords hold inventory item IDs. Entries without saved ownership
/// are skipped; M4A1 and MP5A5 ownership includes their upgraded variants.
/// Displayed row indices address that filtered order, not these ten slots.
static s16 _gMistShootingGalleryWeaponItemIds[MIST_SHOOTING_GALLERY_WEAPON_COUNT] = {
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_M93R,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_M950,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_MP5A5,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_M4A1,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_MONGOOSE,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_P229,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_M249,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_PA3,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_SP12,
    MIST_SHOOTING_GALLERY_WEAPON_ITEM_AS12,
};

/// Single row handler used for every owned weapon through shared-row dispatch.
static UiListRowCallback _gMistShootingGalleryWeaponRowCallbacks[1] = {
    _mistShootingGalleryWeaponRow,
};

/// Mutable singleton weapon-picker list with clamped navigation and 15-pixel rows.
///
/// Initialization replaces the one-row seed with the owned-weapon count and
/// enables shared-row dispatch. At most ten rows are drawn. The gallery must
/// remain loaded, and only one weapon picker may use this state at a time.
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
///
/// The display queue supplies request 0x44 in the first spawn argument and zero
/// in the second. The gallery recipe and its callback must stay loaded until
/// the inventory-menu session closes.
static TaskDesc _gMistShootingGalleryWeaponMenuTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, 192 } },
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
///
/// Spawning copies centered pixel bounds (-72, -48, 144, 56), OT base 24 and a
/// bodyless priority-192 task. The first spawn argument is the signed total
/// score. Keep the gallery and controller work live until the child closes.
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
///
/// The selector limits the four initial rows to the unlocked modes and enables
/// the shared row callback before dispatch. Rows are 15 pixels apart. Only one
/// carryover menu may use this state at a time while the gallery stays loaded.
static UiList _gMistShootingGalleryModeList = {
    .rowCallbacks    = _gMistShootingGalleryModeRowCallbacks,
    .itemCount       = 4,
    .visibleRowCount = { .unsignedValue = 4 },
    .wrapNavigation  = 1,
    .rowHeight       = 15,
};

/// SELECT panel recipe for carryover run modes, with STATUS, help and DATA children.
///
/// Bounds are screen-centered pixels; the bodyless priority-192 task uses OT
/// base 32. Spawning copies the recipe; its callback stays live with the gallery.
static UiObjectDesc _gMistShootingGalleryModeSelectPanelDesc = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -96, 176, 64 },
    .otIndexSeed     = 32,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _mistShootingGalleryModeSelectPanelTask,
};

enum { MIST_SHOOTING_GALLERY_MODE_HELP_UNTITLED_STYLE = 3 };

/// Carryover selector child recipes in STATUS, untitled help, DATA order.
///
/// Bounds are screen-centered pixels; OT bases 28, 24 and 20 order the children.
/// Every task is bodyless at priority 192 and borrows the live run mode 0..3.
/// Spawning copies each recipe; its callback must stay loaded until teardown.
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
        .style           = MIST_SHOOTING_GALLERY_MODE_HELP_UNTITLED_STYLE,
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
///
/// The display queue borrows this descriptor asynchronously; it and its callback
/// must stay loaded through session completion. Both spawn arguments are unused
/// initially; the session later owns the selector object through its second argument.
static TaskDesc _gMistShootingGalleryCarryoverModeSessionDesc = {
    .header   = { .fields = { TASK_BODY_NONE, 192 } },
    .callback = _mistShootingGalleryCarryoverModeSessionTask,
    .data     = { .value = 0 },
};

/// Bodyless priority-192 task recipe for the gallery's room CAP commands.
///
/// The first spawn argument is a command index; the second selects loaded file
/// slot 1 for commands 5..8 or slot 3 for commands 33..34. Playback holds player
/// control until completion. The gallery and selected CAP resources stay loaded.
static TaskDesc _gMistShootingGalleryCapPlaybackTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, 192 } },
    .callback = _mistShootingGalleryCapPlaybackTask,
    .data     = { .value = 0 },
};

/// Borrowed room-task message dispatch, terminated by TASK_MESSAGE_TABLE_END.
///
/// Handles transition request/reply records, key-item ID words, room-action
/// requests and CAP-command index words. Payloads live only for the synchronous
/// call; the table and callback code stay loaded throughout the room task.
static TaskMessageEntry _gMistShootingGalleryRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mistShootingGalleryResolveRoomEvent },
    { MIST_SHOOTING_GALLERY_MESSAGE_USE_KEY_ITEM, _mistShootingGalleryRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mistShootingGalleryHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _mistShootingGalleryHandleCommand },
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
///
/// Initialization replaces the one-row seed with three training or four regular
/// tracks, resets selection/scroll origin and enables the shared callback.
/// Only one jukebox panel may use this state while the gallery stays loaded.
static UiList _gMistShootingGalleryJukeboxList = {
    .rowCallbacks    = _gMistShootingGalleryJukeboxRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .rowHeight       = 17,
};

/// Recipe for this overlay's SELECT jukebox panel and track-picker task.
///
/// Bounds are pixels relative to screen centre; the 224-by-128 panel uses
/// ordering-table base 48 and a bodyless priority-192 task. Spawning reads the
/// recipe synchronously and retains its content callback for the panel's
/// lifetime, so this overlay must remain loaded until the panel closes.
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
///
/// Queued with both spawn arguments zero. The shared host borrows the gallery
/// panel until it closes, then restores frame timing and exits the display mode.
static TaskDesc _gMistShootingGalleryJukeboxSessionDesc = {
    .header   = { .fields = { TASK_BODY_NONE, 192 } },
    .callback = _jukeboxHostTask,
    .data     = { .value = 0 },
};

TaskDesc gMistShootingGalleryMovieTaskDescs[MIST_SHOOTING_GALLERY_MOVIE_TASK_COUNT] = {
    {
        .header   = { .fields = { TASK_BODY_NONE, 192 } },
        .callback = _mistShootingGalleryMovieSessionTask,
        .data     = { .value = 0 },
    },
    {
        .header   = { .fields = { TASK_BODY_NONE, 192 } },
        .callback = _mistShootingGalleryClearMovieTask,
        .data     = { .value = 0 },
    },
    {
        .header   = { .fields = { TASK_BODY_NONE, 192 } },
        .callback = _mistShootingGalleryModeSplashTask,
        .data     = { .value = 0 },
    },
};
#include "assets/rooms/mist/shooting_gallery/header.inc"
#include "assets/rooms/mist/shooting_gallery/markers.inc"

enum { MIST_SHOOTING_GALLERY_LOGICAL_VIEW_COUNT = 18 };

// Arrival/departure script entries in the gallery's area sound bank.
enum {
    MIST_SHOOTING_GALLERY_WARP_1_DEPARTURE_SOUND   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 1),
    MIST_SHOOTING_GALLERY_WARP_1_ARRIVAL_SOUND     = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 2),
    MIST_SHOOTING_GALLERY_WARP_2_4_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 3),
    MIST_SHOOTING_GALLERY_WARP_2_4_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 4),
    MIST_SHOOTING_GALLERY_WARP_5_6_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 5),
    MIST_SHOOTING_GALLERY_WARP_5_6_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 6),
};

static void _mistShootingGalleryInitRoomTask(Task* task);
static void _mistShootingGalleryUpdateRoomTask(Task* unusedTask);

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

void mistShootingGalleryPrepareTrainingLoadout(s32 courseLevel)
{
    enum {
        MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM              = 0x40,
        MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM        = 1,
        MIST_SHOOTING_GALLERY_TRAINING_COLA_ITEM             = 5,
        MIST_SHOOTING_GALLERY_TRAINING_MP_BOOST1_ITEM        = 6,
        MIST_SHOOTING_GALLERY_TRAINING_COMBUSTION_INDEX      = 1,
        MIST_SHOOTING_GALLERY_TRAINING_GPS_ATTACHMENT_SLOT   = 2,
        MIST_SHOOTING_GALLERY_TRAINING_COLA_ATTACHMENT_SLOT1 = 3,
        MIST_SHOOTING_GALLERY_TRAINING_COLA_ATTACHMENT_SLOT2 = 4
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
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            break;
        case 4:
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_COLA_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1)->attachSlot  = MIST_SHOOTING_GALLERY_TRAINING_GPS_ATTACHMENT_SLOT;
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_COLA_ITEM, 1)->attachSlot = MIST_SHOOTING_GALLERY_TRAINING_COLA_ATTACHMENT_SLOT1;
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_COLA_ITEM, 1)->attachSlot = MIST_SHOOTING_GALLERY_TRAINING_COLA_ATTACHMENT_SLOT2;
            gAttachmentTrainingLevels[ATTACHMENT_INDEX_ENERGY_SHOT]                                      = 1;
            gAttachmentTrainingLevels[MIST_SHOOTING_GALLERY_TRAINING_COMBUSTION_INDEX]                   = 1;
            break;
        case 5:
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM, 1);
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_MP_BOOST1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_MP_BOOST1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_MP_BOOST1_ITEM, 1);
            gAttachmentTrainingLevels[ATTACHMENT_INDEX_ENERGY_SHOT]                    = 1;
            gAttachmentTrainingLevels[MIST_SHOOTING_GALLERY_TRAINING_COMBUSTION_INDEX] = 1;
            break;
    }
    equipmentRestoreHpMp();
    equipmentInitializeWeaponSupplies();
}
/// Draws an owned weapon row and handles its training-loadout and help commands.
///
/// Borrows the picker object and list. The current row indexes only the owned
/// entries, in weapon-table order. Confirm replaces carried items with the weapon,
/// training armour and 999 ammunition units; Triangle suspends input for item help.
static void _mistShootingGalleryWeaponRow(UiList* list, UiObject* object)
{
    enum {
        MIST_SHOOTING_GALLERY_TRAINING_ARMOR_ITEM = 0x6C,
        MIST_SHOOTING_GALLERY_TRAINING_AMMO_UNITS = 999,
        MIST_SHOOTING_GALLERY_WEAPON_HELP_PANEL   = 45,
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
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_ARMOR_ITEM, 1);
            equipmentEquipCarriedArmor(MIST_SHOOTING_GALLERY_TRAINING_ARMOR_ITEM);
            inventoryGiveItem(carriedInventory, ammoItemId, MIST_SHOOTING_GALLERY_TRAINING_AMMO_UNITS)->attachSlot = rowInputMode;
            equipmentLoadWeaponConsumable(carriedInventory, weaponItemId, ammoItemId, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY);
            equipmentRestoreHpMp();
            object->result = USER_INTERFACE_RESULT_CONFIRM;
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&gItemMenuPanelDescriptors[MIST_SHOOTING_GALLERY_WEAPON_HELP_PANEL], weaponItemId, 1, 1, object);
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
///
/// Borrows the task-owned UI object and singleton list. Initialization identifies
/// and counts owned weapons; the row callback maps displayed rows to that same
/// filtered table. Completed child panels close before input returns to the picker.
static void _mistShootingGalleryWeaponSelectPanelTask(Task* task)
{
    enum { MIST_SHOOTING_GALLERY_WEAPON_MAX_VISIBLE_ROWS = 10 };
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
        if ((u8)ownedWeaponCount > MIST_SHOOTING_GALLERY_WEAPON_MAX_VISIBLE_ROWS) {
            list->visibleRowCount.unsignedValue = MIST_SHOOTING_GALLERY_WEAPON_MAX_VISIBLE_ROWS;
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
///
/// Borrows the task-owned object and live controller work. Nonzero kill counts
/// form the rows; negative per-kind points deduct from the total. A pending battle
/// reset suppresses a second panel. Confirm/cancel opens an unclaimed bonus child
/// or completes the panel; the child credits BP and reports confirmation.
static void _mistShootingGalleryResultPanelTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_RESULT_INIT             = 0,
        MIST_SHOOTING_GALLERY_RESULT_SUPPRESSED       = 0x100,
        MIST_SHOOTING_GALLERY_RESULT_TEXT_COLOR       = 0x606060,
        MIST_SHOOTING_GALLERY_COURSE_PRIZE_FLAG_FIRST = GAME_FLAG_SHOOTING_GALLERY_PRIZE_4_STATE - 4,
        MIST_SHOOTING_GALLERY_COURSE_BP_ONLY          = 1,
        MIST_SHOOTING_GALLERY_COURSE_PRIZE_WAITING    = 2,
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

    /// Sets a result line's colour, font and draw style above its owning panel.
    ///
    /// `request` is a writable TextDrawReq lvalue, evaluated five times; its X/Y
    /// placement is retained. `panelObject` is a live UiObject pointer, evaluated
    /// once. Selector arguments are evaluated once in field order and narrow to
    /// signed bytes. Arguments must be stable and side-effect-free; no captures.
#define MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(request, panelObject, glyph, align, mode) \
    {                                                                                         \
        (request).otIndex    = (panelObject)->panel.otIndex.signedValue + 1;                  \
        (request).colorRgb   = MIST_SHOOTING_GALLERY_RESULT_TEXT_COLOR;                       \
        (request).glyphTable = (glyph);                                                       \
        (request).alignment  = (align);                                                       \
        (request).drawMode   = (mode);                                                        \
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
                MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(nameText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
                textDrawString(&nameText, _gMistShootingGalleryTargetScores[targetKind].name);

                pointsText.x = object->panel.contentOriginX.unsignedValue + 0x6E + contentInset;
                pointsText.y = object->panel.contentOriginY.unsignedValue + rowY;
                MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(pointsText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
                textDrawString(&pointsText, textItoaSigned(numberText, pointsPerKill));

                killsText.x = object->panel.contentOriginX.unsignedValue + 0x91 + contentInset;
                killsText.y = object->panel.contentOriginY.unsignedValue + rowY;
                MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(killsText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
                textDrawString(&killsText, textItoaSigned(numberText, killCount));
                kindScore = killCount * pointsPerKill;

                subtotalText.x = object->panel.contentOriginX.unsignedValue - 5 - contentInset;
                subtotalText.y = object->panel.contentOriginY.unsignedValue + rowY;
                MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(subtotalText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
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
        MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(totalCaption, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
        textDrawString(&totalCaption, _gMistShootingGalleryTotalScoreCaption);

        totalText.x        = object->panel.contentOriginX.unsignedValue - 5 - contentInset;
        totalValueBaseline = object->panel.contentOriginY.unsignedValue - 4;
        totalText.y        = object->panel.contentBottom.unsignedValue + totalValueBaseline;
        MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(totalText, object, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&totalText, textItoaSigned(numberText, totalScore));

        uiDrawHorizontalSeparator(&object->panel, contentInset, -contentInset, object->panel.contentTop.signedValue + 0xA);

        rowY          = object->panel.contentTop.signedValue + 6;
        nameHeading.x = object->panel.contentOriginX.unsignedValue + 0x1E + contentInset;
        nameHeading.y = object->panel.contentOriginY.unsignedValue + rowY;
        MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(nameHeading, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_CENTER, TEXT_DRAW_OUTLINED);
        textDrawString(&nameHeading, "NMC");

        pointsHeading.x = object->panel.contentOriginX.unsignedValue + 0x73 + contentInset;
        pointsHeading.y = object->panel.contentOriginY.unsignedValue + rowY;
        MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(pointsHeading, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
        textDrawString(&pointsHeading, "SCORE");

        killsHeading.x = object->panel.contentOriginX.unsignedValue + 0x96 + contentInset;
        killsHeading.y = object->panel.contentOriginY.unsignedValue + rowY;
        MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(killsHeading, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
        textDrawString(&killsHeading, "KILL");

        subtotalHeading.x = object->panel.contentOriginX.unsignedValue - contentInset;
        subtotalHeading.y = object->panel.contentOriginY.unsignedValue + rowY;
        MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE(subtotalHeading, object, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
        textDrawString(&subtotalHeading, "TOTAL");
    }

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, "Result");

    if (task->state == MIST_SHOOTING_GALLERY_RESULT_INIT) {
        if (gGameSession->battleResetPending == 1) {
            uiStartPanelHiding(object, object->owner);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
            task->state    = MIST_SHOOTING_GALLERY_RESULT_SUPPRESSED;
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
                prizeFlag = work->course + MIST_SHOOTING_GALLERY_COURSE_PRIZE_FLAG_FIRST;
                if (gameFlagGetNibble(prizeFlag) == 0) {
                    if (mistShootingGalleryQualifiesForPrize(bonusBp) == panelPhase) {
                        gameFlagSetNibble(prizeFlag, MIST_SHOOTING_GALLERY_COURSE_PRIZE_WAITING);
                    } else {
                        gameFlagSetNibble(prizeFlag, MIST_SHOOTING_GALLERY_COURSE_BP_ONLY);
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
#undef MIST_SHOOTING_GALLERY_SET_RESULT_TEXT_STYLE
}

/// Credits the course's bonus BP once and draws its score and reward panel.
///
/// The first spawn argument is the score; the second borrows the task-owned UI
/// object. Requires live controller work for course selection. BP is capped at
/// 999999. Confirm or cancel completes the panel when input is active.
static void _mistShootingGalleryBonusPanelTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_BONUS_INIT        = 0,
        MIST_SHOOTING_GALLERY_BONUS_BP_MAX      = 999999,
        MIST_SHOOTING_GALLERY_BONUS_TEXT_COLOR  = 0x606060,
        MIST_SHOOTING_GALLERY_BONUS_VALUE_COLOR = 0x037A78,
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
    if (task->state == MIST_SHOOTING_GALLERY_BONUS_INIT) {
        bonusBp = mistShootingGalleryGetBonusBp(score);
        player  = &gPlayerStatus;
        if (bonusBp > 0) {
            newBp      = player->bp + bonusBp;
            player->bp = newBp;
            if (newBp > MIST_SHOOTING_GALLERY_BONUS_BP_MAX) {
                player->bp = MIST_SHOOTING_GALLERY_BONUS_BP_MAX;
            }
        }
        task->state = task->state + 1;
    }

    captionColor = MIST_SHOOTING_GALLERY_BONUS_TEXT_COLOR;
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
    bonusText.colorRgb   = MIST_SHOOTING_GALLERY_BONUS_VALUE_COLOR;
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
///
/// Borrowed text stays live with the gallery; the reader copies these four pointers.
static const _MistShootingGalleryModeTexts _gMistShootingGalleryModeNames = { { _gMistShootingGalleryReplayModeLabel, _gMistShootingGalleryBountyModeLabel, _gMistShootingGalleryScavengerModeLabel, _gMistShootingGalleryNightmareModeLabel } };

/// Offers the unlocked carryover modes with status, help and difficulty children.
///
/// Borrows the task-owned object and singleton list. Rank zero offers two modes,
/// rank one three, and higher ranks four; demo scene one exposes all four. The row
/// callback updates the live mode as selection moves; this panel confirms it.
static void _mistShootingGalleryModeSelectPanelTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_MODE_SELECT_INIT           = 0,
        MIST_SHOOTING_GALLERY_MODE_COUNT_BASE            = 2,
        MIST_SHOOTING_GALLERY_MODE_COUNT_RANK_ONE        = 3,
        MIST_SHOOTING_GALLERY_MODE_COUNT_ALL             = 4,
        MIST_SHOOTING_GALLERY_MODE_UNLOCK_NIGHTMARE_RANK = 2,
        MIST_SHOOTING_GALLERY_MODE_DEMO_SCENE            = 1,
        MIST_SHOOTING_GALLERY_MODE_STATUS_PANEL          = 0,
        MIST_SHOOTING_GALLERY_MODE_HELP_PANEL            = 1,
        MIST_SHOOTING_GALLERY_MODE_DATA_PANEL            = 2,
    };
    UiObject* object = task->spawnArg2.pointer;
    UiList*   list   = &_gMistShootingGalleryModeList;

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, "SELECT");
    // Replay rank unlocks the extra rows; the demo exposes every mode.
    if (task->state == MIST_SHOOTING_GALLERY_MODE_SELECT_INIT) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.replayRank == 0) {
            list->itemCount                     = MIST_SHOOTING_GALLERY_MODE_COUNT_BASE;
            list->visibleRowCount.unsignedValue = MIST_SHOOTING_GALLERY_MODE_COUNT_BASE;
        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.replayRank < MIST_SHOOTING_GALLERY_MODE_UNLOCK_NIGHTMARE_RANK) {
            list->itemCount                     = MIST_SHOOTING_GALLERY_MODE_COUNT_RANK_ONE;
            list->visibleRowCount.unsignedValue = MIST_SHOOTING_GALLERY_MODE_COUNT_RANK_ONE;
        } else {
            list->itemCount                     = MIST_SHOOTING_GALLERY_MODE_COUNT_ALL;
            list->visibleRowCount.unsignedValue = MIST_SHOOTING_GALLERY_MODE_COUNT_ALL;
        }
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == MIST_SHOOTING_GALLERY_MODE_DEMO_SCENE) {
            list->itemCount                     = MIST_SHOOTING_GALLERY_MODE_COUNT_ALL;
            list->visibleRowCount.unsignedValue = MIST_SHOOTING_GALLERY_MODE_COUNT_ALL;
        }
        list->selectedItemIndex = 0;
        uiFitPanelToList(list, &object->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSpawnObject(&_gMistShootingGalleryModeChildPanelDescs[MIST_SHOOTING_GALLERY_MODE_STATUS_PANEL], 0, 0, 1, object);
        uiSpawnObject(&_gMistShootingGalleryModeChildPanelDescs[MIST_SHOOTING_GALLERY_MODE_HELP_PANEL], 0, 0, 1, object);
        uiSpawnObject(&_gMistShootingGalleryModeChildPanelDescs[MIST_SHOOTING_GALLERY_MODE_DATA_PANEL], 0, 0, 1, object);
        task->state = task->state + 1;
    }
    uiUpdateList(list, &object->panel);
    if ((object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        object->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}
/// Converts an accumulated EXP or BP total to the selected run mode's carryover.
///
/// Reads the live save's mode: Replay divides by 10, Bounty by 20, Scavenger
/// by 100, and Nightmare returns zero. Other mode values use Replay's divisor.
/// Signed division truncates toward zero; only the upper limit of 999999 is
/// clamped. The STATUS panel and the closing sequence use the same result.
static inline s32 _mistShootingGalleryScaleReward(s32 unscaledTotal)
{
    enum {
        MIST_SHOOTING_GALLERY_REWARD_MODE_BOUNTY       = 1,
        MIST_SHOOTING_GALLERY_REWARD_MODE_SCAVENGER    = 2,
        MIST_SHOOTING_GALLERY_REWARD_MODE_NIGHTMARE    = 3,
        MIST_SHOOTING_GALLERY_REPLAY_REWARD_DIVISOR    = 10,
        MIST_SHOOTING_GALLERY_BOUNTY_REWARD_DIVISOR    = 20,
        MIST_SHOOTING_GALLERY_SCAVENGER_REWARD_DIVISOR = 100,
        MIST_SHOOTING_GALLERY_CARRYOVER_REWARD_MAX     = 999999
    };
    s32 reward;

    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
        case MIST_SHOOTING_GALLERY_REWARD_MODE_NIGHTMARE:
            return 0;
        case MIST_SHOOTING_GALLERY_REWARD_MODE_SCAVENGER:
            reward = unscaledTotal / MIST_SHOOTING_GALLERY_SCAVENGER_REWARD_DIVISOR;
            break;
        case MIST_SHOOTING_GALLERY_REWARD_MODE_BOUNTY:
            reward = unscaledTotal / MIST_SHOOTING_GALLERY_BOUNTY_REWARD_DIVISOR;
            break;
        default:
            reward = unscaledTotal / MIST_SHOOTING_GALLERY_REPLAY_REWARD_DIVISOR;
            break;
    }
    if (reward > MIST_SHOOTING_GALLERY_CARRYOVER_REWARD_MAX) {
        reward = MIST_SHOOTING_GALLERY_CARRYOVER_REWARD_MAX;
    }
    return reward;
}

/// Draws the selected run mode's starting HP/MP and scaled EXP/BP carryover.
///
/// Borrows the task-owned UI object. The live mode must be 0..3; HP below the
/// normal starting 100 or MP below 30 uses the warning colour. EXP/BP use the
/// captured totals, rather than the player totals being changed by the session.
static void _mistShootingGalleryModeStatusPanelTask(Task* task)
{
    enum { MIST_SHOOTING_GALLERY_BASE_HP_WARNING_THRESHOLD = 100,
           MIST_SHOOTING_GALLERY_BASE_MP_WARNING_THRESHOLD = 30 };
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
    ///
    /// Captures object, captionX and rowY in panel-relative pixels. caption
    /// must be a plain writable TextDrawReq local, evaluated repeatedly;
    /// captionText is evaluated once and borrowed through the draw call.
    /// Invoke inside a braced caller context; expansion is a compound statement.
#define MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(caption, captionText)                                                                \
    {                                                                                                                                  \
        enum { MIST_SHOOTING_GALLERY_STATUS_BASELINE_INSET = 8,                                                                        \
               MIST_SHOOTING_GALLERY_STATUS_TEXT_OT_BIAS   = 1 };                                                                        \
        (caption).x          = object->panel.contentOriginX.unsignedValue + captionX;                                                  \
        (caption).y          = (s16)(object->panel.contentOriginY.unsignedValue - MIST_SHOOTING_GALLERY_STATUS_BASELINE_INSET) + rowY; \
        (caption).otIndex    = object->panel.otIndex.signedValue + MIST_SHOOTING_GALLERY_STATUS_TEXT_OT_BIAS;                          \
        (caption).colorRgb   = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;                                                                  \
        (caption).glyphTable = TEXT_GLYPH_TABLE_SMALL;                                                                                 \
        (caption).alignment  = TEXT_ALIGNMENT_LEFT;                                                                                    \
        (caption).drawMode   = TEXT_DRAW_OUTLINED;                                                                                     \
        textDrawString(&(caption), captionText);                                                                                       \
    }

    object = task->spawnArg2.pointer;
    uiDrawTitle(&(object)->panel, "STATUS");
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(4));
        task->state = task->state + 1;
    }

    valueColor     = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;
    contentTop     = object->panel.contentTop.signedValue;
    rowY           = contentTop + 0xF;
    displayedValue = gEquipmentModeBaseStats[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode].baseHp.hpWord;
    captionX       = object->panel.contentLeft.signedValue + 6;
    if (displayedValue < MIST_SHOOTING_GALLERY_BASE_HP_WARNING_THRESHOLD) {
        valueColor = MIST_SHOOTING_GALLERY_MODE_WARNING_TEXT_COLOR;
    }

    MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(hpCaption, "HP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), valueColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY           = contentTop + 0x1E;
    displayedValue = gEquipmentModeBaseStats[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode].baseMp;
    valueColor     = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;
    if (displayedValue < MIST_SHOOTING_GALLERY_BASE_MP_WARNING_THRESHOLD) {
        valueColor = MIST_SHOOTING_GALLERY_MODE_WARNING_TEXT_COLOR;
    }

    MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(mpCaption, "MP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), valueColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY           = contentTop + 0x2D;
    displayedValue = _mistShootingGalleryScaleReward(gMistShootingGalleryUnscaledExp);

    MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(expCaption, "EXP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY          += 0xF;
    displayedValue = _mistShootingGalleryScaleReward(gMistShootingGalleryUnscaledBp);

    MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(bpCaption, "BP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
#undef MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION
}
/// Three-line help text indexed by run mode (0 Replay, 1 Bounty, 2 Scavenger, 3 Nightmare).
///
/// Borrowed text stays live with the gallery; the reader copies these four pointers.
static const _MistShootingGalleryModeTexts _gMistShootingGalleryModeDescriptions = { { _gMistShootingGalleryReplayModeDescription, _gMistShootingGalleryBountyModeDescription, _gMistShootingGalleryScavengerModeDescription, _gMistShootingGalleryNightmareModeDescription } };

/// Draws the selected run mode's mission, condition, enemy and supply ratings.
///
/// Borrows the task-owned UI object and reads the live mode in 0..3. The local
/// tables supply levels 1..5 and their names; each gauge contains that many marks.
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
    ///
    /// Captures object, rowY, rating and gaugeByLevel. Requests must be plain
    /// writable TextDrawReq locals; caption X/Y are already set. ratings is a
    /// four-entry table indexed by live gameMode 0..3; levels 1..5 select the
    /// six-entry gauge table. Table/text arguments are evaluated once, requests
    /// repeatedly. Text uses the font's byte view, borrowed through each draw.
    /// Invoke inside a braced caller context; expansion is a compound statement.
#define MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(caption, ratingText, ratings, captionText)                                                                                                                       \
    {                                                                                                                                                                                                           \
        enum { MIST_SHOOTING_GALLERY_RATING_RIGHT_X        = 65,                                                                                                                                                \
               MIST_SHOOTING_GALLERY_RATING_BASELINE_INSET = 3,                                                                                                                                                 \
               MIST_SHOOTING_GALLERY_RATING_GAUGE_X        = 70,                                                                                                                                                \
               MIST_SHOOTING_GALLERY_RATING_TEXT_OT_BIAS   = 1 };                                                                                                                                                 \
        (caption).otIndex    = object->panel.otIndex.signedValue + MIST_SHOOTING_GALLERY_RATING_TEXT_OT_BIAS;                                                                                                   \
        rating               = &(ratings)[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode];                                                                                                                   \
        (caption).colorRgb   = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;                                                                                                                                           \
        (caption).glyphTable = TEXT_GLYPH_TABLE_SMALL;                                                                                                                                                          \
        (caption).alignment  = TEXT_ALIGNMENT_LEFT;                                                                                                                                                             \
        (caption).drawMode   = TEXT_DRAW_OUTLINED;                                                                                                                                                              \
        textDrawString(&(caption), captionText);                                                                                                                                                                \
                                                                                                                                                                                                                \
        (ratingText).x          = object->panel.contentOriginX.unsignedValue + MIST_SHOOTING_GALLERY_RATING_RIGHT_X;                                                                                            \
        (ratingText).y          = (s16)(object->panel.contentOriginY.unsignedValue - MIST_SHOOTING_GALLERY_RATING_BASELINE_INSET) + rowY;                                                                       \
        (ratingText).otIndex    = object->panel.otIndex.signedValue + MIST_SHOOTING_GALLERY_RATING_TEXT_OT_BIAS;                                                                                                \
        (ratingText).colorRgb   = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;                                                                                                                                        \
        (ratingText).glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                                                                                                                                      \
        (ratingText).alignment  = TEXT_ALIGNMENT_RIGHT;                                                                                                                                                         \
        (ratingText).drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;                                                                                                                                               \
        textDrawString(&(ratingText), rating->name);                                                                                                                                                            \
        textDrawUiLine(object, MIST_SHOOTING_GALLERY_RATING_GAUGE_X, rowY, (const u8*)gaugeByLevel[rating->level], MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT); \
    }

    uiDrawTitle(&(object)->panel, "DATA");

    contentLeft      = object->panel.contentLeft.signedValue;
    object->result   = USER_INTERFACE_RESULT_NONE;
    captionX         = contentLeft + 0xB;
    contentTop       = object->panel.contentTop.signedValue;
    missionCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    rowY             = contentTop + 0xB;
    missionCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(missionCaption, missionRatingText, missionLevels, (const u8*)"MISSION LEVEL");
    uiDrawHorizontalSeparator(&(object)->panel, contentLeft + 6, -captionX + 5, contentTop + 0xD);

    rowY               = contentTop + 0x1E;
    conditionCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    conditionCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(conditionCaption, conditionRatingText, conditions, (const u8*)"CONDITION");

    rowY           = contentTop + 0x2D;
    enemyCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    enemyCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(enemyCaption, enemyRatingText, enemyLevels, (const u8*)"ENEMY LEVEL");

    rowY            = contentTop + 0x3C;
    supplyCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    supplyCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(supplyCaption, supplyRatingText, supplyLevels, (const u8*)_gMistShootingGallerySupplyLevelCaption);
#undef MIST_SHOOTING_GALLERY_DRAW_MODE_RATING
}
/// Runs carryover-mode selection and commits its previewed EXP/BP conversion.
///
/// Captures the live totals after creating SELECT and before row callbacks change
/// the mode. Confirmation recalculates maximum HP/MP, commits scaled carryover and
/// restores HP/MP. Once the closing countdown becomes negative it restores
/// 30 Hz display timing and ends the session. The overlay must stay loaded.
static void _mistShootingGalleryCarryoverModeSessionTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_CARRYOVER_OPEN            = 0,
        MIST_SHOOTING_GALLERY_CARRYOVER_SELECT          = 1,
        MIST_SHOOTING_GALLERY_CARRYOVER_CLOSE_COUNTDOWN = 10,
    };
    UiObject*     object;
    PlayerStatus* player = &gPlayerStatus;

    if (task->state == MIST_SHOOTING_GALLERY_CARRYOVER_OPEN) {
        object = uiSpawnObject(&_gMistShootingGalleryModeSelectPanelDesc, 0, 1, 1, NULL);
        if (object != NULL) {
            // Capture before the row callback starts changing the selected run mode.
            gMistShootingGalleryUnscaledBp  = player->bp;
            gMistShootingGalleryUnscaledExp = player->exp;
            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            task->spawnArg2.pointer = object;
            task->state             = task->state + 1;
        }
    } else if (task->state == MIST_SHOOTING_GALLERY_CARRYOVER_SELECT) {
        object = task->spawnArg2.pointer;
        if (object->result == USER_INTERFACE_RESULT_CONFIRM) {
            task->killCountdown = MIST_SHOOTING_GALLERY_CARRYOVER_CLOSE_COUNTDOWN;
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
    enum { MIST_SHOOTING_GALLERY_WEAPON_MENU_REQUEST = 0x44 };

    displayQueueModeTask(&_gMistShootingGalleryWeaponMenuTaskDesc, MIST_SHOOTING_GALLERY_WEAPON_MENU_REQUEST, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

/// Draws a run-mode row and selects its mode whenever the row has input.
///
/// The borrowed list's current row must be 0..3. Selection writes the live save
/// immediately as the cursor moves; the enclosing panel handles confirmation.
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
///
/// Uses `_mistShootingGalleryScaleReward`'s contract. Nothing calls this copy;
/// its instructions remain part of the room overlay.
static s32 _mistShootingGalleryScaleRewardOutOfLine(s32 unscaledTotal)
{
    return _mistShootingGalleryScaleReward(unscaledTotal);
}
/// Draws the three-line description of the selected run mode below its menu.
///
/// Borrows the task-owned UI object. The live mode must be 0..3; initialization
/// sizes and anchors the panel, and every tick reflects the mode row's selection.
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
    textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)modeDescriptions.byMode[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode], MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}
void mistShootingGalleryOpenCarryoverModeMenu(void)
{
    enum { MIST_SHOOTING_GALLERY_CARRYOVER_ENTRY_WARP = 7 };

    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) && (gGameSession->location.warp == MIST_SHOOTING_GALLERY_CARRYOVER_ENTRY_WARP)) {
        displayQueueModeTask(&_gMistShootingGalleryCarryoverModeSessionDesc, 0, 0, STAGE_ENTRY_RELOAD);
    }
}
/// Registers the gallery room task and prepares captions, barriers and entry events.
///
/// Initial state of the room controller: installs its borrowed message table,
/// selects loaded caption resource 2 at VRAM word X 832 and sets story-barrier
/// visibility. Demo scene 7 starts course 0; otherwise arrival 7 starts the
/// carryover actor task. Arrival 6 with the story flag set queues its CAP entry
/// command. Replaces flow flags with the area-music skip flag and advances state.
/// Requires both the gallery and actor-215100 overlays and caption resources live.
static void _mistShootingGalleryInitRoomTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_CAPTION_TEXTURE_X      = 832,
        MIST_SHOOTING_GALLERY_CAPTION_RESOURCE_INDEX = 2,
        MIST_SHOOTING_GALLERY_DEMO_SCENE             = 7,
        MIST_SHOOTING_GALLERY_CARRYOVER_WARP         = 7,
        MIST_SHOOTING_GALLERY_TRAINING_WARP          = 6,
        MIST_SHOOTING_GALLERY_STORY_ENTRY_COMMAND    = 22,
    };
    s32 storyBarrierLowered;

    task->msgTable = _gMistShootingGalleryRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    actor215100SelectCaptionResource(MIST_SHOOTING_GALLERY_CAPTION_TEXTURE_X, 0, MIST_SHOOTING_GALLERY_CAPTION_RESOURCE_INDEX);
    if (gameFlagGetNibble(GAME_FLAG_0ED) != 0) {
        sceneSetPlacedActorDrawMode(1, 0);
        storyBarrierLowered = 1;
    } else {
        storyBarrierLowered = 0;
    }
    _mistShootingGallerySetStoryBarrierLowered(storyBarrierLowered);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == MIST_SHOOTING_GALLERY_DEMO_SCENE) {
        taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER, 0, 0);
    } else if (gGameSession->location.warp == MIST_SHOOTING_GALLERY_CARRYOVER_WARP) {
        taskSpawnFromTable(gActor215100GalleryTaskDescs, ACTOR_215100_GALLERY_INTRO_TASK, 0, 0);
    }
    if ((gGameSession->location.warp == MIST_SHOOTING_GALLERY_TRAINING_WARP) && (gameFlagGetNibble(GAME_FLAG_0ED) != 0)) {
        capRunCommandWithTransition(MIST_SHOOTING_GALLERY_STORY_ENTRY_COMMAND);
    }
    gGameSession->flowFlags = GAME_SESSION_FLOW_SKIP_AREA_MUSIC;
    task->state             = task->state + 1;
}

/// Updates the gallery story actor visibility and checks training-exit input.
///
/// In variant 1 outside a session event, hides placed actor 1 in views 3, 9 and
/// 18; other views show it only while the story flag is clear. Checks exit input
/// every tick regardless of that visibility gate. The task argument is unused;
/// requires the live gallery, actor-215100 and player resources.
static void _mistShootingGalleryUpdateRoomTask(Task* unusedTask)
{
    enum {
        MIST_SHOOTING_GALLERY_STORY_VARIANT      = 1,
        MIST_SHOOTING_GALLERY_HIDE_ACTOR_VIEW_3  = 3,
        MIST_SHOOTING_GALLERY_HIDE_ACTOR_VIEW_9  = 9,
        MIST_SHOOTING_GALLERY_HIDE_ACTOR_VIEW_18 = 18,
    };
    u8 viewIndex;

    if ((gGameSession->location.variant == MIST_SHOOTING_GALLERY_STORY_VARIANT) && (gGameSession->eventState == 0)) {
        viewIndex = gGameSession->location.view;
        if ((viewIndex == MIST_SHOOTING_GALLERY_HIDE_ACTOR_VIEW_3) || (viewIndex == MIST_SHOOTING_GALLERY_HIDE_ACTOR_VIEW_9) || (viewIndex == MIST_SHOOTING_GALLERY_HIDE_ACTOR_VIEW_18)) {
            sceneSetPlacedActorDrawMode(1, 0);
        } else if (gameFlagGetNibble(GAME_FLAG_0ED) == 0) {
            sceneSetPlacedActorDrawMode(1, 1);
        }
    }
    actor215100CheckGalleryExitInput();
}
/// Plays one room CAP command, then restores player control and releases itself.
///
/// State 0 selects loaded CAP slot 3 when spawnArg2 is 3, otherwise slot 1;
/// spawnArg1 is that file's command index. State 1 waits for playback to finish,
/// and state 2 resumes the player, resets CAP state and kills the task.
/// The selected file and textures must remain loaded throughout playback.
static void _mistShootingGalleryCapPlaybackTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_CAP_PLAYBACK_START = 0,
        MIST_SHOOTING_GALLERY_CAP_PLAYBACK_WAIT  = 1,
        MIST_SHOOTING_GALLERY_CAP_PLAYBACK_EXIT  = 2,
    };
    s16 texturePageX;

    switch (task->state) {
        case MIST_SHOOTING_GALLERY_CAP_PLAYBACK_START:
            gCapFile = NULL;
            // Select the relocated CAP file and its VRAM texture-page origin.
            if (task->spawnArg2.value == MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS) {
                capSelectLoadedFile(MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS);
                texturePageX = MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_HIGH_COMMANDS;
            } else {
                capSelectLoadedFile(MIST_SHOOTING_GALLERY_CAP_FILE_LOW_COMMANDS);
                texturePageX = MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_LOW_COMMANDS;
            }
            capSetTexturePage(texturePageX, 0);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state += 1;
            return;
        case MIST_SHOOTING_GALLERY_CAP_PLAYBACK_WAIT:
            if (capIsBusy() != 0) {
                return;
            }
            task->state += 1;
            return;
        case MIST_SHOOTING_GALLERY_CAP_PLAYBACK_EXIT:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
            taskKill(task);
            break;
    }
}

/// Caption for the DATA panel's supply rating.
///
/// The three bytes after the NUL retain the overlay's fixed rodata layout.
static const char _gMistShootingGallerySupplyLevelCaption[16] = "SUPPLY LEVEL\0\xD0\x0E\xF0";

/// Rejects every key-item use request without changing room or inventory state.
///
/// Handles `MIST_SHOOTING_GALLERY_MESSAGE_USE_KEY_ITEM`; all arguments are
/// ignored. Returns zero, which makes the item menu show its unavailable-use
/// notice. The receiver and its message table must belong to this loaded room.
static s32 _mistShootingGalleryRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { MIST_SHOOTING_GALLERY_KEY_ITEM_UNUSABLE = 0 };

    return MIST_SHOOTING_GALLERY_KEY_ITEM_UNUSABLE;
}

/// Resolves gallery transitions and selects the training or carried loadout.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with borrowed eight-byte request and
/// writable reply records, which may alias. Copies the request; executing a
/// parking destination after chapter 0 adds two to its room. Gallery warp 5 can
/// return 2 through actor-owned confirmation, before any loadout change. Other
/// requests return 1. Executing gallery warp 6 initializes training equipment;
/// warp 5 restores carried equipment. Both select scene/player resources and
/// hide the HUD. Queries suppress those changes, and no payload is retained here.
static s32 _mistShootingGalleryResolveRoomEvent(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        MIST_SHOOTING_GALLERY_CARRIED_LOADOUT_WARP      = 5,
        MIST_SHOOTING_GALLERY_TRAINING_LOADOUT_WARP     = 6,
        MIST_SHOOTING_GALLERY_SCENE_CARRIED_LOADOUT     = 1,
        MIST_SHOOTING_GALLERY_SCENE_TRAINING_LOADOUT    = 2,
        MIST_SHOOTING_GALLERY_CARRIED_RESOURCE_VARIANT  = 3,
        MIST_SHOOTING_GALLERY_TRAINING_RESOURCE_VARIANT = 4,
        MIST_SHOOTING_GALLERY_PARKING_STORY_ROOM_OFFSET = 2,
    };
    *reply = *request;
    if (request->areaId == GAME_AREA_MIST_PARKING && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) != 0) {
            reply->roomState += MIST_SHOOTING_GALLERY_PARKING_STORY_ROOM_OFFSET;
        }
    }
    if (request->areaId == GAME_AREA_MIST_SHOOTING_GALLERY) {
        if (reply->warp == MIST_SHOOTING_GALLERY_CARRIED_LOADOUT_WARP && actor215100ResolveGalleryExit(request) == ACTOR_215100_GALLERY_EXIT_DEFER) {
            return ACTOR_215100_GALLERY_EXIT_DEFER;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (reply->warp == MIST_SHOOTING_GALLERY_TRAINING_LOADOUT_WARP) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = MIST_SHOOTING_GALLERY_SCENE_TRAINING_LOADOUT;
                gPlayerStatus.resourceVariant                       = MIST_SHOOTING_GALLERY_TRAINING_RESOURCE_VARIANT;
                gGameSession->hideHud                               = 1;
                inventoryInitializeShootingGalleryLoadout();
            }
            if (reply->warp == MIST_SHOOTING_GALLERY_CARRIED_LOADOUT_WARP) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = MIST_SHOOTING_GALLERY_SCENE_CARRIED_LOADOUT;
                gPlayerStatus.resourceVariant                       = MIST_SHOOTING_GALLERY_CARRIED_RESOURCE_VARIANT;
                gGameSession->hideHud                               = 1;
                inventoryRestoreCarriedLoadout();
            }
        }
    }
    return ACTOR_215100_GALLERY_EXIT_ALLOW;
}

/// Handles room command requests for CAP commands 5..8 and 33..34.
///
/// Holds player control and starts the matching CAP playback task: commands
/// 5..8 use loaded file slot 1, commands 33..34 slot 3. Other indices do nothing.
/// The receiver and second payload are unused. Returns the room-command reply 0,
/// including when task creation fails; control is resumed by the playback task.
static s32 _mistShootingGalleryHandleCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedSecondArg)
{
    enum {
        MIST_SHOOTING_GALLERY_LOW_COMMAND_FIRST  = 5,
        MIST_SHOOTING_GALLERY_LOW_COMMAND_END    = 9,
        MIST_SHOOTING_GALLERY_HIGH_COMMAND_FIRST = 33,
        MIST_SHOOTING_GALLERY_HIGH_COMMAND_END   = 35,
    };

    // Keep the nested bounds: their separate signed comparisons match dispatch.
    if (commandIndex >= MIST_SHOOTING_GALLERY_LOW_COMMAND_FIRST) {
        if (commandIndex >= MIST_SHOOTING_GALLERY_LOW_COMMAND_END) {
            if (commandIndex < MIST_SHOOTING_GALLERY_HIGH_COMMAND_END) {
                if (commandIndex >= MIST_SHOOTING_GALLERY_HIGH_COMMAND_FIRST) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(&_gMistShootingGalleryCapPlaybackTaskDesc, 0, commandIndex, MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS);
                }
            }
        } else {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&_gMistShootingGalleryCapPlaybackTaskDesc, 0, commandIndex, MIST_SHOOTING_GALLERY_CAP_FILE_LOW_COMMANDS);
        }
    }
    return 0;
}

/// Dispatches the gallery course-menu, Pierce-talk, training and story action points.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed typed request. Action
/// 1 holds player control and queues the course menu while training is inactive;
/// 2 starts Pierce talk before the story flag; 3 signals the training interaction;
/// 4 starts its story event once and sets objective 59. Menu action rearms input
/// for ten ticks. Ignores the other payload fields and arguments, retains no
/// request and returns zero. Requires both loaded gallery and actor overlays.
static s32 _mistShootingGalleryHandleRoomAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        MIST_SHOOTING_GALLERY_ACTION_REARM_TICKS = 10,
        MIST_SHOOTING_GALLERY_STORY_OBJECTIVE    = 59,
    };
    if ((request->actionId == MIST_SHOOTING_GALLERY_ACTION_OPEN_COURSE_MENU) && (gActor215100GalleryTrainingActive == 0)) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(gActor215100GalleryTaskDescs, ACTOR_215100_GALLERY_MENU_TASK, 1, 0);
        gDirectionInteractionRearmUpdates = MIST_SHOOTING_GALLERY_ACTION_REARM_TICKS;
    }
    if ((request->actionId == MIST_SHOOTING_GALLERY_ACTION_TALK_TO_PIERCE) && (gameFlagGetNibble(GAME_FLAG_0ED) == 0)) {
        actor215100StartPierceConversation();
    }
    if (request->actionId == MIST_SHOOTING_GALLERY_ACTION_TRAINING_POINT) {
        actor215100HandleGalleryAction();
    }
    if ((request->actionId == MIST_SHOOTING_GALLERY_ACTION_STORY_EVENT) && (gameFlagGetNibble(GAME_FLAG_SHOOTING_GALLERY_ACTION_4_SEEN) == 0)) {
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, MIST_SHOOTING_GALLERY_STORY_OBJECTIVE);
        gameFlagSetNibble(GAME_FLAG_SHOOTING_GALLERY_ACTION_4_SEEN, 1);
        evsStartScriptWithSkip(gActor215100PierceStoryScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor215100PierceStorySkipScript);
    }
    return 0;
}

/// Room task handlers in state order: initialization, running, then teardown.
///
/// Dispatch copies all three callback pointers to the stack before indexing
/// with task state 0..2. There is no terminator or bounds check; handlers and
/// the table belong to the loaded gallery overlay.
static const TaskFuncTable3 _gMistShootingGalleryRoomStates = {
    { _mistShootingGalleryInitRoomTask, _mistShootingGalleryUpdateRoomTask, taskKill },
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
///
/// Zero leaves the template position; nonzero lowers it below the floor.
/// Borrows the loaded collision grid's first three normals/faces and eight
/// vertices. The entry handler chooses the lowered position after the story flag.
static void _mistShootingGallerySetStoryBarrierLowered(s32 lowered)
{
    enum { MIST_SHOOTING_GALLERY_STORY_BARRIER_FACE_COUNT   = 3,
           MIST_SHOOTING_GALLERY_STORY_BARRIER_VERTEX_COUNT = 8,
           MIST_SHOOTING_GALLERY_STORY_BARRIER_LOWER_Y      = 3000 };
    WorldCollisionGrid*       liveGrid     = &gMistShootingGalleryCollisionGrid;
    const WorldCollisionGrid* templateGrid = &_gMistShootingGalleryStoryBarrierCollisionTemplate;
    SVECTOR                   offset;
    s32                       index;

    for (index = 0; index < MIST_SHOOTING_GALLERY_STORY_BARRIER_FACE_COUNT; index++) {
        liveGrid->normals[index].vx = templateGrid->normals[index].vx;
        liveGrid->normals[index].vy = templateGrid->normals[index].vy;
        liveGrid->normals[index].vz = templateGrid->normals[index].vz;
        liveGrid->faces[index]      = templateGrid->faces[index];
    }
    for (index = 0; index < MIST_SHOOTING_GALLERY_STORY_BARRIER_VERTEX_COUNT; index++) {
        liveGrid->vertices[index].vx = templateGrid->vertices[index].vx;
        liveGrid->vertices[index].vy = templateGrid->vertices[index].vy;
        liveGrid->vertices[index].vz = templateGrid->vertices[index].vz;
    }
    if (lowered == 0) {
        offset.vx = 0;
        offset.vy = 0;
    } else {
        offset.vx = 0;
        offset.vy = MIST_SHOOTING_GALLERY_STORY_BARRIER_LOWER_Y;
    }
    offset.vz = 0;
    for (index = 0; index < MIST_SHOOTING_GALLERY_STORY_BARRIER_VERTEX_COUNT; index++) {
        liveGrid->vertices[index].vx += offset.vx;
        liveGrid->vertices[index].vy += offset.vy;
        liveGrid->vertices[index].vz += offset.vz;
    }
}

void mistShootingGallerySetTrainingBarrierLowered(s32 lowered)
{
    enum { MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET = 3,
           MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET = 8,
           MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_COUNT  = 4,
           MIST_SHOOTING_GALLERY_TRAINING_BARRIER_OFFSET_COUNT  = 8,
           MIST_SHOOTING_GALLERY_TRAINING_BARRIER_LOWER_Y       = 4000 };
    WorldCollisionGrid*       liveGrid     = &gMistShootingGalleryCollisionGrid;
    const WorldCollisionGrid* templateGrid = &_gMistShootingGalleryTrainingBarrierCollisionTemplate;
    WorldCollisionGridFace*   liveFace     = &gMistShootingGalleryCollisionGrid.faces[MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET];
    WorldCollisionGridFace*   templateFace = _gMistShootingGalleryTrainingBarrierCollisionTemplate.faces;
    SVECTOR                   offset;
    s32                       index;
    s32                       corner;

    for (index = 0; index < (s32)ARRAY_SIZE(_gMistShootingGalleryTrainingBarrierCollisionTemplateFaces); index++) {
        liveGrid->normals[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET].vx = templateGrid->normals[index].vx;
        liveGrid->normals[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET].vy = templateGrid->normals[index].vy;
        liveGrid->normals[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET].vz = templateGrid->normals[index].vz;
        for (corner = 0; corner < ARRAY_SIZE(liveFace->vertexIndices); corner++) {
            liveFace->vertexIndices[corner] = templateFace->vertexIndices[corner] + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET;
        }
        liveFace->normalIndex  = templateFace->normalIndex + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET;
        liveFace->surfaceClass = templateFace->surfaceClass;
        liveFace++;
        templateFace++;
    }
    for (index = 0; index < MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_COUNT; index++) {
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vx = templateGrid->vertices[index].vx;
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vy = templateGrid->vertices[index].vy;
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vz = templateGrid->vertices[index].vz;
    }
    if (lowered == 0) {
        offset.vx = 0;
        offset.vy = 0;
    } else {
        offset.vx = 0;
        offset.vy = MIST_SHOOTING_GALLERY_TRAINING_BARRIER_LOWER_Y;
    }
    offset.vz = 0;
    // Preserve the eight-vertex offset pass: only the first four were restored.
    for (index = 0; index < MIST_SHOOTING_GALLERY_TRAINING_BARRIER_OFFSET_COUNT; index++) {
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vx += offset.vx;
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vy += offset.vy;
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vz += offset.vz;
    }
}

#define ROOM_PART _gJukeboxTrackLists
#include "assets/rooms/mist/shooting_gallery/jukebox.inc"
/// SELECT title for the jukebox panel, ending at its first NUL.
///
/// The eighth stored byte is 0xE1 and is not rendered; its role is unproven.
static const char _gMistShootingGalleryJukeboxTitle[8] = "SELECT\0\xE1";

#include "../../shared/room/jukebox_row.inc.c"

/// Updates the gallery jukebox's track picker and asynchronous music change.
///
/// Borrows the task-owned UI object and singleton track list (three training
/// rows, four regular rows). The row callback supplies a pending sequence ID;
/// phase 1 waits for MIDI to stop, phase 2 waits for its file load, then starts it.
/// A cancel during loading hides the panel until playback starts. The initial
/// training sentinel keeps the panel open until a track has been selected.
static void _mistShootingGalleryJukeboxPanelTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_JUKEBOX_INITIALIZE          = 0,
        MIST_SHOOTING_GALLERY_JUKEBOX_WAIT_MIDI           = 1,
        MIST_SHOOTING_GALLERY_JUKEBOX_TRACK_ID_LIMIT      = 0xF1,
        MIST_SHOOTING_GALLERY_JUKEBOX_TRAINING_UNSELECTED = 0xFE,
        MIST_SHOOTING_GALLERY_JUKEBOX_IDLE                = 0xFF,
        MIST_SHOOTING_GALLERY_JUKEBOX_NO_ROW              = -1,
        MIST_SHOOTING_GALLERY_JUKEBOX_MAX_VISIBLE_ROWS    = 10,
        MIST_SHOOTING_GALLERY_JUKEBOX_FILE_GROUP          = 4,
        MIST_SHOOTING_GALLERY_JUKEBOX_FILE_ID_HUNDREDS    = 1
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
    if (task->state == MIST_SHOOTING_GALLERY_JUKEBOX_INITIALIZE) {
        task->spawnArg1.value = MIST_SHOOTING_GALLERY_JUKEBOX_NO_ROW;
        if (attachmentIsTrainingMode() == 0) {
            list->itemCount = ARRAY_SIZE(_gMistShootingGalleryJukeboxRegularReplayTracks);
        } else {
            list->itemCount = ARRAY_SIZE(_gMistShootingGalleryJukeboxTrainingReplayTracks);
        }
        if (list->itemCount > MIST_SHOOTING_GALLERY_JUKEBOX_MAX_VISIBLE_ROWS) {
            list->visibleRowCount.unsignedValue = MIST_SHOOTING_GALLERY_JUKEBOX_MAX_VISIBLE_ROWS;
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
            task->status = MIST_SHOOTING_GALLERY_JUKEBOX_IDLE;
        } else {
            task->status = MIST_SHOOTING_GALLERY_JUKEBOX_TRAINING_UNSELECTED;
        }
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    sequenceId = task->status;
    if (sequenceId < MIST_SHOOTING_GALLERY_JUKEBOX_TRACK_ID_LIMIT) {
        // Stop the old sequence before loading its replacement; start after CD idle.
        phase = task->state;
        if (phase == MIST_SHOOTING_GALLERY_JUKEBOX_WAIT_MIDI) {
            if (midiIsSequenceBusy(0) == 0) {
                fileKey[3]  = 0;
                fileKey[2]  = MIST_SHOOTING_GALLERY_JUKEBOX_FILE_GROUP;
                fileKey[0]  = sequenceId;
                loadArgs[0] = MIST_SHOOTING_GALLERY_JUKEBOX_FILE_ID_HUNDREDS;
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
                task->state  = MIST_SHOOTING_GALLERY_JUKEBOX_WAIT_MIDI;
                task->status = MIST_SHOOTING_GALLERY_JUKEBOX_IDLE;
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
            if (task->status != MIST_SHOOTING_GALLERY_JUKEBOX_TRAINING_UNSELECTED) {
                if (task->status == MIST_SHOOTING_GALLERY_JUKEBOX_IDLE) {
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    uiStartPanelHiding(object, object->owner);
                    object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        }
    }
}

#include "../../shared/room/jukebox_host.inc.c"

s32 mistShootingGalleryOpenJukebox(s32 unused)
{
    displayQueueModeTask(&_gMistShootingGalleryJukeboxSessionDesc, 0, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

/// Queues a 320x240 subtractive tile for the mode splash's current ramp tick.
///
/// Nonzero `fadeIn` complements the low countdown byte; zero uses it directly
/// for all RGB channels. Requires a live frame packet arena with room for a TILE
/// and DR_TPAGE at ordering-table entry 0. The current draw origin is retained,
/// including shake; packets live until GPU completion. Prepending the mode
/// after the tile makes subtractive blending and dithering active before drawing.
static inline void _mistShootingGalleryDrawSplashFade(const Task* task, s32 fadeIn)
{
    enum {
        MIST_SHOOTING_GALLERY_SPLASH_FADE_WIDTH_PIXELS       = 320,
        MIST_SHOOTING_GALLERY_SPLASH_FADE_HEIGHT_PIXELS      = 240,
        MIST_SHOOTING_GALLERY_SPLASH_FADE_TEXTURE_DEPTH_4BIT = 0,
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
    tile->x0 = -MIST_SHOOTING_GALLERY_SPLASH_FADE_WIDTH_PIXELS / 2;
    tile->y0 = -MIST_SHOOTING_GALLERY_SPLASH_FADE_HEIGHT_PIXELS / 2;
    tile->w  = MIST_SHOOTING_GALLERY_SPLASH_FADE_WIDTH_PIXELS;
    tile->h  = MIST_SHOOTING_GALLERY_SPLASH_FADE_HEIGHT_PIXELS;

    addPrim(gGpuCurrentOt, tile);
    // Insertion prepends, so the mode queued last runs before the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(MIST_SHOOTING_GALLERY_SPLASH_FADE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt, drawMode);
}

/// Displays the cleared run's mode splash with a fade in, hold and fade out.
///
/// The live mode must be 0..3, selecting stage-zero image file 41..44. Borrows
/// the serialized CD/display session and frame arena. Ramp steps are 8 per tick;
/// the hold ends after 151 ticks or Start. Completion blanks the display, clears
/// the complete image buffer and resumes the game loop after killing this task.
static void _mistShootingGalleryModeSplashTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_SPLASH_QUEUE_FILE               = 0,
        MIST_SHOOTING_GALLERY_SPLASH_WAIT_FILE                = 1,
        MIST_SHOOTING_GALLERY_SPLASH_FADE_IN                  = 2,
        MIST_SHOOTING_GALLERY_SPLASH_HOLD                     = 3,
        MIST_SHOOTING_GALLERY_SPLASH_FADE_OUT                 = 4,
        MIST_SHOOTING_GALLERY_SPLASH_FINISH                   = 5,
        MIST_SHOOTING_GALLERY_SPLASH_REPLAY_FILE              = 41,
        MIST_SHOOTING_GALLERY_SPLASH_BOUNTY_FILE              = 42,
        MIST_SHOOTING_GALLERY_SPLASH_SCAVENGER_FILE           = 43,
        MIST_SHOOTING_GALLERY_SPLASH_NIGHTMARE_FILE           = 44,
        MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_STEP           = 8,
        MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_END            = 256,
        MIST_SHOOTING_GALLERY_SPLASH_DISPLAY_ENABLE_THRESHOLD = 17,
        MIST_SHOOTING_GALLERY_SPLASH_HOLD_TICKS               = 151
    };
    u8 fileKey[4];
    u8 loadArgs[4];

    switch (task->state) {
        case MIST_SHOOTING_GALLERY_SPLASH_QUEUE_FILE:
            fileKey[3]  = 0;
            fileKey[2]  = 0;
            loadArgs[0] = 0;
            loadArgs[1] = 0;
            loadArgs[2] = 0;
            loadArgs[3] = 0;
            switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
                case 0:
                    fileKey[0] = MIST_SHOOTING_GALLERY_SPLASH_REPLAY_FILE;
                    break;
                case 1:
                    fileKey[0] = MIST_SHOOTING_GALLERY_SPLASH_BOUNTY_FILE;
                    break;
                case 2:
                    fileKey[0] = MIST_SHOOTING_GALLERY_SPLASH_SCAVENGER_FILE;
                    break;
                case 3:
                    fileKey[0] = MIST_SHOOTING_GALLERY_SPLASH_NIGHTMARE_FILE;
                    break;
            }
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs);
            task->state++;
            return;

        case MIST_SHOOTING_GALLERY_SPLASH_WAIT_FILE:
            if (cdCmdIsIdle() & 0xFFFF) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                task->killCountdown                     = 0;
                task->state++;
                return;
            }
            return;

        case MIST_SHOOTING_GALLERY_SPLASH_FADE_IN: {
            // Keep display blank until the initial black overlay has started fading.
            _mistShootingGalleryDrawSplashFade(task, true);

            task->killCountdown += MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_STEP;
            if (task->killCountdown >= MIST_SHOOTING_GALLERY_SPLASH_DISPLAY_ENABLE_THRESHOLD) {
                SetDispMask(1);
            }
            if (task->killCountdown < MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_END) {
                return;
            }
            task->killCountdown = 0;
            task->state++;
            return;
        }

        case MIST_SHOOTING_GALLERY_SPLASH_HOLD:
            task->killCountdown += 1;
            if (task->killCountdown < MIST_SHOOTING_GALLERY_SPLASH_HOLD_TICKS && padIsStartPressed() == 0) {
                return;
            }
            task->killCountdown = 0;
            task->state++;
            return;

        case MIST_SHOOTING_GALLERY_SPLASH_FADE_OUT: {
            _mistShootingGalleryDrawSplashFade(task, false);

            task->killCountdown += MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_STEP;
            if (task->killCountdown < MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_END) {
                return;
            }
            memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
            SetDispMask(0);
            task->state++;
            return;
        }

        case MIST_SHOOTING_GALLERY_SPLASH_FINISH:
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            taskKill(task);
            displayResumeGameLoop();
            return;

        default:
            return;
    }
}

/// Plays the gallery's post-clear movie and restores game graphics afterwards.
///
/// The current room must have a loaded view-100, sub-ID-zero movie slot. The
/// display/CD session must be serialized; this task reserves movie workspace,
/// waits for startup, accepts Start to cancel, then waits for CD idle and game
/// restoration. Completion clears the full image buffer and resumes the loop.
static void _mistShootingGalleryClearMovieTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_MOVIE_PREPARE    = 0,
        MIST_SHOOTING_GALLERY_MOVIE_QUEUE      = 1,
        MIST_SHOOTING_GALLERY_MOVIE_WAIT_READY = 2,
        MIST_SHOOTING_GALLERY_MOVIE_PLAY       = 3,
        MIST_SHOOTING_GALLERY_MOVIE_WAIT_STOP  = 4,
        MIST_SHOOTING_GALLERY_MOVIE_RESTORE    = 5,
        MIST_SHOOTING_GALLERY_CLEAR_MOVIE_VIEW = 100
    };
    u8              streamArgs[4];
    GameLocationKey movieLocation;
    s16             movieSlot;
    CdCmdQueue*     cdQueue;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case MIST_SHOOTING_GALLERY_MOVIE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_QUEUE:
            movieLocation      = gGameSession->location;
            movieLocation.view = MIST_SHOOTING_GALLERY_CLEAR_MOVIE_VIEW;
            movieSlot          = streamFindMovieSlot(&movieLocation, 0, 0);
            streamArgs[0]      = movieSlot;
            // This opcode consumes only the slot byte; its other bytes are retained.
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_WAIT_READY:
            if (cdQueue->movieReady == 0) {
                break;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_PLAY:
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
        case MIST_SHOOTING_GALLERY_MOVIE_WAIT_STOP:
            // Do not release the decoder workspace until playback/cancellation ends.
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_RESTORE:
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
///
/// The three states start presentation, resume after the splash, then queue the
/// movie. The splash is skipped without a clear. Display children serialize both
/// presentations; the movie child restores graphics and resumes the game loop.
static void _mistShootingGalleryMovieSessionTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_MOVIE_SESSION_START       = 0,
        MIST_SHOOTING_GALLERY_MOVIE_SESSION_WAIT_SPLASH = 1,
        MIST_SHOOTING_GALLERY_MOVIE_SESSION_PLAY        = 2,
        MIST_SHOOTING_GALLERY_MOVIE_TASK_INDEX          = 1,
        MIST_SHOOTING_GALLERY_MODE_SPLASH_TASK_INDEX    = 2,
    };
    switch (task->state) {
        case MIST_SHOOTING_GALLERY_MOVIE_SESSION_START:
            SetDispMask(0);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount == 0) {
                task->state = MIST_SHOOTING_GALLERY_MOVIE_SESSION_PLAY;
                return;
            }
            // The display child owns the splash before this session runs again.
            displaySpawnTaskFromTable(gMistShootingGalleryMovieTaskDescs, MIST_SHOOTING_GALLERY_MODE_SPLASH_TASK_INDEX, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_FULL;
            viewQueueCurrentCameraAndPackets();
        case MIST_SHOOTING_GALLERY_MOVIE_SESSION_WAIT_SPLASH:
            task->state = task->state + 1;
            return;
        case MIST_SHOOTING_GALLERY_MOVIE_SESSION_PLAY:
            displaySpawnTaskFromTable(gMistShootingGalleryMovieTaskDescs, MIST_SHOOTING_GALLERY_MOVIE_TASK_INDEX, 0, 0);
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
        MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE = 0x200,
        MIST_SHOOTING_GALLERY_CAPSULE_COLOR        = 0x222,
        MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE    = 0x300,
        MIST_SHOOTING_GALLERY_DISC_COLOR           = 0x111
    };
    u8 mappedView;

    // View mapping selects the visible strips and point glows in world space.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[0], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[8], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[10], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            break;
        case 3:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[4], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[6], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[14], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[16], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[18], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            break;
        case 7:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[14], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[16], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[18], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[20], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            break;
        case 8:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[24], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            break;
        case 9:
        case 18:
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[20], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[22], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[26], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&_gMistShootingGalleryLightGlowPoints[28], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[36], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[44], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 10:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[37], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[39], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[41], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[43], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 11:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[38], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[39], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[41], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[42], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 12:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[40], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[41], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 13:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[39], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 14:
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[36], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[37], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[39], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[41], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[43], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&_gMistShootingGalleryLightGlowPoints[44], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/glow_draw_disc.inc.c"
