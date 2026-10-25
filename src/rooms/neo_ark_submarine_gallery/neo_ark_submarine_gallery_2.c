#include "rooms/neo_ark_submarine_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_submarine_gallery_private.h"

#include "actors/actor_00400.h"
#include "actors/actor_206100.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/sound_types.h"
#include "main/sound_ids.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/effects/water_effects.h"
#include "../../shared/effects/glow_draw.h"

enum { NEO_ARK_SUBMARINE_GALLERY_PRISM_FIRST_VERTEX = 32 };

static void _neoArkSubmarineGalleryDrawLightPrism(const GfxCoord* coord, s16 firstVertex);
static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

TaskDesc gNeoArkSubmarineGalleryWaterTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, waterRefractionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, waterDistortBandTask, { .value = 0 } },
};

TaskMessageEntry gNeoArkSubmarineGalleryRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, neoArkSubmarineGalleryResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, neoArkSubmarineGalleryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, neoArkSubmarineGalleryIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, neoArkSubmarineGalleryHandleCapCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gNeoArkSubmarineGalleryIslandDepartureTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, neoArkSubmarineGalleryDepartToIslandTask, { .value = 0 } };

s16 gNeoArkSubmarineGallerySeaDiverAwaitingArrival = NEO_ARK_SUBMARINE_GALLERY_SEA_DIVER_ARRIVAL_NOT_PENDING;

TaskDesc gNeoArkSubmarineGalleryRedDiscTaskDesc = { { { TASK_BODY_COORD, TASK_PRIORITY_ACTOR } }, neoArkSubmarineGalleryRedDiscTask, { .value = 0 } };

/// Fixed capsule endpoints, disc centres and prism corners for the gallery lights.
///
/// All positions use signed game-coordinate units. Capsule endpoints are stored
/// as eight pairs, although mapped view 5 also joins points 5 and 6. Disc
/// centres use world space; prism corners are local to the glow task's coordinate.
/// Storage is borrowed only during drawing and retains all authored positions.
static struct {
    SVECTOR worldCapsulePoints[16]; // World-space endpoints, normally paired consecutively
    SVECTOR worldDiscCenters[16];   // World-space centres of the disc glows
    SVECTOR localPrismCorners[8];   // Four lit corners followed by four dark corners, in task-local space
} _gNeoArkSubmarineGalleryLightGeometry = {
    .worldCapsulePoints = {
        { -760, 2800, 3820, 0 },
        { -760, 3200, 3820, 0 },
        { 2170, 2800, 3250, 0 },
        { 2170, 3200, 3250, 0 },
        { 3830, 2800, 760, 0 },
        { 3830, 3200, 760, 0 },
        { 3250, 2800, -2170, 0 },
        { 3250, 3200, -2170, 0 },
        { 770, 2800, -3830, 0 },
        { 770, 3200, -3830, 0 },
        { -2170, 2800, -3250, 0 },
        { -2170, 3200, -3250, 0 },
        { -3830, 2800, -760, 0 },
        { -3830, 3200, -760, 0 },
        { -3250, 2800, 2170, 0 },
        { -3250, 3200, 2170, 0 },
    },
    .worldDiscCenters = {
        { -640, 4800, 3240, 0 },
        { -500, 4800, 2500, 0 },
        { 1840, 4800, 2740, 0 },
        { 1420, 4800, 2130, 0 },
        { 3240, 4800, 640, 0 },
        { 2500, 4800, 500, 0 },
        { 2750, 4800, -1830, 0 },
        { 2120, 4800, -1420, 0 },
        { 640, 4800, -3240, 0 },
        { 500, 4800, -2500, 0 },
        { -1830, 4800, -2750, 0 },
        { -1420, 4800, -2120, 0 },
        { -3240, 4800, -640, 0 },
        { -2500, 4800, -500, 0 },
        { -2740, 4800, 1830, 0 },
        { -2120, 4800, 1420, 0 },
    },
    .localPrismCorners = {
        { 820, 2110, 3780, 0 },
        { 1610, 2110, 3500, 0 },
        { 1340, 2110, 2710, 0 },
        { 540, 2110, 2990, 0 },
        { 820, 3400, 3780, 0 },
        { 1610, 3400, 3500, 0 },
        { 1340, 3400, 2710, 0 },
        { 540, 3400, 2990, 0 },
    },
};

u8* gNeoArkSubmarineGalleryViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkSubmarineGalleryViewCounts[1] = { NEO_ARK_SUBMARINE_GALLERY_VIEW_COUNT };

DirectionWarpEntry gNeoArkSubmarineGalleryWarpEntries[1] = {
    {
        .player         = { { .word = 2304 }, 1300, 5000, 3100 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 2304 }, 1300, 5000, 3100 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_NEO_ARK_SUB_GALLERY_TO_ISLAND,
        .departureSound = SOUND_NEO_ARK_SUB_GALLERY_TO_ISLAND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

s16 gNeoArkSubmarineGalleryDiverWaterY = 5000;

/// Bog Diver patrol ring for spawn-argument ring selectors 0 and 3.
///
/// Two laps through four XZ positions in room units, at depth 1500 below the
/// sampled water surface. Indices wrap 0..7; fourth halfwords are unused.
/// The room lends these points to the actors for their loaded lifetimes.
static SVECTOR _gNeoArkSubmarineGalleryDiverPatrolRing0[8] = {
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
};

/// Bog Diver patrol ring for spawn-argument ring selector 1.
///
/// Two laps through four XZ positions in room units, starting at negative Z,
/// with depth 1500 added to the sampled water surface. Wraps 0..7 without a
/// terminator; fourth halfwords are unused. Borrowed for the actors' lifetimes.
static SVECTOR _gNeoArkSubmarineGalleryDiverPatrolRing1[8] = {
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
};

/// Bog Diver patrol ring for spawn-argument ring selector 2.
///
/// Two laps through four XZ positions in room units, starting at positive Z,
/// with depth 1500 added to the sampled water surface. Wraps 0..7 without a
/// terminator; fourth halfwords are unused. Borrowed for the actors' lifetimes.
static SVECTOR _gNeoArkSubmarineGalleryDiverPatrolRing2[8] = {
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
};

SVECTOR* gNeoArkSubmarineGalleryDiverPatrolRings[4] = {
    _gNeoArkSubmarineGalleryDiverPatrolRing0,
    _gNeoArkSubmarineGalleryDiverPatrolRing1,
    _gNeoArkSubmarineGalleryDiverPatrolRing2,
    _gNeoArkSubmarineGalleryDiverPatrolRing0,
};

enum {
    NEO_ARK_SUBMARINE_GALLERY_SURFACE_SPOT_FREE = 0,
    NEO_ARK_SUBMARINE_GALLERY_SURFACE_SPOT_END  = -1,
};

SVECTOR gNeoArkSubmarineGalleryDiverSurfaceSpots[6] = {
    { 0, 0, 0, NEO_ARK_SUBMARINE_GALLERY_SURFACE_SPOT_FREE },
    { 1000, 1500, 0, NEO_ARK_SUBMARINE_GALLERY_SURFACE_SPOT_FREE },
    { -1000, 1500, 0, NEO_ARK_SUBMARINE_GALLERY_SURFACE_SPOT_FREE },
    { 0, 1500, 1000, NEO_ARK_SUBMARINE_GALLERY_SURFACE_SPOT_FREE },
    { 0, 1500, -1000, NEO_ARK_SUBMARINE_GALLERY_SURFACE_SPOT_FREE },
    { 0, 0, 0, NEO_ARK_SUBMARINE_GALLERY_SURFACE_SPOT_END },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gNeoArkSubmarineGalleryCollisionGridNormals[31] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_normals.inc"
};

/// Collision vertices in signed room-coordinate units.
static SVECTOR _gNeoArkSubmarineGalleryCollisionGridVerts[104] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_verts.inc"
};

/// Indexed gallery quads selecting local surface classes 0, 3 and 4.
static WorldCollisionGridFace _gNeoArkSubmarineGalleryCollisionGridFaces[50] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_faces.inc"
};

/// Authored cell face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
///
/// Halfwords between selected lists remain part of the asset storage.
static s16 _gNeoArkSubmarineGalleryCollisionGridCells[206] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_cells.inc"
};

enum {
    NEO_ARK_SUBMARINE_GALLERY_GRID_CELL_COUNT_X = 3,
    NEO_ARK_SUBMARINE_GALLERY_GRID_CELL_COUNT_Z = 3,
    NEO_ARK_SUBMARINE_GALLERY_GRID_CELL_SIZE    = 4000,
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gNeoArkSubmarineGalleryCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..205,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define NEO_ARK_SUBMARINE_GALLERY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkSubmarineGalleryCollisionGridCells[(halfwordOffset)])
/// X-major directory of the gallery's nine 4000-unit collision cells.
static s16* _gNeoArkSubmarineGalleryCollisionGridTable[NEO_ARK_SUBMARINE_GALLERY_GRID_CELL_COUNT_X * NEO_ARK_SUBMARINE_GALLERY_GRID_CELL_COUNT_Z] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_table.inc"
};
#undef NEO_ARK_SUBMARINE_GALLERY_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gNeoArkSubmarineGalleryCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkSubmarineGalleryCollisionGridNormals,
    .vertices    = _gNeoArkSubmarineGalleryCollisionGridVerts,
    .faces       = _gNeoArkSubmarineGalleryCollisionGridFaces,
    .cellFaceIds = _gNeoArkSubmarineGalleryCollisionGridTable,
    .xBias       = 5000,
    .zBias       = 5000,
    .cellCountX  = NEO_ARK_SUBMARINE_GALLERY_GRID_CELL_COUNT_X,
    .cellCountZ  = NEO_ARK_SUBMARINE_GALLERY_GRID_CELL_COUNT_Z,
    .cellSize    = NEO_ARK_SUBMARINE_GALLERY_GRID_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gNeoArkSubmarineGalleryCollisionGridFaces),
};

ViewCamera gNeoArkSubmarineGalleryViewCameras[NEO_ARK_SUBMARINE_GALLERY_VIEW_COUNT] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x4E20, 0 } }, 603 },
    { { { { 2429, 0, -3297 }, { 0, 4096, 0 }, { 3297, 0, 2429 } }, { 3800, -4000, -200 } }, 257 },
    { { { { 0, 0, -4095 }, { -3996, 899, 0 }, { 899, 3996, 0 } }, { 3340, 27, 0 } }, 257 },
    { { { { -2429, 0, -3297 }, { 0, 4096, 0 }, { 3297, 0, -2429 } }, { 3800, -4000, 200 } }, 257 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { 3000, -4140, 0 } }, 257 },
    { { { { -4096, 0, 0 }, { 0, 3973, -995 }, { 0, -995, -3973 } }, { -5, -4380, -3925 } }, 272 },
    { { { { -639, 0, 4045 }, { 1045, 3956, 165 }, { -3908, 1058, -617 } }, { -7620, -3215, -3705 } }, 272 },
};

/// Empty sprite list for mapped view 1, retaining decoded-image presentation.
///
/// The source slot also stores this list's address but is never read as sprites.
static SpriteBatch _gNeoArkSubmarineGalleryView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-2 sprite rectangles and depths: 110 opaque and 39 semitransparent elements.
static SpriteSource _gNeoArkSubmarineGalleryView2SpriteSources[149] = {
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 32, 40, 712, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 40, 40, 637, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, 40, 587, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 56, 56, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 64, 56, 500, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 72, 56, 375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 80, 64, 250, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 96, 64, 250, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 112, 64, 250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 250, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 72, 250, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1000, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 64, 24, 1100, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 24, 1000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 24, 1000, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 24, 1000, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 112, 24, 1000, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 128, 24, 1000, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 144, 24, 1000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -152, -120, 700, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -136, -120, 875, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -120, -120, 950, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -104, -120, 1000, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 128, -72, 1875, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, -72, 1875, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 96, -72, 1875, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 80, -72, 1875, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 64, -72, 1875, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 48, -72, 1875, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 1875, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 64, 16, 1875, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 16, 1875, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 16, 1875, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 112, 16, 1875, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 128, 16, 1875, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 144, 16, 1875, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 152, -48, 1875, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -160, 40, 625, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -152, 40, 750, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -144, 40, 875, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -136, 32, 1000, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -120, 32, 1062, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -104, 24, 1125, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, -24, 1300, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -40, -24, 1575, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -24, -24, 1687, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -8, -24, 1750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 8, -24, 1875, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 24, -24, 1875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 40, -24, 1875, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -88, -104, 1250, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -56, -88, 1300, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -128, -40, 937, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -104, 937, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -40, -88, 1575, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -24, -88, 1687, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1750, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -72, 1875, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 24, -72, 1875, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 40, -72, 1875, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -64, 112, 725, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, 8, 112, 625, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 0, 104, 725, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -64, 104, 750, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, 96, 830, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 0, 96, 787, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 88, 878, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -56, 88, 862, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -48, 80, 921, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 80, 1017, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 1012, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 72, 1012, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -40, 64, 1224, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -8, 64, 1224, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -32, 56, 1176, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 0, 56, 1352, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1500, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -64, -24, 1250, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -72, -24, 1312, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -64, -88, 1250, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -72, -104, 1312, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, 152, -72, 1875, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, 144, -72, 1875, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -24, 1250, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -88, 24, 1250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -88, -120, 1000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -120, 1000, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -64, -120, 1000, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -160, -120, 3750, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -144, -120, 3750, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, -128, -120, 3750, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 56 } }, -128, -96, 3750, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -112, -120, 3750, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -96, -120, 3750, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -96, -88, 3750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, -80, -80, 3750, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 8 } }, -64, -48, 3750, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, -48, -56, 3750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -32, -48, 3750, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, -16, -56, 3750, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 0, -56, 3750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 16, -64, 3750, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, 32, -64, 3750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 48, -56, 3750, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 64, -56, 3750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 32 } }, 80, -72, 3750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, 96, -80, 3750, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, 112, -80, 3750, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, 128, -80, 3750, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, 144, -88, 3750, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 88 } }, -160, -120, 750, { .fields = { 104, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, -160, -32, 750, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 80 } }, -152, -32, 812, { .fields = { 96, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, -152, -120, 812, { .fields = { 112, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, -144, -120, 875, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 80 } }, -144, -32, 875, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 80 } }, -136, -32, 1000, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 72 } }, -128, -32, 1062, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 72 } }, -120, -32, 1125, { .fields = { 16, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, 40, -64, 1900, { .fields = { 120, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 48, -64, 1875, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 64, -56, 1875, { .fields = { 48, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 80, -56, 1875, { .fields = { 48, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 96, -56, 1875, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 112, -64, 1875, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 128, -64, 1875, { .fields = { 80, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -80, -96, 1250, { .fields = { 104, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 56 } }, -72, -88, 1325, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -72, -32, 1325, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -80, -32, 1250, { .fields = { 88, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -88, -32, 1225, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -88, -96, 1225, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 32 } }, -112, -112, 750, { .fields = { 40, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 32 } }, -104, -112, 750, { .fields = { 16, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 72 } }, -104, -32, 1125, { .fields = { 24, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 64 } }, -96, -32, 1125, { .fields = { 8, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 48 } }, -96, -80, 1125, { .fields = { 56, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 48 } }, -104, -80, 1125, { .fields = { 48, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 8, 24 } }, -96, -104, 750, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 48 } }, -112, -80, 1125, { .fields = { 16, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 72 } }, -112, -32, 1125, { .fields = { 16, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 32 } }, -120, -120, 750, { .fields = { 8, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 56 } }, -120, -88, 1125, { .fields = { 80, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 8, 32 } }, -128, -120, 750, { .fields = { 120, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 56 } }, -128, -88, 1062, { .fields = { 72, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 32 } }, -136, -120, 750, { .fields = { 56, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 56 } }, -136, -88, 1000, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, 144, -64, 1900, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, 152, -64, 1900, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// View-2 source ranges [0,19), [19,88), [88,110) and [110,149), then END.
///
/// The leading zero-count record selects decoded-image strips; all ranges are
/// visible and included in cached packet processing. Authored trailing bytes
/// are retained with their role unproven.
static SpriteBatch _gNeoArkSubmarineGalleryView2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 69, 0, 0, { 0, 0 } },
    { 88, 22, 0, 0, { 2, 0 } },
    { 110, ARRAY_SIZE(_gNeoArkSubmarineGalleryView2SpriteSources) - 110, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-3 opaque sprite rectangles and depths, selected in two contiguous ranges.
static SpriteSource _gNeoArkSubmarineGalleryView3SpriteSources[78] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, -120, 1175, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, -104, 1175, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, -88, 1175, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, -72, 1175, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -56, 1175, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, -56, 1175, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, -48, 1175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -40, 1175, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, -32, 1175, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -32, 1175, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -32, 1175, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -40, 1175, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -48, 1175, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -120, 1175, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, -104, 1175, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -88, 1175, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, -72, 1175, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -56, 1175, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -56, 1175, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 0, 1282, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -120, 1425, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -112, 1425, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -104, 1425, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -96, 1425, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, -88, 1425, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, -80, 1425, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, -72, 1425, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -144, -64, 1423, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -144, -56, 1398, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -104, -56, 1398, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -144, -48, 1398, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -96, -48, 1398, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -144, -40, 1389, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -88, -40, 1389, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -32, 1389, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -80, -32, 1389, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 0, -32, 1325, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, -32, 1325, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 32, -40, 1350, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 88, -40, 1350, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 48, -48, 1350, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 96, -48, 1350, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -56, 1350, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 104, -56, 1350, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 64, -64, 1375, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 72, -72, 1375, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 72, -80, 1400, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 72, -88, 1400, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, -96, 1425, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, -104, 1425, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, -112, 1425, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, -120, 1425, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -128, -24, 1360, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -56, -24, 1360, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 48, -24, 1405, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -128, -16, 1360, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -48, -16, 1312, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 48, -16, 1392, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -40, -8, 1348, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 48, -8, 1387, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -128, -8, 1348, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -120, 0, 1364, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -32, 0, 1348, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 56, 0, 1387, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -112, 8, 1364, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -24, 8, 1323, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 64, 8, 1323, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -32, 16, 1371, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 56, 16, 1371, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -96, 24, 1342, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -16, 24, 1342, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 48, 24, 1342, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -96, 32, 1298, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -24, 32, 1298, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 48, 32, 1298, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 40, 1298, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -8, 40, 1298, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -112, 16, 1364, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
};

/// View-3 source ranges [0,19) and [19,78), a zero-count end range, then END.
///
/// The leading zero-count record selects decoded-image strips. The empty range
/// starts one past the source array and is kept; authored trailing bytes have
/// an unproven role. All nonempty ranges are visible and cached.
static SpriteBatch _gNeoArkSubmarineGalleryView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, ARRAY_SIZE(_gNeoArkSubmarineGalleryView3SpriteSources) - 19, 0, 0, { 2, 0 } },
    { ARRAY_SIZE(_gNeoArkSubmarineGalleryView3SpriteSources), 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-4 sprite rectangles and depths: 83 opaque and 18 semitransparent elements.
static SpriteSource _gNeoArkSubmarineGalleryView4SpriteSources[101] = {
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -144, 88, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -128, 88, 250, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 250, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, 56, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -80, 56, 450, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, 24, 1125, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, 24, 1156, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 24, 1125, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 24, 1125, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -96, 24, 1125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, 32, 1125, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 72, 500, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -32, 48, 750, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -64, 32, 1000, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 375, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -40, 48, 750, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 32, 849, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 72, 695, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, -88, 1875, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, -88, 1875, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -88, 1875, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -88, 1875, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -88, 1875, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -88, 1875, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, -88, 1875, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -88, 1875, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -88, 1875, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -88, 1875, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 0, -88, 1875, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -160, 16, 2000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -144, 16, 2000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -128, 16, 2000, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -112, 16, 2000, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -96, 16, 2000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 16, 1875, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -64, 16, 1875, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -48, 16, 1875, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -32, 16, 1875, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -16, 16, 1875, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -48, 1875, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -104, -48, 1700, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -80, 112, 625, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -16, 112, 625, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -64, 104, 625, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -16, 104, 625, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -64, 96, 771, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -16, 96, 771, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -48, 88, 1006, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -8, 88, 1006, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -48, 80, 1120, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -8, 80, 1120, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -32, 72, 1190, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -32, 64, 1288, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -32, 56, 1324, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, -56, 1875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 0, 16, 1875, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 16, 16, 1875, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -88, 1875, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 16, -40, 1875, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 32, -104, 1687, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 32, -40, 1687, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 48, -104, 1625, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 48, -40, 1625, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -104, 1625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 64, -40, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 80, -40, 1500, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 80, -104, 1500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 96, -104, 1375, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 96, -40, 1375, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 112, -32, 1125, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -104, 1125, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, 128, -104, 875, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 128, -32, 875, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -104, 875, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -24, 875, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -160, -64, 3750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -144, -56, 3750, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -128, -48, 3750, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, -24, 3750, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -72, 3750, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -80, 3750, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -64, -88, 3750, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, -160, -64, 1900, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -144, -64, 1900, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -128, -64, 1900, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, -112, -56, 1900, { .fields = { 16, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, -96, -56, 1900, { .fields = { 40, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, -80, -56, 1900, { .fields = { 88, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -64, -64, 1900, { .fields = { 64, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -48, -64, 1900, { .fields = { 32, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -32, -64, 1900, { .fields = { 48, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -16, -64, 1900, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 96 } }, 0, -72, 1900, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 96 } }, 8, -72, 1900, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, 144, -120, 862, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 96 } }, 144, -40, 862, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 88 } }, 128, -40, 900, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 88 } }, 136, -40, 862, { .fields = { 80, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, 136, -120, 862, { .fields = { 32, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 56 } }, 128, -96, 900, { .fields = { 48, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// View-4 source ranges [0,19), [19,76), [76,83) and [83,101), then END.
///
/// The leading zero-count record selects decoded-image strips. All ranges are
/// visible and cached; authored trailing bytes retain an unproven role.
static SpriteBatch _gNeoArkSubmarineGalleryView4SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 57, 0, 0, { 0, 0 } },
    { 76, 7, 0, 0, { 2, 0 } },
    { 83, ARRAY_SIZE(_gNeoArkSubmarineGalleryView4SpriteSources) - 83, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-5 sprite rectangles and depths: 103 opaque and 20 semitransparent elements.
static SpriteSource _gNeoArkSubmarineGalleryView5SpriteSources[123] = {
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 24, 625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -144, 24, 750, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 24, 750, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 16, 936, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -64, 16, 1095, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -48, 16, 1095, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 0, 16, 1095, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 16, 16, 1095, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 32, 16, 1015, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 16, 856, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 96, 24, 825, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, 24, 750, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 48, 16, 968, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, 16, 904, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -112, 24, 763, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 16, 923, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -32, 24, 1096, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -16, 24, 1096, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, 32, 627, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 32, 600, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 356, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 250, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 80, 250, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 250, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -64, 88, 250, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, 88, 250, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -32, 104, 250, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, 112, 250, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 0, 112, 250, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 16, 112, 250, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 32, 104, 250, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 48, 104, 250, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 104, 250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 104, 250, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, 104, 250, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 128, 96, 250, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 72, 250, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -160, 8, 1875, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 1875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -128, 8, 1875, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, 8, 1875, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 1875, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 8, 1875, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 8, 1875, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -48, 8, 1875, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -32, 8, 1875, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -16, 8, 1875, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 0, 8, 1875, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 16, 8, 1875, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 32, 8, 1875, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 48, 8, 1875, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 64, 8, 1875, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 80, 8, 1875, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 96, 8, 1875, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, 8, 1875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 128, 8, 1875, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 144, 8, 1875, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -160, -104, 1875, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, -104, 1875, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -104, 1875, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -112, -104, 1875, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -96, -104, 1875, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -104, 1875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, -104, 1875, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, -104, 1875, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -32, -104, 1875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -16, -104, 1875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 0, -104, 1875, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -104, 1875, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, -104, 1875, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 48, -104, 1875, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 64, -104, 1875, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 80, -104, 1875, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, -104, 1875, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -104, 1875, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 128, -104, 1875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 144, -104, 1875, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 80 } }, -160, -72, 1875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -40, -56, 1875, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 24, -56, 1875, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 80, -56, 1875, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, -160, -72, 3750, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, -144, -72, 3750, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 32 } }, -128, -64, 3750, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 32 } }, -112, -64, 3750, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, -96, -72, 3750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -80, -80, 3750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -64, -80, 3750, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -48, -80, 3750, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -32, -80, 3750, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -16, -80, 3750, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, 0, -72, 3750, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, 16, -56, 3750, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, 32, -56, 3750, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 48, -48, 3750, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 144, -112, 3750, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 72 } }, 128, -104, 3750, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 112, -96, 3750, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 56 } }, 96, -88, 3750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, 80, -72, 3750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 96 } }, -160, -80, 1900, { .fields = { 112, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, -144, -80, 1900, { .fields = { 80, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, -128, -80, 1900, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, -112, -72, 1900, { .fields = { 32, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, -96, -72, 1900, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, -80, -72, 1900, { .fields = { 48, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, -64, -64, 1900, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, -48, -64, 1900, { .fields = { 0, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, -32, -64, 1900, { .fields = { 32, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, -16, -64, 1900, { .fields = { 16, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 0, -64, 1900, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 16, -64, 1900, { .fields = { 64, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 32, -64, 1900, { .fields = { 0, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 48, -64, 1900, { .fields = { 48, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 64, -72, 1900, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 80, -72, 1900, { .fields = { 64, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 96, -72, 1900, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, 112, -80, 1900, { .fields = { 96, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, 128, -80, 1900, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, 144, -80, 1900, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// View-5 source ranges [0,20), [20,40), [40,84), [84,103) and [103,123), then END.
///
/// The leading zero-count record selects decoded-image strips. All ranges are
/// visible and cached; authored trailing bytes retain an unproven role.
static SpriteBatch _gNeoArkSubmarineGalleryView5SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { 20, 20, 0, 0, { 3, 0 } },
    { 40, 44, 0, 0, { 2, 0 } },
    { 84, 19, 0, 0, { 4, 0 } },
    { 103, ARRAY_SIZE(_gNeoArkSubmarineGalleryView5SpriteSources) - 103, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-6 opaque sprite rectangles and depths, selected as one complete range.
static SpriteSource _gNeoArkSubmarineGalleryView6SpriteSources[19] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 500, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 500, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 88, 500, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 500, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 88, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, 88, 500, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -32, 88, 500, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 104, 500, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 112, 500, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 104, 500, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 104, 500, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 104, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 104, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 104, 500, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 104, 500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 104, 500, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 96, 500, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

/// View-6 list drawing every source sprite after selecting decoded-image strips.
static SpriteBatch _gNeoArkSubmarineGalleryView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkSubmarineGalleryView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-7 sprite rectangles and depths: nine opaque and 24 semitransparent elements.
static SpriteSource _gNeoArkSubmarineGalleryView7SpriteSources[33] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -136, -16, 1125, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -120, -16, 1125, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -104, -16, 1125, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -88, -16, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -72, -16, 1125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -56, -16, 1125, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, -16, 1125, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 0, 1125, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 0, 1125, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -160, -48, 1000, { .fields = { 48, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -160, -120, 1500, { .fields = { 112, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -144, -120, 1500, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -128, -120, 1500, { .fields = { 112, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -112, -120, 1500, { .fields = { 96, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -96, -120, 1500, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -96, -48, 1000, { .fields = { 8, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -112, -48, 1000, { .fields = { 0, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -80, -120, 1500, { .fields = { 80, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -80, -56, 1000, { .fields = { 96, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -128, -48, 1000, { .fields = { 0, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -144, -48, 1000, { .fields = { 64, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -64, -120, 1500, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -64, -56, 1000, { .fields = { 64, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -48, -56, 1125, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -48, -120, 1500, { .fields = { 64, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -32, -120, 1500, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -32, -56, 1125, { .fields = { 80, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -16, -64, 1500, { .fields = { 80, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -16, -120, 1500, { .fields = { 16, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 64 } }, 8, -64, 2664, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 64 } }, 0, -64, 2717, { .fields = { 48, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -120, 2584, { .fields = { 24, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, 8, -120, 2560, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// View-7 source ranges [0,9) and [9,33), then END, with decoded-image strips.
///
/// Both ranges are visible and cached; authored trailing bytes retain an
/// unproven role.
static SpriteBatch _gNeoArkSubmarineGalleryView7SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, ARRAY_SIZE(_gNeoArkSubmarineGalleryView7SpriteSources) - 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkSubmarineGallerySpriteViews[NEO_ARK_SUBMARINE_GALLERY_VIEW_COUNT] = {
    { { .empty = _gNeoArkSubmarineGalleryView1SpriteBatches }, _gNeoArkSubmarineGalleryView1SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineGalleryView2SpriteSources }, _gNeoArkSubmarineGalleryView2SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineGalleryView3SpriteSources }, _gNeoArkSubmarineGalleryView3SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineGalleryView4SpriteSources }, _gNeoArkSubmarineGalleryView4SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineGalleryView5SpriteSources }, _gNeoArkSubmarineGalleryView5SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineGalleryView6SpriteSources }, _gNeoArkSubmarineGalleryView6SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineGalleryView7SpriteSources }, _gNeoArkSubmarineGalleryView7SpriteBatches, NULL },
};

/// Four all-view directional sources, with Q12 RGB intensity and room-axis vectors.
///
/// Coordinates and attenuation remain writable for composition and light queries.
static WorldCoordLight _gNeoArkSubmarineGalleryRoomLightsDirectional[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
};

/// Ten all-view point sources, with Q12 RGB and integer room-distance falloff radii.
///
/// Coordinates and attenuation remain writable for composition and light queries.
static WorldCoordPointLight _gNeoArkSubmarineGalleryRoomLightsPoint[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2400, 4608, 1600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2900, 4608, -600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1601, 4608, -2400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 600, 4608, -2900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2400, 4608, -1600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2900, 4608, 600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1600, 4608, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -600, 4608, 2800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 6165, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 413, 413, 618 }, { 0, 0 } }, 1568, 1940 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1100, 4608, 3300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
};

WorldCoordRoomLights gNeoArkSubmarineGalleryRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gNeoArkSubmarineGalleryRoomLightsDirectional),
    .directionalLights     = _gNeoArkSubmarineGalleryRoomLightsDirectional,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkSubmarineGalleryRoomLightsPoint),
    .pointLights           = _gNeoArkSubmarineGalleryRoomLightsPoint,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gNeoArkSubmarineGalleryViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 1901, 1952, -1890, 0 }, { { 1807, -3712, -1820, 0 }, { -1807, -3712, 1819, 0 }, { 1807, 3712, -1820, 0 }, { -1807, 3712, 1819, 0 } }, { 2912, 0, 2892, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1983, 1952, -1761, 0 }, { { -1813, -3712, 1813, 0 }, { 1813, -3712, -1814, 0 }, { -1813, 3712, 1813, 0 }, { 1813, 3712, -1814, 0 } }, { -2904, 0, -2903, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1854, 1888, 1791, 0 }, { { -1819, -3712, -1808, 0 }, { 1819, -3712, 1807, 0 }, { -1819, 3712, -1808, 0 }, { 1819, 3712, 1807, 0 } }, { 2893, 0, -2913, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2014, 1888, 1791, 0 }, { { 1819, -3712, 1807, 0 }, { -1819, -3712, -1808, 0 }, { 1819, 3712, 1807, 0 }, { -1819, 3712, -1808, 0 } }, { -2895, 0, 2911, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2081, 2016, 1696, 0 }, { { 1977, -3712, -1634, 0 }, { -1977, -3712, 1633, 0 }, { 1977, 3712, -1634, 0 }, { -1977, 3712, 1633, 0 } }, { 2615, 0, 3164, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1922, 1984, 1727, 0 }, { { -1928, -3712, 1690, 0 }, { 1929, -3712, -1691, 0 }, { -1928, 3712, 1690, 0 }, { 1929, 3712, -1691, 0 } }, { -2707, 0, -3088, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1859, 1888, -1762, 0 }, { { -1821, -3712, -1804, 0 }, { 1822, -3712, 1805, 0 }, { -1821, 3712, -1804, 0 }, { 1822, 3712, 1805, 0 } }, { 2888, 0, -2917, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1825, 1792, -1920, 0 }, { { 1821, -3712, 1804, 0 }, { -1822, -3712, -1805, 0 }, { 1821, 3712, 1804, 0 }, { -1822, 3712, -1805, 0 } }, { -2890, 0, 2915, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    NEO_ARK_SUBMARINE_GALLERY_ISLAND_WARP_SLOTS           = (1 << 4) | 2,
    NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_DIVER_ARRIVAL = 2,
    NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_ROOM_STATE    = 3,
    NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CAP_COMMAND_1       = 1,
    NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CAP_COMMAND_7       = 7,
};

WorldCollisionTrigger gNeoArkSubmarineGalleryActionTriggers[10] = {
    { NULL, NULL, NULL, { 1567, 4944, 3199, 0 }, { { -1224, 0, 11, 0 }, { 479, 0, -1127, 0 }, { -478, 0, 1128, 0 }, { 1225, 0, -10, 0 } }, { 0, 4112, 0, 0 }, { -2440, 0, -3290, 0 }, 1221, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_ISLAND, NEO_ARK_SUBMARINE_GALLERY_ISLAND_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, 4960, 0, 0 }, { { -6696, 0, 139, 0 }, { 127, 0, -6951, 0 }, { -126, 0, 6952, 0 }, { 6697, 0, -138, 0 } }, { 0, 4096, 0, 0 }, { -2440, 0, -3290, 0 }, 6945, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_ROOM_STATE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1200, 4928, 3520, 0 }, { { -1976, 0, 427, 0 }, { 1199, 0, -1511, 0 }, { -1230, 0, 1544, 0 }, { 2009, 0, -458, 0 } }, { 0, 4108, 0, 0 }, { -2106, 0, -3513, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CAP_COMMAND_1, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -33, 4928, -3488, 0 }, { { -2136, 0, -560, 0 }, { 2072, 0, -782, 0 }, { -2071, 0, 782, 0 }, { 2137, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { 402, 0, 4076, 0 }, 2202, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CAP_COMMAND_7, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3456, 4928, 1447, 0 }, { { -1077, 0, 626, 0 }, { 85, 0, -1461, 0 }, { -245, 0, 1605, 0 }, { 1237, 0, -769, 0 } }, { 0, 4111, 0, 0 }, { -3974, 0, -995, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_DIVER_ARRIVAL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3871, 4896, -385, 0 }, { { -574, 0, 1224, 0 }, { -767, 0, -1157, 0 }, { 768, 0, 1158, 0 }, { 575, 0, -1222, 0 } }, { 0, 4104, 0, 0 }, { -4093, 0, 201, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_DIVER_ARRIVAL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2656, 4928, -2273, 0 }, { { 332, 0, 1311, 0 }, { -1327, 0, -407, 0 }, { 1328, 0, 408, 0 }, { -331, 0, -1309, 0 } }, { 0, 4104, 0, 0 }, { -2898, 0, 2897, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_DIVER_ARRIVAL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2320, 4928, 3024, 0 }, { { 1644, 0, 1443, 0 }, { -2158, 0, -201, 0 }, { 2158, 0, 201, 0 }, { -1643, 0, -1442, 0 } }, { 0, 4108, 0, 0 }, { 3036, 0, -2752, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_DIVER_ARRIVAL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3489, 4960, 735, 0 }, { { -286, 0, 2169, 0 }, { -1031, 0, -1906, 0 }, { 1032, 0, 1907, 0 }, { 287, 0, -2167, 0 } }, { 0, 4105, 0, 0 }, { 4053, 0, -602, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_DIVER_ARRIVAL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3169, 4896, -1632, 0 }, { { -1575, 0, 924, 0 }, { 496, 0, -1725, 0 }, { -495, 0, 1725, 0 }, { 1575, 0, -923, 0 } }, { 0, 4102, 0, 0 }, { 3614, 0, 1930, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_GALLERY_TRIGGER_CHECK_DIVER_ARRIVAL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR = 4,
    NEO_ARK_SUBMARINE_GALLERY_RESOURCE_SEA_DIVER      = 61,
};

/// Waypoint-actor and Sea Diver load/spawn resources for layout variant 1.
///
/// Placement entry IDs select file numbers 4 and 61 in groups 10 and 20,
/// respectively; descriptor 0 starts each actor. AREA_PLACEMENT_END terminates
/// the list. Resource and task-table storage borrow their loaded overlays.
static AreaResource _gNeoArkSubmarineGalleryVariant1Resources[3] = {
    { NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR, NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor100400Tasks },
    { NEO_ARK_SUBMARINE_GALLERY_RESOURCE_SEA_DIVER, NEO_ARK_SUBMARINE_GALLERY_RESOURCE_SEA_DIVER, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor206100SeaDiverTaskDescriptors },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Bog Diver load/spawn recipe shared by the three patrol placements of variant 2.
///
/// Entry ID/file number 4 selects base-10 file 100400 and body descriptor 0.
/// AREA_PLACEMENT_END terminates the list. Borrows the loaded actor task table.
static AreaResource _gNeoArkSubmarineGalleryVariant2Resources[2] = {
    { NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR, NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Bog Diver load/spawn recipe for variant 3's wounded floating placement.
///
/// Entry ID/file number 4 selects base-10 file 100400 and body descriptor 0.
/// AREA_PLACEMENT_END terminates the list. Borrows the loaded actor task table.
static AreaResource _gNeoArkSubmarineGalleryVariant3Resources[2] = {
    { NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR, NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Bog Diver load/spawn recipe for variant 11's single patrol placement.
///
/// Entry ID/file number 4 selects base-10 file 100400 and body descriptor 0.
/// AREA_PLACEMENT_END terminates the list. Borrows the loaded actor task table.
static AreaResource _gNeoArkSubmarineGalleryVariant11Resources[2] = {
    { NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR, NEO_ARK_SUBMARINE_GALLERY_RESOURCE_WAYPOINT_ACTOR, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gNeoArkSubmarineGalleryPlacementVariants[13] = {
    { NULL, NULL },
    { gMapNeoArkSubmarineGalleryVariant1Placements, _gNeoArkSubmarineGalleryVariant1Resources },
    { gMapNeoArkSubmarineGalleryVariant2Placements, _gNeoArkSubmarineGalleryVariant2Resources },
    { gMapNeoArkSubmarineGalleryVariant3Placements, _gNeoArkSubmarineGalleryVariant3Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { gMapNeoArkSubmarineGalleryVariant11Placements, _gNeoArkSubmarineGalleryVariant11Resources },
    { NULL, NULL },
};

enum {
    NEO_ARK_SUBMARINE_GALLERY_FOOTSTEP_WALK_CUE2 = SOUND_SCRIPT_REQUEST_TYPE_1 | 21,
    NEO_ARK_SUBMARINE_GALLERY_FOOTSTEP_RUN_CUE2  = SOUND_SCRIPT_REQUEST_TYPE_1 | 23,
};

/// Cue-2 bases in the loaded type-1 bank for the gallery's class-4 floor.
///
/// Cue 1 uses the next entry; companions add 100. Walking and scripted jumps
/// share a pair, while running uses the following pair.
static WorldCollisionFootstepSounds _gNeoArkSubmarineGalleryFloorFootstepSounds = {
    .walk         = NEO_ARK_SUBMARINE_GALLERY_FOOTSTEP_WALK_CUE2,
    .run          = NEO_ARK_SUBMARINE_GALLERY_FOOTSTEP_RUN_CUE2,
    .scriptedJump = NEO_ARK_SUBMARINE_GALLERY_FOOTSTEP_WALK_CUE2,
};

/// Silent blocking collision policy shared by classes 0, 1, 2, 5, 6 and 7.
static WorldCollisionSurfaceProperties _gNeoArkSubmarineGalleryBlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-3 collision policy passing probes and suppressing weapon hit effects.
///
/// Pushback remains enabled; this class has no footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkSubmarineGalleryProbePassSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-4 gallery floor: blocking contacts, weapon impacts and footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkSubmarineGalleryFloorSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkSubmarineGalleryFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkSubmarineGallerySurfacePropertiesByClass[NEO_ARK_SUBMARINE_GALLERY_SURFACE_CLASS_COUNT] = {
    &_gNeoArkSubmarineGalleryBlockingSurface,
    &_gNeoArkSubmarineGalleryBlockingSurface,
    &_gNeoArkSubmarineGalleryBlockingSurface,
    &_gNeoArkSubmarineGalleryProbePassSurface,
    &_gNeoArkSubmarineGalleryFloorSurface,
    &_gNeoArkSubmarineGalleryBlockingSurface,
    &_gNeoArkSubmarineGalleryBlockingSurface,
    &_gNeoArkSubmarineGalleryBlockingSurface,
};

/// Draws two additive capsule glows from consecutive world-space endpoint pairs.
///
/// `worldPoints` supplies four readable, word-aligned SVECTORs, paired as
/// [0, 1] and [2, 3] and borrowed only during the call. Projects each capsule
/// through the current view in that order; a negative GTE flag at either
/// endpoint rejects that capsule without suppressing the other.
///
/// The signed low halfword of `radiusScale` gives each pixel radius as
/// `radiusScale * 64 / depth`. Depth is camera Z / 4, reduced by 64 when
/// greater than 80 in this gallery; accepted endpoints require nonzero depth.
/// `packedColor` bits 8..11, 4..7 and 0..3 supply red, green and blue nibbles
/// scaled by 16, with bit 3 set in each channel on odd animation frames.
/// Both capsules share the radius scale and colour.
///
/// Requires the composed view and projection settings, an initialized scratch
/// stack with room for one `OverlayPointPairScratch`, and the current ordering
/// table and frame arena with room for up to twelve Gouraud quads and their
/// additive blend commands. Releases scratch between capsules; queued packets
/// remain live until GPU drawing completes. Does not restore GTE state.
static inline void _neoArkSubmarineGalleryDrawGlowPair(const SVECTOR worldPoints[4], s32 radiusScale, s32 packedColor)
{
    _glowDrawCapsule(&worldPoints[0], radiusScale, packedColor);
    _glowDrawCapsule(&worldPoints[2], radiusScale, packedColor);
}

void neoArkSubmarineGalleryDrawViewGlowsTask(Task* task)
{
    enum {
        NEO_ARK_SUBMARINE_GALLERY_GLOWS_STATE_NEW   = 0,
        NEO_ARK_SUBMARINE_GALLERY_GLOWS_STATE_READY = 1,
        NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE = 0x200, // Pixel radius = scale * 64 / biased camera depth
        NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR        = 0x444, // Packed RGB nibbles; channels 64, or 72 on odd frames
    };
    const GfxCoord* prismCoord;
    u8              mappedViewIndex;

    prismCoord = task->extra.coordBody->coord;
    // Install this overlay's water-effect handlers once.
    if (task->state == NEO_ARK_SUBMARINE_GALLERY_GLOWS_STATE_NEW) {
        gRoomEffectWaterRippleId = EFFECT_NEO_ARK_SUBMARINE_GALLERY_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_NEO_ARK_SUBMARINE_GALLERY_WATER_SPRAY;
        task->state              = NEO_ARK_SUBMARINE_GALLERY_GLOWS_STATE_READY;
    }
    // Each mapped camera sees its own subset of the fixed world-space glows.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _neoArkSubmarineGalleryDrawGlowPair(_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints, NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            _glowDrawCapsule(&_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints[4], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[0], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[1], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[2], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[15], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            _neoArkSubmarineGalleryDrawLightPrism(prismCoord, NEO_ARK_SUBMARINE_GALLERY_PRISM_FIRST_VERTEX);
            break;
        case 3:
            _glowDrawCapsule(&_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints[12], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[0], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[1], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[10], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[11], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[12], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[13], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[14], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[15], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            break;
        case 4:
            _neoArkSubmarineGalleryDrawGlowPair(&_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints[6], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            _glowDrawCapsule(&_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints[10], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[8], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[9], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[10], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            glowDrawDisc(&_gNeoArkSubmarineGalleryLightGeometry.worldDiscCenters[11], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            break;
        case 5:
            _neoArkSubmarineGalleryDrawGlowPair(&_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints[2], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            // The final capsule starts at the preceding capsule's second endpoint.
            _glowDrawCapsule(&_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints[5], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            break;
        case 6:
            _neoArkSubmarineGalleryDrawGlowPair(&_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints[6], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            _glowDrawCapsule(&_gNeoArkSubmarineGalleryLightGeometry.worldCapsulePoints[10], NEO_ARK_SUBMARINE_GALLERY_GLOW_RADIUS_SCALE, NEO_ARK_SUBMARINE_GALLERY_GLOW_COLOR);
            break;
    }
}

#include "../../shared/effects/water_ripple_task.inc.c"

void neoArkSubmarineGalleryWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task_u16.inc.c"

void neoArkSubmarineGalleryWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/effects/water_spin_u16.inc.c"

/// Depth subtracted from each capsule endpoint above camera Z / 4 = 80.
///
/// This signed integer constant changes both perspective sizing and sorting;
/// 64 leaves a minimum adjusted depth of 17. Define before the capsule fragment,
/// which clears it. An undefined binding leaves depth unchanged; 0 still
/// selects the pull branch. Valid nonnegative pulls are at most 80.
#define GLOW_DRAW_CAPSULE_DEPTH_PULL 0x40
#include "../../shared/effects/glow_draw_capsule.inc.c"

/// Depth subtracted from a disc centre above camera Z / 4 = 80.
///
/// This signed integer constant changes both perspective sizing and sorting;
/// 64 leaves a minimum adjusted depth of 17. Define before the disc fragment,
/// which clears it. An undefined binding leaves depth unchanged; 0 still
/// selects the pull branch. Valid nonnegative pulls are at most 80.
#define GLOW_DRAW_DISC_DEPTH_PULL 0x40
#include "../../shared/effects/glow_draw_disc.inc.c"

/// Projects one face of the gallery's light prism into screen pixels.
///
/// `quadScratch` borrows a live, writable, word-aligned `EffectQuadScratch`.
/// Initialize all four vertices' XYZ components as signed 16-bit positions
/// in `GsWSMATRIX`'s input space, in GPU quad strip order. Loads that matrix's
/// Q12 rotation; its translation and the GTE projection settings must already
/// be loaded.
///
/// Writes all four `screenCorners` and the final RTPT `projectionFlags`, even
/// when projection fails. Corner 0's RTPS flags are discarded; the caller
/// rejects a negative final FLAG. Leaves `vertices` and `depth` untouched,
/// with corner 3's depth in GTE SZ3 for reading before another depth-changing
/// GTE command. GTE state is not restored. Reserves no storage and retains no
/// pointer.
static inline void _neoArkSubmarineGalleryProjectPrismQuad(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&GsWSMATRIX);
    // Save corner 0 before the triple transform replaces the screen FIFO.
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Draws a pulsing additive light prism with four fading sides and a lit cap.
///
/// `firstVertex` is the vector-slot offset of `localPrismCorners` in the room's
/// light geometry; the only caller selects slots 32..39. The first four form the lit ring,
/// the next four its dark counterpart. `coord` is borrowed read-only with
/// `workm` already composed in the input space of `GsWSMATRIX`. Rotation is
/// Q12; rotated corners and translated positions retain their low 16 bits as
/// signed GTE coordinates. Brightness is 22..26 on a four-frame sine cycle.
///
/// Each nonnegative final RTPT FLAG emits one `POLY_G4`, ordered by its last
/// corner's SZ3 / 4; the preceding RTPS FLAG is discarded. Requires room for
/// five quad packets and their blend commands in the frame primitive arena,
/// and one word-aligned `EffectQuadScratch` on the initialized scratch stack.
/// The scratch block is released on return; no pointer is retained and GTE
/// state is overwritten.
static void _neoArkSubmarineGalleryDrawLightPrism(const GfxCoord* coord, s16 firstVertex)
{
    /// Rotates and translates one prism corner into the scratch quad.
    ///
    /// Captures `coord` (borrowed, composed const GfxCoord*) and `quadScratch`
    /// (live EffectQuadScratch*). The GTE rotation must already be `coord->workm`.
    /// Arguments have no side effects: vertexIndex is a table index evaluated
    /// once; cornerSlot is 0..3 and evaluated repeatedly. Each axis retains its
    /// low 16 bits as a signed projection coordinate. Use only as a standalone
    /// statement sequence here, never as an unbraced branch body.
#define NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(vertexIndex, cornerSlot)                                                     \
    gte_ldv0(&_gNeoArkSubmarineGalleryLightGeometry.localPrismCorners[(vertexIndex) - NEO_ARK_SUBMARINE_GALLERY_PRISM_FIRST_VERTEX]); \
    gte_rtv0();                                                                                                                       \
    gte_stsv(&quadScratch->vertices[(cornerSlot)]);                                                                                   \
    quadScratch->vertices[(cornerSlot)].vx = (u16)quadScratch->vertices[(cornerSlot)].vx + (u16)coord->workm.t[0];                    \
    quadScratch->vertices[(cornerSlot)].vy = (u16)quadScratch->vertices[(cornerSlot)].vy + (u16)coord->workm.t[1];                    \
    quadScratch->vertices[(cornerSlot)].vz = (u16)quadScratch->vertices[(cornerSlot)].vz + (u16)coord->workm.t[2]

    enum {
        PRISM_RING_CORNERS          = 4,
        PRISM_PULSE_ANGLE_SHIFT     = 10,
        PRISM_PULSE_INTENSITY_SHIFT = 11,
        PRISM_BASE_INTENSITY        = 24,
    };

    EffectQuadScratch* quadScratch;
    POLY_G4*           quad;
    s32                cornerIndex;
    s32                nextCorner;
    s32                farCorner;
    s32                farNextCorner;
    u8                 intensity;

    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    quadScratch = SCRATCH_STACK_CURSOR(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    intensity = (rsin(gDisplayState.animFrame << PRISM_PULSE_ANGLE_SHIFT) >> PRISM_PULSE_INTENSITY_SHIFT) + PRISM_BASE_INTENSITY;
    // Join each adjacent pair of lit corners to the corresponding dark pair.
    for (cornerIndex = 0; cornerIndex < PRISM_RING_CORNERS; cornerIndex++) {
        gte_SetRotMatrix(&coord->workm);
        NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + cornerIndex, 0);
        gte_SetRotMatrix(&coord->workm);
        nextCorner = (cornerIndex + 1) & (PRISM_RING_CORNERS - 1);
        NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + nextCorner, 1);
        gte_SetRotMatrix(&coord->workm);
        farCorner = cornerIndex + PRISM_RING_CORNERS;
        NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + farCorner, 2);
        gte_SetRotMatrix(&coord->workm);
        farNextCorner = nextCorner + PRISM_RING_CORNERS;
        NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + farNextCorner, 3);
        _neoArkSubmarineGalleryProjectPrismQuad(quadScratch);
        if (quadScratch->projectionFlags >= 0) {
            gte_stszotz(&quadScratch->depth);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, intensity, intensity, intensity);
            setRGB1(quad, intensity, intensity, intensity);
            setRGB2(quad, 0, 0, 0);
            setRGB3(quad, 0, 0, 0);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth),
                    quad);
            quad->x0 = quadScratch->screenCorners[0].vx;
            quad->y0 = quadScratch->screenCorners[0].vy;
            quad->x1 = quadScratch->screenCorners[1].vx;
            quad->y1 = quadScratch->screenCorners[1].vy;
            quad->x2 = quadScratch->screenCorners[2].vx;
            quad->y2 = quadScratch->screenCorners[2].vy;
            quad->x3 = quadScratch->screenCorners[3].vx;
            quad->y3 = quadScratch->screenCorners[3].vy;
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, quadScratch->depth);
        }
    }
    // Close the lit ring in the GPU quad's strip order, 0, 1, 3, 2.
    gte_SetRotMatrix(&coord->workm);
    NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex, 0);
    gte_SetRotMatrix(&coord->workm);
    NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + 1, 1);
    gte_SetRotMatrix(&coord->workm);
    NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + 3, 2);
    gte_SetRotMatrix(&coord->workm);
    NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + 2, 3);
    _neoArkSubmarineGalleryProjectPrismQuad(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        setRGB0(quad, intensity, intensity, intensity);
        setRGB1(quad, intensity, intensity, intensity);
        setRGB2(quad, intensity, intensity, intensity);
        setRGB3(quad, intensity, intensity, intensity);
        quad->x0 = quadScratch->screenCorners[0].vx;
        quad->y0 = quadScratch->screenCorners[0].vy;
        quad->x1 = quadScratch->screenCorners[1].vx;
        quad->y1 = quadScratch->screenCorners[1].vy;
        quad->x2 = quadScratch->screenCorners[2].vx;
        quad->y2 = quadScratch->screenCorners[2].vy;
        quad->x3 = quadScratch->screenCorners[3].vx;
        quad->y3 = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth), quad);
        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, quadScratch->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
#undef NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER
}
