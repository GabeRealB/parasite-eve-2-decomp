#include "stages/akropolis/stage.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/object_task.h"

#include "main/areas.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/fs_types.h"
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

/* The Akropolis stage overlay (stage 1: Akropolis and the MIST), resident while any of its rooms is loaded: the
 * per-stage tables gameplay and main index by stage (decoded data, rooms/akropolis/stage.toml, included below), the
 * placed objects' spawn recipes and the rooms' tasks, the movie buffer layout per area and the MIST briefing's
 * key-item menu.
 */

static void _akropolisStageDrawKeyItemRow(UiList* list, UiObject* object);
static void _akropolisStageKeyItemPanelTask(Task* task);
static void _akropolisStageKeyItemMenuTask(Task* task);

/// The briefing menu's rows and their height in pixels.
enum {
    BRIEFING_ITEM_COUNT = 4,
    KEY_ITEM_ROW_HEIGHT = 15,
};

static const char   _gAkropolisStageKeyItemPanelTitle[12];
static s32          _gAkropolisStageDryfieldMapOpened;
static s32          _gAkropolisStageBriefingItemIds[BRIEFING_ITEM_COUNT];
static UiList       _gAkropolisStageKeyItemList;
static UiObjectDesc _gAkropolisStageKeyItemPanelDescriptor;
static TaskDesc     _gAkropolisStageKeyItemMenuTaskDescriptor;

/// Movie ring byte extent and decoded-column staging origins in VRAM words/rows.
enum {
    MOVIE_RING_BYTES         = 0x10000,
    PLAZA_MOVIE_STAGING_X    = 704,
    FOUNTAIN_MOVIE_STAGING_X = 384,
    FOUNTAIN_MOVIE_STAGING_Y = 256,
};

/// Lifecycle steps and teardown delay of the key-item display-mode task.
enum {
    KEY_ITEM_MENU_START       = 0,
    KEY_ITEM_MENU_ACTIVE      = 1,
    KEY_ITEM_MENU_CLOSING     = 2,
    KEY_ITEM_MENU_CLOSE_TICKS = 10,
};

/// Places the VLC-expanded input buffers and two RGB16 decoded frames.
///
/// `decodedFrameBytes` is width in pixels * height in rows * two bytes, positive
/// and word-aligned. The movie workspace reserves that many bytes for input
/// slot 1, followed by two frames; input slot 0 starts after the actor region's
/// 64 KiB ring. Both regions must fit their VLC output and stay available until
/// playback and pending DMA finish. Only pointers are assigned; no allocation,
/// clearing or capacity check occurs.
static inline void _akropolisStagePlaceMovieDecodeBuffers(size_t decodedFrameBytes)
{
    u8* firstDecodedFrame;

    gMdecMovieExpandedBuffers[0] = (u_long*)((u8*)gStreamMovieRingBuffer + MOVIE_RING_BYTES);
    gMdecMovieExpandedBuffers[1] = gStreamMovieWorkspace;
    firstDecodedFrame            = (u8*)gStreamMovieWorkspace + decodedFrameBytes;
    gMdecMovieOutputBuffers[0]   = (u_long*)firstDecodedFrame;
    gMdecMovieOutputBuffers[1]   = (u_long*)(firstDecodedFrame + decodedFrameBytes);
}

void akropolisStageSetupMovieBuffers(const GameLocationKey* location)
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
            queue->movieStagingX           = PLAZA_MOVIE_STAGING_X;
            gStreamMovieReleasePauseOnStop = false;
            queue->movieStagingY           = 0;
            gMdecMovieExpandedBuffers[0]   = (u_long*)((u8*)gStreamMovieRingBuffer + MOVIE_RING_BYTES);
            gMdecMovieExpandedBuffers[1]   = gStreamMovieWorkspace;
            gMdecMovieOutputBuffers[1]     = (u_long*)((u8*)gStreamMovieWorkspace + gStreamMovieWidthPixels * gStreamMovieHeightRows);
            gMdecMovieOutputBuffers[0]     = gMdecMovieOutputBuffers[1];
            break;
        case GAME_AREA_AKROPOLIS_FOUNTAIN:
            // This one-iteration loop preserves the original instruction schedule.
            do {
                stagingEnabled = 1;
            } while (0);
            queue->movieStagingX           = FOUNTAIN_MOVIE_STAGING_X;
            queue->movieStagingY           = FOUNTAIN_MOVIE_STAGING_Y;
            gStreamMovieBackgroundPlayback = stagingEnabled;
            queue->movieVramStaging        = stagingEnabled;
            fountainFrameBytes             = gStreamMovieWidthPixels * gStreamMovieHeightRows * 2;
            gMdecMovieExpandedBuffers[0]   = (u_long*)((u8*)gStreamMovieRingBuffer + MOVIE_RING_BYTES);
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
            _akropolisStagePlaceMovieDecodeBuffers(frameBytes);
            break;
        case GAME_AREA_AKROPOLIS_PROMENADE:
            promenadePixelCount          = gStreamMovieWidthPixels * gStreamMovieHeightRows;
            gMdecMovieExpandedBuffers[0] = (u_long*)((u8*)gStreamMovieRingBuffer + MOVIE_RING_BYTES);
            gMdecMovieExpandedBuffers[1] = gStreamMovieWorkspace;
            gMdecMovieOutputBuffers[0]   = (u_long*)((u8*)gStreamMovieWorkspace + promenadePixelCount * 3 / 2);
            gMdecMovieOutputBuffers[1]   = (u_long*)((u8*)gMdecMovieOutputBuffers[0] + promenadePixelCount * 2);
            break;
        case GAME_AREA_AKROPOLIS_BRIDGE:
            bridgePixelCount             = gStreamMovieWidthPixels * gStreamMovieHeightRows;
            gMdecMovieExpandedBuffers[0] = (u_long*)((u8*)gStreamMovieRingBuffer + MOVIE_RING_BYTES);
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
static void _akropolisStageDrawKeyItemRow(UiList* list, UiObject* object)
{
    enum { DETAIL_DESCRIPTOR_INDEX = 45 };
    s32 itemId;
    s32 inputEnabled;

    itemId = _gAkropolisStageBriefingItemIds[list->currentItemIndex];
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
            uiSpawnObject(&gItemMenuPanelDescriptors[DETAIL_DESCRIPTOR_INDEX], itemId, USER_INTERFACE_PANEL_ACTIVE, 1, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            if (itemId == INVENTORY_COLLECTION_ID_DRYFIELD_MAP) {
                _gAkropolisStageDryfieldMapOpened = inputEnabled;
            }
        }
    }
}

/// Updates the briefing's key-item panel and its item-detail child.
///
/// The owning UI task supplies its live UiObject in spawnArg2. First dispatch
/// fits the singleton list to the panel; confirm closes the detail child and
/// resumes list input, while cancel propagates to the menu host.
static void _akropolisStageKeyItemPanelTask(Task* task)
{
    enum { INITIALIZE = 0 };
    UiObject* object;
    UiList*   list;
    UiObject* detailObject;
    s32       detailResult;

    list           = &_gAkropolisStageKeyItemList;
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, _gAkropolisStageKeyItemPanelTitle);
    if (task->state == INITIALIZE) {
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
static void _akropolisStageKeyItemMenuTask(Task* task)
{
    UiObject* object;
    s16       result;

    if (task->state == KEY_ITEM_MENU_START) {
        stageEnsureHeapTaskPrimitiveBuffer();
        itemMenuClearPreviewItems();
        object = uiSpawnObject(&_gAkropolisStageKeyItemPanelDescriptor, task->spawnArg1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
        if (object == NULL) {
            return;
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen    = 1;
        task->spawnArg2.pointer = object;
        task->state            += 1;
    }

    if (task->state == KEY_ITEM_MENU_ACTIVE) {
        object = task->spawnArg2.pointer;
        result = object->result;
        if ((result == USER_INTERFACE_RESULT_CANCEL) || (result == USER_INTERFACE_RESULT_CONFIRM)) {
            uiStartTreeClosing(object, object->owner);
            task->killCountdown = KEY_ITEM_MENU_CLOSE_TICKS;
            task->state         = KEY_ITEM_MENU_CLOSING;
        }
    }

    // Keep the primitive storage until the UI closing animation has run.
    if (task->state == KEY_ITEM_MENU_CLOSING) {
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

s32 akropolisStageOpenKeyItemMenu(s32 unused, s32 retainSelection)
{
    s32* itemIdCursor;
    s32  itemIndex;

    if (retainSelection == 0) {
        itemIndex    = 0;
        itemIdCursor = _gAkropolisStageBriefingItemIds;
        do {
            inventorySetCollectedBit(*itemIdCursor++);
            itemIndex++;
        } while (itemIndex < ARRAY_SIZE(_gAkropolisStageBriefingItemIds));
        _gAkropolisStageDryfieldMapOpened = 0;
    }
    displayQueueModeTask(&_gAkropolisStageKeyItemMenuTaskDescriptor, 0, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

s32 akropolisStageWasDryfieldMapSelected(void)
{
    return _gAkropolisStageDryfieldMapOpened;
}

/// Object kinds the spawn tables pair recipes with: an item's catalog id, or a room prop's key.
enum {
    KIND_STIM               = 0x4,
    KIND_LANDING_PAD_PICKUP = 0xA4, // an empty catalog slot; no place selects it
    KIND_LANDING_PAD_LIFT   = 0x204,
    KIND_SECURITY_ROOM_LID  = 0x701,
    KIND_SANCTUARY_LID      = 0x702,
};

#include "assets/rooms/akropolis/memory.inc"
#include "assets/rooms/akropolis/map_1.inc"

/// The square's object spawns: none; its places only seed object states.
static AreaObjectSpawn _gAkropolisStageSquareObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// The east elevator hall's object spawns: none; its places only seed object states.
static AreaObjectSpawn _gAkropolisStageEastElevatorHallObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// The cafeteria's object spawns: the Stim and Mendel's journal.
static AreaObjectSpawn _gAkropolisStageCafeteriaObjectSpawns[3] = {
    { KIND_STIM, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, akropolisCafeteriaStimPickupTask, { &gAkropolisCafeteriaStimPickupModel } } },
    { INVENTORY_COLLECTION_ID_MENDEL_JOURNAL, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, akropolisCafeteriaMendelPickupTask, { &gAkropolisCafeteriaMendelJournalModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// The security room's object spawns: the item box lid, which no place selects.
static AreaObjectSpawn _gAkropolisStageSecurityRoomObjectSpawns[2] = {
    { KIND_SECURITY_ROOM_LID, { { { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, itemPickupContainerLidTask, { &gAkropolisSecurityRoomItemBoxModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// The hallway's object spawns: the blue key.
static AreaObjectSpawn _gAkropolisStageHallwayObjectSpawns[2] = {
    { INVENTORY_COLLECTION_ID_BLUE_KEY, { { { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, akropolisHallwayBlueKeyPickupTask, { &gAkropolisHallwayBlueKeyPickupModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// The fountain's object spawns: none; its places only seed object states.
static AreaObjectSpawn _gAkropolisStageFountainObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// The forked road's object spawns: none; its places only seed object states.
static AreaObjectSpawn _gAkropolisStageForkedRoadObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// The observatory's object spawns: none; its places only seed object states.
static AreaObjectSpawn _gAkropolisStageObservatoryObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// The promenade's object spawns: none; its places only seed object states.
static AreaObjectSpawn _gAkropolisStagePromenadeObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// The sanctuary's object spawns: the red key, and the item box lid, which no place selects.
static AreaObjectSpawn _gAkropolisStageSanctuaryObjectSpawns[3] = {
    { INVENTORY_COLLECTION_ID_RED_KEY, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, akropolisSanctuaryItemVisibilityTask, { &gAkropolisSanctuaryRedKeyPickupModel } } },
    { KIND_SANCTUARY_LID, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, itemPickupContainerLidTask, { &gAkropolisSanctuaryItemBoxModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// The roof garden's object spawns: the armory cardkey.
static AreaObjectSpawn _gAkropolisStageRoofGardenObjectSpawns[2] = {
    { INVENTORY_COLLECTION_ID_ARMORY_CARDKEY, { { { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, akropolisRoofGardenPickupModelTask, { &gAkropolisRoofGardenArmoryCardkeyPickupModel } } },
    { AREA_OBJECT_SPAWN_END },
};

/// The fire escape's object spawns: none; its places only seed object states.
static AreaObjectSpawn _gAkropolisStageFireEscapeObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// The west elevator hall's object spawns: none, and no places.
static AreaObjectSpawn _gAkropolisStageWestElevatorHallObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// The MIST parking lot's object spawns: none; its places only seed object states.
static AreaObjectSpawn _gAkropolisStageMistParkingObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

#include "assets/rooms/akropolis/object_places_1.inc"

/// The landing pad's object spawns: the lift, and the pickup, which no place selects.
static AreaObjectSpawn _gAkropolisStageHelicopterLandingPadObjectSpawns[3] = {
    { KIND_LANDING_PAD_LIFT, { { { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, akropolisHelicopterLandingPadLiftTask, { &gAkropolisHelicopterLandingPadLiftModel } } },
    { KIND_LANDING_PAD_PICKUP, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_REDRAW } }, akropolisHelicopterLandingPadPickupModelTask, { &gAkropolisHelicopterLandingPadAmmunitionPickupModel } } },
    { AREA_OBJECT_SPAWN_END },
};

#include "assets/rooms/akropolis/objects.inc"

TaskDesc gAkropolisStageRoomTaskDescriptors[21] = {
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
static s32 _gAkropolisStageDryfieldMapOpened = 0;

/// Collection catalog IDs shown and granted by the MIST briefing menu.
///
/// Row indices 0..3 select signed-word IDs; names come from the item catalog.
static s32 _gAkropolisStageBriefingItemIds[BRIEFING_ITEM_COUNT] = {
    INVENTORY_COLLECTION_ID_MIST_SEARCH_WARRANT,
    INVENTORY_COLLECTION_ID_NMC_PHOTO,
    INVENTORY_COLLECTION_ID_MANUAL,
    INVENTORY_COLLECTION_ID_DRYFIELD_MAP,
};

/// Shared row renderer for all four briefing items.
///
/// The list's shared-callback flag selects entry zero for every item index.
static UiListRowCallback _gAkropolisStageKeyItemRowCallbacks[1] = { _akropolisStageDrawKeyItemRow };

/// Mutable singleton list for the four MIST briefing items.
///
/// Shows all four rows with wrapping navigation and 15-pixel row spacing.
/// Panel initialization resets selection/scroll state and enables the shared
/// row renderer; closing and reopening can retain the Dryfield-map latch.
static UiList _gAkropolisStageKeyItemList = {
    .rowCallbacks    = _gAkropolisStageKeyItemRowCallbacks,
    .itemCount       = ARRAY_SIZE(_gAkropolisStageBriefingItemIds),
    .visibleRowCount = { .unsignedValue = ARRAY_SIZE(_gAkropolisStageBriefingItemIds) },
    .wrapNavigation  = 1,
    .rowHeight       = KEY_ITEM_ROW_HEIGHT,
};

/// Panel recipe for the briefing's key-item list and its detail children.
///
/// Bounds are screen-centered pixels; the bodyless content task uses priority
/// 192 and ordering-table tag base 60. Spawning reads this recipe synchronously.
static UiObjectDesc _gAkropolisStageKeyItemPanelDescriptor = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -100, -40, 200, BRIEFING_ITEM_COUNT* KEY_ITEM_ROW_HEIGHT },
    .otIndexSeed     = 60,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = TASK_PRIORITY_CONTROL,
    .contentCallback = _akropolisStageKeyItemPanelTask,
};

/// Bodyless display-mode host for the briefing's key-item UI tree.
///
/// Runs at priority `TASK_PRIORITY_CONTROL`; spawning copies this recipe, which the stage overlay owns.
static TaskDesc _gAkropolisStageKeyItemMenuTaskDescriptor = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _akropolisStageKeyItemMenuTask,
};
#include "assets/rooms/akropolis/map_2.inc"
#include "assets/rooms/akropolis/tables.inc"
#include "assets/rooms/akropolis/placements.inc"
#include "assets/rooms/akropolis/object_places_2.inc"
#include "assets/rooms/akropolis/rewards.inc"
#include "assets/rooms/akropolis/music.inc"

/// NUL-terminated title displayed above the briefing's key-item list.
///
/// The final three bytes have no established consumer; their role is unproven.
static const char _gAkropolisStageKeyItemPanelTitle[12] = "Key Item\0L#\6";
