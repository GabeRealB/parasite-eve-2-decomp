#include "rooms/dryfield_night_dilapidated_house.h"

#include "common.h"

#include "dryfield_night_dilapidated_house_private.h"

#include "actors/actor_01600.h"
#include "actors/actor_02500.h"
#include "actors/actor_04000.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room/room_events.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The request the event gate latched for the event task.
extern RoomEventReq gRoomEventReq;

WorldCoordRoomLights gDryfieldNightDilapidatedHouseRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(gDryfieldNightDilapidatedHousePointLights),
    .pointLights           = gDryfieldNightDilapidatedHousePointLights,
    .coneLightCount        = ARRAY_SIZE(gDryfieldNightDilapidatedHouseConeLights),
    .coneLights            = gDryfieldNightDilapidatedHouseConeLights,
};

/// IDs stored in action records; warp pairs pack departure then arrival in four-bit nibbles.
enum {
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_WARP_TO_BACK_STREET = (1 << 4) | 4,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_WARP_TO_WAREHOUSE   = (2 << 4) | 2,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_2       = 2,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_3       = 3,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_5       = 5,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_18      = 18,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_17      = 17,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_23      = 23,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_21      = 21,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_22      = 22,
};

WorldCollisionTrigger gDryfieldNightDilapidatedHouseActionTriggers[12] = {
    { NULL, NULL, NULL, { 1712, -48, -2768, 0 }, { { -624, 0, -304, 0 }, { 624, 0, -304, 0 }, { -624, 0, 304, 0 }, { 624, 0, 304, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 692, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_WARP_TO_BACK_STREET, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5680, -63, 192, 0 }, { { 304, 0, -624, 0 }, { 304, 0, 624, 0 }, { -304, 0, -624, 0 }, { -304, 0, 624, 0 } }, { 0, 4106, 0, 0 }, { ONE, 0, 0, 0 }, 692, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_WARP_TO_WAREHOUSE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4688, -64, 352, 0 }, { { -512, 0, -608, 0 }, { 512, 0, -608, 0 }, { -512, 0, 608, 0 }, { 512, 0, 608, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, 201, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4704, -64, -944, 0 }, { { -512, 0, -560, 0 }, { 512, 0, -560, 0 }, { -512, 0, 560, 0 }, { 512, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { -4091, 0, 201, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, -496, 0 }, { { -336, 0, -928, 0 }, { 1456, 0, -928, 0 }, { -336, 0, 928, 0 }, { 1136, 0, 928, 0 } }, { 0, 4107, 0, 0 }, { -4091, 0, 201, 0 }, 1722, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1104, -64, -2800, 0 }, { { -1264, 0, -288, 0 }, { 912, 0, -288, 0 }, { -1264, 0, 1024, 0 }, { 912, 0, 1024, 0 } }, { 0, 4105, 0, 0 }, { -4091, 0, 201, 0 }, 1624, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5280, -64, 704, 0 }, { { -624, 0, -224, 0 }, { 624, 0, -224, 0 }, { -624, 0, 224, 0 }, { 624, 0, 224, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 662, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5120, -64, -2720, 0 }, { { -720, 0, -288, 0 }, { 720, 0, -288, 0 }, { -720, 0, 288, 0 }, { 720, 0, 288, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, ONE, 0 }, 773, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_23, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2448, -64, 2688, 0 }, { { -544, 0, -288, 0 }, { 544, 0, -288, 0 }, { -544, 0, 288, 0 }, { 544, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -ONE, 0 }, 613, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1072, -64, 2688, 0 }, { { -576, 0, -288, 0 }, { 576, 0, -288, 0 }, { -576, 0, 288, 0 }, { 576, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -ONE, 0 }, 643, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2400, -64, -2752, 0 }, { { -720, 0, -288, 0 }, { 720, 0, -288, 0 }, { -720, 0, 288, 0 }, { 720, 0, 288, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, ONE, 0 }, 773, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -928, -64, 2688, 0 }, { { -544, 0, -288, 0 }, { 544, 0, -288, 0 }, { -544, 0, 288, 0 }, { 544, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -ONE, 0 }, 613, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_TRIGGER_CAP_COMMAND_21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder gDryfieldNightDilapidatedHouseSightOccluder = { NULL, NULL, { -4096, -2000, 1232, 0 }, { { 0, 2576, -2544, 0 }, { 0, -2576, -2544, 0 }, { 0, 2576, 2544, 0 }, { 0, -2576, 2544, 0 } }, { 4097, 0, 0, 0 }, 3620, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 };

/// Placement-entry keys used by this room's resource recipes.
enum {
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SCORPION_RESOURCE_ENTRY      = 25,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SCAVENGER_RESOURCE_ENTRY     = 16,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BLOOD_SUCKLER_RESOURCE_ENTRY = 40,
};

/// Scorpion and Blood Suckler recipes for night dilapidated-house variant 3.
///
/// Five Scorpions use entry 25, base-group-10 file 25; two Blood Sucklers
/// use entry 40, base-group-20 file 40. Both spawn descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightDilapidatedHouseVariant3Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SCORPION_RESOURCE_ENTRY, .fileNumber = 25, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102500Tasks },
    { .entryId = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BLOOD_SUCKLER_RESOURCE_ENTRY, .fileNumber = 40, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor204000BloodSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Scavenger and Blood Suckler recipes for night dilapidated-house variant 4.
///
/// Four Scavengers use entry 16, base-group-10 file 16; two Blood Sucklers
/// use entry 40, base-group-20 file 40. Both spawn descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightDilapidatedHouseVariant4Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SCAVENGER_RESOURCE_ENTRY, .fileNumber = 16, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_BLOOD_SUCKLER_RESOURCE_ENTRY, .fileNumber = 40, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor204000BloodSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Scavenger recipe for night dilapidated-house variant 21.
///
/// Five placements use entry 16, base-group-10 file 16 and descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightDilapidatedHouseVariant21Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SCAVENGER_RESOURCE_ENTRY, .fileNumber = 16, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightDilapidatedHouseAreaVariants[22] = {
    [3]  = { .placements = gMapDryfieldFullDilapidatedHouseVariant3Placements, .resources = _gDryfieldNightDilapidatedHouseVariant3Resources },
    [4]  = { .placements = gMapDryfieldFullDilapidatedHouseVariant4Placements, .resources = _gDryfieldNightDilapidatedHouseVariant4Resources },
    [21] = { .placements = gMapDryfieldFullDilapidatedHouseVariant21Placements, .resources = _gDryfieldNightDilapidatedHouseVariant21Resources },
};

WorldCoordRoomAmbientEntry gDryfieldNightDilapidatedHouseAmbientByView[12] = {
    { .viewCount = ARRAY_SIZE(gDryfieldNightDilapidatedHouseAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 250, 250, 500, 281 } },
    { .color = { 500, 500, 750, 531 } },
    { .color = { 250, 250, 500, 281 } },
    { .color = { 250, 250, 500, 281 } },
    { .color = { 250, 250, 500, 281 } },
};

/// Cue-2 sound requests in the loaded type-1 bank; cue 1 selects the next entry.
enum {
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x45,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x47
};

/// Footstep cue bases for surface class 1.
///
/// Walking and scripted jumps share one pair; running uses the other.
/// Cue 1 adds one to each base, and companion playback adds 100.
/// Requests borrow the loaded type-1 bank while the room is active.
static WorldCollisionFootstepSounds _gDryfieldNightDilapidatedHouseSurface1FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_1_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_1_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_1_WALK_CUE_BASE,
};

/// Cue-2 sound requests in the loaded type-1 bank; cue 1 selects the next entry.
enum {
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4D,
    DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4F
};

/// Footstep cue bases for surface class 2.
///
/// Walking and scripted jumps share one pair; running uses the other.
/// Cue 1 adds one to each base, and companion playback adds 100.
/// Requests borrow the loaded type-1 bank while the room is active.
static WorldCollisionFootstepSounds _gDryfieldNightDilapidatedHouseSurface2FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_2_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SURFACE_2_WALK_CUE_BASE,
};

/// Collision policy for surface classes 0 and 3..7: probes block, impacts are enabled and pushback applies.
///
/// No footstep cues; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gDryfieldNightDilapidatedHouseDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for surface class 1: probes block, impacts are enabled and pushback applies.
///
/// Borrowed footstep cue bases; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gDryfieldNightDilapidatedHouseSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightDilapidatedHouseSurface1FootstepSounds,
};

/// Collision policy for surface class 2: probes block, impacts are enabled and pushback applies.
///
/// Borrowed footstep cue bases; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gDryfieldNightDilapidatedHouseSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightDilapidatedHouseSurface2FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightDilapidatedHouseSurfacePropertiesByClass[8] = {
    &_gDryfieldNightDilapidatedHouseDefaultSurfaceProperties,
    &_gDryfieldNightDilapidatedHouseSurface1Properties,
    &_gDryfieldNightDilapidatedHouseSurface2Properties,
    &_gDryfieldNightDilapidatedHouseDefaultSurfaceProperties,
    &_gDryfieldNightDilapidatedHouseDefaultSurfaceProperties,
    &_gDryfieldNightDilapidatedHouseDefaultSurfaceProperties,
    &_gDryfieldNightDilapidatedHouseDefaultSurfaceProperties,
    &_gDryfieldNightDilapidatedHouseDefaultSurfaceProperties,
};

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 192, 47, 192 } };

RoomEventReq gRoomEventReq;

static void _dryfieldNightDilapidatedHouseSetupRoomTask(Task* task);
static void _dryfieldNightDilapidatedHouseIdleRoomTask(Task* task);

#include "../../shared/room/room_event.inc.c"

s32 dryfieldNightDilapidatedHouseRefuseKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArgument)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 dryfieldNightDilapidatedHouseGateBackStreetExit(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_EXIT_CAP_COMMAND  = 12,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_EXIT_SOUND  = 0x53090005,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SECOND_EXIT_SOUND = 0x53090001,
    };

    RoomEventReq eventRequest;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_NIGHT_BACK_STREET) {
        eventRequest.capCmd        = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_EXIT_CAP_COMMAND;
        eventRequest.missingCapCmd = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_EXIT_CAP_COMMAND;
        eventRequest.firstSnd      = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_FIRST_EXIT_SOUND;
        eventRequest.secondSnd     = DRYFIELD_NIGHT_DILAPIDATED_HOUSE_SECOND_EXIT_SOUND;
        eventRequest.flagId        = GAME_FLAG_DILAPIDATED_HOUSE_DOOR_UNLOCKED;
        eventRequest.collectedBit  = ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED;
        return _roomEventGate(&eventRequest, request);
    }
    return ROOM_EVENT_GATE_BYPASSED;
}

s32 dryfieldNightDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

s32 dryfieldNightDilapidatedHouseIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArgument)
{
    return 0;
}

/// Commits first-visit progress and starts the scene when a companion is present.
///
/// An unseen visit sets chapter 3 and the gas-station objective even without a
/// companion. The normal and skip scripts remain borrowed from this room while
/// the scene runs; subsequent calls do nothing once the visit flag is set.
static inline void _dryfieldNightDilapidatedHouseStartFirstVisitEvent(void)
{
    enum {
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_EVENT_UNSEEN          = 0,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_EVENT_SEEN            = 1,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_STORY_CHAPTER         = 3,
        DRYFIELD_NIGHT_DILAPIDATED_HOUSE_GAS_STATION_OBJECTIVE = 0x11
    };

    if (gameFlagGetNibble(GAME_FLAG_NIGHT_DILAPIDATED_HOUSE_EVENT_SEEN) == DRYFIELD_NIGHT_DILAPIDATED_HOUSE_EVENT_UNSEEN) {
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
            evsStartScriptWithSkip(gDryfieldNightDilapidatedHouseFirstVisitScript, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                   gDryfieldNightDilapidatedHouseFirstVisitSkipScript);
        }
        // Progress advances even when the companion is absent and no scene starts.
        gameFlagSetNibble(GAME_FLAG_NIGHT_DILAPIDATED_HOUSE_EVENT_SEEN, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_EVENT_SEEN);
        gameFlagSetNibble(EVENT_FLAG_STORY_CHAPTER, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_STORY_CHAPTER);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_GAS_STATION_OBJECTIVE);
    }
}

/// Registers the room's message receiver and enters idle before first-visit scene setup.
static void _dryfieldNightDilapidatedHouseSetupRoomTask(Task* task)
{
    task->msgTable = gDryfieldNightDilapidatedHouseRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
    _dryfieldNightDilapidatedHouseStartFirstVisitEvent();
}

/// Keeps the registered room task alive without performing per-frame work.
static void _dryfieldNightDilapidatedHouseIdleRoomTask(Task* task)
{
    // Retain the idle state's unused 16-byte stack frame.
    char unusedStackFrame[16];
}

/// Room-task lifecycle: state 0 initializes, state 1 waits or updates, state 2 releases.
///
/// The dispatcher copies all three callback pointers before indexing by `Task::state`.
/// The selector must be in 0..2; there is no terminator or bounds check.
/// Keep this overlay loaded until the room task is released.
static const TaskFuncTable3 _gDryfieldNightDilapidatedHouseRoomTaskStates = {
    {
        _dryfieldNightDilapidatedHouseSetupRoomTask,
        _dryfieldNightDilapidatedHouseIdleRoomTask,
        taskKill,
    },
};

void dryfieldNightDilapidatedHouseRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gDryfieldNightDilapidatedHouseRoomTaskStates;
    handlers.funcs[task->state](task);
}

void dryfieldNightDilapidatedHouseStageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

void dryfieldNightDilapidatedHouseEnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

void dryfieldNightDilapidatedHouseFinishScene(void)
{
    streamFinishScene();
}

void dryfieldNightDilapidatedHouseCancelScene(void)
{
    cdCmdCancelScene();
}

void dryfieldNightDilapidatedHouseStartMovie(void)
{
    enum { DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_LAUNCHER = 0 };

    taskSpawnFromTable(gDryfieldNightDilapidatedHouseMovieTaskDescriptors, DRYFIELD_NIGHT_DILAPIDATED_HOUSE_MOVIE_LAUNCHER, 0, 0);
}
