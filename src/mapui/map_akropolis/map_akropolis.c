#include "mapui/map_akropolis.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/map.h"
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

#include "rooms/acropolis_bridge.h"

#include "rooms/acropolis_cafeteria.h"

#include "rooms/acropolis_east_elevator_hall.h"

#include "rooms/acropolis_fire_escape.h"

#include "rooms/acropolis_forked_road.h"

#include "rooms/acropolis_fountain.h"

#include "rooms/acropolis_hallway.h"

#include "rooms/acropolis_helicopter_landing_pad.h"

#include "rooms/acropolis_observatory.h"

#include "rooms/acropolis_patio.h"

#include "rooms/acropolis_plaza.h"

#include "rooms/acropolis_promenade.h"

#include "rooms/acropolis_roof_garden.h"

#include "rooms/acropolis_sanctuary.h"

#include "rooms/acropolis_security_room.h"

#include "rooms/acropolis_square.h"

#include "rooms/acropolis_west_elevator_hall.h"

#include "rooms/mist_parking.h"

#include "rooms/mist_r18.h"

#include "rooms/mist_r21.h"

#include "rooms/mist_shooting_gallery.h"

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
    MAP_AKROPOLIS_PLACEMENT_ENTRY_OFFICER_1       = 108,
    MAP_AKROPOLIS_PLACEMENT_ENTRY_OFFICER_2       = 109,
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

/// Catalogue row count, task priority and panel spacing of the briefing menu.
enum {
    MAP_AKROPOLIS_BRIEFING_ITEM_COUNT        = 4,
    MAP_AKROPOLIS_KEY_ITEM_TASK_PRIORITY     = 192,
    MAP_AKROPOLIS_KEY_ITEM_ROW_HEIGHT_PIXELS = 15,
};

static const char      _gMapAkropolisKeyItemPanelTitle[12];
static s32             _gMapAkropolisDryfieldMapOpened;
static s32             _gMapAkropolisBriefingItemIds[MAP_AKROPOLIS_BRIEFING_ITEM_COUNT];
static UiList          _gMapAkropolisKeyItemList;
static UiObjectDesc    _gMapAkropolisKeyItemPanelDescriptor;
static TaskDesc        _gMapAkropolisKeyItemMenuTaskDescriptor;
static AreaObjectPlace _gMapAkropolisSquareObjectPlaces[6];
static AreaObjectPlace _gMapAkropolisEastElevatorHallObjectPlaces[2];
static AreaObjectPlace _gMapAkropolisCafeteriaObjectPlaces[5];
static AreaObjectPlace _gMapAkropolisSecurityRoomObjectPlaces[1];
static AreaObjectPlace _gMapAkropolisHallwayObjectPlaces[2];
static AreaObjectPlace _gMapAkropolisFountainObjectPlaces[4];
static AreaObjectPlace _gMapAkropolisForkedRoadObjectPlaces[2];
static AreaObjectPlace _gMapAkropolisObservatoryObjectPlaces[2];
static AreaObjectPlace _gMapAkropolisPromenadeObjectPlaces[3];
static AreaObjectPlace _gMapAkropolisSanctuaryObjectPlaces[2];
static AreaObjectPlace _gMapAkropolisRoofGardenObjectPlaces[2];
static AreaObjectPlace _gMapAkropolisFireEscapeObjectPlaces[2];
static AreaObjectPlace _gMapAkropolisWestElevatorHallObjectPlaces[1];
static AreaObjectPlace _gMapAkropolisMistParkingObjectPlaces[10];

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
static __inline__ void _mapAkropolisPlaceMovieDecodeBuffers(size_t decodedFrameBytes)
{
    u8* firstDecodedFrame;

    D_8006AC50[0]     = (u_long*)((u8*)D_8006AC60 + MAP_AKROPOLIS_MOVIE_RING_BYTES);
    D_8006AC50[1]     = D_8006AC40;
    firstDecodedFrame = (u8*)D_8006AC40 + decodedFrameBytes;
    D_8006AC48[0]     = (u_long*)firstDecodedFrame;
    D_8006AC48[1]     = (u_long*)(firstDecodedFrame + decodedFrameBytes);
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
        case GAME_AREA_ACROPOLIS_PLAZA:
            queue->movieVramStaging = 1;
            queue->movieStagingX    = MAP_AKROPOLIS_MOVIE_PLAZA_STAGING_X_WORDS;
            D_8006AC3C              = 0;
            queue->movieStagingY    = 0;
            D_8006AC50[0]           = (u_long*)((u8*)D_8006AC60 + MAP_AKROPOLIS_MOVIE_RING_BYTES);
            D_8006AC50[1]           = D_8006AC40;
            D_8006AC48[1]           = (u_long*)((u8*)D_8006AC40 + D_8006AC5A * D_8006AC6C);
            D_8006AC48[0]           = D_8006AC48[1];
            break;
        case GAME_AREA_ACROPOLIS_FOUNTAIN:
            // This one-iteration loop preserves the original instruction schedule.
            do {
                stagingEnabled = 1;
            } while (0);
            queue->movieStagingX    = MAP_AKROPOLIS_MOVIE_FOUNTAIN_STAGING_X_WORDS;
            queue->movieStagingY    = MAP_AKROPOLIS_MOVIE_FOUNTAIN_STAGING_Y_ROWS;
            D_8006AC5C              = stagingEnabled;
            queue->movieVramStaging = stagingEnabled;
            fountainFrameBytes      = D_8006AC5A * D_8006AC6C * 2;
            D_8006AC50[0]           = (u_long*)((u8*)D_8006AC60 + MAP_AKROPOLIS_MOVIE_RING_BYTES);
            D_8006AC48[0]           = D_8006AC40;
            D_8006AC50[1]           = (u_long*)((u8*)D_8006AC50[0] + fountainFrameBytes);
            D_8006AC48[1]           = (u_long*)((u8*)D_8006AC48[0] + fountainFrameBytes);
            gGameSession->field_80  = 0;
            queue->field_24A        = stagingEnabled;
            break;
        case GAME_AREA_ACROPOLIS_SECURITY_ROOM:
        case GAME_AREA_ACROPOLIS_FORKED_ROAD:
        case GAME_AREA_ACROPOLIS_OBSERVATORY:
            frameBytes = D_8006AC5A * D_8006AC6C * 2;
            _mapAkropolisPlaceMovieDecodeBuffers(frameBytes);
            break;
        case GAME_AREA_ACROPOLIS_PROMENADE:
            promenadePixelCount = D_8006AC5A * D_8006AC6C;
            D_8006AC50[0]       = (u_long*)((u8*)D_8006AC60 + MAP_AKROPOLIS_MOVIE_RING_BYTES);
            D_8006AC50[1]       = D_8006AC40;
            D_8006AC48[0]       = (u_long*)((u8*)D_8006AC40 + promenadePixelCount * 3 / 2);
            D_8006AC48[1]       = (u_long*)((u8*)D_8006AC48[0] + promenadePixelCount * 2);
            break;
        case GAME_AREA_ACROPOLIS_BRIDGE:
            bridgePixelCount = D_8006AC5A * D_8006AC6C;
            D_8006AC50[0]    = (u_long*)((u8*)D_8006AC60 + MAP_AKROPOLIS_MOVIE_RING_BYTES);
            D_8006AC48[0]    = D_8006AC40;
            D_8006AC50[1]    = (u_long*)((u8*)D_8006AC50[0] + bridgePixelCount * 3 / 2);
            D_8006AC48[1]    = (u_long*)((u8*)D_8006AC48[0] + bridgePixelCount * 2);
            break;
    }
    // Record the decoded-storage end and clear the actor-buffer reuse fields.
    D_8006AC44             = (u8*)D_8006AC48[1] + D_8006AC5A * D_8006AC6C * 2;
    gGameSession->field_7C = 0;
    gGameSession->field_7E = 0;
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
            uiSpawnObject(&D_8010EAB4[MAP_AKROPOLIS_KEY_ITEM_DETAIL_DESCRIPTOR_INDEX], itemId, USER_INTERFACE_PANEL_ACTIVE, 1, object);
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

GfxImageSlot gMapAkropolisImageSlots[GAME_AREA_MIST_R21 + 1] = {
    [0]                                          = { NULL, 0 },
    [GAME_AREA_ACROPOLIS_SQUARE]                 = GFX_IMAGE_SLOT(0x4AF80),
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL]     = GFX_IMAGE_SLOT(0x4DB90),
    [GAME_AREA_ACROPOLIS_PATIO]                  = GFX_IMAGE_SLOT(0x4D340),
    [GAME_AREA_ACROPOLIS_CAFETERIA]              = GFX_IMAGE_SLOT(0x461C0),
    [GAME_AREA_ACROPOLIS_PLAZA]                  = GFX_IMAGE_SLOT(0x3C030),
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM]          = GFX_IMAGE_SLOT(0x4E910),
    [GAME_AREA_ACROPOLIS_HALLWAY]                = GFX_IMAGE_SLOT(0x56FA0),
    [GAME_AREA_ACROPOLIS_FOUNTAIN]               = GFX_IMAGE_SLOT(0x50630),
    [GAME_AREA_ACROPOLIS_FORKED_ROAD]            = GFX_IMAGE_SLOT(0x50660),
    [GAME_AREA_ACROPOLIS_OBSERVATORY]            = GFX_IMAGE_SLOT(0x51060),
    [GAME_AREA_ACROPOLIS_PROMENADE]              = GFX_IMAGE_SLOT(0x4EB10),
    [GAME_AREA_ACROPOLIS_SANCTUARY]              = GFX_IMAGE_SLOT(0x4C9F0),
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN]            = GFX_IMAGE_SLOT(0x4ED80),
    [GAME_AREA_ACROPOLIS_BRIDGE]                 = GFX_IMAGE_SLOT(0x435E0),
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE]            = GFX_IMAGE_SLOT(0x51250),
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD] = GFX_IMAGE_SLOT(0x4BC60),
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL]     = GFX_IMAGE_SLOT(0x4F450),
    [GAME_AREA_MIST_R18]                         = GFX_IMAGE_SLOT(0x4F110),
    [GAME_AREA_MIST_PARKING]                     = GFX_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_MIST_SHOOTING_GALLERY]            = GFX_IMAGE_SLOT(0x3FAD0),
    [GAME_AREA_MIST_R21]                         = GFX_IMAGE_SLOT(0x582D0),
};

s32 gMapAkropolisMovieWorkspaceBytesByArea[GAME_AREA_MIST_SHOOTING_GALLERY + 1] = {
    [0]                                          = 0,
    [GAME_AREA_ACROPOLIS_SQUARE]                 = 0,
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL]     = 0,
    [GAME_AREA_ACROPOLIS_PATIO]                  = 0,
    [GAME_AREA_ACROPOLIS_CAFETERIA]              = 0,
    [GAME_AREA_ACROPOLIS_PLAZA]                  = 0x2D000,
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM]          = 0x25200,
    [GAME_AREA_ACROPOLIS_HALLWAY]                = 0,
    [GAME_AREA_ACROPOLIS_FOUNTAIN]               = 0x18C00,
    [GAME_AREA_ACROPOLIS_FORKED_ROAD]            = 0x25200,
    [GAME_AREA_ACROPOLIS_OBSERVATORY]            = 0x1B000,
    [GAME_AREA_ACROPOLIS_PROMENADE]              = 0x28380,
    [GAME_AREA_ACROPOLIS_SANCTUARY]              = 0,
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN]            = 0,
    [GAME_AREA_ACROPOLIS_BRIDGE]                 = 0x21000,
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE]            = 0,
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD] = 0,
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL]     = 0,
    [GAME_AREA_MIST_R18]                         = 0,
    [GAME_AREA_MIST_PARKING]                     = 0,
    [GAME_AREA_MIST_SHOOTING_GALLERY]            = 0,
};

/// Page IDs used by the map records; MIST's page cannot navigate to Acropolis.
enum {
    MAP_AKROPOLIS_PAGE_TOWER          = 1,
    MAP_AKROPOLIS_PAGE_LANDING_PAD    = 2,
    MAP_AKROPOLIS_PAGE_MIST           = 3,
    MAP_AKROPOLIS_PAGE_NAVIGATION_END = 0xFF,
};

u8 gMapAkropolisPageFlagIds[8] = {
    [MAP_AKROPOLIS_PAGE_TOWER]       = GAME_FLAG_015,
    [MAP_AKROPOLIS_PAGE_LANDING_PAD] = GAME_FLAG_015,
    [MAP_AKROPOLIS_PAGE_MIST]        = MAP_AKROPOLIS_PAGE_NAVIGATION_END,
};

MenuMapArea gMapAkropolisAreas[GAME_AREA_MIST_SHOOTING_GALLERY + 2] = {
    [0]                                          = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_ACROPOLIS_SQUARE]                 = { -0x196E, 0x1FF, 0xFFE2, 0xFFBA, 0xE6, 0xC8, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL]     = { -0x1480, 0x42C, 0x28, 0xFFBD, 0xE6, 0xC8, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_PATIO]                  = { 0xF42, 0x300, 0xFFE9, 0xFFDD, 0xC8, 0xC8, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_CAFETERIA]              = { 0x731, -0xCB8, 0xFFCE, 0x19, 0xC8, 0xC8, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_PLAZA]                  = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM]          = { -0x2E, -0xB3D, 0xFFDD, 0x19, 0xC8, 0xC8, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_HALLWAY]                = { 0xBC4, -0x2BA, 0xFFE0, 0x23, 0xC8, 0x12C, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_FOUNTAIN]               = { -0xF38, 0xD8A, 0xFFEC, 0xFFFB, 0xC8, 0xE6, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_FORKED_ROAD]            = { -0xFA4, 0x355, 0x19, 0xFFD8, 0xC8, 0x96, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_OBSERVATORY]            = { -0x8F8, -0x2BF2, 0x4E, 7, 0xC8, 0xBE, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_PROMENADE]              = { 0x282, 0x23A0, 0x28, 6, 0xDC, 0xCD, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_SANCTUARY]              = { -0x261A, -0x1F86, 0x2B, 0x4A, 0x96, 0xC8, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN]            = { -0x1D22, -0x25F0, 0x3C, 0x2F, 0xE6, 0xC8, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_BRIDGE]                 = { 0x282, 0x23A0, 0x28, 0xF, 0xD2, 0xE1, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE]            = { 0xA44, -0xB7, 0xFFD3, 0x40, 0xA5, 0xCD, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD] = { -0x18F3, 0x18A2, 0xFFDE, 0xFFE2, 0xC8, 0xD2, MAP_AKROPOLIS_PAGE_LANDING_PAD },
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL]     = { 0x1508, 0x40F, 0xFFDC, 0xFFC0, 0xC8, 0xC8, MAP_AKROPOLIS_PAGE_TOWER },
    [GAME_AREA_MIST_R18]                         = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_MIST_PARKING]                     = { 0x2100, -0xEF4, 0xFFE4, 0xFFFC, 0xC8, 0xC8, MAP_AKROPOLIS_PAGE_MIST },
    [GAME_AREA_MIST_SHOOTING_GALLERY]            = { 0x2B5C, 0x1450, 0x6E, 1, 0xC8, 0xC8, MAP_AKROPOLIS_PAGE_MIST },
    [GAME_AREA_MIST_SHOOTING_GALLERY + 1]        = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_END },
};

MenuMapAreaShape D_map_akropolis_8017A288[21] = {
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100SquareShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100EastElevatorHallShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100PatioShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100CafeteriaShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100SecurityRoomShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100HallwayShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100FountainShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100ForkedRoadShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100ObservatoryShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100PromenadeShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100SanctuaryShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS100RoofGardenShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F91C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F9AC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_02_8012EFA0, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012FAFC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker gMapAkropolisMarkers[MAP_AKROPOLIS_MARKER_COUNT + 1] = {
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, -33, -69 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, 33, -69 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, -20, -7 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, 20, -7 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, 43, -42 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, -80, -7 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, -62, 34 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, -52, 29 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, -35, 29 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, -28, 34 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, 41, 76 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, 59, 54 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, 26, 65 },
    { MAP_AKROPOLIS_PAGE_TOWER, MENU_MAP_MARKER_AREA_ANY, -44, 65 },
    { 0, 0, 0, 0 },
};

/// Fixed icon picture and cafeteria objective number used by this map.
enum {
    MAP_AKROPOLIS_ICON_KIND_FIXED_PICTURE = 0,
    MAP_AKROPOLIS_OBJECTIVE_CAFETERIA     = 2,
};

MenuMapIcon gMapAkropolisIcons[6] = {
    { MAP_AKROPOLIS_PAGE_TOWER, GAME_AREA_ACROPOLIS_SQUARE, MENU_MAP_ICON_KIND_TELEPHONE, 0, 31, -77 },
    { MAP_AKROPOLIS_PAGE_TOWER, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, MENU_MAP_ICON_KIND_TELEPHONE, 0, -76, 53 },
    { MAP_AKROPOLIS_PAGE_MIST, GAME_AREA_MIST_PARKING, MENU_MAP_ICON_KIND_TELEPHONE, 0, -110, -26 },
    { MAP_AKROPOLIS_PAGE_MIST, GAME_AREA_MIST_PARKING, MAP_AKROPOLIS_ICON_KIND_FIXED_PICTURE, GAME_FLAG_STORY_CHAPTER, -57, -7 },
    { MAP_AKROPOLIS_PAGE_TOWER, GAME_AREA_ACROPOLIS_CAFETERIA, MENU_MAP_ICON_KIND_OBJECTIVE, MAP_AKROPOLIS_OBJECTIVE_CAFETERIA, -77, 17 },
    { 0, 0, 0, 0, 0, 0 },
};

MenuMapAreaName gMapAkropolisAreaNames[GAME_AREA_MIST_R21] = {
    [GAME_AREA_ACROPOLIS_SQUARE - 1]                 = { "Square" },
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL - 1]     = { "East elevator hall" },
    [GAME_AREA_ACROPOLIS_PATIO - 1]                  = { "Patio" },
    [GAME_AREA_ACROPOLIS_CAFETERIA - 1]              = { "Cafeteria" },
    [GAME_AREA_ACROPOLIS_PLAZA - 1]                  = { "Plaza" },
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM - 1]          = { "Security room" },
    [GAME_AREA_ACROPOLIS_HALLWAY - 1]                = { "Hallway" },
    [GAME_AREA_ACROPOLIS_FOUNTAIN - 1]               = { "Fountain" },
    [GAME_AREA_ACROPOLIS_FORKED_ROAD - 1]            = { "Forked road" },
    [GAME_AREA_ACROPOLIS_OBSERVATORY - 1]            = { "Observatory" },
    [GAME_AREA_ACROPOLIS_PROMENADE - 1]              = { "Promenade" },
    [GAME_AREA_ACROPOLIS_SANCTUARY - 1]              = { "Sanctuary" },
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN - 1]            = { "Roof garden" },
    [GAME_AREA_ACROPOLIS_BRIDGE - 1]                 = { "Bridge" },
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE - 1]            = { "Fire escape" },
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD - 1] = { "Helicopter landing pad" },
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL - 1]     = { "West elevator hall" },
    [GAME_AREA_MIST_R18 - 1]                         = { "" },
    [GAME_AREA_MIST_PARKING - 1]                     = { "MIST parking" },
    [GAME_AREA_MIST_SHOOTING_GALLERY - 1]            = { "Shooting gallery" },
    [GAME_AREA_MIST_R21 - 1]                         = { "" },
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

/// Catalogue kinds and room-prop recipes in this stage's object tables.
///
/// The low-bank values are item catalogue IDs. The additional landing-pad
/// pickup kind selects an empty catalogue slot and has no placement here.
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
enum { MAP_AKROPOLIS_OBJECT_TASK_PRIORITY = 0x62 };

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
    { MAP_AKROPOLIS_KIND_STIM, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, acropolisCafeteriaStimPickupTask, { &gAcropolisCafeteriaModel0F7A4 } } },
    { INVENTORY_COLLECTION_ID_MENDEL_JOURNAL, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, acropolisCafeteriaMendelPickupTask, { &gAcropolisCafeteriaModel0FDFC } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Container-lid model-spawn recipe for the Security room.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
/// The area's empty placement list selects none of these recipes.
static AreaObjectSpawn _gMapAkropolisSecurityRoomObjectSpawns[2] = {
    { MAP_AKROPOLIS_KIND_SECURITY_ROOM_CONTAINER_LID, { { { TASK_BODY_TMD, MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, itemPickupContainerLidTask, { &gAcropolisSecurityRoomAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Blue-key model-spawn recipe for the Hallway.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
static AreaObjectSpawn _gMapAkropolisHallwayObjectSpawns[2] = {
    { INVENTORY_COLLECTION_ID_BLUE_KEY, { { { TASK_BODY_TMD, MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, acropolisHallwayBlueKeyPickupTask, { &gAcropolisHallwayModel01AE0 } } },
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
    { INVENTORY_COLLECTION_ID_RED_KEY, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, acropolisSanctuaryItemVisibilityTask, { &gAcropolisSanctuaryModel09584 } } },
    { MAP_AKROPOLIS_KIND_SANCTUARY_CONTAINER_LID, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, itemPickupContainerLidTask, { &gAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Armory-cardkey model-spawn recipe for the Roof garden.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
static AreaObjectSpawn _gMapAkropolisRoofGardenObjectSpawns[2] = {
    { INVENTORY_COLLECTION_ID_ARMORY_CARDKEY, { { { TASK_BODY_TMD, MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, acropolisRoofGardenPickupModelTask, { &gAcropolisRoofGardenModel09868 } } },
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

/// Lift state and model placement for the Helicopter landing pad.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
static AreaObjectPlace _gMapAkropolisHelicopterLandingPadObjectPlaces[2] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_HELICOPTER_LANDING_PAD_LIFT, MAP_AKROPOLIS_KIND_HELICOPTER_LANDING_PAD_LIFT, 0, MAP_AKROPOLIS_STATE_AVAILABLE, -0x14DC, -0xB40, -0x76C },
    { AREA_OBJECT_PLACE_END },
};

/// Lift and additional pickup model-spawn recipes for the Helicopter landing pad.
///
/// Ended by `AREA_OBJECT_SPAWN_END`; the owning area pairs this table with
/// its placement list. Callbacks and borrowed models require their room
/// overlay to remain loaded for the spawned task.
/// Only the lift has a matching placement; the pickup recipe is unselected.
static AreaObjectSpawn _gMapAkropolisHelicopterLandingPadObjectSpawns[3] = {
    { MAP_AKROPOLIS_KIND_HELICOPTER_LANDING_PAD_LIFT, { { { TASK_BODY_TMD, MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, acropolisHelicopterLandingPadLiftTask, { &gAcropolisHelicopterLandingPadModel0547C } } },
    { MAP_AKROPOLIS_KIND_HELICOPTER_LANDING_PAD_PICKUP, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), MAP_AKROPOLIS_OBJECT_TASK_PRIORITY } }, acropolisHelicopterLandingPadPickupModelTask, { &gAcropolisHelicopterLandingPadModel0A8E8 } } },
    { AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom gMapAkropolisObjectRooms[GAME_AREA_MIST_SHOOTING_GALLERY + 2] = {
    [0]                                          = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_ACROPOLIS_SQUARE]                 = { .places = { .list = _gMapAkropolisSquareObjectPlaces }, .spawns = _gMapAkropolisSquareObjectSpawns },
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL]     = { .places = { .list = _gMapAkropolisEastElevatorHallObjectPlaces }, .spawns = _gMapAkropolisEastElevatorHallObjectSpawns },
    [GAME_AREA_ACROPOLIS_PATIO]                  = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_ACROPOLIS_CAFETERIA]              = { .places = { .list = _gMapAkropolisCafeteriaObjectPlaces }, .spawns = _gMapAkropolisCafeteriaObjectSpawns },
    [GAME_AREA_ACROPOLIS_PLAZA]                  = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM]          = { .places = { .list = _gMapAkropolisSecurityRoomObjectPlaces }, .spawns = _gMapAkropolisSecurityRoomObjectSpawns },
    [GAME_AREA_ACROPOLIS_HALLWAY]                = { .places = { .list = _gMapAkropolisHallwayObjectPlaces }, .spawns = _gMapAkropolisHallwayObjectSpawns },
    [GAME_AREA_ACROPOLIS_FOUNTAIN]               = { .places = { .list = _gMapAkropolisFountainObjectPlaces }, .spawns = _gMapAkropolisFountainObjectSpawns },
    [GAME_AREA_ACROPOLIS_FORKED_ROAD]            = { .places = { .list = _gMapAkropolisForkedRoadObjectPlaces }, .spawns = _gMapAkropolisForkedRoadObjectSpawns },
    [GAME_AREA_ACROPOLIS_OBSERVATORY]            = { .places = { .list = _gMapAkropolisObservatoryObjectPlaces }, .spawns = _gMapAkropolisObservatoryObjectSpawns },
    [GAME_AREA_ACROPOLIS_PROMENADE]              = { .places = { .list = _gMapAkropolisPromenadeObjectPlaces }, .spawns = _gMapAkropolisPromenadeObjectSpawns },
    [GAME_AREA_ACROPOLIS_SANCTUARY]              = { .places = { .list = _gMapAkropolisSanctuaryObjectPlaces }, .spawns = _gMapAkropolisSanctuaryObjectSpawns },
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN]            = { .places = { .list = _gMapAkropolisRoofGardenObjectPlaces }, .spawns = _gMapAkropolisRoofGardenObjectSpawns },
    [GAME_AREA_ACROPOLIS_BRIDGE]                 = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE]            = { .places = { .list = _gMapAkropolisFireEscapeObjectPlaces }, .spawns = _gMapAkropolisFireEscapeObjectSpawns },
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD] = { .places = { .list = _gMapAkropolisHelicopterLandingPadObjectPlaces }, .spawns = _gMapAkropolisHelicopterLandingPadObjectSpawns },
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL]     = { .places = { .list = _gMapAkropolisWestElevatorHallObjectPlaces }, .spawns = _gMapAkropolisWestElevatorHallObjectSpawns },
    [GAME_AREA_MIST_R18]                         = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_MIST_PARKING]                     = { .places = { .list = _gMapAkropolisMistParkingObjectPlaces }, .spawns = _gMapAkropolisMistParkingObjectSpawns },
    [GAME_AREA_MIST_SHOOTING_GALLERY]            = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_MIST_SHOOTING_GALLERY + 1]        = { .places = { .sentinel = AREA_OBJECT_ROOM_END }, .spawns = NULL },
};

/// Priority required by the room-controller descriptor search.
enum { MAP_AKROPOLIS_ROOM_TASK_PRIORITY = 32 };

TaskDesc gMapAkropolisRoomTaskDescriptors[21] = {
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisSquareRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SQUARE, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisEastElevatorHallRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisPatioRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisCafeteriaRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_CAFETERIA, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisSecurityRoomMessageTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisHallwayRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_HALLWAY, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisFountainRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisForkedRoadRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FORKED_ROAD, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisObservatoryRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_OBSERVATORY, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisPromenadeRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisSanctuaryRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisRoofGardenRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisBridgeRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_BRIDGE, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisFireEscapeRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisHelicopterLandingPadRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, acropolisWestElevatorHallRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL, 1) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, mistR18BriefingTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_R18, 1) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, mistParkingRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_PARKING, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, mistShootingGalleryRoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0) } },
    { { { TASK_BODY_NONE, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, mistR21RoomTask, { .value = GP_TASK_LOC_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_R21, 0) } },
    { { { TASK_DESC_END, MAP_AKROPOLIS_ROOM_TASK_PRIORITY } }, NULL, { 0 } },
};

/// Whether the Dryfield map detail has been opened during the briefing menu.
///
/// Signed-word latch (0 not opened, 1 opened). A fresh menu request clears it;
/// retained requests preserve it, independently of the list's selected row.
static s32 _gMapAkropolisDryfieldMapOpened = 0;

/// Collection catalogue IDs shown and granted by the MIST briefing menu.
///
/// Row indices 0..3 select signed-word IDs; names come from the item catalogue.
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
    .taskPriority    = MAP_AKROPOLIS_KEY_ITEM_TASK_PRIORITY,
    .contentCallback = _mapAkropolisKeyItemPanelTask,
};

/// Bodyless display-mode host for the briefing's key-item UI tree.
///
/// Runs at priority 192; spawning copies this map-owned recipe.
static TaskDesc _gMapAkropolisKeyItemMenuTaskDescriptor = {
    .header   = { .fields = { TASK_BODY_NONE, MAP_AKROPOLIS_KEY_ITEM_TASK_PRIORITY } },
    .callback = _mapAkropolisKeyItemMenuTask,
};

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
    MAP_AKROPOLIS_MARKER_FLAG_SQUARE_EAST_ELEVATOR    = 0x1F6,
    MAP_AKROPOLIS_MARKER_FLAG_SQUARE_WEST_ELEVATOR    = 0x1F7,
};

u16 gMapAkropolisMarkerFlags[MAP_AKROPOLIS_MARKER_COUNT] = {
    MAP_AKROPOLIS_MARKER_FLAG_SQUARE_WEST_ELEVATOR,
    MAP_AKROPOLIS_MARKER_FLAG_SQUARE_EAST_ELEVATOR,
    MAP_AKROPOLIS_MARKER_FLAG_PATIO_FOUNTAIN,
    MAP_AKROPOLIS_MARKER_FLAG_FOUNTAIN_FORKED_ROAD,
    GAME_FLAG_MAP_MARK_SECURITY_ROOM,
    MAP_AKROPOLIS_MARKER_FLAG_PATIO_CAFETERIA,
    MAP_AKROPOLIS_MARKER_FLAG_CAFETERIA_HALLWAY_VIEW2,
    MAP_AKROPOLIS_MARKER_FLAG_CAFETERIA_HALLWAY_VIEW3,
    MAP_AKROPOLIS_MARKER_FLAG_HALLWAY_SECURITY_ROOM,
    MAP_AKROPOLIS_MARKER_FLAG_HALLWAY_FOUNTAIN,
    MAP_AKROPOLIS_MARKER_FLAG_PROMENADE_SANCTUARY,
    MAP_AKROPOLIS_MARKER_FLAG_SANCTUARY_ROOF_GARDEN,
    MAP_AKROPOLIS_MARKER_FLAG_PROMENADE_BRIDGE,
    MAP_AKROPOLIS_MARKER_FLAG_BRIDGE_FIRE_ESCAPE,
};

WorldCoordRoomLighting* gMapAkropolisAreaRoomLightingTables[GAME_AREA_MIST_SHOOTING_GALLERY] = {
    [GAME_AREA_ACROPOLIS_SQUARE - 1]                 = gAcropolisSquareRoomLightingTable,
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL - 1]     = gAcropolisEastElevatorHallRoomLightingTable,
    [GAME_AREA_ACROPOLIS_PATIO - 1]                  = gAcropolisPatioRoomLightingTable,
    [GAME_AREA_ACROPOLIS_CAFETERIA - 1]              = gAcropolisCafeteriaRoomLightingTable,
    [GAME_AREA_ACROPOLIS_PLAZA - 1]                  = gAcropolisPlazaRoomLightingTable,
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM - 1]          = gAcropolisSecurityRoomRoomLightingTable,
    [GAME_AREA_ACROPOLIS_HALLWAY - 1]                = gAcropolisHallwayRoomLightingTable,
    [GAME_AREA_ACROPOLIS_FOUNTAIN - 1]               = gAcropolisFountainRoomLightingTable,
    [GAME_AREA_ACROPOLIS_FORKED_ROAD - 1]            = gAcropolisForkedRoadRoomLightingTable,
    [GAME_AREA_ACROPOLIS_OBSERVATORY - 1]            = gAcropolisObservatoryRoomLightingTable,
    [GAME_AREA_ACROPOLIS_PROMENADE - 1]              = gAcropolisPromenadeRoomLightingTable,
    [GAME_AREA_ACROPOLIS_SANCTUARY - 1]              = gAcropolisSanctuaryRoomLightingTable,
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN - 1]            = gAcropolisRoofGardenRoomLightingTable,
    [GAME_AREA_ACROPOLIS_BRIDGE - 1]                 = gAcropolisBridgeRoomLightingTable,
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE - 1]            = gAcropolisFireEscapeRoomLightingTable,
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD - 1] = gAcropolisHelicopterLandingPadRoomLightingTable,
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL - 1]     = gAcropolisWestElevatorHallRoomLightingTable,
    [GAME_AREA_MIST_R18 - 1]                         = gMistR18RoomLightingTable,
    [GAME_AREA_MIST_PARKING - 1]                     = gMistParkingRoomLightingTable,
    [GAME_AREA_MIST_SHOOTING_GALLERY - 1]            = gMistShootingGalleryRoomLightingTable,
};

static WorldCollisionRoomResources* D_map_akropolis_8017AA78[20] = {
    gAcropolisSquareRoomCollisionResources,
    gAcropolisEastElevatorHallRoomCollisionResources,
    gAcropolisPatioRoomCollisionResources,
    gAcropolisCafeteriaCollisionRooms,
    gAcropolisPlazaRoomCollisionResources,
    gAcropolisSecurityRoomRoomCollisionResources,
    gAcropolisHallwayRoomCollisionResources,
    gAcropolisFountainRoomCollisionResources,
    gAcropolisForkedRoadRoomCollisionResources,
    gAcropolisObservatoryRoomCollisionResources,
    gAcropolisPromenadeRoomCollisionResources,
    gAcropolisSanctuaryRoomCollisionResources,
    gAcropolisRoofGardenCollisionRoomResources,
    gAcropolisBridgeRoomCollisionResources,
    gAcropolisFireEscapeCollisionRoomResources,
    D_acropolis_helicopter_landing_pad_80184F10,
    gAcropolisWestElevatorHallCollisionRoomResources,
    gMistR18CollisionRoomResources,
    gMistParkingRoomCollisionResources,
    gMistShootingGalleryRoomCollisionResources,
};

WorldCollisionStageResources D_map_akropolis_8017AAC8 = { D_map_akropolis_8017AA78 };

/// Area directory of borrowed sprite-view arrays for Acropolis and MIST.
///
/// Indexed by area - 1 for areas 1..20; no terminator or area-21 entry.
/// Each selected array requires its room overlay to remain loaded; its mapped
/// 1-based view index must fit that array independently of this directory.
static SpriteView* _gMapAkropolisAreaSpriteViews[GAME_AREA_MIST_SHOOTING_GALLERY] = {
    [GAME_AREA_ACROPOLIS_SQUARE - 1]                 = gAcropolisSquareSpriteViews,
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL - 1]     = gAcropolisEastElevatorHallSpriteViews,
    [GAME_AREA_ACROPOLIS_PATIO - 1]                  = gAcropolisPatioSpriteViews,
    [GAME_AREA_ACROPOLIS_CAFETERIA - 1]              = gAcropolisCafeteriaSpriteViews,
    [GAME_AREA_ACROPOLIS_PLAZA - 1]                  = gAcropolisPlazaSpriteViews,
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM - 1]          = gAcropolisSecurityRoomSpriteViews,
    [GAME_AREA_ACROPOLIS_HALLWAY - 1]                = gAcropolisHallwaySpriteViews,
    [GAME_AREA_ACROPOLIS_FOUNTAIN - 1]               = gAcropolisFountainSpriteViews,
    [GAME_AREA_ACROPOLIS_FORKED_ROAD - 1]            = gAcropolisForkedRoadSpriteViews,
    [GAME_AREA_ACROPOLIS_OBSERVATORY - 1]            = gAcropolisObservatorySpriteViews,
    [GAME_AREA_ACROPOLIS_PROMENADE - 1]              = gAcropolisPromenadeSpriteViews,
    [GAME_AREA_ACROPOLIS_SANCTUARY - 1]              = gAcropolisSanctuarySpriteViews,
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN - 1]            = gAcropolisRoofGardenSpriteViews,
    [GAME_AREA_ACROPOLIS_BRIDGE - 1]                 = gAcropolisBridgeSpriteViews,
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE - 1]            = gAcropolisFireEscapeSpriteViews,
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD - 1] = gAcropolisHelicopterLandingPadSpriteViews,
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL - 1]     = gAcropolisWestElevatorHallSpriteViews,
    [GAME_AREA_MIST_R18 - 1]                         = gMistR18SpriteViews,
    [GAME_AREA_MIST_PARKING - 1]                     = gMistParkingSpriteViews,
    [GAME_AREA_MIST_SHOOTING_GALLERY - 1]            = gMistShootingGallerySpriteViews,
};

SpriteAreaTable gMapAkropolisSpriteAreaTable = { .areaViews = _gMapAkropolisAreaSpriteViews };

DirectionWarpEntry* D_map_akropolis_8017AB20[20] = {
    gAcropolisSquareWarpEntries,
    gAcropolisEastElevatorHallWarpEntries,
    gAcropolisPatioWarpEntries,
    gAcropolisCafeteriaWarps,
    gAcropolisPlazaWarpEntries,
    gAcropolisSecurityRoomWarpEntries,
    gAcropolisHallwayWarpEntries,
    gAcropolisFountainWarpEntries,
    gAcropolisForkedRoadWarpEntries,
    gAcropolisObservatoryWarpEntries,
    gAcropolisPromenadeWarpEntries,
    gAcropolisSanctuaryWarpEntries,
    gAcropolisRoofGardenWarpEntries,
    gAcropolisBridgeWarpEntries,
    gAcropolisFireEscapeWarpEntries,
    gAcropolisHelicopterLandingPadWarps,
    gAcropolisWestElevatorHallWarpEntries,
    gMistR18WarpEntries,
    gMistParkingWarpEntries,
    gMistShootingGalleryWarpEntries,
};

/// Borrowed room-count arrays for Acropolis and MIST areas 1..20.
///
/// Indexed by area - 1; no terminator or area-21 entry. The selected room
/// overlay owns the signed-halfword counts and must remain loaded for lookup.
static ViewCount* _gMapAkropolisAreaViewCounts[GAME_AREA_MIST_SHOOTING_GALLERY] = {
    [GAME_AREA_ACROPOLIS_SQUARE - 1]                 = gAcropolisSquareViewCounts,
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL - 1]     = gAcropolisEastElevatorHallViewCounts,
    [GAME_AREA_ACROPOLIS_PATIO - 1]                  = gAcropolisPatioViewCounts,
    [GAME_AREA_ACROPOLIS_CAFETERIA - 1]              = gAcropolisCafeteriaViewCounts,
    [GAME_AREA_ACROPOLIS_PLAZA - 1]                  = gAcropolisPlazaViewCounts,
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM - 1]          = gAcropolisSecurityRoomViewCounts,
    [GAME_AREA_ACROPOLIS_HALLWAY - 1]                = gAcropolisHallwayViewCounts,
    [GAME_AREA_ACROPOLIS_FOUNTAIN - 1]               = gAcropolisFountainViewCounts,
    [GAME_AREA_ACROPOLIS_FORKED_ROAD - 1]            = gAcropolisForkedRoadViewCounts,
    [GAME_AREA_ACROPOLIS_OBSERVATORY - 1]            = gAcropolisObservatoryViewCounts,
    [GAME_AREA_ACROPOLIS_PROMENADE - 1]              = gAcropolisPromenadeViewCounts,
    [GAME_AREA_ACROPOLIS_SANCTUARY - 1]              = gAcropolisSanctuaryViewCounts,
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN - 1]            = gAcropolisRoofGardenViewCounts,
    [GAME_AREA_ACROPOLIS_BRIDGE - 1]                 = gAcropolisBridgeViewCounts,
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE - 1]            = gAcropolisFireEscapeViewCounts,
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD - 1] = gAcropolisHelicopterLandingPadViewCounts,
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL - 1]     = gAcropolisWestElevatorHallViewCounts,
    [GAME_AREA_MIST_R18 - 1]                         = gMistR18ViewCounts,
    [GAME_AREA_MIST_PARKING - 1]                     = gMistParkingViewCounts,
    [GAME_AREA_MIST_SHOOTING_GALLERY - 1]            = gMistShootingGalleryViewCounts,
};

ViewCountTable gMapAkropolisViewCountTable = { .viewCounts = _gMapAkropolisAreaViewCounts };

/// Area directory of borrowed camera arrays for Acropolis and MIST.
///
/// Indexed by area - 1 for areas 1..20; no terminator or area-21 entry.
/// Each selected array requires its room overlay to remain loaded; its mapped
/// 1-based view index must fit that array independently of this directory.
static ViewCamera* _gMapAkropolisAreaCameras[GAME_AREA_MIST_SHOOTING_GALLERY] = {
    [GAME_AREA_ACROPOLIS_SQUARE - 1]                 = gAcropolisSquareViewCameras,
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL - 1]     = gAcropolisEastElevatorHallViewCameras,
    [GAME_AREA_ACROPOLIS_PATIO - 1]                  = gAcropolisPatioViewCameras,
    [GAME_AREA_ACROPOLIS_CAFETERIA - 1]              = gAcropolisCafeteriaViewCameras,
    [GAME_AREA_ACROPOLIS_PLAZA - 1]                  = gAcropolisPlazaViewCameras,
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM - 1]          = gAcropolisSecurityRoomViewCameras,
    [GAME_AREA_ACROPOLIS_HALLWAY - 1]                = gAcropolisHallwayViewCameras,
    [GAME_AREA_ACROPOLIS_FOUNTAIN - 1]               = gAcropolisFountainViewCameras,
    [GAME_AREA_ACROPOLIS_FORKED_ROAD - 1]            = gAcropolisForkedRoadViewCameras,
    [GAME_AREA_ACROPOLIS_OBSERVATORY - 1]            = gAcropolisObservatoryViewCameras,
    [GAME_AREA_ACROPOLIS_PROMENADE - 1]              = gAcropolisPromenadeViewCameras,
    [GAME_AREA_ACROPOLIS_SANCTUARY - 1]              = gAcropolisSanctuaryViewCameras,
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN - 1]            = gAcropolisRoofGardenViewCameras,
    [GAME_AREA_ACROPOLIS_BRIDGE - 1]                 = gAcropolisBridgeViewCameras,
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE - 1]            = gAcropolisFireEscapeViewCameras,
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD - 1] = gAcropolisHelicopterLandingPadViewCameras,
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL - 1]     = gAcropolisWestElevatorHallViewCameras,
    [GAME_AREA_MIST_R18 - 1]                         = gMistR18ViewCameras,
    [GAME_AREA_MIST_PARKING - 1]                     = gMistParkingViewCameras,
    [GAME_AREA_MIST_SHOOTING_GALLERY - 1]            = gMistShootingGalleryViewCameras,
};

ViewCameraTable gMapAkropolisViewCameraTable = { .cameras = _gMapAkropolisAreaCameras };

/// Area directory of room-local logical-view maps for Acropolis and MIST.
///
/// Indexed by area - 1 for areas 1..20, then room - 1 and view - 1. No
/// terminator or area-21 entry. Room pointers are borrowed while the selected
/// room overlay is loaded; shared identity-map bytes belong to gameplay.
static u8** _gMapAkropolisAreaViewMaps[GAME_AREA_MIST_SHOOTING_GALLERY] = {
    [GAME_AREA_ACROPOLIS_SQUARE - 1]                 = gAcropolisSquareViewMaps,
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL - 1]     = gAcropolisEastElevatorHallViewMaps,
    [GAME_AREA_ACROPOLIS_PATIO - 1]                  = gAcropolisPatioViewMaps,
    [GAME_AREA_ACROPOLIS_CAFETERIA - 1]              = gAcropolisCafeteriaViewMaps,
    [GAME_AREA_ACROPOLIS_PLAZA - 1]                  = gAcropolisPlazaViewMaps,
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM - 1]          = gAcropolisSecurityRoomViewMaps,
    [GAME_AREA_ACROPOLIS_HALLWAY - 1]                = gAcropolisHallwayViewMaps,
    [GAME_AREA_ACROPOLIS_FOUNTAIN - 1]               = gAcropolisFountainViewMaps,
    [GAME_AREA_ACROPOLIS_FORKED_ROAD - 1]            = gAcropolisForkedRoadViewMaps,
    [GAME_AREA_ACROPOLIS_OBSERVATORY - 1]            = gAcropolisObservatoryViewMaps,
    [GAME_AREA_ACROPOLIS_PROMENADE - 1]              = gAcropolisPromenadeViewMaps,
    [GAME_AREA_ACROPOLIS_SANCTUARY - 1]              = gAcropolisSanctuaryViewMaps,
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN - 1]            = gAcropolisRoofGardenViewMaps,
    [GAME_AREA_ACROPOLIS_BRIDGE - 1]                 = gAcropolisBridgeViewMaps,
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE - 1]            = gAcropolisFireEscapeViewMaps,
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD - 1] = gAcropolisHelicopterLandingPadViewMaps,
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL - 1]     = gAcropolisWestElevatorHallViewMaps,
    [GAME_AREA_MIST_R18 - 1]                         = gMistR18ViewMaps,
    [GAME_AREA_MIST_PARKING - 1]                     = gMistParkingViewMaps,
    [GAME_AREA_MIST_SHOOTING_GALLERY - 1]            = gMistShootingGalleryViewMaps,
};

ViewIndexTable gMapAkropolisViewIndexTable = { _gMapAkropolisAreaViewMaps };

WorldCollisionSurfaceProperties** gMapAkropolisSurfacePropertiesByArea[GAME_AREA_MIST_SHOOTING_GALLERY] = {
    [GAME_AREA_ACROPOLIS_SQUARE - 1]                 = gAcropolisSquareSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL - 1]     = gAcropolisEastElevatorHallSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_PATIO - 1]                  = gAcropolisPatioSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_CAFETERIA - 1]              = gAcropolisCafeteriaSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_PLAZA - 1]                  = gAcropolisPlazaSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_SECURITY_ROOM - 1]          = gAcropolisSecurityRoomSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_HALLWAY - 1]                = gAcropolisHallwaySurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_FOUNTAIN - 1]               = gAcropolisFountainSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_FORKED_ROAD - 1]            = gAcropolisForkedRoadSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_OBSERVATORY - 1]            = gAcropolisObservatorySurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_PROMENADE - 1]              = gAcropolisPromenadeSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_SANCTUARY - 1]              = gAcropolisSanctuarySurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_ROOF_GARDEN - 1]            = gAcropolisRoofGardenSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_BRIDGE - 1]                 = gAcropolisBridgeSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_FIRE_ESCAPE - 1]            = gAcropolisFireEscapeSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD - 1] = gAcropolisHelicopterLandingPadSurfacePropertiesByClass,
    [GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL - 1]     = gAcropolisWestElevatorHallSurfacePropertiesByClass,
    [GAME_AREA_MIST_R18 - 1]                         = gMistR18SurfacePropertiesByClass,
    [GAME_AREA_MIST_PARKING - 1]                     = gMistParkingSurfacePropertiesByClass,
    [GAME_AREA_MIST_SHOOTING_GALLERY - 1]            = gMistShootingGallerySurfacePropertiesByClass,
};

AreaPlacement gMapAkropolisSquareVariant1Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER, .variant = 0, .mode = 0, .x = 6000, .y = -2200, .z = 3215, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = -1, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisEastElevatorHallVariant1Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_VIEW_FIGURE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = -1, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPatioVariant1Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SWAT_MEMBER, .variant = 0, .mode = 0, .x = -2900, .y = 0, .z = 1600, .yaw = 0x600, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = 0, .x = -2800, .y = 0, .z = 1500, .yaw = -0x200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPatioVariant2Placements[5] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = 0, .mode = 2, .x = -6000, .y = -3780, .z = -1900, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = 0, .mode = 2, .x = -5100, .y = -3780, .z = -1900, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = MAP_AKROPOLIS_PLACEMENT_STRANGER_TUNING_FIRST, .x = -7000, .y = 0, .z = 700, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = MAP_AKROPOLIS_PLACEMENT_STRANGER_TUNING_FIRST, .x = -7300, .y = 0, .z = -800, .yaw = -0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPatioVariant3Placements[7] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5900, .y = -1000, .z = 1500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5700, .y = -1000, .z = 1500, .yaw = -0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -6100, .y = -1000, .z = 1500, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -7700, .y = 0, .z = 300, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -7700, .y = 0, .z = 800, .yaw = 0x9C4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -7500, .y = 0, .z = 500, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPatioVariant4Placements[5] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = 0, .x = -7000, .y = 0, .z = 700, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = 0, .x = -7300, .y = 0, .z = -800, .yaw = -0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -6200, .y = -800, .z = 1170, .yaw = 0x10E, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5400, .y = -1000, .z = 1100, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPatioVariant5Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = 0, .mode = 0, .x = 100, .y = 1, .z = -1000, .yaw = 0x1CC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = 0, .mode = 0, .x = -2500, .y = 0, .z = 500, .yaw = -0x200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPatioVariant10Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_PATIO_UNMATCHED, .variant = 4, .mode = 0x212, .x = 5290, .y = 1, .z = -270, .yaw = 0x456, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_PATIO_UNMATCHED, .variant = 0, .mode = 0x1212, .x = 5340, .y = 1, .z = 830, .yaw = 0x456, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisCafeteriaVariant1Placements[4] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_BOSS_HIDDEN_RESTORED, .mode = MAP_AKROPOLIS_PLACEMENT_BOSS_MOVEMENT_LARGEST, .x = -3400, .y = -300, .z = -1800, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_WOMAN, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RUPERT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisCafeteriaVariant2Placements[4] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_BOSS_HIDDEN_RESTORED, .mode = MAP_AKROPOLIS_PLACEMENT_BOSS_MOVEMENT_LARGEST, .x = -3400, .y = -300, .z = -1800, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RUPERT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisCafeteriaVariant3Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_ODD_STRANGER_STALK | MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = MAP_AKROPOLIS_PLACEMENT_STRANGER_TUNING_FIRST, .x = 100, .y = -300, .z = 0, .yaw = 0x7D0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisCafeteriaVariant4Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_ODD_STRANGER_STALK | MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = MAP_AKROPOLIS_PLACEMENT_STRANGER_TUNING_FIRST, .x = 100, .y = -300, .z = 0, .yaw = 0x7D0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_ODD_STRANGER_STALK, .mode = MAP_AKROPOLIS_PLACEMENT_STRANGER_TUNING_FIRST, .x = 100, .y = -300, .z = 1500, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisCafeteriaVariant5Placements[6] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BONE_SUCKLER, .variant = MAP_AKROPOLIS_PLACEMENT_BONE_SUCKLER_PATROL, .mode = 0, .x = 2500, .y = -300, .z = 550, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BONE_SUCKLER, .variant = MAP_AKROPOLIS_PLACEMENT_BONE_SUCKLER_PATROL, .mode = 0, .x = 260, .y = -300, .z = 1600, .yaw = 0x71E, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BONE_SUCKLER, .variant = MAP_AKROPOLIS_PLACEMENT_BONE_SUCKLER_PATROL, .mode = 0, .x = 1150, .y = -300, .z = -160, .yaw = 0x762, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BONE_SUCKLER, .variant = MAP_AKROPOLIS_PLACEMENT_BONE_SUCKLER_IDLE, .mode = 0, .x = 770, .y = -300, .z = -2070, .yaw = 0xE9C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BONE_SUCKLER, .variant = MAP_AKROPOLIS_PLACEMENT_BONE_SUCKLER_IDLE, .mode = 0, .x = 1200, .y = -300, .z = 1730, .yaw = 0x15E, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisCafeteriaVariant7Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ROOK_GOLEM, .variant = 3, .mode = MAP_AKROPOLIS_PLACEMENT_ROOK_GOLEM_PATROL, .x = -3800, .y = -300, .z = 1000, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPlazaVariant1Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_OFFICER_2, .variant = 0, .mode = 0, .x = 13600, .y = 0, .z = 4600, .yaw = 0x6AA, .fileIdLow = 2, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_OFFICER_1, .variant = 0, .mode = 0, .x = 970, .y = 0, .z = 14650, .yaw = 0x800, .fileIdLow = 1, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisSecurityRoomVariant1Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RUPERT, .variant = 0, .mode = 0, .x = -5188, .y = 0, .z = -2800, .yaw = 0x100, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SECURITY_SWAT, .variant = 0, .mode = 0, .x = -4730, .y = 0, .z = -3442, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisHallwayVariant1Placements[9] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 1000, .y = -1000, .z = -1000, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 1000, .y = -1000, .z = -500, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 800, .y = -1000, .z = -900, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 1000, .y = -1000, .z = 1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 500, .y = -1000, .z = 900, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -1100, .y = -1000, .z = -1000, .yaw = 0x5DC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -800, .y = -1000, .z = -800, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -600, .y = -1000, .z = -900, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisHallwayVariant2Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_ODD_STRANGER_STALK, .mode = 0, .x = 0, .y = -1000, .z = -500, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisHallwayVariant3Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_ODD_STRANGER_STALK, .mode = 0, .x = 0, .y = -1000, .z = -500, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisHallwayVariant4Placements[6] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_ODD_STRANGER_STALK, .mode = MAP_AKROPOLIS_PLACEMENT_STRANGER_TUNING_FIRST, .x = 0, .y = -1000, .z = -500, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 1000, .y = -1000, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 1000, .y = -1000, .z = -500, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 1000, .y = -1000, .z = 1000, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 500, .y = -1000, .z = 1000, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisFountainVariant2Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_NEAR, .mode = 0, .x = -3760, .y = 0, .z = -1700, .yaw = -0x200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_NEAR, .mode = 0, .x = 520, .y = 2, .z = -4400, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisFountainVariant7Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_LARGE_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_NEAR, .mode = 0, .x = -3760, .y = 0, .z = -1700, .yaw = -0x200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_LARGE_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_NEAR, .mode = 0, .x = 520, .y = 2, .z = -4400, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant2Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = 0, .x = -500, .y = 1, .z = -700, .yaw = 0x44C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant3Placements[5] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = 0, .mode = 0, .x = -4000, .y = 0, .z = -1000, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -500, .y = 0, .z = -1200, .yaw = 0x2BC, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = -800, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = -300, .yaw = 0x514, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant4Placements[5] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 3, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE, .x = 700, .y = 0, .z = -4500, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE, .x = 1400, .y = 0, .z = -2400, .yaw = 0xD48, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 2, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE, .x = 2000, .y = 0, .z = 0, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 1, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE, .x = 1200, .y = 0, .z = 1500, .yaw = 0xB86, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant7Placements[5] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_PAWN_GOLEM, .variant = 3, .mode = MAP_AKROPOLIS_PLACEMENT_PAWN_GOLEM_PATROL, .x = -1850, .y = 0, .z = 800, .yaw = 0x9F6, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -500, .y = 0, .z = -1200, .yaw = 0x2BC, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = -800, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = -300, .yaw = 0x514, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 7 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant8Placements[5] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 3, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE, .x = 700, .y = 0, .z = -4500, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE, .x = 1400, .y = 0, .z = -2400, .yaw = 0xD48, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 2, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE, .x = 2000, .y = 0, .z = 0, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 1, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_LEAP_ENTRANCE, .x = 1200, .y = 0, .z = 1500, .yaw = 0xB86, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant9Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = 0, .x = -390, .y = 1, .z = -900, .yaw = 0x44C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant10Placements[12] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -6176, .y = 0, .z = -448, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -736, .y = 0, .z = -1152, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -5376, .y = 0, .z = -2720, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -1184, .y = 0, .z = 1312, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 5 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -960, .y = 0, .z = 128, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -2656, .y = 0, .z = -1352, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -6272, .y = -800, .z = -1664, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -6272, .y = -800, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -6400, .y = -800, .z = -2848, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5823, .y = -800, .z = -1953, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5344, .y = -800, .z = -1696, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant11Placements[7] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_AMOEBA, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_AMOEBA_VARIANT_NORMAL, .x = -2600, .y = 0, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_AMOEBA, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_AMOEBA_VARIANT_ALTERNATE, .x = -6176, .y = 0, .z = -448, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_AMOEBA, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_AMOEBA_VARIANT_NORMAL, .x = -736, .y = 0, .z = -1152, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_AMOEBA, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_AMOEBA_VARIANT_ALTERNATE, .x = -5376, .y = 0, .z = -2720, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_AMOEBA, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_AMOEBA_VARIANT_NORMAL, .x = -1184, .y = 0, .z = 1312, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_AMOEBA, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_AMOEBA_VARIANT_ALTERNATE, .x = -960, .y = 0, .z = 128, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 5 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant12Placements[7] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCORPION, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_SCORPION_ROAM, .x = -2600, .y = 0, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCORPION, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_SCORPION_AMBUSH_LEADER, .x = -6176, .y = 0, .z = -448, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCORPION, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_SCORPION_AMBUSH_LEADER, .x = -736, .y = 0, .z = -1152, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCORPION, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_SCORPION_AMBUSH_LEADER, .x = -5376, .y = 0, .z = -2720, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCORPION, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_SCORPION_AMBUSH_LEADER, .x = -1184, .y = 0, .z = 1312, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCORPION, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_SCORPION_AMBUSH_LEADER, .x = -960, .y = 0, .z = 128, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 5 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant13Placements[5] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -2600, .y = 0, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_AIMED, .x = -5400, .y = 0, .z = -2000, .yaw = 0x200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_LEADER, .x = -5400, .y = -6000, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_LEADER, .x = -5000, .y = -6000, .z = -2700, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant14Placements[7] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_PERCH_DROP, .x = -2600, .y = -1000, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_PERCH_BACK, .x = -5376, .y = -1000, .z = -2720, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_WANDER, .x = -736, .y = -1000, .z = -1152, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_PERCH_DROP, .x = -960, .y = -1000, .z = 128, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_PERCH_DROP, .x = -1184, .y = -1000, .z = 1312, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_PERCH_DROP, .x = -6176, .y = -1000, .z = -448, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 5 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant15Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MIND_SUCKLER, .variant = 0, .mode = 0, .x = -2600, .y = -1500, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MIND_SUCKLER, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_MIND_SUCKLER_PERCH, .x = -5300, .y = -1500, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant16Placements[7] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BLACK_BEETLE, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BLACK_BEETLE_IDLE, .x = -2600, .y = 0, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BLACK_BEETLE, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BLACK_BEETLE_WANDER, .x = -6176, .y = 0, .z = -448, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BLACK_BEETLE, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BLACK_BEETLE_IDLE, .x = -736, .y = 0, .z = -1152, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BLACK_BEETLE, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BLACK_BEETLE_WANDER, .x = -5376, .y = 0, .z = -2720, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BLACK_BEETLE, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BLACK_BEETLE_IDLE, .x = -1184, .y = 0, .z = 1312, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BLACK_BEETLE, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BLACK_BEETLE_WANDER, .x = -960, .y = 0, .z = 128, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 5 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant20Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCAVENGER, .variant = 0, .mode = 0, .x = -2656, .y = 0, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant21Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SUCKLERCEPH, .variant = 0, .mode = 0, .x = -5245, .y = 0, .z = -3279, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SKULL_STALKER, .variant = 0, .mode = 0, .x = -2656, .y = 0, .z = -1000, .yaw = 0x3E8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisForkedRoadVariant22Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SLOUCH, .variant = 0, .mode = 0, .x = -2656, .y = 0, .z = -1000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CREEPING_STRANGER, .variant = 0, .mode = 0, .x = 60, .y = 0, .z = 896, .yaw = 0x3E8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 3, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisObservatoryVariant1Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER, .variant = 0, .mode = 0, .x = -2500, .y = -2990, .z = -5200, .yaw = 0x708, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GRINNING_STRANGER, .variant = MAP_AKROPOLIS_PLACEMENT_STRANGER_SCRIPTED_DORMANT, .mode = 0, .x = -5022, .y = -2990, .z = -8087, .yaw = -0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisObservatoryVariant2Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_RUN_OR_NEAR, .mode = 0, .x = -1600, .y = -2990, .z = -4500, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_NEAR, .mode = 0, .x = -4500, .y = -2990, .z = -8000, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisObservatoryVariant3Placements[10] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -1900, .y = -9300, .z = -7300, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -2300, .y = -9500, .z = -7000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -2600, .y = -9400, .z = -7400, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -1600, .y = -9600, .z = -7300, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -1400, .y = -9500, .z = -7100, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -1000, .y = -9400, .z = -6800, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -1500, .y = -9300, .z = -6900, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -2100, .y = -9500, .z = -7200, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BAT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_BAT_SCRIPTED_ENTRANCE, .x = -1700, .y = -9500, .z = -7000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisObservatoryVariant7Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_LARGE_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_RUN_OR_NEAR, .mode = 0, .x = -1600, .y = -2990, .z = -4500, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_LARGE_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_NEAR, .mode = 0, .x = -4500, .y = -2990, .z = -8000, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisObservatoryVariant10Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_ODD_STRANGER, .variant = 0, .mode = 0, .x = -4500, .y = -2990, .z = -7500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPromenadeVariant1Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_RUN_OR_NEAR, .mode = 0, .x = -1500, .y = 40, .z = -6000, .yaw = 0x8FC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOSSBACK, .variant = MAP_AKROPOLIS_PLACEMENT_MOSSBACK_AWARENESS_RUN_OR_NEAR, .mode = 0, .x = -500, .y = 40, .z = -1000, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPromenadeVariant2Placements[8] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -2200, .y = 40, .z = -6700, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -2000, .y = 40, .z = -7400, .yaw = 0x8FC, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -1600, .y = 40, .z = -6950, .yaw = 0x9C4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RAT, .variant = 0, .mode = 0, .x = -1800, .y = 40, .z = -7700, .yaw = 0xB86, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -2000, .z = -2800, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -1800, .z = -3000, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -2200, .z = -3200, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPromenadeVariant3Placements[11] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -2000, .y = 40, .z = 600, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -1600, .y = 40, .z = -900, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -300, .y = 40, .z = -600, .yaw = -0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -1650, .y = 40, .z = -5000, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -2000, .z = -200, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -1800, .z = -400, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -2200, .z = -600, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = 800, .y = -2500, .z = -4200, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = 800, .y = -2800, .z = -3900, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = 800, .y = -2400, .z = -3600, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPromenadeVariant7Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_KNIGHT_GOLEM, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisPromenadeVariant8Placements[11] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -2000, .y = 40, .z = 600, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -1600, .y = 40, .z = -900, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -300, .y = 40, .z = -600, .yaw = -0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DORMANT, .x = -1650, .y = 40, .z = -5000, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -2000, .z = -200, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -1800, .z = -400, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -2200, .y = -2200, .z = -600, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = 800, .y = -2500, .z = -4200, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = 800, .y = -2800, .z = -3900, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = 800, .y = -2400, .z = -3600, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisSanctuaryNo9AndRupertPlacements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_NO9_GOLEM, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RUPERT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisSanctuaryRupertPlacements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_UNUSED, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_RUPERT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisRoofGardenVariant1Placements[6] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_VIEW_FIGURE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE, .x = -6700, .y = -5000, .z = -7500, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 1, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE, .x = -5200, .y = -5000, .z = -7700, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 2, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE, .x = -7600, .y = -5000, .z = -9000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 3, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE, .x = -3900, .y = -5000, .z = -5700, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisRoofGardenVariant2Placements[8] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_FOLLOWER, .x = -6700, .y = -5000, .z = -7500, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_FOLLOWER, .x = -5200, .y = -5300, .z = -7700, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_FOLLOWER, .x = -7600, .y = -5600, .z = -9000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MAGGOT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_LEADER, .x = -3900, .y = -5900, .z = -5700, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5500, .y = -2000, .z = -2500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -4000, .y = -2000, .z = -2500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5000, .y = -2000, .z = -2500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisRoofGardenVariant7Placements[6] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_VIEW_FIGURE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE, .x = -6700, .y = -5000, .z = -7500, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 1, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE, .x = -5200, .y = -5000, .z = -7700, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 2, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE, .x = -7600, .y = -5000, .z = -9000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 3, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_DROP_ENTRANCE, .x = -3900, .y = -5000, .z = -5700, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisRoofGardenVariant8Placements[8] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_FOLLOWER, .x = -6700, .y = -5000, .z = -7500, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_FOLLOWER, .x = -5200, .y = -5300, .z = -7700, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_FOLLOWER, .x = -7600, .y = -5600, .z = -9000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_CATERPILLAR, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_CRAWLER_HANG_LEADER, .x = -3900, .y = -5900, .z = -5700, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5500, .y = -2000, .z = -2500, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -4000, .y = -2000, .z = -2500, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_MOTH, .variant = 0, .mode = 0, .x = -5000, .y = -2000, .z = -2500, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 7 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisBridgeDefaultPlacements[7] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BRIDGE_ENEMY, .variant = 0, .mode = 0, .x = -12000, .y = 500, .z = -1952, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BRIDGE_ENEMY, .variant = 0, .mode = 0, .x = -12000, .y = 500, .z = -1352, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BRIDGE_ENEMY, .variant = 0, .mode = 0, .x = -12000, .y = 500, .z = -1552, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BRIDGE_ENEMY, .variant = 0, .mode = 0, .x = -12000, .y = 500, .z = -1952, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BRIDGE_ENEMY, .variant = 0, .mode = 0, .x = -12000, .y = 500, .z = -1352, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BRIDGE_ENEMY, .variant = 0, .mode = 0, .x = -12000, .y = 500, .z = -1900, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisBridgeVariant2Placements[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisBridgeVariant3Placements[1] = {
    { AREA_PLACEMENT_END },
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

AreaPlacement gMapAkropolisFireEscapePlacements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_STRANGER, .variant = 0, .mode = 0, .x = 1200, .y = 0, .z = -1500, .yaw = -512, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisHelicopterLandingPadVariant1Placements[2] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_NO9_GOLEM, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisHelicopterLandingPadVariant2Placements[4] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_NO9_GOLEM, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCENE_RUPERT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_HELICOPTER, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisMistR18Placements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_SCENE_RUPERT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_BALDWIN, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisMistParkingPlacements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_PIERCE, .variant = 0, .mode = 0, .x = 5030, .y = 0, .z = -5680, .yaw = -296, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_JODIE, .variant = 0, .mode = 0, .x = 8460, .y = 0, .z = 92, .yaw = 2047, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapAkropolisMistShootingGalleryPlacements[3] = {
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_GALLERY_MOUNT, .variant = 0, .mode = MAP_AKROPOLIS_PLACEMENT_GALLERY_SPAWN_REJECTED, .x = -7600, .y = 0, .z = 1260, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_AKROPOLIS_PLACEMENT_ENTRY_PIERCE, .variant = 0, .mode = 0, .x = -10230, .y = 0, .z = 5000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 18, .rowIndex = 0 },
    { AREA_PLACEMENT_END },
};

/// Five object-state placements for the Square: ammunition, recovery items and a save point.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
/// Stored state bit 8 (0x100) has no established reader; its role is unproven.
static AreaObjectPlace _gMapAkropolisSquareObjectPlaces[6] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_AMMUNITION, MAP_AKROPOLIS_KIND_9MM_PARABELLUM, 0, MAP_AKROPOLIS_STATE_REPLENISHABLE },
    { MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_FIRST_RECOVERY_2, MAP_AKROPOLIS_KIND_RECOVERY_2, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_SECOND_RECOVERY_2, MAP_AKROPOLIS_KIND_RECOVERY_2, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_RECOVERY_1, MAP_AKROPOLIS_KIND_RECOVERY_1, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { MAP_AKROPOLIS_OBJECT_SLOT_SQUARE_SAVE_POINT, MAP_AKROPOLIS_KIND_SQUARE_SAVE_POINT, 0, 0x100 | MAP_AKROPOLIS_STATE_AVAILABLE, 0x18D9, -0xE74, 0x95C },
    { AREA_OBJECT_PLACE_END },
};

/// Parthenon-key state placement for the East elevator hall.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
/// The prompt flag collects the key immediately when its pickup UI opens.
static AreaObjectPlace _gMapAkropolisEastElevatorHallObjectPlaces[2] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_EAST_ELEVATOR_HALL_PARTHENON_KEY, INVENTORY_COLLECTION_ID_PARTHENON_KEY, 0, AREA_OBJECT_PLACE_PROMPT | MAP_AKROPOLIS_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Four object-state placements for the Cafeteria: Micro Device, Mendel journal, Stim and Recovery1.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
static AreaObjectPlace _gMapAkropolisCafeteriaObjectPlaces[5] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_CAFETERIA_MICRO_DEVICE, MAP_AKROPOLIS_KIND_MICRO_DEVICE, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { MAP_AKROPOLIS_OBJECT_SLOT_CAFETERIA_MENDEL_JOURNAL, INVENTORY_COLLECTION_ID_MENDEL_JOURNAL, 0, MAP_AKROPOLIS_STATE_AVAILABLE, -0x96A, -0x3F2, -0x8FC },
    { MAP_AKROPOLIS_OBJECT_SLOT_CAFETERIA_STIM, MAP_AKROPOLIS_KIND_STIM, 0, MAP_AKROPOLIS_STATE_AVAILABLE, 0x654, -0x672, 0xAC8 },
    { MAP_AKROPOLIS_OBJECT_SLOT_CAFETERIA_RECOVERY_1, MAP_AKROPOLIS_KIND_RECOVERY_1, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Empty object-state placement list for the Security room.
///
/// Contains only `AREA_OBJECT_PLACE_END`, so seeds no state and spawns nothing.
static AreaObjectPlace _gMapAkropolisSecurityRoomObjectPlaces[1] = {
    { AREA_OBJECT_PLACE_END },
};

/// Blue-key state and model placement for the Hallway.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
static AreaObjectPlace _gMapAkropolisHallwayObjectPlaces[2] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_HALLWAY_BLUE_KEY, INVENTORY_COLLECTION_ID_BLUE_KEY, 0, MAP_AKROPOLIS_STATE_AVAILABLE, 0x410, -0x960, -0x4D8 },
    { AREA_OBJECT_PLACE_END },
};

/// Protein Capsule, Grenade Pistol and MP Boost2 state placements for the Fountain.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
static AreaObjectPlace _gMapAkropolisFountainObjectPlaces[4] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_FOUNTAIN_PROTEIN_CAPSULE, MAP_AKROPOLIS_KIND_PROTEIN_CAPSULE, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { MAP_AKROPOLIS_OBJECT_SLOT_FOUNTAIN_GRENADE_PISTOL, MAP_AKROPOLIS_KIND_GRENADE_PISTOL, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { MAP_AKROPOLIS_OBJECT_SLOT_FOUNTAIN_MP_BOOST_2, MAP_AKROPOLIS_KIND_MP_BOOST_2, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Penicillin state placement for the Forked road.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
static AreaObjectPlace _gMapAkropolisForkedRoadObjectPlaces[2] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_FORKED_ROAD_PENICILLIN, MAP_AKROPOLIS_KIND_PENICILLIN, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Tactical Vest state placement for the Observatory.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
static AreaObjectPlace _gMapAkropolisObservatoryObjectPlaces[2] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_OBSERVATORY_TACTICAL_VEST, MAP_AKROPOLIS_KIND_TACTICAL_VEST, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Grenade ammunition and MP5A5 state placements for the Promenade.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
static AreaObjectPlace _gMapAkropolisPromenadeObjectPlaces[3] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_PROMENADE_GRENADES, MAP_AKROPOLIS_KIND_GRENADE, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { MAP_AKROPOLIS_OBJECT_SLOT_PROMENADE_MP5A5, MAP_AKROPOLIS_KIND_MP5A5, 0, MAP_AKROPOLIS_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Red-key state and model placement for the Sanctuary.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds two-bit saved states and supplies
/// object identity. Model spawning additionally requires a matching kind in
/// the area's spawn list. XYZ use room-coordinate units; yaw uses 4096 per turn.
static AreaObjectPlace _gMapAkropolisSanctuaryObjectPlaces[2] = {
    { MAP_AKROPOLIS_OBJECT_SLOT_SANCTUARY_RED_KEY, INVENTORY_COLLECTION_ID_RED_KEY, 0, MAP_AKROPOLIS_STATE_AVAILABLE, -0x19C8, -0xA, -0x1F86, 0xFCE0 },
    { AREA_OBJECT_PLACE_END },
};

/// Armory Cardkey saved state and model placement for the Roof garden.
///
/// Flag slot 0x13 begins available and kind selects the key and its paired
/// pickup-model recipe. XYZ use world units; yaw uses 4096 units per turn.
/// The extent includes `AREA_OBJECT_PLACE_END`. Keep the map overlay loaded for
/// state initialization/publication and the room overlay loaded for spawning.
static AreaObjectPlace _gMapAkropolisRoofGardenObjectPlaces[2] = {
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_ROOF_GARDEN_ARMORY_CARDKEY, .kind = INVENTORY_COLLECTION_ID_ARMORY_CARDKEY, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = -4695, .y = 0, .z = -2299, .yaw = 0xFCE0 },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// Fire escape save-point state and retained transform.
///
/// Slot 0x2F begins available; kind publishes save-point key 0x802. State bit 8
/// is retained but unread. The empty spawn list creates no model from this
/// record. XYZ use world units; yaw is zero. The extent includes
/// `AREA_OBJECT_PLACE_END` and stays borrowed while the map overlay is loaded.
static AreaObjectPlace _gMapAkropolisFireEscapeObjectPlaces[2] = {
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_FIRE_ESCAPE_SAVE_POINT, .kind = MAP_AKROPOLIS_KIND_FIRE_ESCAPE_SAVE_POINT, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE | MAP_AKROPOLIS_OBJECT_STATE_UNREAD_BIT_8, .x = -3168, .y = -3168, .z = 2528, .yaw = 0 },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// Empty placed-object list for the West elevator hall.
///
/// Contains only `AREA_OBJECT_PLACE_END`: seeds no saved state, publishes no
/// object metadata and selects no spawn. Storage belongs to this map overlay.
static AreaObjectPlace _gMapAkropolisWestElevatorHallObjectPlaces[1] = {
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// MIST parking save point, item container, recoveries and shooting-gallery prizes.
///
/// All nine slots begin available. Save-point key 0x803 retains unread state
/// bit 8 and its world-unit transform; the remaining transforms are zero.
/// Prize slots 0x20..0x24 identify Hydra ammunition, Ringer's Solution, Hunter
/// Goggles, Lipstick and Shoulder Holster for courses 0..4, independently of
/// their global earned/waiting flags. Two separate slots publish Recovery3.
/// The empty spawn list creates no models. The extent includes
/// `AREA_OBJECT_PLACE_END`; keep this map overlay loaded for state or metadata use.
static AreaObjectPlace _gMapAkropolisMistParkingObjectPlaces[10] = {
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_SAVE_POINT, .kind = MAP_AKROPOLIS_KIND_MIST_PARKING_SAVE_POINT, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE | MAP_AKROPOLIS_OBJECT_STATE_UNREAD_BIT_8, .x = -3168, .y = -3168, .z = 2528, .yaw = 0 },
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_ITEM_BOX, .kind = MAP_AKROPOLIS_KIND_MIST_PARKING_ITEM_BOX, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = 0, .y = 0, .z = 0, .yaw = 0 },
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_RECOVERY_3_1, .kind = MAP_AKROPOLIS_KIND_RECOVERY_3, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = 0, .y = 0, .z = 0, .yaw = 0 },
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_RECOVERY_3_2, .kind = MAP_AKROPOLIS_KIND_RECOVERY_3, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = 0, .y = 0, .z = 0, .yaw = 0 },
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_0, .kind = MAP_AKROPOLIS_KIND_9MM_HYDRA, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = 0, .y = 0, .z = 0, .yaw = 0 },
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_1, .kind = MAP_AKROPOLIS_KIND_RINGERS_SOLUTION, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = 0, .y = 0, .z = 0, .yaw = 0 },
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_2, .kind = MAP_AKROPOLIS_KIND_HUNTER_GOGGLES, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = 0, .y = 0, .z = 0, .yaw = 0 },
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_3, .kind = MAP_AKROPOLIS_KIND_LIPSTICK, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = 0, .y = 0, .z = 0, .yaw = 0 },
    { .flagIndex = MAP_AKROPOLIS_OBJECT_SLOT_MIST_PARKING_PRIZE_4, .kind = MAP_AKROPOLIS_KIND_SHOULDER_HOLSTER, .placeKeyHigh = 0, .state = MAP_AKROPOLIS_STATE_AVAILABLE, .x = 0, .y = 0, .z = 0, .yaw = 0 },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// Additional item catalogue IDs selected by the Acropolis battle reward lists.
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

InventoryBattleReward gMapAkropolisBattleRewards[12] = {
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 2, 0), .items = { MAP_AKROPOLIS_REWARD_ITEM_PEPPER_SPRAY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 4, 0), .items = { MAP_AKROPOLIS_KIND_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_AKROPOLIS_REWARD_ITEM_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_CAFETERIA, 1, 0), .items = { MAP_AKROPOLIS_KIND_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_AKROPOLIS_REWARD_ITEM_MD_PLAYER } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 2, 0), .items = { MAP_AKROPOLIS_KIND_GRENADE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_AKROPOLIS_REWARD_ITEM_AIRBURST } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FORKED_ROAD, 3, 0), .items = { MAP_AKROPOLIS_KIND_PENICILLIN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FORKED_ROAD, 4, 0), .items = { MAP_AKROPOLIS_KIND_RECOVERY_1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_OBSERVATORY, 2, 0), .items = { MAP_AKROPOLIS_KIND_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_AKROPOLIS_REWARD_ITEM_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_OBSERVATORY, 3, 0), .items = { MAP_AKROPOLIS_KIND_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_AKROPOLIS_REWARD_ITEM_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 1, 0), .items = { MAP_AKROPOLIS_KIND_RECOVERY_1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 1, 0), .items = { MAP_AKROPOLIS_REWARD_ITEM_MP_BOOST_1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_AKROPOLIS_KIND_RECOVERY_1 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, 1, 0), .items = { MAP_AKROPOLIS_KIND_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_AKROPOLIS_REWARD_ITEM_MP_BOOST_1 } },
    { .areaLayoutKey = INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward gMapAkropolisHardBattleRewards[6] = {
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_CAFETERIA, 1, 0), .items = { MAP_AKROPOLIS_REWARD_ITEM_PEPPER_SPRAY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FOUNTAIN, 7, 0), .items = { MAP_AKROPOLIS_REWARD_ITEM_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FORKED_ROAD, 7, 0), .items = { MAP_AKROPOLIS_REWARD_ITEM_COMBAT_LIGHT, MAP_AKROPOLIS_REWARD_ITEM_9MM_SPARTAN, MAP_AKROPOLIS_REWARD_ITEM_FLARE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_OBSERVATORY, 7, 0), .items = { MAP_AKROPOLIS_REWARD_ITEM_MP_BOOST_1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 7, 0), .items = { MAP_AKROPOLIS_REWARD_ITEM_P08, MAP_AKROPOLIS_REWARD_ITEM_9MM_SPARTAN, MAP_AKROPOLIS_KIND_PROTEIN_CAPSULE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry gMapAkropolisAreaMusicEntries[(GAME_AREA_MIST_SHOOTING_GALLERY + 1) * MAP_AKROPOLIS_MUSIC_COLUMN_COUNT] = {
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x21, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x1D, STAGE_MUSIC_START_DEFERRED },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 5, 0 },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 6, 1 },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x1F, STAGE_MUSIC_START_DEFERRED },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x23, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_DEFERRED },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x24, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, 3 },
    { 0x26, 3 },
    { 0x26, 3 },
    { 0x26, 3 },
    { 0x26, STAGE_MUSIC_START_DEFERRED },
    { 0x26, 3 },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x26, STAGE_MUSIC_START_DEFERRED },
    { 0xC, STAGE_MUSIC_START_DEFERRED },
    { 0x26, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x20, STAGE_MUSIC_START_DEFERRED },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x22, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x25, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x11, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x11, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
};

StageMusicEntry gMapAkropolisCountdownMusicEntries[20] = {
    { 0x1E, STAGE_MUSIC_START_IMMEDIATE },
    { 0xB, STAGE_MUSIC_START_IMMEDIATE },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { STAGE_MUSIC_NO_SEQUENCE, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0x45, STAGE_MUSIC_START_IMMEDIATE },
    { 0x46, STAGE_MUSIC_START_IMMEDIATE },
    { 0, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0, STAGE_MUSIC_START_WHEN_VIEW_READY },
    { 0, STAGE_MUSIC_START_WHEN_VIEW_READY },
};

/// NUL-terminated title displayed above the briefing's key-item list.
///
/// The final three bytes have no established consumer; their role is unproven.
static const char _gMapAkropolisKeyItemPanelTitle[12] = "Key Item\0L#\6";
