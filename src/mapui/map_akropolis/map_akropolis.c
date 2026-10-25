#include "mapui/map_akropolis.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/map.h"
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag_ids.h"
#include "main/gfx_types.h"
#include "main/pad.h"
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

#include "mappic/mappic.h"

#include "stages/akropolis/rooms/bridge.h"

#include "stages/akropolis/rooms/cafeteria.h"

#include "stages/akropolis/rooms/east_elevator_hall.h"

#include "stages/akropolis/rooms/fire_escape.h"

#include "stages/akropolis/rooms/forked_road.h"

#include "stages/akropolis/rooms/fountain.h"

#include "stages/akropolis/rooms/hallway.h"

#include "stages/akropolis/rooms/helicopter_landing_pad.h"

#include "stages/akropolis/rooms/observatory.h"

#include "stages/akropolis/rooms/patio.h"

#include "stages/akropolis/rooms/plaza.h"

#include "stages/akropolis/rooms/promenade.h"

#include "stages/akropolis/rooms/roof_garden.h"

#include "stages/akropolis/rooms/sanctuary.h"

#include "stages/akropolis/rooms/security_room.h"

#include "stages/akropolis/rooms/square.h"

#include "stages/akropolis/rooms/west_elevator_hall.h"

#include "stages/akropolis/rooms/mist_parking.h"

#include "stages/akropolis/rooms/mist_r18.h"

#include "stages/akropolis/rooms/mist_r21.h"

#include "stages/akropolis/rooms/mist_shooting_gallery.h"

#include "assets/rooms/akropolis/declarations.inc"

/* The Akropolis stage's map UI overlay (stage 1, Akropolis and MIST): its
 * stream setup, the key-item panel, and the per-stage tables gameplay and main
 * index by stage, most of which point into the stage's room packages or at the
 * map pictures' marker models.
 */

/// Placement-entry keys matched against each selected room variant's resource list.
///
/// Keys identify recipes within that list, rather than actor-file numbers.
/// UNUSED retains a record position without matching these variants' recipes.
enum {
    MAP_AKROPOLIS_PLACEMENT_ENTRY_UNUSED            = 0,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT               = 7,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH              = 8,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER          = 10,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_MOSSBACK          = 11,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_MIND_SUCKLER      = 15,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SCAVENGER         = 16,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER      = 18,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER = 19,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_PAWN_GOLEM        = 20,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_KNIGHT_GOLEM      = 22,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_AMOEBA            = 24,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SCORPION          = 25,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR       = 26,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_NO9_GOLEM         = 27,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT               = 37,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_BLACK_BEETLE      = 38,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_BRIDGE_ENEMY      = 41,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SUCKLERCEPH       = 46,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SKULL_STALKER     = 47,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_LARGE_MOSSBACK    = 49,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT            = 55,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SLOUCH            = 70,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_CREEPING_STRANGER = 72,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_RUPERT            = 102,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_VIEW_FIGURE       = 110,
};

/// Actor-specific placement modes and variant spawn selectors used below.
///
/// Crawlers use the decimal entrance suffix for leap/drop and variant 0..3
/// for their entrance slot; the Pawn uses variant as patrol distance in thousands
/// of world units. These constants retain the records' byte/halfword storage.
enum {
    MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT                = 0,
    MAP_AKROPOLIS_PLACEMENT_CRAWLER_AIMED                  = 1,
    MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_LEADER            = 2,
    MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_FOLLOWER          = 3,
    MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE          = 10,
    MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE          = 11,
    MAP_AKROPOLIS_PLACEMENT_SCORPION_ROAM                  = 0,
    MAP_AKROPOLIS_PLACEMENT_SCORPION_AMBUSH_LEADER         = 1,
    MAP_AKROPOLIS_PLACEMENT_BAT_PERCH_DROP                 = 0,
    MAP_AKROPOLIS_PLACEMENT_BAT_PERCH_BACK                 = 1,
    MAP_AKROPOLIS_PLACEMENT_BAT_WANDER                     = 2,
    MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE          = 30,
    MAP_AKROPOLIS_PLACEMENT_AMOEBA_VARIANT_NORMAL          = 0,
    MAP_AKROPOLIS_PLACEMENT_AMOEBA_VARIANT_ALTERNATE       = 1,
    MAP_AKROPOLIS_PLACEMENT_BLACK_BEETLE_IDLE              = 0,
    MAP_AKROPOLIS_PLACEMENT_BLACK_BEETLE_WANDER            = 1,
    MAP_AKROPOLIS_PLACEMENT_MIND_SUCKLER_PERCH             = 1,
    MAP_AKROPOLIS_PLACEMENT_PAWN_GOLEM_PATROL              = 1,
    MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT      = 4,
    MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_NEAR        = 0,
    MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_RUN_OR_NEAR = 1,
};

/// Additional resource-list keys used by the Acropolis actor layouts.
///
/// Keys select recipes within the paired variant, not actor file IDs. Entry 9
/// has no recipe in Patio variant 10. Entry 110 selects a SWAT actor in the
/// Security room, independently of the East elevator hall's view figure.
enum {
    MAP_AKROPOLIS_PLACEMENT_ENTRY_PATIO_UNMATCHED = 9,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_BONE_SUCKLER    = 12,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_WOMAN           = 29,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_ROOK_GOLEM      = 57,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SWAT_MEMBER     = 107,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_OFFICER         = 108,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SERGEANT        = 109,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SECURITY_SWAT   = 110,
};

/// Spawn selectors interpreted by the actors used in these layouts.
///
/// Stranger behavior occupies the placement's variant byte. STALK combines
/// with its low-nibble opening state. Boss movement tuning is in mode bits
/// 4..7. Bone Suckler variant 1 starts idle; the Rook's mode bit 0 patrols and
/// its variant measures patrol length in thousands of world units.
enum {
    MAP_AKROPOLIS_PLACEMENT_STRANGER_TUNING_FIRST = 2,
    MAP_AKROPOLIS_PLACEMENT_ODD_STRANGER_STALK    = 0x10,
    MAP_AKROPOLIS_PLACEMENT_BOSS_HIDDEN_RESTORED  = 1,
    MAP_AKROPOLIS_PLACEMENT_BOSS_MOVEMENT_LARGEST = 0x20,
    MAP_AKROPOLIS_PLACEMENT_BONE_SUCKLER_PATROL   = 0,
    MAP_AKROPOLIS_PLACEMENT_BONE_SUCKLER_IDLE     = 1,
    MAP_AKROPOLIS_PLACEMENT_ROOK_GOLEM_PATROL     = 1,
};

static void _mapAkropolisDrawKeyItemRow(UiList* list, UiObject* object);
static void _mapAkropolisKeyItemPanelTask(Task* task);
static void _mapAkropolisKeyItemMenuTask(Task* task);

/// Catalog row count, task priority and panel spacing of the briefing menu.
enum {
    MAP_AKROPOLIS_BRIEFING_ITEM_COUNT        = 4,
    MAP_AKROPOLIS_KEY_ITEM_ROW_HEIGHT_PIXELS = 15,
};

static const char   _gMapAkropolisKeyItemPanelTitle[12];
static s32          _gMapAkropolisDryfieldMapOpened;
static s32          _gMapAkropolisBriefingItemIds[MAP_AKROPOLIS_BRIEFING_ITEM_COUNT];
static UiList       _gMapAkropolisKeyItemList;
static UiObjectDesc _gMapAkropolisKeyItemPanelDescriptor;
static TaskDesc     _gMapAkropolisKeyItemMenuTaskDescriptor;

/// Movie ring byte extent and decoded-column staging origins in VRAM words/rows.
enum {
    MAP_AKROPOLIS_MOVIE_RING_BYTES               = 0x10000,
    MAP_AKROPOLIS_MOVIE_PLAZA_STAGING_X_WORDS    = 704,
    MAP_AKROPOLIS_MOVIE_FOUNTAIN_STAGING_X_WORDS = 384,
    MAP_AKROPOLIS_MOVIE_FOUNTAIN_STAGING_Y_ROWS  = 256,
};

/// Lifecycle steps and teardown delay of the key-item display-mode task.
enum {
    MAP_AKROPOLIS_KEY_ITEM_MENU_START       = 0,
    MAP_AKROPOLIS_KEY_ITEM_MENU_ACTIVE      = 1,
    MAP_AKROPOLIS_KEY_ITEM_MENU_CLOSING     = 2,
    MAP_AKROPOLIS_KEY_ITEM_MENU_CLOSE_TICKS = 10,
};

/// Places the VLC-expanded input buffers and two RGB16 decoded frames.
///
/// `decodedFrameBytes` is width in pixels * height in rows * two bytes, positive
/// and word-aligned. The movie workspace reserves that many bytes for input
/// slot 1, followed by two frames; input slot 0 starts after the actor region's
/// 64 KiB ring. Both regions must fit their VLC output and stay available until
/// playback and pending DMA finish. Only pointers are assigned; no allocation,
/// clearing or capacity check occurs.
static inline void _mapAkropolisPlaceMovieDecodeBuffers(size_t decodedFrameBytes)
{
    u8* firstDecodedFrame;

    gMdecMovieExpandedBuffers[0] = (u_long*)((u8*)gStreamMovieRingBuffer + MAP_AKROPOLIS_MOVIE_RING_BYTES);
    gMdecMovieExpandedBuffers[1] = gStreamMovieWorkspace;
    firstDecodedFrame            = (u8*)gStreamMovieWorkspace + decodedFrameBytes;
    gMdecMovieOutputBuffers[0]   = (u_long*)firstDecodedFrame;
    gMdecMovieOutputBuffers[1]   = (u_long*)(firstDecodedFrame + decodedFrameBytes);
}

void mapAkropolisSetupMovieBuffers(const GameLocationKey* location)
{
    CdCmdQueue* queue = &gCdCmdQueue;
    s16         stagingEnabled;
    s32         fountainFrameBytes;
    s32         frameBytes;
    s32         promenadePixelCount;
    s32         bridgePixelCount;

    // Select byte offsets for the current area's movie and shared actor storage.
    switch (location->area) {
        case GAME_AREA_AKROPOLIS_PLAZA:
            queue->movieVramStaging        = 1;
            queue->movieStagingX           = MAP_AKROPOLIS_MOVIE_PLAZA_STAGING_X_WORDS;
            gStreamMovieReleasePauseOnStop = false;
            queue->movieStagingY           = 0;
            gMdecMovieExpandedBuffers[0]   = (u_long*)((u8*)gStreamMovieRingBuffer + MAP_AKROPOLIS_MOVIE_RING_BYTES);
            gMdecMovieExpandedBuffers[1]   = gStreamMovieWorkspace;
            gMdecMovieOutputBuffers[1]     = (u_long*)((u8*)gStreamMovieWorkspace + gStreamMovieWidthPixels * gStreamMovieHeightRows);
            gMdecMovieOutputBuffers[0]     = gMdecMovieOutputBuffers[1];
            break;
        case GAME_AREA_AKROPOLIS_FOUNTAIN:
            // This one-iteration loop preserves the original instruction schedule.
            do {
                stagingEnabled = 1;
            } while (0);
            queue->movieStagingX           = MAP_AKROPOLIS_MOVIE_FOUNTAIN_STAGING_X_WORDS;
            queue->movieStagingY           = MAP_AKROPOLIS_MOVIE_FOUNTAIN_STAGING_Y_ROWS;
            gStreamMovieBackgroundPlayback = stagingEnabled;
            queue->movieVramStaging        = stagingEnabled;
            fountainFrameBytes             = gStreamMovieWidthPixels * gStreamMovieHeightRows * 2;
            gMdecMovieExpandedBuffers[0]   = (u_long*)((u8*)gStreamMovieRingBuffer + MAP_AKROPOLIS_MOVIE_RING_BYTES);
            gMdecMovieOutputBuffers[0]     = gStreamMovieWorkspace;
            gMdecMovieExpandedBuffers[1]   = (u_long*)((u8*)gMdecMovieExpandedBuffers[0] + fountainFrameBytes);
            gMdecMovieOutputBuffers[1]     = (u_long*)((u8*)gMdecMovieOutputBuffers[0] + fountainFrameBytes);
            gGameSession->field_80         = 0;
            queue->field_24A               = stagingEnabled;
            break;
        case GAME_AREA_AKROPOLIS_SECURITY_ROOM:
        case GAME_AREA_AKROPOLIS_FORKED_ROAD:
        case GAME_AREA_AKROPOLIS_OBSERVATORY:
            frameBytes = gStreamMovieWidthPixels * gStreamMovieHeightRows * 2;
            _mapAkropolisPlaceMovieDecodeBuffers(frameBytes);
            break;
        case GAME_AREA_AKROPOLIS_PROMENADE:
            promenadePixelCount          = gStreamMovieWidthPixels * gStreamMovieHeightRows;
            gMdecMovieExpandedBuffers[0] = (u_long*)((u8*)gStreamMovieRingBuffer + MAP_AKROPOLIS_MOVIE_RING_BYTES);
            gMdecMovieExpandedBuffers[1] = gStreamMovieWorkspace;
            gMdecMovieOutputBuffers[0]   = (u_long*)((u8*)gStreamMovieWorkspace + promenadePixelCount * 3 / 2);
            gMdecMovieOutputBuffers[1]   = (u_long*)((u8*)gMdecMovieOutputBuffers[0] + promenadePixelCount * 2);
            break;
        case GAME_AREA_AKROPOLIS_BRIDGE:
            bridgePixelCount             = gStreamMovieWidthPixels * gStreamMovieHeightRows;
            gMdecMovieExpandedBuffers[0] = (u_long*)((u8*)gStreamMovieRingBuffer + MAP_AKROPOLIS_MOVIE_RING_BYTES);
            gMdecMovieOutputBuffers[0]   = gStreamMovieWorkspace;
            gMdecMovieExpandedBuffers[1] = (u_long*)((u8*)gMdecMovieExpandedBuffers[0] + bridgePixelCount * 3 / 2);
            gMdecMovieOutputBuffers[1]   = (u_long*)((u8*)gMdecMovieOutputBuffers[0] + bridgePixelCount * 2);
            break;
    }
    // Record the decoded-storage end and clear the actor-buffer reuse fields.
    gStreamMovieBufferLayoutEnd = (u8*)gMdecMovieOutputBuffers[1] + gStreamMovieWidthPixels * gStreamMovieHeightRows * 2;
    gGameSession->field_7C      = 0;
    gGameSession->field_7E      = 0;
}

/// Draws, previews and opens the current row of the MIST briefing's key-item list.
///
/// The list supplies a valid index into the four-item briefing table and row
/// coordinates in panel pixels. Opening the Dryfield map latches that choice
/// immediately, before the detail child reports its result.
static void _mapAkropolisDrawKeyItemRow(UiList* list, UiObject* object)
{
    enum { MAP_AKROPOLIS_KEY_ITEM_DETAIL_DESCRIPTOR_INDEX = 45 };
    s32 itemId;
    s32 inputEnabled;

    itemId = _gMapAkropolisBriefingItemIds[list->currentItemIndex];
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, itemGetText(itemId, ITEM_TEXT_NAME, 0), list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (((object->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            itemMenuSetPreviewItem(itemId, CD_COMMAND_DISPLAY_LOAD_MENU);
        }
    }
    inputEnabled = list->rowInputEnabled;
    if (inputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&gItemMenuPanelDescriptors[MAP_AKROPOLIS_KEY_ITEM_DETAIL_DESCRIPTOR_INDEX], itemId, USER_INTERFACE_PANEL_ACTIVE, 1, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            if (itemId == INVENTORY_COLLECTION_ID_DRYFIELD_MAP) {
                _gMapAkropolisDryfieldMapOpened = inputEnabled;
            }
        }
    }
}

/// Updates the briefing's key-item panel and its item-detail child.
///
/// The owning UI task supplies its live UiObject in spawnArg2. First dispatch
/// fits the singleton list to the panel; confirm closes the detail child and
/// resumes list input, while cancel propagates to the menu host.
static void _mapAkropolisKeyItemPanelTask(Task* task)
{
    enum { MAP_AKROPOLIS_KEY_ITEM_PANEL_INITIALIZE = 0 };
    UiObject* object;
    UiList*   list;
    UiObject* detailObject;
    s32       detailResult;

    list           = &_gMapAkropolisKeyItemList;
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, _gMapAkropolisKeyItemPanelTitle);
    if (task->state == MAP_AKROPOLIS_KEY_ITEM_PANEL_INITIALIZE) {
        uiFitPanelToList(list, &object->panel);
        list->flags           = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        task->spawnArg1.value = -1;
        task->state          += 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadCancelButtonMask | gPadMenuButtonMask) != 0) {
        object->result = USER_INTERFACE_RESULT_CANCEL;
    }
    if (task->firstChild != NULL) {
        detailObject = task->firstChild->spawnArg2.pointer;
        detailResult = detailObject->result;
        switch (detailResult) {
            case USER_INTERFACE_RESULT_CONFIRM:
                uiStartTreeClosing(detailObject, detailObject->owner);
                object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                break;
            case USER_INTERFACE_RESULT_CANCEL:
                object->result = detailResult;
                break;
        }
    }
}

/// Owns the briefing's key-item UI tree until its closing animation finishes.
///
/// spawnArg2 holds the live root object after successful allocation. Runs the
/// menu every VBLANK, then restores two-VBLANK timing and releases its primitive
/// buffer before requesting display-mode exit. This singleton owns uiOpen.
static void _mapAkropolisKeyItemMenuTask(Task* task)
{
    UiObject* object;
    s16       result;

    if (task->state == MAP_AKROPOLIS_KEY_ITEM_MENU_START) {
        stageEnsureHeapTaskPrimitiveBuffer();
        itemMenuClearPreviewItems();
        object = uiSpawnObject(&_gMapAkropolisKeyItemPanelDescriptor, task->spawnArg1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
        if (object == NULL) {
            return;
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen    = 1;
        task->spawnArg2.pointer = object;
        task->state            += 1;
    }

    if (task->state == MAP_AKROPOLIS_KEY_ITEM_MENU_ACTIVE) {
        object = task->spawnArg2.pointer;
        result = object->result;
        if ((result == USER_INTERFACE_RESULT_CANCEL) || (result == USER_INTERFACE_RESULT_CONFIRM)) {
            uiStartTreeClosing(object, object->owner);
            task->killCountdown = MAP_AKROPOLIS_KEY_ITEM_MENU_CLOSE_TICKS;
            task->state         = MAP_AKROPOLIS_KEY_ITEM_MENU_CLOSING;
        }
    }

    // Keep the primitive storage until the UI closing animation has run.
    if (task->state == MAP_AKROPOLIS_KEY_ITEM_MENU_CLOSING) {
        task->killCountdown -= 1;
        if (task->killCountdown <= 0) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gGameSession->uiOpen = 0;
            taskKill(task);
            stageReleaseTaskPrimitiveBuffer();
            stageRequestModeTaskExit();
        }
    }
}

s32 mapAkropolisOpenKeyItemMenu(s32 unused, s32 retainSelection)
{
    s32* itemIdCursor;
    s32  itemIndex;

    if (retainSelection == 0) {
        itemIndex    = 0;
        itemIdCursor = _gMapAkropolisBriefingItemIds;
        do {
            inventorySetCollectedBit(*itemIdCursor++);
            itemIndex++;
        } while (itemIndex < ARRAY_SIZE(_gMapAkropolisBriefingItemIds));
        _gMapAkropolisDryfieldMapOpened = 0;
    }
    displayQueueModeTask(&_gMapAkropolisKeyItemMenuTaskDescriptor, 0, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

s32 mapAkropolisWasDryfieldMapSelected(void)
{
    return _gMapAkropolisDryfieldMapOpened;
}

/// Page IDs used by the map records; MIST's page cannot navigate to Acropolis.
enum {
    MAP_AKROPOLIS_PAGE_TOWER          = 1,
    MAP_AKROPOLIS_PAGE_LANDING_PAD    = 2,
    MAP_AKROPOLIS_PAGE_MIST           = 3,
    MAP_AKROPOLIS_PAGE_NAVIGATION_END = 0xFF,
};

/// Fixed icon picture and cafeteria objective number used by this map.
enum {
    MAP_AKROPOLIS_ICON_KIND_FIXED_PICTURE = 0,
    MAP_AKROPOLIS_OBJECTIVE_CAFETERIA     = 2,
};

/// Initial saved object states used by this stage's placements.
///
/// Available objects can be collected; collected state suppresses pickup models.
/// Replenishable ammunition grants a full stack and is not marked collected.
enum {
    MAP_AKROPOLIS_STATE_AVAILABLE     = 1,
    MAP_AKROPOLIS_STATE_COLLECTED     = 2,
    MAP_AKROPOLIS_STATE_REPLENISHABLE = 3,
};

/// Catalog kinds and room-prop recipes in this stage's object tables.
///
/// The low-bank values are item catalog IDs. The additional landing-pad
/// pickup kind selects an empty catalog slot and has no placement here.
enum {
    MAP_AKROPOLIS_KIND_RECOVERY_1                    = 0x1,
    MAP_AKROPOLIS_KIND_RECOVERY_2                    = 0x2,
    MAP_AKROPOLIS_KIND_STIM                          = 0x4,
    MAP_AKROPOLIS_KIND_MP_BOOST_2                    = 0x7,
    MAP_AKROPOLIS_KIND_PENICILLIN                    = 0x8,
    MAP_AKROPOLIS_KIND_PROTEIN_CAPSULE               = 0x3C,
    MAP_AKROPOLIS_KIND_TACTICAL_VEST                 = 0x65,
    MAP_AKROPOLIS_KIND_GRENADE_PISTOL                = 0x8A,
    MAP_AKROPOLIS_KIND_MP5A5                         = 0x9D,
    MAP_AKROPOLIS_KIND_9MM_PARABELLUM                = 0xA0,
    MAP_AKROPOLIS_KIND_GRENADE                       = 0xA9,
    MAP_AKROPOLIS_KIND_MICRO_DEVICE                  = 0x102,
    MAP_AKROPOLIS_KIND_HELICOPTER_LANDING_PAD_LIFT   = 0x204,
    MAP_AKROPOLIS_KIND_SECURITY_ROOM_CONTAINER_LID   = 0x701,
    MAP_AKROPOLIS_KIND_SANCTUARY_CONTAINER_LID       = 0x702,
    MAP_AKROPOLIS_KIND_SQUARE_SAVE_POINT             = 0x801,
    MAP_AKROPOLIS_KIND_HELICOPTER_LANDING_PAD_PICKUP = 0xA4,
};

/// Indices of this stage's saved two-bit object states (sixteen per word).
enum {
    MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_AMMUNITION                = 0x1,
    MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_FIRST_RECOVERY_2          = 0xA,
    MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_SECOND_RECOVERY_2         = 0x17,
    MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_RECOVERY_1                = 0x1B,
    MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_SAVE_POINT                = 0x2E,
    MAP_AKROPOLIS_OBJECT_SLOT_EAST_ELEVATOR_HALL_PARTHENON_KEY = 0x2,
    MAP_AKROPOLIS_OBJECT_SLOT_CAFETERIA_MICRO_DEVICE           = 0x3,
    MAP_AKROPOLIS_OBJECT_SLOT_CAFETERIA_MENDEL_JOURNAL         = 0x4,
    MAP_AKROPOLIS_OBJECT_SLOT_CAFETERIA_STIM                   = 0x9,
    MAP_AKROPOLIS_OBJECT_SLOT_CAFETERIA_RECOVERY_1             = 0x16,
    MAP_AKROPOLIS_OBJECT_SLOT_HALLWAY_BLUE_KEY                 = 0xB,
    MAP_AKROPOLIS_OBJECT_SLOT_FOUNTAIN_PROTEIN_CAPSULE         = 0xC,
    MAP_AKROPOLIS_OBJECT_SLOT_FOUNTAIN_GRENADE_PISTOL          = 0xD,
    MAP_AKROPOLIS_OBJECT_SLOT_FOUNTAIN_MP_BOOST_2              = 0xE,
    MAP_AKROPOLIS_OBJECT_SLOT_FORKED_ROAD_PENICILLIN           = 0x18,
    MAP_AKROPOLIS_OBJECT_SLOT_OBSERVATORY_TACTICAL_VEST        = 0x10,
    MAP_AKROPOLIS_OBJECT_SLOT_PROMENADE_GRENADES               = 0x12,
    MAP_AKROPOLIS_OBJECT_SLOT_PROMENADE_MP5A5                  = 0x15,
    MAP_AKROPOLIS_OBJECT_SLOT_SANCTUARY_RED_KEY                = 0x1C,
    MAP_AKROPOLIS_OBJECT_SLOT_HELICOPTER_LANDING_PAD_LIFT      = 0x28,
};

/// Additional saved-state slots and object kinds for the Roof garden, Fire escape and MIST.
///
/// Slots index the Acropolis bank's four packed state words, in 0..63. Prize
/// slots 0x20..0x24 correspond to shooting-gallery courses 0..4. Save-point kinds
/// publish their full bank-8 key; bank 7 selects the parking item container.
/// Bit 8 of the saved-state initializer is retained metadata with no reader.
enum {
    MAP_AKROPOLIS_OBJECT_SLOT_ROOF_GARDEN_ARMORY_CARDKEY = 0x13,
    MAP_AKROPOLIS_OBJECT_SLOT_FIRE_ESCAPE_SAVE_POINT     = 0x2F,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_SAVE_POINT    = 0x2D,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_ITEM_BOX      = 0x1E,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_RECOVERY_3_1  = 0x1A,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_RECOVERY_3_2  = 0x1F,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_0       = 0x20,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_1       = 0x21,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_2       = 0x22,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_3       = 0x23,
    MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_4       = 0x24,
    MAP_AKROPOLIS_KIND_RECOVERY_3                        = 0x3,
    MAP_AKROPOLIS_KIND_LIPSTICK                          = 0xB,
    MAP_AKROPOLIS_KIND_RINGERS_SOLUTION                  = 0x3D,
    MAP_AKROPOLIS_KIND_HUNTER_GOGGLES                    = 0x3F,
    MAP_AKROPOLIS_KIND_SHOULDER_HOLSTER                  = 0x6C,
    MAP_AKROPOLIS_KIND_9MM_HYDRA                         = 0xA1,
    MAP_AKROPOLIS_KIND_MIST_PARKING_ITEM_BOX             = 0x703,
    MAP_AKROPOLIS_KIND_FIRE_ESCAPE_SAVE_POINT            = 0x802,
    MAP_AKROPOLIS_KIND_MIST_PARKING_SAVE_POINT           = 0x803,
    MAP_AKROPOLIS_OBJECT_STATE_UNREAD_BIT_8              = 0x100,
};

/// Execution priority of the placed-object model tasks.

#include "assets/rooms/akropolis/memory.inc"
#define ROOM_PART gMapAkropolisPageFlagIds
#include "assets/rooms/akropolis/map.inc"

/// Empty model-spawn list for the Square.
///
/// Contains only `AREA_OBJECT_SPAWN_END`. Its places still seed saved states
/// and supply pickup metadata, but instantiate no model from this table.
static AreaObjectSpawn _gMapAkropolisSquareObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for the East elevator hall.
///
/// Contains only `AREA_OBJECT_SPAWN_END`. Its places still seed saved states
/// and supply pickup metadata, but instantiate no model from this table.
static AreaObjectSpawn _gMapAkropolisEastElevatorHallObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Stim and Mendel-journal model-spawn recipes for the Cafeteria.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
static AreaObjectSpawn _gMapAkropolisCafeteriaObjectSpawns[3] = {
    { MAP_AKROPOLIS_KIND_STIM, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, akropolisCafeteriaStimPickupTask, { &gAkropolisCafeteriaStimPickupModel } } },
    { INVENTORY_COLLECTION_ID_MENDEL_JOURNAL, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, akropolisCafeteriaMendelPickupTask, { &gAkropolisCafeteriaMendelJournalModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Container-lid model-spawn recipe for the Security room.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
/// The area's empty placement list selects none of these recipes.
static AreaObjectSpawn _gMapAkropolisSecurityRoomObjectSpawns[2] = {
    { MAP_AKROPOLIS_KIND_SECURITY_ROOM_CONTAINER_LID, { { { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, itemPickupContainerLidTask, { &gAkropolisSecurityRoomItemBoxModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Blue-key model-spawn recipe for the Hallway.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
static AreaObjectSpawn _gMapAkropolisHallwayObjectSpawns[2] = {
    { INVENTORY_COLLECTION_ID_BLUE_KEY, { { { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, akropolisHallwayBlueKeyPickupTask, { &gAkropolisHallwayBlueKeyPickupModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for the Fountain.
///
/// Contains only `AREA_OBJECT_SPAWN_END`. Its places still seed saved states
/// and supply pickup metadata, but instantiate no model from this table.
static AreaObjectSpawn _gMapAkropolisFountainObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for the Forked road.
///
/// Contains only `AREA_OBJECT_SPAWN_END`. Its places still seed saved states
/// and supply pickup metadata, but instantiate no model from this table.
static AreaObjectSpawn _gMapAkropolisForkedRoadObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for the Observatory.
///
/// Contains only `AREA_OBJECT_SPAWN_END`. Its places still seed saved states
/// and supply pickup metadata, but instantiate no model from this table.
static AreaObjectSpawn _gMapAkropolisObservatoryObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for the Promenade.
///
/// Contains only `AREA_OBJECT_SPAWN_END`. Its places still seed saved states
/// and supply pickup metadata, but instantiate no model from this table.
static AreaObjectSpawn _gMapAkropolisPromenadeObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Red-key and container-lid model-spawn recipes for the Sanctuary.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
/// Only the red key has a matching placement; the lid recipe is unselected.
static AreaObjectSpawn _gMapAkropolisSanctuaryObjectSpawns[3] = {
    { INVENTORY_COLLECTION_ID_RED_KEY, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, akropolisSanctuaryItemVisibilityTask, { &gAkropolisSanctuaryRedKeyPickupModel } } },
    { MAP_AKROPOLIS_KIND_SANCTUARY_CONTAINER_LID, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, itemPickupContainerLidTask, { &gAkropolisSanctuaryItemBoxModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Armory-cardkey model-spawn recipe for the Roof garden.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
static AreaObjectSpawn _gMapAkropolisRoofGardenObjectSpawns[2] = {
    { INVENTORY_COLLECTION_ID_ARMORY_CARDKEY, { { { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, akropolisRoofGardenPickupModelTask, { &gAkropolisRoofGardenArmoryCardkeyPickupModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for the Fire escape.
///
/// Contains only `AREA_OBJECT_SPAWN_END`. Its places still seed saved states
/// and supply pickup metadata, but instantiate no model from this table.
static AreaObjectSpawn _gMapAkropolisFireEscapeObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for the West elevator hall.
///
/// Contains only `AREA_OBJECT_SPAWN_END`; the paired placement list is also
/// empty, so this area seeds no object state and instantiates no object model.
static AreaObjectSpawn _gMapAkropolisWestElevatorHallObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for MIST parking.
///
/// Contains only `AREA_OBJECT_SPAWN_END`. Its places still seed saved states
/// and supply pickup metadata, but instantiate no model from this table.
static AreaObjectSpawn _gMapAkropolisMistParkingObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

#define ROOM_PART _gMapAkropolisHelicopterLandingPadObjectPlaces
#include "assets/rooms/akropolis/objects.inc"

/// Lift and additional pickup model-spawn recipes for the Helicopter landing pad.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
/// Only the lift has a matching placement; the pickup recipe is unselected.
static AreaObjectSpawn _gMapAkropolisHelicopterLandingPadObjectSpawns[3] = {
    { MAP_AKROPOLIS_KIND_HELICOPTER_LANDING_PAD_LIFT, { { { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, akropolisHelicopterLandingPadLiftTask, { &gAkropolisHelicopterLandingPadAkropolisHelicopterLandingPadModel0547C } } },
    { MAP_AKROPOLIS_KIND_HELICOPTER_LANDING_PAD_PICKUP, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, akropolisHelicopterLandingPadPickupModelTask, { &gAkropolisHelicopterLandingPadAmmunitionPickupModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Priority required by the room-controller descriptor search.

#define ROOM_PART gMapAkropolisObjectRooms
#include "assets/rooms/akropolis/objects.inc"

TaskDesc gMapAkropolisRoomTaskDescriptors[21] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisSquareRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_SQUARE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisEastElevatorHallRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_EAST_ELEVATOR_HALL, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisPatioRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisCafeteriaRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisSecurityRoomMessageTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_SECURITY_ROOM, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHallwayRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisFountainRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FOUNTAIN, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisForkedRoadRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FORKED_ROAD, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisObservatoryRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_OBSERVATORY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisPromenadeRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PROMENADE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisSanctuaryRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_SANCTUARY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisRoofGardenRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisBridgeRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_BRIDGE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisFireEscapeRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FIRE_ESCAPE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHelicopterLandingPadRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisWestElevatorHallRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_WEST_ELEVATOR_HALL, 1) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, mistR18BriefingTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_R18, 1) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, mistParkingRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_PARKING, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, mistShootingGalleryRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, mistR21RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_R21, 0) } },
    { { { TASK_DESC_END, TASK_PRIORITY_SCENE } }, NULL, { 0 } },
};

/// Whether the Dryfield map detail has been opened during the briefing menu.
///
/// Signed-word latch (0 not opened, 1 opened). A fresh menu request clears it;
/// retained requests preserve it, independently of the list's selected row.
static s32 _gMapAkropolisDryfieldMapOpened = 0;

/// Collection catalog IDs shown and granted by the MIST briefing menu.
///
/// Row indices 0..3 select signed-word IDs; names come from the item catalog.
static s32 _gMapAkropolisBriefingItemIds[MAP_AKROPOLIS_BRIEFING_ITEM_COUNT] = {
    INVENTORY_COLLECTION_ID_MIST_SEARCH_WARRANT,
    INVENTORY_COLLECTION_ID_NMC_PHOTO,
    INVENTORY_COLLECTION_ID_MANUAL,
    INVENTORY_COLLECTION_ID_DRYFIELD_MAP,
};

/// Shared row renderer for all four briefing items.
///
/// The list's shared-callback flag selects entry zero for every item index.
static UiListRowCallback _gMapAkropolisKeyItemRowCallbacks[1] = { _mapAkropolisDrawKeyItemRow };

/// Mutable singleton list for the four MIST briefing items.
///
/// Shows all four rows with wrapping navigation and 15-pixel row spacing.
/// Panel initialization resets selection/scroll state and enables the shared
/// row renderer; closing and reopening can retain the Dryfield-map latch.
static UiList _gMapAkropolisKeyItemList = {
    .rowCallbacks    = _gMapAkropolisKeyItemRowCallbacks,
    .itemCount       = ARRAY_SIZE(_gMapAkropolisBriefingItemIds),
    .visibleRowCount = { .unsignedValue = ARRAY_SIZE(_gMapAkropolisBriefingItemIds) },
    .wrapNavigation  = 1,
    .rowHeight       = MAP_AKROPOLIS_KEY_ITEM_ROW_HEIGHT_PIXELS,
};

/// Panel recipe for the briefing's key-item list and its detail children.
///
/// Bounds are screen-centered pixels; the bodyless content task uses priority
/// 192 and ordering-table tag base 60. Spawning reads this recipe synchronously.
static UiObjectDesc _gMapAkropolisKeyItemPanelDescriptor = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -100, -40, 200, MAP_AKROPOLIS_BRIEFING_ITEM_COUNT* MAP_AKROPOLIS_KEY_ITEM_ROW_HEIGHT_PIXELS },
    .otIndexSeed     = 60,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = TASK_PRIORITY_CONTROL,
    .contentCallback = _mapAkropolisKeyItemPanelTask,
};

/// Bodyless display-mode host for the briefing's key-item UI tree.
///
/// Runs at priority `TASK_PRIORITY_CONTROL`; spawning copies this map-owned recipe.
static TaskDesc _gMapAkropolisKeyItemMenuTaskDescriptor = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _mapAkropolisKeyItemMenuTask,
};
#define ROOM_PART gMapAkropolisMarkerFlags
#include "assets/rooms/akropolis/map.inc"
#include "assets/rooms/akropolis/tables.inc"
#include "assets/rooms/akropolis/placements.inc"
#define ROOM_PART _gMapAkropolisSquareObjectPlaces
#include "assets/rooms/akropolis/objects.inc"
#include "assets/rooms/akropolis/rewards.inc"
#include "assets/rooms/akropolis/music.inc"

/// Door-marker nibble IDs shared with the corresponding room warp endpoints.
enum {
    MAP_AKROPOLIS_MARKER_FLAG_BRIDGE_FIRE_ESCAPE      = 0x1EA,
    MAP_AKROPOLIS_MARKER_FLAG_PROMENADE_BRIDGE        = 0x1EB,
    MAP_AKROPOLIS_MARKER_FLAG_SANCTUARY_ROOF_GARDEN   = 0x1EC,
    MAP_AKROPOLIS_MARKER_FLAG_PROMENADE_SANCTUARY     = 0x1ED,
    MAP_AKROPOLIS_MARKER_FLAG_FOUNTAIN_FORKED_ROAD    = 0x1EF,
    MAP_AKROPOLIS_MARKER_FLAG_PATIO_FOUNTAIN          = 0x1F0,
    MAP_AKROPOLIS_MARKER_FLAG_HALLWAY_FOUNTAIN        = 0x1F1,
    MAP_AKROPOLIS_MARKER_FLAG_HALLWAY_SECURITY_ROOM   = 0x1F2,
    MAP_AKROPOLIS_MARKER_FLAG_CAFETERIA_HALLWAY_VIEW3 = 0x1F3,
    MAP_AKROPOLIS_MARKER_FLAG_CAFETERIA_HALLWAY_VIEW2 = 0x1F4,
    MAP_AKROPOLIS_MARKER_FLAG_PATIO_CAFETERIA         = 0x1F5,
};

/// Resource-entry keys for the scene actors in these placement lists.
///
/// These select each room variant's recipe, independently of actor-file IDs.
enum {
    MAP_AKROPOLIS_PLACEMENT_ENTRY_GALLERY_MOUNT    = 76,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_BALDWIN          = 105,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_JODIE            = 115,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_PIERCE           = 143,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_SCENE_RUPERT     = 144,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_HELICOPTER       = 254,
    MAP_AKROPOLIS_PLACEMENT_GALLERY_SPAWN_REJECTED = 0xFF,
};

/// Additional item catalog IDs selected by the Acropolis battle reward lists.
enum {
    MAP_AKROPOLIS_REWARD_ITEM_MP_BOOST_1   = 6,
    MAP_AKROPOLIS_REWARD_ITEM_MD_PLAYER    = 14,
    MAP_AKROPOLIS_REWARD_ITEM_FLARE        = 0x3A,
    MAP_AKROPOLIS_REWARD_ITEM_PEPPER_SPRAY = 0x3B,
    MAP_AKROPOLIS_REWARD_ITEM_COMBAT_LIGHT = 0x41,
    MAP_AKROPOLIS_REWARD_ITEM_P08          = 0x83,
    MAP_AKROPOLIS_REWARD_ITEM_9MM_SPARTAN  = 0xA2,
    MAP_AKROPOLIS_REWARD_ITEM_AIRBURST     = 0xAA,
    MAP_AKROPOLIS_REWARD_ITEM_RIOT         = 0xAB,
};

/// NUL-terminated title displayed above the briefing's key-item list.
///
/// The final three bytes have no established consumer; their role is unproven.
static const char _gMapAkropolisKeyItemPanelTitle[12] = "Key Item\0L#\6";
