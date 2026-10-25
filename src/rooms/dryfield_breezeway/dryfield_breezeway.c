#include "rooms/dryfield_breezeway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_breezeway_private.h"

#include "actors/actor_00100.h"
#include "actors/actor_02500.h"
#include "actors/actor_323400.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_events.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */

/// The request the event gate latched, and the descriptor of the
/// event task it spawns to act on it.
extern RoomEventReq gRoomEventReq;

static Task* _gDryfieldBreezewayKeyItemModelTask;

/// Raised by the room's event gate `_roomEventGate` when it
/// latched a request and spawned the event task, cleared on every other call.
extern u8 gRoomEventActive;

static WorldCollisionGrid    _gDryfieldBreezewayCollisionGrid;
static WorldCollisionTrigger _gDryfieldBreezewayViewBoundaryTriggers[4];
static WorldCollisionTrigger _gDryfieldBreezewayActionTriggers[5];
static WorldCoordRoomLights  _gDryfieldBreezewayRoomLights;
/// Hanging-line palette: 256 RGB555 entries packed two per word (512 bytes).
///
/// Borrowed by the GPU until its complete transfer finishes; keep this room loaded.
static u_long _gDryfieldBreezewayKeyItemLineClut[128] = {
#include "assets/dryfield_breezeway_clut_05984.inc"
};

const GpuImageUpload gDryfieldBreezewayKeyItemLineClutUploads[2] __attribute__((section(".data"))) = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 256, ARRAY_SIZE(_gDryfieldBreezewayKeyItemLineClut) * 2, 1 }, _gDryfieldBreezewayKeyItemLineClut },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

const SVECTOR gDryfieldBreezewayGlowLocalPoint __attribute__((section(".data"))) = { 17990, -1365, 2825, 0 };

WorldCollisionRoomResources gDryfieldBreezewayRoomCollisionResources[1] = {
    { &_gDryfieldBreezewayCollisionGrid, _gDryfieldBreezewayViewBoundaryTriggers, _gDryfieldBreezewayActionTriggers, NULL },
};

u8* gDryfieldBreezewayViewMaps[1] = {
    gViewIdentityMap,
};

WorldCoordRoomLighting gDryfieldBreezewayRoomLighting[1] = {
    { &_gDryfieldBreezewayRoomLights, NULL },
};

ViewCount gDryfieldBreezewayViewCounts[2] = { DRYFIELD_BREEZEWAY_VIEW_COUNT, 0 };

/// Map-marker nibble IDs attached to the two breezeway warp endpoints.
enum {
    DRYFIELD_BREEZEWAY_WATER_TOWER_ENDPOINT_MAP_FLAG = 476,
    DRYFIELD_BREEZEWAY_FACTORY_ENDPOINT_MAP_FLAG     = 475,
};

DirectionWarpEntry gDryfieldBreezewayWarpEntries[2] = {
    { { { .word = 1024 }, 6656, 1, 1568 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 6656, 1, 1568 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 2), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_BREEZEWAY_WATER_TOWER_ENDPOINT_MAP_FLAG },
    { { { .word = 3072 }, 0x44F3, 1, 2075 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x44F3, 1, 2075 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 4), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 3), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 5), 4, DIRECTION_WARP_FLAG_NONE, DRYFIELD_BREEZEWAY_FACTORY_ENDPOINT_MAP_FLAG },
};

/// Sixteen room-space face normals, with ONE for unit length.
static SVECTOR _gDryfieldBreezewayCollisionGridNormals[16] = {
#include "assets/dryfield_breezeway_collision_06068_normals.inc"
};

/// Forty-six collision-mesh vertices in signed room-coordinate game units.
static SVECTOR _gDryfieldBreezewayCollisionGridVerts[46] = {
#include "assets/dryfield_breezeway_collision_06068_verts.inc"
};

/// Twenty-four indexed quads; face 21 uses footstep class 1 and the others class 0.
static WorldCollisionGridFace _gDryfieldBreezewayCollisionGridFaces[24] = {
#include "assets/dryfield_breezeway_collision_06068_faces.inc"
};

/// Eight concatenated signed face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gDryfieldBreezewayCollisionGridCells[126] = {
#include "assets/dryfield_breezeway_collision_06068_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldBreezewayCollisionGridCells` (126 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_BREEZEWAY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldBreezewayCollisionGridCells[(halfwordOffset)])

/// X-major 4-by-2 directory into the borrowed collision face-index lists.
static s16* _gDryfieldBreezewayCollisionGridTable[8] = {
#include "assets/dryfield_breezeway_collision_06068_table.inc"
};
#undef DRYFIELD_BREEZEWAY_CELL_FACE_LIST_AT_OFFSET

/// The breezeway's room collision mesh and 4-by-2 XZ cell index.
///
/// Activation binds the view coordinate; mesh pools remain writable and live
/// with the room. Cells are 4000 game units wide, with additive biases -5000
/// on X and 1000 on Z. Cell lists index all 24 faces and terminate with -1.
static WorldCollisionGrid _gDryfieldBreezewayCollisionGrid = {
    NULL,
    _gDryfieldBreezewayCollisionGridNormals,
    _gDryfieldBreezewayCollisionGridVerts,
    _gDryfieldBreezewayCollisionGridFaces,
    _gDryfieldBreezewayCollisionGridTable,
    -5000,
    1000,
    4,
    2,
    4000,
    ARRAY_SIZE(_gDryfieldBreezewayCollisionGridFaces),
};

ViewCamera gDryfieldBreezewayViewCameras[DRYFIELD_BREEZEWAY_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -0x2EE0, 0x5DC0, 0 } }, 322 },
    { { { { 0, 0, ONE }, { -1081, 3950, 0 }, { -3950, -1081, 0 } }, { -0x38A4, 200, -1600 } }, 257 },
    { { { { 0, 0, -4095 }, { 831, 4010, 0 }, { 4010, -831, 0 } }, { -9800, 300, -1600 } }, 257 },
    { { { { 1090, 0, -3948 }, { 702, 4030, 194 }, { 3885, -729, 1072 } }, { -0x35CA, 500, -1150 } }, 257 },
    { { { { 1589, 0, -3775 }, { -1365, 3818, -575 }, { 3519, 1481, 1481 } }, { -0x459C, 1450, -2780 } }, 257 },
    { { { { 0, 0, -ONE }, { 0, ONE, 0 }, { ONE, 0, 0 } }, { -0x41AC, 295, -3354 } }, 680 },
};

/// Empty sprite list for mapped view 1; the zero first count keeps the decoded background.
static SpriteBatch _gDryfieldBreezewayView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite modulation and GPU code choices shared by this room's source arrays.
enum {
    DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION = 128,
    DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED   = 0,
};

/// Seven depth-sorted sprite rectangles for mapped view 2, in view-centred pixels.
static SpriteSource _gDryfieldBreezewayView2SpriteSources[7] = {
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 32, 16, 1000, { .fields = { 120, 136 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, 16, 1000, { .fields = { 0, 136 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, 16, 1000, { .fields = { 8, 136 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 32, 56, 750, { .fields = { 120, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 40, 56, 750, { .fields = { 0, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 48, 56, 750, { .fields = { 8, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, 16, 64, 750, { .fields = { 0, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
};

/// Two view-2 ranges cover all seven sprites after the decoded background.
static SpriteBatch _gDryfieldBreezewayView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, ARRAY_SIZE(_gDryfieldBreezewayView2SpriteSources) - 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Thirty-one depth-sorted sprite rectangles for mapped view 3, in view-centred pixels.
static SpriteSource _gDryfieldBreezewayView3SpriteSources[31] = {
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -160, -120, 750, { .fields = { 112, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -160, 0, 750, { .fields = { 112, 120 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -144, -120, 750, { .fields = { 96, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -144, 0, 750, { .fields = { 96, 120 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -128, -120, 750, { .fields = { 80, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -128, 0, 750, { .fields = { 80, 120 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -112, -120, 750, { .fields = { 64, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -112, 0, 750, { .fields = { 64, 120 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -96, -120, 750, { .fields = { 48, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -96, 0, 750, { .fields = { 48, 120 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -80, -120, 750, { .fields = { 32, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -80, 0, 750, { .fields = { 32, 120 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, -120, 750, { .fields = { 16, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -64, -56, 750, { .fields = { 16, 152 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -120, 750, { .fields = { 16, 208 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, 24, 750, { .fields = { 16, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, 48, 750, { .fields = { 0, 56 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -32, 56, 750, { .fields = { 0, 224 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 250, { .fields = { 112, 192 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 250, { .fields = { 0, 168 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 250, { .fields = { 0, 112 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 32, 250, { .fields = { 112, 56 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 250, { .fields = { 112, 152 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -96, 48, 250, { .fields = { 96, 120 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 80, 250, { .fields = { 96, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -80, 24, 250, { .fields = { 112, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 250, { .fields = { 96, 40 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, 32, 250, { .fields = { 112, 104 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 80, 250, { .fields = { 96, 80 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, 56, 250, { .fields = { 0, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, 80, 404, { .fields = { 112, 232 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
};

/// Mapped view 3 ranges: eighteen sprites at depth 750, then thirteen nearer sprites.
static SpriteBatch _gDryfieldBreezewayView3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, ARRAY_SIZE(_gDryfieldBreezewayView3SpriteSources) - 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 4; the zero first count keeps the decoded background.
static SpriteBatch _gDryfieldBreezewayView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 5; the zero first count keeps the decoded background.
static SpriteBatch _gDryfieldBreezewayView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Thirty-eight depth-sorted sprite rectangles for mapped view 6, used by the key-item interaction.
static SpriteSource _gDryfieldBreezewayView6SpriteSources[38] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -88, 296, { .fields = { 16, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, 0, 293, { .fields = { 96, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, 0, 259, { .fields = { 64, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -88, 298, { .fields = { 0, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -88, 298, { .fields = { 32, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -112, 0, 298, { .fields = { 96, 160 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, -88, 298, { .fields = { 16, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -96, 0, 298, { .fields = { 112, 160 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -88, 298, { .fields = { 0, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -80, 0, 298, { .fields = { 80, 80 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -64, 0, 298, { .fields = { 32, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -88, 298, { .fields = { 96, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -48, -88, 298, { .fields = { 80, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -48, 0, 297, { .fields = { 64, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -32, 0, 297, { .fields = { 96, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -32, -88, 298, { .fields = { 96, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -88, 298, { .fields = { 112, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -16, 0, 298, { .fields = { 112, 80 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -88, 298, { .fields = { 112, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 0, 0, 298, { .fields = { 96, 80 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -88, 298, { .fields = { 80, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 16, 0, 298, { .fields = { 80, 160 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -88, 298, { .fields = { 48, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 32, 0, 298, { .fields = { 80, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 48, -88, 298, { .fields = { 32, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, 0, 298, { .fields = { 80, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 64, -88, 298, { .fields = { 48, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, 0, 298, { .fields = { 112, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 80, -88, 297, { .fields = { 64, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, 0, 298, { .fields = { 48, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, -88, 301, { .fields = { 112, 240 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 96, -56, 298, { .fields = { 64, 80 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 0, 298, { .fields = { 0, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, -32, 292, { .fields = { 64, 136 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, -88, 301, { .fields = { 96, 240 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 112, 0, 298, { .fields = { 112, 0 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 128, -88, 298, { .fields = { 64, 88 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, 0, 298, { .fields = { 16, 176 } }, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BREEZEWAY_SPRITE_OPAQUE_MODULATED },
};

/// Mapped view 6 range covering all key-item-view sprites, after the decoded background.
static SpriteBatch _gDryfieldBreezewayView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldBreezewayView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldBreezewaySpriteViews[DRYFIELD_BREEZEWAY_VIEW_COUNT] = {
    { { .empty = _gDryfieldBreezewayView1SpriteBatches }, _gDryfieldBreezewayView1SpriteBatches, NULL },
    { { .elements = _gDryfieldBreezewayView2SpriteSources }, _gDryfieldBreezewayView2SpriteBatches, NULL },
    { { .elements = _gDryfieldBreezewayView3SpriteSources }, _gDryfieldBreezewayView3SpriteBatches, NULL },
    { { .empty = _gDryfieldBreezewayView4SpriteBatches }, _gDryfieldBreezewayView4SpriteBatches, NULL },
    { { .empty = _gDryfieldBreezewayView5SpriteBatches }, _gDryfieldBreezewayView5SpriteBatches, NULL },
    { { .elements = _gDryfieldBreezewayView6SpriteSources }, _gDryfieldBreezewayView6SpriteBatches, NULL },
};

/// 4 directed camera boundaries in room-local game units.
///
/// Source/destination views are 1-based: 3->2, 2->3, 3->4, 4->3.
/// Room binding installs the view coordinate and enables all records through LAST.
/// Normals use 4096 per unit; keep the writable list live until it is unlinked.
static WorldCollisionTrigger _gDryfieldBreezewayViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 0x2E60, -2880, 1440, 0 }, { { 0, -3568, -1024, 0 }, { 0, -3568, 1024, 0 }, { 0, 3568, -1024, 0 }, { 0, 3568, 1024, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3709, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2F20, -2816, 1472, 0 }, { { 0, -3280, 1024, 0 }, { 0, -3280, -1024, 0 }, { 0, 3280, 1024, 0 }, { 0, 3280, -1024, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3434, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3E6D, -2848, 2218, 0 }, { { -235, -3632, 1596, 0 }, { 223, -3632, -1607, 0 }, { -235, 3632, 1596, 0 }, { 223, 3632, -1607, 0 } }, { -4064, 0, -582, 0 }, { 0, 0, ONE, 0 }, 3974, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3E0D, -3120, 2186, 0 }, { { 211, -3392, -1637, 0 }, { -225, -3392, 1627, 0 }, { 211, 3392, -1637, 0 }, { -225, 3392, 1627, 0 } }, { 4067, 0, 543, 0 }, { 0, 0, ONE, 0 }, 3762, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// IDs stored in action records; warp pairs pack departure then arrival in four-bit nibbles.
enum {
    DRYFIELD_BREEZEWAY_TRIGGER_WARP_TO_WATER_TOWER    = (1 << 4) | 2,
    DRYFIELD_BREEZEWAY_TRIGGER_WARP_TO_FACTORY        = (2 << 4) | 2,
    DRYFIELD_BREEZEWAY_TRIGGER_COMMAND_KEY_ITEM       = 3,
    DRYFIELD_BREEZEWAY_TRIGGER_COMMAND_FACTORY_DOOR   = 1,
    DRYFIELD_BREEZEWAY_TRIGGER_ACTION_FIRST_ENCOUNTER = 1,
};

/// 5 interaction quads: 2 warps, 2 room commands, 1 room action.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Room binding enables all records through LAST; keep them live until unlinking.
static WorldCollisionTrigger _gDryfieldBreezewayActionTriggers[5] = {
    { NULL, NULL, NULL, { 6416, -48, 1680, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_TOWER, DRYFIELD_BREEZEWAY_TRIGGER_WARP_TO_WATER_TOWER, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4580, -64, 1760, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4091, 0, 201, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_FACTORY, DRYFIELD_BREEZEWAY_TRIGGER_WARP_TO_FACTORY, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x42B0, -64, 3584, 0 }, { { -992, 0, -560, 0 }, { 992, 0, -560, 0 }, { -992, 0, 560, 0 }, { 992, 0, 560, 0 } }, { 0, 4115, 0, 0 }, { ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BREEZEWAY_TRIGGER_COMMAND_KEY_ITEM, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4580, -64, 3024, 0 }, { { -368, 0, -512, 0 }, { 368, 0, -512, 0 }, { -368, 0, 512, 0 }, { 368, 0, 512, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 630, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BREEZEWAY_TRIGGER_COMMAND_FACTORY_DOOR, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3C5B, -64, 1807, 0 }, { { 32, 0, -1792, 0 }, { 608, 0, -1792, 0 }, { -608, 0, 1792, 0 }, { -32, 0, 1792, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1889, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_BREEZEWAY_TRIGGER_ACTION_FIRST_ENCOUNTER, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Four all-view directional lights; translation vectors and RGB intensities use the room-light contract.
static WorldCoordLight _gDryfieldBreezewayDirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2500, 5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -2500, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -2500, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2500, -5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
};

/// All-view point light at (16457, -1500, 3608), fading between radii 1536 and 4096 game units.
static WorldCoordPointLight _gDryfieldBreezewayPointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4049, -1500, 3608 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1536, 4096 },
};

/// Mutable room light collection: four directional lights, one point light, no cones.
static WorldCoordRoomLights _gDryfieldBreezewayRoomLights = {
    ARRAY_SIZE(_gDryfieldBreezewayDirectionalLights),
    _gDryfieldBreezewayDirectionalLights,
    ARRAY_SIZE(_gDryfieldBreezewayPointLights),
    _gDryfieldBreezewayPointLights,
    0,
    NULL,
};

/// Default variant resources: the cutscene and combat Desert Chasers, followed by the end marker.
static AreaResource _gDryfieldBreezewayVariant1Resources[3] = {
    { 101, 234, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor323400DesertChaserTaskDesc },
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3 resource: the scorpion actor package, followed by the end marker.
static AreaResource _gDryfieldBreezewayVariant3Resources[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 10 resources: the cutscene and combat Desert Chasers, followed by the end marker.
static AreaResource _gDryfieldBreezewayVariant10Resources[3] = {
    { 101, 234, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor323400DesertChaserTaskDesc },
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 10 Desert Chaser placements at (16853, 0, 2735), yaw -1400, with CLUT row offset 2.
static AreaPlacement _gDryfieldBreezewayVariant10Placements[3] = {
    { 101, 0, 0, 0x41D5, 0, 2735, -1400, 0, 0, 2, 0 },
    { 1, 0, 0, 0x41D5, 0, 2735, -1400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gDryfieldBreezewayAreaVariants[13] = {
    { NULL, NULL },
    { gMapDryfieldBreezewayVariant1Placements, _gDryfieldBreezewayVariant1Resources },
    { NULL, NULL },
    { gMapDryfieldBreezewayVariant3Placements, _gDryfieldBreezewayVariant3Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { _gDryfieldBreezewayVariant10Placements, _gDryfieldBreezewayVariant10Resources },
    { NULL, NULL },
    { NULL, NULL },
};

/// Type-1 bank cue-2 bases; playback adds one for cue 1.
enum {
    DRYFIELD_BREEZEWAY_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    DRYFIELD_BREEZEWAY_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
};

/// Class-1 footstep cue bases; scripted jumps use the walking pair.
///
/// Read-only cues retain their authored position in the room data section.
static const WorldCollisionFootstepSounds _gDryfieldBreezewayFootstepSounds __attribute__((section(".data"))) = {
    DRYFIELD_BREEZEWAY_FOOTSTEP_WALK,
    DRYFIELD_BREEZEWAY_FOOTSTEP_RUN,
    DRYFIELD_BREEZEWAY_FOOTSTEP_WALK,
};

/// Blocking collision properties without footstep cues, used by classes 0 and 2..7.
static WorldCollisionSurfaceProperties _gDryfieldBreezewaySilentSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Blocking collision properties with footstep cues, used by class 1.
static WorldCollisionSurfaceProperties _gDryfieldBreezewayFootstepSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldBreezewayFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldBreezewaySurfacePropertiesByClass[8] = {
    &_gDryfieldBreezewaySilentSurface,
    &_gDryfieldBreezewayFootstepSurface,
    &_gDryfieldBreezewaySilentSurface,
    &_gDryfieldBreezewaySilentSurface,
    &_gDryfieldBreezewaySilentSurface,
    &_gDryfieldBreezewaySilentSurface,
    &_gDryfieldBreezewaySilentSurface,
    &_gDryfieldBreezewaySilentSurface,
};

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

/// Borrowed live model task receiving key-item-use queries, or NULL between sessions.
///
/// Published after spawn, retained through synchronous messages and child-exit polling,
/// then cleared when the child is reaped. Room-message initialization also clears it.
/// This handle does not own the child or extend its lifetime.
static Task* _gDryfieldBreezewayKeyItemModelTask = NULL;

RoomEventReq gRoomEventReq = { 0 };

Task* gDryfieldBreezewayFirstEventTask;

/// Progress values shared by this room's factory-door handlers.
enum {
    DRYFIELD_BREEZEWAY_DOOR_MAGNET_READY                = 2,
    DRYFIELD_BREEZEWAY_DOOR_FACTORY_KEY_HELD            = 3,
    DRYFIELD_BREEZEWAY_DOOR_UNLOCKED                    = 4,
    DRYFIELD_BREEZEWAY_DOOR_MAGNET_MISSING              = 5,
    DRYFIELD_BREEZEWAY_DOOR_MAGNET_MISSING_ACKNOWLEDGED = 6,
};

static void _dryfieldBreezewayInitMessageTask(Task* task);
static void _dryfieldBreezewayMessageTaskIdle(Task* unusedTask);

#include "../../shared/room/room_event.inc.c"

s32 dryfieldBreezewayForwardKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 secondArg)
{
    s32 reply;

    if (_gDryfieldBreezewayKeyItemModelTask == NULL) {
        reply = ROOM_KEY_ITEM_USE_REFUSED;
    } else {
        reply = taskMessageDispatch(_gDryfieldBreezewayKeyItemModelTask, messageId, itemId, secondArg);
    }
    return reply;
}

s32 dryfieldBreezewayResolveRoomEventMessage(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        DRYFIELD_BREEZEWAY_FACTORY_ROOM_DEFAULT           = 1,
        DRYFIELD_BREEZEWAY_FACTORY_ROOM_BARRIER_CLEARED   = 2,
        DRYFIELD_BREEZEWAY_CAP_UNLOCK_FACTORY_DOOR        = 4,
        DRYFIELD_BREEZEWAY_CAP_FACTORY_KEY_MISSING        = 2,
        DRYFIELD_BREEZEWAY_OBJECTIVE_AFTER_FACTORY_UNLOCK = 0x38,
    };
    RoomEventReq eventRequest;
    s32          result;

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_FACTORY) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                reply->roomState = DRYFIELD_BREEZEWAY_FACTORY_ROOM_DEFAULT;
            } else {
                reply->roomState = DRYFIELD_BREEZEWAY_FACTORY_ROOM_BARRIER_CLEARED;
            }
        }
        if (request->areaId == GAME_AREA_DRYFIELD_FACTORY) {
            eventRequest.capCmd        = DRYFIELD_BREEZEWAY_CAP_UNLOCK_FACTORY_DOOR;
            eventRequest.missingCapCmd = DRYFIELD_BREEZEWAY_CAP_FACTORY_KEY_MISSING;
            eventRequest.firstSnd      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 6);
            eventRequest.secondSnd     = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 3);
            eventRequest.flagId        = GAME_FLAG_BREEZEWAY_FACTORY_DOOR_UNLOCKED;
            eventRequest.collectedBit  = INVENTORY_COLLECTION_ID_FACTORY_KEY & 0x7F;
            result                     = _roomEventGate(&eventRequest, reply);
            if (gRoomEventActive != 0) {
                gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, DRYFIELD_BREEZEWAY_DOOR_UNLOCKED);
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, DRYFIELD_BREEZEWAY_OBJECTIVE_AFTER_FACTORY_UNLOCK);
            }
            return result;
        }
    }
    return 1;
}

/// Selects locked-door progress from the collected factory key and magnet.
///
/// Unlocked progress is final. A collected key overrides retrieval progress;
/// after a retrieval attempt, an absent magnet preserves the acknowledged
/// missing-magnet notice and otherwise selects the missing or ready state.
static inline void _dryfieldBreezewayUpdateFactoryDoorProgress(void)
{
    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) != DRYFIELD_BREEZEWAY_DOOR_UNLOCKED) {
        if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_FACTORY_KEY) != 0) {
            gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, DRYFIELD_BREEZEWAY_DOOR_FACTORY_KEY_HELD);
        } else if (gameFlagGetNibble(GAME_FLAG_DRYFIELD_BREEZEWAY_0FE) != 0) {
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BOTTLECAP_MAGNET) == 0) {
                if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) != DRYFIELD_BREEZEWAY_DOOR_MAGNET_MISSING_ACKNOWLEDGED) {
                    gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, DRYFIELD_BREEZEWAY_DOOR_MAGNET_MISSING);
                }
            } else {
                gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, DRYFIELD_BREEZEWAY_DOOR_MAGNET_READY);
            }
        }
    }
}

s32 dryfieldBreezewayHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_BREEZEWAY_COMMAND_FACTORY_DOOR     = 1,
        DRYFIELD_BREEZEWAY_COMMAND_KEY_ITEM         = 3,
        DRYFIELD_BREEZEWAY_CAP_BATTLE_REFUSAL       = 5,
        DRYFIELD_BREEZEWAY_KEY_ITEM_OBJECT          = 6,
        DRYFIELD_BREEZEWAY_OBJECT_AVAILABLE         = 1,
        DRYFIELD_BREEZEWAY_KEY_ITEM_TASK_INDEX      = 0,
        DRYFIELD_BREEZEWAY_DOOR_DIALOGUE_TASK_INDEX = 1,
    };
    switch (command) {
        case DRYFIELD_BREEZEWAY_COMMAND_FACTORY_DOOR:
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                capRunCommandWithTransition(DRYFIELD_BREEZEWAY_CAP_BATTLE_REFUSAL);
            } else {
                _dryfieldBreezewayUpdateFactoryDoorProgress();
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(gDryfieldBreezewayInteractionTaskDescs, DRYFIELD_BREEZEWAY_DOOR_DIALOGUE_TASK_INDEX, command, 0);
            }
            break;
        case DRYFIELD_BREEZEWAY_COMMAND_KEY_ITEM:
            if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) >= DRYFIELD_BREEZEWAY_DOOR_MAGNET_READY) {
                if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                    if (areaGetCurrentObjectState(DRYFIELD_BREEZEWAY_KEY_ITEM_OBJECT) == DRYFIELD_BREEZEWAY_OBJECT_AVAILABLE) {
                        taskSpawnFromTable(gDryfieldBreezewayInteractionTaskDescs, DRYFIELD_BREEZEWAY_KEY_ITEM_TASK_INDEX, 0, 0);
                        gameFlagSetNibble(GAME_FLAG_DRYFIELD_BREEZEWAY_0FE, 1);
                    }
                } else {
                    capRunCommandWithTransition(DRYFIELD_BREEZEWAY_CAP_BATTLE_REFUSAL);
                }
            }
            break;
    }
    return 0;
}

s32 dryfieldBreezewayHandleSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 soundCue, s32 unusedSecondArg)
{
    enum { DRYFIELD_BREEZEWAY_SOUND_CUE_7 = 7 };

    if (soundCue == DRYFIELD_BREEZEWAY_SOUND_CUE_7) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, DRYFIELD_BREEZEWAY_SOUND_CUE_7), 0, 0);
    }
    return 0;
}

s32 dryfieldBreezewayHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_BREEZEWAY_ACTION_FIRST_ENCOUNTER     = 1,
        DRYFIELD_BREEZEWAY_FIRST_ENCOUNTER_TASK_INDEX = 1,
    };
    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN) == 0 && request->actionId == DRYFIELD_BREEZEWAY_ACTION_FIRST_ENCOUNTER) {
        gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN, 1);
        taskSpawnFromTable(gDryfieldBreezewayFirstEventTaskDescs, DRYFIELD_BREEZEWAY_FIRST_ENCOUNTER_TASK_INDEX, 0, 0);
    }
    return 0;
}

void dryfieldBreezewayKeyItemSessionTask(Task* task)
{
    enum {
        DRYFIELD_BREEZEWAY_KEY_ITEM_SESSION_START = 0,
        DRYFIELD_BREEZEWAY_KEY_ITEM_SESSION_WAIT  = 1,
    };
    s32 childResult;

    switch (task->state) {
        case DRYFIELD_BREEZEWAY_KEY_ITEM_SESSION_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            _gDryfieldBreezewayKeyItemModelTask = taskSpawnFromTable(&gDryfieldBreezewayKeyItemModelTaskDesc, 0, 0, 0);
            task->state                        += 1;
            return;
        case DRYFIELD_BREEZEWAY_KEY_ITEM_SESSION_WAIT:
            if (taskPollKill(_gDryfieldBreezewayKeyItemModelTask, &childResult) != 0) {
                _gDryfieldBreezewayKeyItemModelTask = NULL;
                taskKill(task);
            }
            return;
    }
}

void dryfieldBreezewayFactoryDoorDialogueTask(Task* task)
{
    enum {
        DRYFIELD_BREEZEWAY_DOOR_DIALOGUE_START       = 0,
        DRYFIELD_BREEZEWAY_DOOR_DIALOGUE_WAIT        = 1,
        DRYFIELD_BREEZEWAY_DOOR_MAGNET_READY_VARIANT = 0xB,
    };
    switch (task->state) {
        case DRYFIELD_BREEZEWAY_DOOR_DIALOGUE_START:
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case DRYFIELD_BREEZEWAY_DOOR_DIALOGUE_WAIT:
            if (capIsBusy() == 0) {
                if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) != DRYFIELD_BREEZEWAY_DOOR_UNLOCKED) {
                    if (capGetVariantKey() == DRYFIELD_BREEZEWAY_DOOR_MAGNET_READY_VARIANT) {
                        gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, DRYFIELD_BREEZEWAY_DOOR_MAGNET_READY);
                    }
                    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) == DRYFIELD_BREEZEWAY_DOOR_MAGNET_MISSING) {
                        gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, DRYFIELD_BREEZEWAY_DOOR_MAGNET_MISSING_ACKNOWLEDGED);
                    }
                }
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
    }
}

/// Registers room messages and stages the unseen first encounter.
///
/// Requires the live scene task for a synchronous actor-command broadcast.
/// Installs the setup task only while the encounter is unseen, advances to idle,
/// and clears the borrowed key-item event handle.
static void _dryfieldBreezewayInitMessageTask(Task* task)
{
    enum {
        DRYFIELD_BREEZEWAY_ACTOR_COMMAND_INITIALIZE     = 0,
        DRYFIELD_BREEZEWAY_FIRST_EVENT_SETUP_TASK_INDEX = 0,
    };
    ActorCommand actorCommand;

    task->msgTable = gDryfieldBreezewayRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN) == 0) {
        actorCommand.context.loc.stage = gGameSession->location.stage;
        actorCommand.context.loc.area  = gGameSession->location.area;
        actorCommand.command           = DRYFIELD_BREEZEWAY_ACTOR_COMMAND_INITIALIZE;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &actorCommand, ACTOR_COMMAND_MESSAGE_APPLY);
        taskSpawnFromTable(gDryfieldBreezewayFirstEventTaskDescs, DRYFIELD_BREEZEWAY_FIRST_EVENT_SETUP_TASK_INDEX, 0, 0);
    }
    task->state++;
    _gDryfieldBreezewayKeyItemModelTask = NULL;
}

/// Keeps the registered room-message task alive without advancing its idle state.
static void _dryfieldBreezewayMessageTaskIdle(Task* unusedTask)
{
}

/// Room-message task states: 0 registers the receiver, 1 idles, 2 releases the task.
///
/// Copied as a complete three-handler value; the task state must remain in 0..2.
static const TaskFuncTable3 _gDryfieldBreezewayMessageTaskStates = {
    { _dryfieldBreezewayInitMessageTask, _dryfieldBreezewayMessageTaskIdle, taskKill }
};

void dryfieldBreezewayMessageTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldBreezewayMessageTaskStates;
    stateHandlers.funcs[task->state](task);
}
