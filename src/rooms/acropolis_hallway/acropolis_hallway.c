#include "rooms/acropolis_hallway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"
#include "../../shared/actor/actor_contacts.h"

extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

static void _acropolisHallwayInitializeRoomTask(Task* task);
static void _acropolisHallwayIdleRoomTask(Task* unusedTask);

/// Room message carrying the selected inventory item ID in its first argument.
enum { ACROPOLIS_HALLWAY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Room-task states 0..2: register the message receiver, idle, release the task.
///
/// Copied by value before unchecked dispatch. The task state must stay in 0..2;
/// the callback code must remain loaded through dispatch.
static const TaskFuncTable3 _gAcropolisHallwayRoomTaskStates = {
    { _acropolisHallwayInitializeRoomTask, _acropolisHallwayIdleRoomTask, taskKill },
};

static u32     _gAcropolisHallwayModel023C0PartVerts[1];
static SVECTOR _gAcropolisHallwayModel023C0Verts[21];
static SVECTOR _gAcropolisHallwayModel023C0Normals[6];
static TmdBone _gAcropolisHallwayModel023C0Skeleton[1];
static u32     _gAcropolisHallwayModel023C0Stream[42];

static WorldCollisionGrid    _gAcropolisHallwayCollisionGrid;
static WorldCollisionTrigger _gAcropolisHallwayViewBoundaryTriggers[4];
static WorldCollisionTrigger _gAcropolisHallwayActionTriggers[9];
static WorldCoordRoomLights  _gAcropolisHallwayRoomLights;
static s32                   _acropolisHallwayResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32                   _acropolisHallwayRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32                   _acropolisHallwayHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg);

/// Synchronous room messages for transition resolution, key-item refusal and sound cues.
///
/// Transition resolution borrows a RoomEventMsg request and writable reply;
/// key-item use takes an integer item ID, and sound takes an integer cue.
/// The fourth record ends lookup and makes unsupported IDs return zero. The
/// room task borrows this table and its callbacks while the overlay is loaded.
static TaskMessageEntry _gAcropolisHallwayRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisHallwayResolveRoomTransition },
    { ACROPOLIS_HALLWAY_MESSAGE_USE_KEY_ITEM, _acropolisHallwayRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisHallwayHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gAcropolisHallwayRoomCollisionResources[1] = {
    {
        .grid                 = &_gAcropolisHallwayCollisionGrid,
        .viewBoundaryTriggers = _gAcropolisHallwayViewBoundaryTriggers,
        .actionTriggers       = _gAcropolisHallwayActionTriggers,
        .occluders            = NULL,
    },
};

u8* gAcropolisHallwayViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gAcropolisHallwayViewCounts[1] = { ARRAY_SIZE(gAcropolisHallwayViewCameras) };

WorldCoordRoomLighting gAcropolisHallwayRoomLightingTable[1] = {
    { .lights = &_gAcropolisHallwayRoomLights, .ambientTable = NULL },
};

/// One-based hallway endpoints selected by departure triggers and arrival warps.
enum {
    ACROPOLIS_HALLWAY_WARP_FOUNTAIN        = 1,
    ACROPOLIS_HALLWAY_WARP_SECURITY_ROOM   = 2,
    ACROPOLIS_HALLWAY_WARP_CAFETERIA_VIEW3 = 3,
    ACROPOLIS_HALLWAY_WARP_CAFETERIA_VIEW2 = 4,
    ACROPOLIS_HALLWAY_WARP_ENDPOINT_SHIFT  = 4,
};

DirectionWarpEntry gAcropolisHallwayWarpEntries[4] = {
    [ACROPOLIS_HALLWAY_WARP_FOUNTAIN - 1]        = { { { .word = 3072 }, 3012, -937, -698 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 3012, -937, -698 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 2), SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 1), DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 497 },
    [ACROPOLIS_HALLWAY_WARP_SECURITY_ROOM - 1]   = { { { .word = 2048 }, 2271, -937, 970 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 2271, -937, 970 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 2), SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 1), DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 498 },
    [ACROPOLIS_HALLWAY_WARP_CAFETERIA_VIEW3 - 1] = { { { .word = 2048 }, -748, -937, 970 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -748, -937, 970 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 2), SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 1), DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, 499 },
    [ACROPOLIS_HALLWAY_WARP_CAFETERIA_VIEW2 - 1] = { { { .word = 1024 }, -2963, -937, -623 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -2963, -937, -623 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 4), SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 3), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 500 },
};

/// Room-space plane normals for the hallway grid, with 4096 per unit length.
static SVECTOR _gAcropolisHallwayCollisionGridNormals[10] = {
#include "assets/acropolis_hallway_collision_01010_normals.inc"
};

/// Hallway grid vertices in signed room-coordinate game units.
static SVECTOR _gAcropolisHallwayCollisionGridVerts[35] = {
#include "assets/acropolis_hallway_collision_01010_verts.inc"
};

/// Hallway grid faces, using surface classes 0 (silent), 1 (pass-through) and 4 (blocking).
static WorldCollisionGridFace _gAcropolisHallwayCollisionGridFaces[17] = {
#include "assets/acropolis_hallway_collision_01010_faces.inc"
};

/// Packed signed face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
///
/// The two cells start at elements 0 and 18. Element 17 is retained storage
/// outside the first list; no active cell references it.
static s16 _gAcropolisHallwayCollisionGridCells[30] = {
#include "assets/acropolis_hallway_collision_01010_cells.inc"
};

/// Resolves a generated face-list start in this grid's packed halfword pool.
///
/// halfwordOffset counts signed-halfword elements and is evaluated once.
/// It must select an in-bounds list start in the captured pool.
/// Captures `_gAcropolisHallwayCollisionGridCells`; returns a borrowed s16* without reading it.
/// The table include supplies list-start constants; empty cells use NULL.
/// Define only for that include and undefine immediately afterward.
#define ACROPOLIS_HALLWAY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gAcropolisHallwayCollisionGridCells[(halfwordOffset)])
/// X-major table of the hallway grid's two cells, each borrowing its face list.
static s16* _gAcropolisHallwayCollisionGridTable[2] = {
#include "assets/acropolis_hallway_collision_01010_table.inc"
};
#undef ACROPOLIS_HALLWAY_CELL_FACE_LIST_AT_OFFSET

/// The hallway's room collision mesh and two-cell XZ index.
///
/// Covers a 2-by-1 grid from (-3250, -1250), with 4000-unit square cells.
/// Room activation binds the view transform; the mesh and cell lists remain
/// borrowed until the active grid is replaced and the overlay can unload.
static WorldCollisionGrid _gAcropolisHallwayCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisHallwayCollisionGridNormals,
    .vertices    = _gAcropolisHallwayCollisionGridVerts,
    .faces       = _gAcropolisHallwayCollisionGridFaces,
    .cellFaceIds = _gAcropolisHallwayCollisionGridTable,
    .xBias       = 3250,
    .zBias       = 1250,
    .cellCountX  = ARRAY_SIZE(_gAcropolisHallwayCollisionGridTable),
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisHallwayCollisionGridFaces),
};

/// Directed view boundaries 2->3, 3->4, 4->3 and 3->2, in scan order.
///
/// Positions and radii use room game units; normals use 4096 per unit. Room
/// setup binds, links and enables all four mutable records through LAST.
/// Keep the overlay loaded until these runtime list nodes are unlinked.
static WorldCollisionTrigger _gAcropolisHallwayViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { -1984, -2016, 0, 0 }, { { 0, -1792, -1760, 0 }, { 0, 1792, -1760, 0 }, { 0, -1792, 1760, 0 }, { 0, 1792, 1760, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 0, 0 }, 2508, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 320, -2016, 0, 0 }, { { -174, -1792, -1752, 0 }, { -174, 1792, -1752, 0 }, { 172, -1792, 1750, 0 }, { 172, 1792, 1750, 0 } }, { -4086, 0, 402, 0 }, { 0, 0, 0, 0 }, 2508, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 192, -1920, 0, 0 }, { { -174, 1792, -1752, 0 }, { -174, -1792, -1752, 0 }, { 172, 1792, 1750, 0 }, { 172, -1792, 1750, 0 } }, { 4084, 0, -404, 0 }, { 0, 0, 0, 0 }, 2508, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2112, -1952, 0, 0 }, { { 0, 1792, -1760, 0 }, { 0, -1792, -1760, 0 }, { 0, 1792, 1760, 0 }, { 0, -1792, 1760, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 0, 0 }, 2508, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Four hallway exit quads followed by CAP interaction commands 2..6.
///
/// Exit parameter1 packs the hallway departure slot above the destination
/// arrival slot. The CAP parameters select commands without presentation flags.
/// Positions/radii use room game units and normals use 4096 per unit. Setup
/// binds and enables all nine records through LAST; retain them until unlinked.
static WorldCollisionTrigger _gAcropolisHallwayActionTriggers[9] = {
    { NULL, NULL, NULL, { -2960, -1024, -384, 0 }, { { -272, 0, -736, 0 }, { 272, 0, -736, 0 }, { -272, 0, 736, 0 }, { 272, 0, 736, 0 } }, { 0, 4102, 0, 0 }, { ONE, 0, 0, 0 }, 783, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_AKROPOLIS_CAFETERIA, (ACROPOLIS_HALLWAY_WARP_CAFETERIA_VIEW2 << ACROPOLIS_HALLWAY_WARP_ENDPOINT_SHIFT) | 3, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -768, -1016, 920, 0 }, { { -496, 0, -352, 0 }, { 496, 0, -352, 0 }, { -496, 0, 352, 0 }, { 496, 0, 352, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -ONE, 0 }, 607, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_AKROPOLIS_CAFETERIA, (ACROPOLIS_HALLWAY_WARP_CAFETERIA_VIEW3 << ACROPOLIS_HALLWAY_WARP_ENDPOINT_SHIFT) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2304, -1024, 928, 0 }, { { -496, 0, -352, 0 }, { 496, 0, -352, 0 }, { -496, 0, 352, 0 }, { 496, 0, 352, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -ONE, 0 }, 607, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_AKROPOLIS_SECURITY_ROOM, (ACROPOLIS_HALLWAY_WARP_SECURITY_ROOM << ACROPOLIS_HALLWAY_WARP_ENDPOINT_SHIFT) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2944, -1024, -672, 0 }, { { 288, 0, -496, 0 }, { 288, 0, 496, 0 }, { -288, 0, -496, 0 }, { -288, 0, 496, 0 } }, { 0, 4108, 0, 0 }, { -ONE, 0, 0, 0 }, 572, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_AKROPOLIS_FOUNTAIN, (ACROPOLIS_HALLWAY_WARP_FOUNTAIN << ACROPOLIS_HALLWAY_WARP_ENDPOINT_SHIFT) | 3, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 800, -1088, -928, 0 }, { { -560, 0, -288, 0 }, { 560, 0, -288, 0 }, { -560, 0, 288, 0 }, { 560, 0, 288, 0 } }, { 0, 4101, 0, 0 }, { -201, 0, 4091, 0 }, 627, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3184, -1056, 368, 0 }, { { -1056, 0, -912, 0 }, { 128, 0, -912, 0 }, { -1056, 0, 208, 0 }, { 128, 0, 208, 0 } }, { 0, 4113, 0, 0 }, { -4076, 0, 401, 0 }, 1390, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -1056, 848, 0 }, { { -528, 0, -240, 0 }, { 528, 0, -240, 0 }, { -528, 0, 432, 0 }, { 528, 0, 432, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -784, -1056, -864, 0 }, { { -832, 0, -368, 0 }, { 832, 0, -368, 0 }, { -832, 0, 240, 0 }, { 832, 0, 240, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, 4091, 0 }, 909, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2128, -1042, -1024, 0 }, { { -400, 0, -304, 0 }, { 400, 0, -304, 0 }, { -400, 0, 304, 0 }, { 400, 0, 304, 0 } }, { 0, 4100, 0, 0 }, { 200, 0, 4090, 0 }, 501, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Rat resource, serving eight placements with entry ID 7.
///
/// Loads actor file 300700 and borrows its descriptor at index 0. The final
/// AREA_PLACEMENT_END record ends resource lookup; retain both overlays during use.
static AreaResource _gAcropolisHallwayVariant1Resources[2] = {
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor300700RatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's Odd Stranger resource, serving its single entry-ID-18 placement.
///
/// Loads actor file 401800 and uses descriptor index 0; AREA_PLACEMENT_END
/// terminates the list. The room and actor overlays stay loaded during use.
static AreaResource _gAcropolisHallwayVariant2Resources[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401800OddStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Odd Stranger resource, serving its single entry-ID-18 placement.
///
/// Loads actor file 401800 with descriptor index 0 and ends at AREA_PLACEMENT_END.
/// This variant owns a separate list; retain the room and actor overlays during use.
static AreaResource _gAcropolisHallwayVariant3Resources[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401800OddStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 4's Odd Stranger and Rat resources, serving one and four placements.
///
/// Entry IDs 18 and 7 select actor files 401800 and 300700 at descriptor index 0.
/// The third record ends lookup; retain the room and actor overlays during use.
static AreaResource _gAcropolisHallwayVariant4Resources[3] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401800OddStrangerTaskDesc },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor300700RatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gAcropolisHallwayAreaVariants[13] = {
    { NULL, NULL },
    { gMapAkropolisHallwayVariant1Placements, _gAcropolisHallwayVariant1Resources },
    { gMapAkropolisHallwayVariant2Placements, _gAcropolisHallwayVariant2Resources },
    { gMapAkropolisHallwayVariant3Placements, _gAcropolisHallwayVariant3Resources },
    { gMapAkropolisHallwayVariant4Placements, _gAcropolisHallwayVariant4Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// The hallway's three white point lights, contributing in every room view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits, initially `ONE` in each channel. Each light is at full
/// strength within 1360 units and fades to zero at 2384 units. The loaded room
/// overlay owns these writable records: coordinate updates parent and compose
/// their transforms, and lighting queries overwrite attenuation. Borrowed
/// pointers must not survive unloading the overlay.
static WorldCoordPointLight _gAcropolisHallwayPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2374, -2432, 0 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1360,
        .outer = 2384,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2384, 0 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1360,
        .outer = 2384,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2297, -2384, 0 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1360,
        .outer = 2384,
    },
};

/// The hallway's three all-view point lights, with no directional or cone lights.
///
/// Borrows the mutable room-owned point array for coordinate composition and
/// light queries. The collection and array remain live while the overlay is loaded.
static WorldCoordRoomLights _gAcropolisHallwayRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gAcropolisHallwayPointLights),
    .pointLights           = _gAcropolisHallwayPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// View 1's decoded-background selection, followed by the batch-list end marker.
///
/// Keep both records: cached linking skips the zero-count first record before
/// testing the next record's end marker. No source sprite is selected.
static SpriteBatch _gAcropolisHallwayView1SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 2's decoded-background selection and end marker, selecting no sprites.
///
/// Cached linking skips the zero-count first record before testing the second.
static SpriteBatch _gAcropolisHallwayView2SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 3's decoded-background selection and end marker, selecting no sprites.
///
/// Cached linking skips the zero-count first record before testing the second.
static SpriteBatch _gAcropolisHallwayView3SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 4's decoded-background selection and end marker, selecting no sprites.
///
/// Cached linking skips the zero-count first record before testing the second.
static SpriteBatch _gAcropolisHallwayView4SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 5's decoded-background selection and end marker, selecting no sprites.
///
/// Cached linking skips the zero-count first record before testing the second.
static SpriteBatch _gAcropolisHallwayView5SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gAcropolisHallwaySpriteViews[5] = {
    { { .empty = _gAcropolisHallwayView1SpriteBatches }, _gAcropolisHallwayView1SpriteBatches, NULL },
    { { .empty = _gAcropolisHallwayView2SpriteBatches }, _gAcropolisHallwayView2SpriteBatches, NULL },
    { { .empty = _gAcropolisHallwayView3SpriteBatches }, _gAcropolisHallwayView3SpriteBatches, NULL },
    { { .empty = _gAcropolisHallwayView4SpriteBatches }, _gAcropolisHallwayView4SpriteBatches, NULL },
    { { .empty = _gAcropolisHallwayView5SpriteBatches }, _gAcropolisHallwayView5SpriteBatches, NULL },
};

ViewCamera gAcropolisHallwayViewCameras[5] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x6590, 0 } }, 911 },
    { { { { -908, 0, 3994 }, { -529, 4059, -120 }, { -3958, -542, -900 } }, { 240, 1610, -240 } }, 207 },
    { { { { -481, 0, 4067 }, { 891, 3996, 105 }, { -3968, 897, -469 } }, { -2520, 2700, -240 } }, 207 },
    { { { { -624, 0, -4048 }, { -1848, 3644, 285 }, { 3601, 1870, -555 } }, { 1430, 3260, -240 } }, 207 },
    { { { { -ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, -ONE } }, { -880, 2360, 640 } }, 207 },
};

/// Type-1 bank footstep bases: entries 9 for walking/jumps and 11 for running.
///
/// Cue 2 uses the base, cue 1 adds one, and companions add 100. Surface classes
/// 1 and 4 borrow these read-only values while the room overlay is loaded.
static WorldCollisionFootstepSounds _gAcropolisHallwayFootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 9,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 11,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 9,
};

/// Silent blocking response shared by hallway classes 0, 2, 3, 5, 6 and 7.
///
/// Blocks probes, permits weapon impacts and applies pushback. The leading
/// stored byte remains zero with an unproven role; no footstep table is supplied.
static WorldCollisionSurfaceProperties _gAcropolisHallwaySilentBlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's footstep-bearing response, passing probes and suppressing weapon impacts.
///
/// Still applies grid pushback. The leading stored byte's role is unproven;
/// the room-owned footstep table must remain loaded while borrowed.
static WorldCollisionSurfaceProperties _gAcropolisHallwayWalkablePassThroughSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisHallwayFootstepSounds,
};

/// Class 4's footstep-bearing response, blocking probes and permitting weapon impacts.
///
/// Applies grid pushback. The leading stored byte's role is unproven;
/// the room-owned footstep table must remain loaded while borrowed.
static WorldCollisionSurfaceProperties _gAcropolisHallwayWalkableBlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisHallwayFootstepSounds,
};

WorldCollisionSurfaceProperties* gAcropolisHallwaySurfacePropertiesByClass[8] = {
    &_gAcropolisHallwaySilentBlockingSurface,
    &_gAcropolisHallwayWalkablePassThroughSurface,
    &_gAcropolisHallwaySilentBlockingSurface,
    &_gAcropolisHallwaySilentBlockingSurface,
    &_gAcropolisHallwayWalkableBlockingSurface,
    &_gAcropolisHallwaySilentBlockingSurface,
    &_gAcropolisHallwaySilentBlockingSurface,
    &_gAcropolisHallwaySilentBlockingSurface,
};

static TmdBone _gAcropolisHallwayModel01AE0Skeleton[1] = {
#include "assets/acropolis_hallway_model_01AE0_skeleton.inc"
};

static u32 _gAcropolisHallwayModel01AE0PartVerts[1] = {
#include "assets/acropolis_hallway_model_01AE0_partVerts.inc"
};

static SVECTOR _gAcropolisHallwayModel01AE0Verts[99] = {
#include "assets/acropolis_hallway_model_01AE0_verts.inc"
};

static u32 _gAcropolisHallwayModel01AE0Stream[495] = {
#include "assets/acropolis_hallway_model_01AE0_stream.inc"
};

TmdSource gAcropolisHallwayModel01AE0 = {
    0,
    3728,
    0,
    1,
    _gAcropolisHallwayModel01AE0PartVerts,
    _gAcropolisHallwayModel01AE0Verts,
    &_gAcropolisHallwayModel01AE0Verts[99],
    _gAcropolisHallwayModel01AE0Skeleton,
    _gAcropolisHallwayModel01AE0Stream,
};

static TmdBone _gAcropolisHallwayModel023C0Skeleton[1] = {
#include "assets/acropolis_hallway_model_023C0_skeleton.inc"
};

static u32 _gAcropolisHallwayModel023C0PartVerts[1] = {
#include "assets/acropolis_hallway_model_023C0_partVerts.inc"
};

static SVECTOR _gAcropolisHallwayModel023C0Verts[21] = {
#include "assets/acropolis_hallway_model_023C0_verts.inc"
};

static SVECTOR _gAcropolisHallwayModel023C0Normals[6] = {
#include "assets/acropolis_hallway_model_023C0_normals.inc"
};

static u32 _gAcropolisHallwayModel023C0Stream[42] = {
#include "assets/acropolis_hallway_model_023C0_stream.inc"
};

static TmdSource _gAcropolisHallwayModel023C0 = {
    0,
    312,
    0,
    1,
    _gAcropolisHallwayModel023C0PartVerts,
    _gAcropolisHallwayModel023C0Verts,
    _gAcropolisHallwayModel023C0Normals,
    _gAcropolisHallwayModel023C0Skeleton,
    _gAcropolisHallwayModel023C0Stream,
};

SVECTOR ActorContact_ScratchPosition = { 0 };

static void _acropolisHallwayUnusedObjectVisibilityTask(Task* task);

/// Resolves hallway exits and blocks the early cafeteria warp-3 departure.
///
/// Copies complete borrowed request/reply records, which may alias. Returns
/// 0 for the blocked exit, starting CAP command 1 only on execution; other
/// exits return 1. Executed fountain exits select room 2 after the second
/// security lock, and cafeteria exits select rooms 3/4 from story progress.
/// Queries only copy the request. The receiver and message ID are unused.
static s32 _acropolisHallwayResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_HALLWAY_SECOND_LOCK_BIT          = 2,
        ACROPOLIS_HALLWAY_CAFETERIA_GATED_WARP     = 3,
        ACROPOLIS_HALLWAY_CAFETERIA_EXIT_COMMAND   = 1,
        ACROPOLIS_HALLWAY_CAFETERIA_EXIT_PROGRESS  = 3,
        ACROPOLIS_HALLWAY_CAFETERIA_SCENE_PROGRESS = 2,
    };

    u16 destinationArea;

    *reply = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_FOUNTAIN) {
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & ACROPOLIS_HALLWAY_SECOND_LOCK_BIT) && request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->roomState = 2;
        }
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_CAFETERIA && request->warp == ACROPOLIS_HALLWAY_CAFETERIA_GATED_WARP && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < ACROPOLIS_HALLWAY_CAFETERIA_EXIT_PROGRESS) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(ACROPOLIS_HALLWAY_CAFETERIA_EXIT_COMMAND);
        }
        return 0;
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_FOUNTAIN && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_HALLWAY_CAFETERIA_EXIT_PROGRESS) {
        return 1;
    }
    destinationArea = request->areaId;
    if (destinationArea == GAME_AREA_AKROPOLIS_CAFETERIA && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= ACROPOLIS_HALLWAY_CAFETERIA_EXIT_PROGRESS) {
            reply->roomState = destinationArea;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_HALLWAY_CAFETERIA_SCENE_PROGRESS) {
            reply->roomState = 3;
        }
    }
    return 1;
}

/// Refuses every key-item use request in the hallway.
///
/// All arguments are ignored. Returning 0 selects the inventory's "No use now"
/// notice without consuming the selected item or starting a room event.
static s32 _acropolisHallwayRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    enum { ACROPOLIS_HALLWAY_KEY_ITEM_USE_REFUSED = 0 };

    return ACROPOLIS_HALLWAY_KEY_ITEM_USE_REFUSED;
}

/// Queues hallway sound-bank entry 6 or 7 for the corresponding room sound cue.
///
/// Other cues do nothing. Uses each script's base mix; queue failures are
/// discarded. Receiver, ID and second payload are unused; always returns zero.
static s32 _acropolisHallwayHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum {
        ACROPOLIS_HALLWAY_SOUND_CUE_ENTRY_6 = 6,
        ACROPOLIS_HALLWAY_SOUND_CUE_ENTRY_7 = 7
    };

    switch (soundCue) {
        case ACROPOLIS_HALLWAY_SOUND_CUE_ENTRY_6:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 6), 0, 0);
            break;
        case ACROPOLIS_HALLWAY_SOUND_CUE_ENTRY_7:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 7), 0, 0);
            break;
    }
    return 0;
}

/// Registers the hallway's room-message receiver and enters its idle state.
///
/// Requires a live task in state 0 and loaded room resources. Borrows the message
/// table, publishes the task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _acropolisHallwayInitializeRoomTask(Task* task)
{
    task->msgTable = _gAcropolisHallwayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Leaves the hallway room task untouched in its idle state (state 1).
static void _acropolisHallwayIdleRoomTask(Task* unusedTask)
{
}

void acropolisHallwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisHallwayRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void acropolisHallwayEffectControlTask84(Task* unusedTask)
{
}

#include "../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../shared/actor/actor_contacts_push.inc.c"

void acropolisHallwayBlueKeyPickupTask(Task* task)
{
    enum {
        ACROPOLIS_HALLWAY_BLUE_KEY_VIEW      = 5,
        ACROPOLIS_HALLWAY_BLUE_KEY_COLLECTED = 2,
    };

    Enemy*     enemy;
    TmdObject* model;
    s32        placementState;

    enemy          = task->spawnArg2.pointer;
    model          = task->extra.tmd;
    placementState = areaGetCurrentObjectState((u8)enemy->placeKey);
    if (task->state == 0) {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
        task->state++;
    }
    if (viewGetMappedIndex() == ACROPOLIS_HALLWAY_BLUE_KEY_VIEW) {
        model->flags = TMD_OBJECT_FLAGGED_PASS;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (placementState == ACROPOLIS_HALLWAY_BLUE_KEY_COLLECTED) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Retains an unreferenced placed-model visibility callback in the room image.
///
/// No placement, call or relocation selects it. With an Enemy-compatible
/// placement argument it would disable automatic buffers for object state 2,
/// otherwise select the flagged draw pass and allocate a primitive buffer.
/// The discarded view query is retained. Its intended object is unproven.
static void _acropolisHallwayUnusedObjectVisibilityTask(Task* task)
{
    enum {
        ACROPOLIS_HALLWAY_OBJECT_COLLECTED = 2,
    };

    Enemy*     enemy;
    TmdObject* model;
    s32        placementState;

    enemy          = task->spawnArg2.pointer;
    model          = task->extra.tmd;
    placementState = areaGetCurrentObjectState((u8)enemy->placeKey);
    viewGetMappedIndex();
    if (placementState == ACROPOLIS_HALLWAY_OBJECT_COLLECTED) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
        tmdAllocPrimitiveBuffer(model);
    }
}
