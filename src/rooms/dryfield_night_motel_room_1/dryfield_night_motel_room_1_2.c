#include "rooms/dryfield_night_motel_room_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_night_motel_room_1_private.h"

#include "actors/actor_00700.h"
#include "actors/actor_01600.h"
#include "actors/actor_02500.h"
#include "actors/actor_04000.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/sound_types.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"
#include "../../shared/effects/glow_draw.h"

/// The night motel room 1 cone-light array and adjacent data of unproven purpose.
///
/// Room lighting borrows only `coneLights`, mutating its transform and
/// attenuation while the overlay is loaded. The remaining bytes are outside
/// the light count; their original type and relationship to the light are unproven.
typedef struct {
    WorldCoordSpotLight coneLights[1];   // One mutable cone light, contributing in every view
    u8                  unknown_6C[324]; // Uninterpreted bytes; purpose and internal boundaries unproven
} _DryfieldNightMotelRoom1SpotLightStorage;
STATIC_ASSERT_SIZEOF(_DryfieldNightMotelRoom1SpotLightStorage, 432);

static WorldCollisionGrid     _gDryfieldNightMotelRoom1CollisionGrid;
static WorldCollisionOccluder _gDryfieldNightMotelRoom1SightOccluders[1];
static WorldCollisionTrigger  _gDryfieldNightMotelRoom1ViewBoundaries[8];
static WorldCollisionTrigger  _gDryfieldNightMotelRoom1ActionTriggers[7];
static WorldCoordRoomLights   _gDryfieldNightMotelRoom1RoomLights;

/// Large textured-flare centre for mapped views 2, 3, 8 and 9, in world units.
static SVECTOR _gDryfieldNightMotelRoom1LargeFlareWorldPoint = { 1920, -970, 470, 0 };

/// Small textured-flare centre for mapped views 5 and 6, in world units.
static SVECTOR _gDryfieldNightMotelRoom1SmallFlareWorldPoint = { 2030, -2070, 6320, 0 };

WorldCoordRoomLighting gDryfieldNightMotelRoom1RoomLighting[2] = {
    { .lights = &_gDryfieldNightMotelRoom1RoomLights, .ambientTable = NULL },
    { .lights = &_gDryfieldNightMotelRoom1RoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightMotelRoom1RoomCollisionResources[2] = {
    { .grid = &_gDryfieldNightMotelRoom1CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMotelRoom1ViewBoundaries, .actionTriggers = _gDryfieldNightMotelRoom1ActionTriggers, .occluders = _gDryfieldNightMotelRoom1SightOccluders },
    { .grid = &_gDryfieldNightMotelRoom1CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMotelRoom1ViewBoundaries, .actionTriggers = _gDryfieldNightMotelRoom1ActionTriggers, .occluders = _gDryfieldNightMotelRoom1SightOccluders },
};

/// Maps logical views 1..9 to the room's 1-based camera, image and sprite indices.
///
/// The room directory borrows this writable byte array while the overlay is
/// loaded. Its last three stored zeros are outside the nine-view search count
/// and do not select valid camera or sprite records; this is not a terminated list.
static u8 _gDryfieldNightMotelRoom1LogicalViewMap[12] = {
    1,
    9,
    8,
    4,
    5,
    6,
    7,
    3,
    2,
    0,
    0,
    0,
};

u8* gDryfieldNightMotelRoom1ViewMaps[1] = {
    _gDryfieldNightMotelRoom1LogicalViewMap,
};

enum { DRYFIELD_NIGHT_MOTEL_ROOM_1_LOGICAL_VIEW_COUNT = 9 };

ViewCount gDryfieldNightMotelRoom1ViewCounts[1] = { DRYFIELD_NIGHT_MOTEL_ROOM_1_LOGICAL_VIEW_COUNT };

/// Sound requests and map-marker index belonging to the motel-door endpoint.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_1_DOOR_ARRIVAL_SOUND   = 0x530B0002,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_DOOR_DEPARTURE_SOUND = 0x530B0001,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_DOOR_MAP_FLAG        = 487,
};

DirectionWarpEntry gDryfieldNightMotelRoom1Warps[1] = {
    {
        .player         = { { .word = 3072 }, 4369, 0, 3388 },
        .companion      = { { .word = 3072 }, 4369, 0, 3388 },
        .arrivalSound   = DRYFIELD_NIGHT_MOTEL_ROOM_1_DOOR_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_MOTEL_ROOM_1_DOOR_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_MOTEL_ROOM_1_DOOR_MAP_FLAG,
    },
};

/// Cell counts along the room grid's X and Z axes.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_1_COLLISION_GRID_CELLS_X = 2,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_COLLISION_GRID_CELLS_Z = 2,
};

/// Q12 room-space normal pool for the room collision mesh.
static SVECTOR _gDryfieldNightMotelRoom1CollisionGridNormals[] = {
#include "assets/dryfield_night_motel_room_1_collision_00AD8_normals.inc"
};

/// Room-space collision vertices in signed game-coordinate units.
static SVECTOR _gDryfieldNightMotelRoom1CollisionGridVerts[] = {
#include "assets/dryfield_night_motel_room_1_collision_00AD8_verts.inc"
};

/// Indexed room collision quads, with room-local surface classes 0..3.
static WorldCollisionGridFace _gDryfieldNightMotelRoom1CollisionGridFaces[] = {
#include "assets/dryfield_night_motel_room_1_collision_00AD8_faces.inc"
};

/// Four signed face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
///
/// The final stored zero follows the last list's terminator and is not a face
/// candidate. The whole serialized pool remains resident with the active grid.
static s16 _gDryfieldNightMotelRoom1CollisionGridCells[] = {
#include "assets/dryfield_night_motel_room_1_collision_00AD8_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMotelRoom1CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..119 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MOTEL_ROOM_1_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMotelRoom1CollisionGridCells[(halfwordOffset)])
/// X-major pointers into the four cell lists, indexed as x * cellCountZ + z.
static s16* _gDryfieldNightMotelRoom1CollisionGridTable[DRYFIELD_NIGHT_MOTEL_ROOM_1_COLLISION_GRID_CELLS_X * DRYFIELD_NIGHT_MOTEL_ROOM_1_COLLISION_GRID_CELLS_Z] = {
#include "assets/dryfield_night_motel_room_1_collision_00AD8_table.inc"
};
#undef DRYFIELD_NIGHT_MOTEL_ROOM_1_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and 2x2 spatial index shared by both room-resource records.
///
/// Cells are 4000 game units wide, starting at room X/Z (200, 200). Activation
/// borrows this writable descriptor and its pools and binds the view coordinate;
/// keep them loaded until the active grid is cleared or replaced.
static WorldCollisionGrid _gDryfieldNightMotelRoom1CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelRoom1CollisionGridNormals,
    .vertices    = _gDryfieldNightMotelRoom1CollisionGridVerts,
    .faces       = _gDryfieldNightMotelRoom1CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMotelRoom1CollisionGridTable,
    .xBias       = -200,
    .zBias       = -200,
    .cellCountX  = DRYFIELD_NIGHT_MOTEL_ROOM_1_COLLISION_GRID_CELLS_X,
    .cellCountZ  = DRYFIELD_NIGHT_MOTEL_ROOM_1_COLLISION_GRID_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelRoom1CollisionGridFaces),
};

ViewCamera gDryfieldNightMotelRoom1ViewCameras[9] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -2500, 0x2968, -3000 } }, 257 },
    { { { { 1367, 0, -3861 }, { -708, 4026, -250 }, { 3795, 751, 1344 } }, { -200, 1600, -940 } }, 212 },
    { { { { 1557, 0, 3788 }, { 539, 4054, -221 }, { -3749, 583, 1541 } }, { -4800, 1450, -840 } }, 216 },
    { { { { 4033, 0, 714 }, { 53, 4084, -300 }, { -712, 305, 4021 } }, { -4400, 1350, -1640 } }, 269 },
    { { { { -508, 0, -4064 }, { -1596, 3766, 199 }, { 3737, 1609, -467 } }, { -200, 1850, -5690 } }, 246 },
    { { { { -459, 0, 4070 }, { 2448, 3271, 276 }, { -3250, 2464, -367 } }, { -3200, 2600, -5740 } }, 246 },
    { { { { -785, 0, 4019 }, { 91, 4094, 17 }, { -4018, 93, -785 } }, { -3171, 325, -2743 } }, 329 },
    { { { { 1271, 0, 3893 }, { 1803, 3630, -588 }, { -3450, 1897, 1126 } }, { -4561, 2558, -1174 } }, 257 },
    { { { { 1324, 0, -3876 }, { -1721, 3669, -588 }, { 3472, 1819, 1186 } }, { -239, 2334, -1269 } }, 225 },
};

/// Mapped view 1's empty sprite list, retaining decoded-image background strips.
///
/// The zero-count first record selects strips; the following record ends the
/// list. The view's retained source-slot address supplies no sprite elements.
static SpriteBatch _gDryfieldNightMotelRoom1View1SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 2's empty sprite list, retaining decoded-image background strips.
///
/// The zero-count first record selects strips; the following record ends the
/// list. The view's retained source-slot address supplies no sprite elements.
static SpriteBatch _gDryfieldNightMotelRoom1View2SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 3's empty sprite list, retaining decoded-image background strips.
///
/// The zero-count first record selects strips; the following record ends the
/// list. The view's retained source-slot address supplies no sprite elements.
static SpriteBatch _gDryfieldNightMotelRoom1View3SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 4's 45 texture rectangles and scene-sorting depths.
///
/// Positions and dimensions are pixels relative to the drawing origin;
/// depths are unscaled ordering values. The view's batches cover this entire
/// writable array, borrowed only while the room overlay is loaded.
static SpriteSource _gDryfieldNightMotelRoom1View4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 64, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 750, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -160, 16, 750, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 32, 750, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 775, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 775, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 775, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 775, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 775, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, 80, 775, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 750, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 750, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 0, 750, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 40, 750, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 80, 750, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -120, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -80, 725, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 725, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 725, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 40, 725, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 80, 725, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 750, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 40, 750, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, 64, 725, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, 24, 725, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -16, 725, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -56, -56, 725, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -64, -96, 725, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -64, -120, 725, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, -104, 1175, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, -64, 975, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, -96, 975, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, -120, 975, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -64, 1125, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -24, 1125, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -32, 16, 1125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 56, 1125, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 56, 1162, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -16, 16, 1175, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, -24, 1175, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -16, -64, 1175, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -96, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -96, 1175, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, -120, 1000, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
};

/// Mapped view 4's four sprite ranges, preceded by a strip-background selector.
///
/// Ranges partition all 45 source elements; the last record ends traversal.
/// All ranges start visible and included in cached packets. The stored
/// `field_6` bytes retain serialized values whose purpose is unproven.
static SpriteBatch _gDryfieldNightMotelRoom1View4SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 3, .field_6 = { 3, 0 } },
    { .firstSprite = 3, .spriteCount = 21, .field_6 = { 0, 0 } },
    { .firstSprite = 24, .spriteCount = 6, .field_6 = { 2, 0 } },
    { .firstSprite = 30, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelRoom1View4SpriteSources) - 30, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 5's 81 texture rectangles and scene-sorting depths.
///
/// Positions and dimensions are pixels relative to the drawing origin;
/// depths are unscaled ordering values. The view's batches cover this entire
/// writable array, borrowed only while the room overlay is loaded.
static SpriteSource _gDryfieldNightMotelRoom1View5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, -120, 800, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, -80, 800, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, -40, 800, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -128, 0, 800, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -88, -120, 800, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -88, -80, 800, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -88, -40, 800, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -88, 0, 800, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 0, 800, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, 0, 800, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -40, 800, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -80, 800, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -120, 800, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -32, -120, 800, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 8, -120, 800, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -120, 800, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -80, 800, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, -40, 831, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, 0, 831, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 562, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 562, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 562, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 562, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 40, 562, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 562, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 40, 838, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 831, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 0, 846, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -40, 837, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -40, 825, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 32, -64, 831, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -80, 800, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 800, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, -120, 667, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, -80, 667, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, -40, 667, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, 0, 667, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, 40, 667, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, 80, 667, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 72, -120, 787, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 72, -80, 787, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 72, -40, 781, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 72, 0, 781, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 72, 40, 781, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, 72, 80, 781, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -120, 825, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -80, 825, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -40, 825, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 825, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 40, 825, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -72, 64, 675, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 48, 675, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -72, 48, 675, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -40, 48, 675, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 32, 700, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 32, 700, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, 32, 700, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -40, 24, 712, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -72, 24, 712, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 16, 700, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 40, 525, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 56, 500, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -120, 40, 525, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 24, 537, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, 24, 537, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -96, 72, 375, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 375, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -80, 375, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -40, 375, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -80, 281, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, -40, 375, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, 0, 375, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 0, 375, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -112, 0, 375, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, -160, 40, 375, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 40, 375, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 40, 375, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 375, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 375, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 375, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 80, 375, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
};

/// Mapped view 5's five sprite ranges, preceded by a strip-background selector.
///
/// Ranges partition all 81 source elements; the last record ends traversal.
/// All ranges start visible and included in cached packets. The stored
/// `field_6` bytes retain serialized values whose purpose is unproven.
static SpriteBatch _gDryfieldNightMotelRoom1View5SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 19, .field_6 = { 0, 0 } },
    { .firstSprite = 19, .spriteCount = 31, .field_6 = { 3, 0 } },
    { .firstSprite = 50, .spriteCount = 10, .field_6 = { 2, 0 } },
    { .firstSprite = 60, .spriteCount = 5, .field_6 = { 4, 0 } },
    { .firstSprite = 65, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelRoom1View5SpriteSources) - 65, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 6's 47 texture rectangles and scene-sorting depths.
///
/// Positions and dimensions are pixels relative to the drawing origin;
/// depths are unscaled ordering values. The view's batches cover this entire
/// writable array, borrowed only while the room overlay is loaded.
static SpriteSource _gDryfieldNightMotelRoom1View6SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 525, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 525, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 525, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 525, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 525, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 525, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 712, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 712, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 712, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 725, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, 40, 731, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, 80, 731, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -96, -120, 862, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -80, 862, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -40, 862, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -96, 0, 837, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -96, 40, 837, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -96, 80, 837, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -120, 1037, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -80, 1037, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -40, 1037, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, 0, 1037, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 40, 1037, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -120, 1200, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -80, 1200, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -40, 1200, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 0, 1200, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 40, 425, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -120, 425, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -120, 425, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 104, -80, 375, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -80, 425, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -80, 425, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -40, 425, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -40, 425, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 0, 425, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, 0, 425, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, 40, 425, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 40, 425, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 80, 425, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 80, 425, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 104, 375, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, 104, 375, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 88, 400, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, 88, 400, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 80, 425, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 72, 80, 425, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
};

/// Mapped view 6's three sprite ranges, preceded by a strip-background selector.
///
/// Ranges partition all 47 source elements; the last record ends traversal.
/// All ranges start visible and included in cached packets. The stored
/// `field_6` bytes retain serialized values whose purpose is unproven.
static SpriteBatch _gDryfieldNightMotelRoom1View6SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 27, .field_6 = { 1, 0 } },
    { .firstSprite = 27, .spriteCount = 14, .field_6 = { 2, 0 } },
    { .firstSprite = 41, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelRoom1View6SpriteSources) - 41, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 7's empty sprite list, retaining decoded-image background strips.
///
/// The zero-count first record selects strips; the following record ends the
/// list. The view's retained source-slot address supplies no sprite elements.
static SpriteBatch _gDryfieldNightMotelRoom1View7SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 8's 111 texture rectangles and scene-sorting depths.
///
/// Positions and dimensions are pixels relative to the drawing origin;
/// depths are unscaled ordering values. The view's batches cover this entire
/// writable array, borrowed only while the room overlay is loaded.
static SpriteSource _gDryfieldNightMotelRoom1View8SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 56, 500, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 64, 487, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 72, 475, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 425, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, 96, 425, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 96, 425, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 96, 425, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 96, 425, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 425, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 425, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 80, 462, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 80, 462, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 80, 462, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 80, 462, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 0, 80, 462, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 40, 80, 462, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 64, 475, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -120, 72, 475, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 72, 475, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 72, 475, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 72, 475, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 72, 475, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 64, 487, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 64, 487, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 64, 487, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, 64, 487, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 56, 500, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 56, 500, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, 48, 537, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 500, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 48, 537, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, -16, 1075, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, -16, 1075, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, -8, 1042, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 0, -8, 1042, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -144, 48, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 48, 1000, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -96, 48, 1006, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 48, 1012, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -144, 24, 1050, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, 24, 1050, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 24, 1050, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 24, 1050, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 24, 1025, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -144, 0, 1050, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, 0, 1050, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 0, 1050, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, 0, 1045, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 0, 1045, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -8, 1045, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -16, 1075, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -8, 1050, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, -8, 1062, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, -8, 1062, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 0, 1045, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, -8, 1045, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, -16, 1075, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 0, 1037, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 0, 1037, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 8, 1037, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 24, 1025, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 24, 1025, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 24, 1025, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 24, 1025, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 16, 887, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, 24, 900, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 48, 875, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -120, 48, 887, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 80, 862, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -120, 80, 875, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 40, 884, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 0, 1200, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, -120, 1152, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -120, 1375, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -40, 1311, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 0, 1327, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -24, 1450, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 120, -16, 1012, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -80, 1292, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -80, 1376, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, -40, 1126, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 104, -16, 1206, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -40, 1203, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 96, -80, 1125, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -64, 1099, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -80, 1064, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, -80, 960, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -80, 944, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -40, 925, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 120, -120, 985, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -120, 888, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -40, 987, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, -40, 992, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 0, 1000, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, 0, 921, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 0, 884, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -16, 891, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -40, 882, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, 16, 1050, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -24, 1075, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -16, 1075, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, 16, 1050, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, -16, 1150, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 8, 1125, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 8, 1125, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -24, 1150, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -24, 1150, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 80, -48, 1227, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, -40, 1230, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -24, 1267, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, -24, 1500, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
};

/// Mapped view 8's eight sprite ranges, preceded by a strip-background selector.
///
/// Ranges partition all 111 source elements; the last record ends traversal.
/// All ranges start visible and included in cached packets. The stored
/// `field_6` bytes retain serialized values whose purpose is unproven.
static SpriteBatch _gDryfieldNightMotelRoom1View8SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 31, .field_6 = { 6, 0 } },
    { .firstSprite = 31, .spriteCount = 33, .field_6 = { 1, 0 } },
    { .firstSprite = 64, .spriteCount = 6, .field_6 = { 4, 0 } },
    { .firstSprite = 70, .spriteCount = 28, .field_6 = { 0, 0 } },
    { .firstSprite = 98, .spriteCount = 4, .field_6 = { 5, 0 } },
    { .firstSprite = 102, .spriteCount = 5, .field_6 = { 3, 0 } },
    { .firstSprite = 107, .spriteCount = 3, .field_6 = { 7, 0 } },
    { .firstSprite = 110, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelRoom1View8SpriteSources) - 110, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 9's 64 texture rectangles and scene-sorting depths.
///
/// Positions and dimensions are pixels relative to the drawing origin;
/// depths are unscaled ordering values. The view's batches cover this entire
/// writable array, borrowed only while the room overlay is loaded.
static SpriteSource _gDryfieldNightMotelRoom1View9SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 96, 500, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 104, 500, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -72, 104, 500, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 112, 500, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 16, 750, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 72, 725, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 72, 725, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 64, 72, 737, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 72, 737, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 72, 750, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, 72, 750, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 48, 750, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 48, 750, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 64, 48, 750, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 48, 712, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 0, 48, 712, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, 48, 712, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 56, 725, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 24, 725, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 24, 725, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 24, 725, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 24, 725, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 0, 24, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -32, 24, 725, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 8, 750, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 8, 750, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 8, 750, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 64, 16, 750, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -24, 1175, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 0, 1175, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 0, 1175, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 24, 1175, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -24, 1175, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 16, 1175, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 112, 16, 1175, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -16, 1175, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 112, -16, 1175, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -24, 1175, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 88, 675, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 128, 72, 675, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 88, 675, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, 72, 675, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 40, 1005, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -120, 906, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -80, 906, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 925, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 0, 925, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 40, 925, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 0, 1003, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -40, 1000, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -80, 981, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -120, 981, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -104, 1043, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -80, 1043, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -40, 1047, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 0, 1050, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -32, 1125, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 0, 1137, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 0, 800, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 32, 800, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 64, 800, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -144, 64, 825, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 32, 825, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 16, 800, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
};

/// Mapped view 9's seven sprite ranges, preceded by a strip-background selector.
///
/// Ranges partition all 64 source elements; the last record ends traversal.
/// All ranges start visible and included in cached packets. The stored
/// `field_6` bytes retain serialized values whose purpose is unproven.
static SpriteBatch _gDryfieldNightMotelRoom1View9SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 4, .field_6 = { 3, 0 } },
    { .firstSprite = 4, .spriteCount = 24, .field_6 = { 4, 0 } },
    { .firstSprite = 28, .spriteCount = 4, .field_6 = { 1, 0 } },
    { .firstSprite = 32, .spriteCount = 6, .field_6 = { 5, 0 } },
    { .firstSprite = 38, .spriteCount = 4, .field_6 = { 0, 0 } },
    { .firstSprite = 42, .spriteCount = 16, .field_6 = { 6, 0 } },
    { .firstSprite = 58, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelRoom1View9SpriteSources) - 58, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gDryfieldNightMotelRoom1SpriteViews[9] = {
    { { .empty = _gDryfieldNightMotelRoom1View1SpriteBatches }, _gDryfieldNightMotelRoom1View1SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelRoom1View2SpriteBatches }, _gDryfieldNightMotelRoom1View2SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelRoom1View3SpriteBatches }, _gDryfieldNightMotelRoom1View3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom1View4SpriteSources }, _gDryfieldNightMotelRoom1View4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom1View5SpriteSources }, _gDryfieldNightMotelRoom1View5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom1View6SpriteSources }, _gDryfieldNightMotelRoom1View6SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelRoom1View7SpriteBatches }, _gDryfieldNightMotelRoom1View7SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom1View8SpriteSources }, _gDryfieldNightMotelRoom1View8SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom1View9SpriteSources }, _gDryfieldNightMotelRoom1View9SpriteBatches, NULL },
};

/// Directed view boundaries for logical-view pairs 2/3, 2/4, 4/5 and 5/6.
///
/// Each direction has its own quad; the eighth record carries LAST and is live.
/// Origins/corners use room units and normals use ONE per unit. Room binding
/// writes coordinates, links, flags and hit latches; storage stays mutable
/// until the active view-boundary list is cleared before unloading the room.
static WorldCollisionTrigger _gDryfieldNightMotelRoom1ViewBoundaries[8] = {
    { NULL, NULL, NULL, { 2527, -1344, 2927, 0 }, { { 22, 1888, 1715, 0 }, { 22, -1888, 1715, 0 }, { -23, 1888, -1716, 0 }, { -23, -1888, -1716, 0 } }, { -4104, 0, 53, 0 }, { 0, 0, ONE, 0 }, 2547, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2368, -1232, 2880, 0 }, { { -23, 1808, -1716, 0 }, { -23, -1808, -1716, 0 }, { 22, 1808, 1715, 0 }, { 22, -1808, 1715, 0 } }, { 4108, 0, -55, 0 }, { 0, 0, ONE, 0 }, 2482, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4047, -1264, 4559, 0 }, { { -995, 1776, -5, 0 }, { -995, -1776, -5, 0 }, { 995, 1776, 5, 0 }, { 995, -1776, 5, 0 } }, { 19, 0, -4105, 0 }, { 0, 0, ONE, 0 }, 2031, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4095, -1248, 4463, 0 }, { { 1011, 1792, -11, 0 }, { 1011, -1792, -11, 0 }, { -1011, 1792, 11, 0 }, { -1011, -1792, 11, 0 } }, { 43, 0, 4095, 0 }, { 0, 0, ONE, 0 }, 2048, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3200, -1280, 5632, 0 }, { { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 }, { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 } }, { 4101, 0, 19, 0 }, { 0, 0, ONE, 0 }, 2031, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3296, -1344, 5824, 0 }, { { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 }, { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 } }, { -4105, 0, -22, 0 }, { 0, 0, ONE, 0 }, 2031, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1920, -1312, 5472, 0 }, { { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 }, { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 } }, { -4105, 0, -22, 0 }, { 0, 0, ONE, 0 }, 2031, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1760, -1344, 5504, 0 }, { { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 }, { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 } }, { 4101, 0, 19, 0 }, { 0, 0, ONE, 0 }, 2031, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Four point lights shading models in every view of night motel room 1.
///
/// Both room lighting records borrow this writable array while the overlay is
/// loaded. Coordinate updates attach the view parent and compose each position;
/// shading queries overwrite attenuation. Positions and falloff radii use integer
/// world units, and RGB intensities have 12 fractional bits (`ONE` is 1.0).
static WorldCoordPointLight _gDryfieldNightMotelRoom1PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2285, -1064, 468 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 6144, 5816, 5652 },
        },
        .inner = 527,
        .outer = 1437,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1480, -1967, 6047 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2400, 2800, 3200 },
        },
        .inner = 690,
        .outer = 3545,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2512, -1113, 2201 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2900, 2800, 2700 },
        },
        .inner = 1140,
        .outer = 3771,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2510, -1531, 2650 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 1100, 1200, 1200 },
        },
        .inner = 572,
        .outer = 5081,
    },
};

/// One model-shading cone light and retained bytes of unproven purpose.
///
/// The light contributes in every view. Lighting borrows and mutates only
/// `coneLights`: position and falloff distances use world units, RGB uses Q12,
/// and the full cone opening uses 4096 angle units per turn. The adjacent bytes
/// are outside the live light count; their type and relationship are unproven.
static _DryfieldNightMotelRoom1SpotLightStorage _gDryfieldNightMotelRoom1SpotLightStorage = {
    .coneLights = {
        {
            .head = {
                .transform = { .lighting = {
                                   .composeStamp = GRAPHICS_COORD_DIRTY,
                                   .local        = { { { -4102, 0, -2 }, { -2, 0, 4102 }, { 0, 4101, 0 } }, { 2512, -2274, 2403 } },
                                   .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                   .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               } },
                .color     = { 1311, 1269, 1208 },
            },
            .axis  = { -1, 4095, 0, 0 },
            .inner = 2320,
            .outer = 3500,
            .angle = 932,
        },
    },
    .unknown_6C = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x04,
        0x00,
        0x00,
        0x00,
        0x18,
        0x1D,
        0x18,
        0x80,
        0x01,
        0x00,
        0x00,
        0x00,
        0x98,
        0x1E,
        0x00,
        0x00,
        0x68,
        0x1F,
        0x18,
        0x80,
        0xB0,
        0x39,
        0x11,
        0x80,
        0x98,
        0xB7,
        0x06,
        0x80,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0x01,
        0x00,
        0x00,
        0x00,
        0x04,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xF0,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x4D,
        0x04,
        0x00,
        0x00,
        0x02,
        0x12,
        0x62,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x1C,
        0x1F,
        0x18,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x01,
        0xC0,
        0xFF,
        0x10,
        0x0D,
        0x00,
        0x00,
        0x20,
        0xFF,
        0x00,
        0x00,
        0x90,
        0xFE,
        0x00,
        0x00,
        0xE0,
        0x00,
        0x00,
        0x00,
        0x90,
        0xFE,
        0x00,
        0x00,
        0x20,
        0xFF,
        0x00,
        0x00,
        0x70,
        0x01,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0xA7,
        0x12,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x0C,
        0x20,
        0x18,
        0x80,
        0x74,
        0x1F,
        0x00,
        0x00,
        0x70,
        0xB7,
        0x06,
        0x80,
        0x90,
        0xF3,
        0x00,
        0x00,
        0xE0,
        0x0E,
        0x00,
        0x00,
        0xF0,
        0xF7,
        0x30,
        0xEF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xF0,
        0xF7,
        0xD0,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x08,
        0xD0,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xEA,
        0xEF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x08,
        0x30,
        0xEF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0xF7,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x13,
        0x10,
        0x00,
        0x00,
    },
};

/// Four point lights and one cone light shared by both room lighting records.
///
/// No directional lights are authored. All five lights contribute in every
/// view; coordinate binding and attenuation updates require writable storage.
/// The collection and its borrowed arrays remain live with the loaded overlay.
static WorldCoordRoomLights _gDryfieldNightMotelRoom1RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMotelRoom1PointLights),
    .pointLights           = _gDryfieldNightMotelRoom1PointLights,
    .coneLightCount        = ARRAY_SIZE(_gDryfieldNightMotelRoom1SpotLightStorage.coneLights),
    .coneLights            = _gDryfieldNightMotelRoom1SpotLightStorage.coneLights,
};

/// Current door slot 1 and Main Street arrival slot 2, packed high/low nibble.
enum { DRYFIELD_NIGHT_MOTEL_ROOM_1_DOOR_WARP_SLOTS = (1 << 4) | 2 };

/// Motel-door warp and six facing-gated CAP interactions for night room 1.
///
/// Warp requests area 2, departure slot 1 and destination slot 2. CAP indices
/// are 4, 8, 6, 7, 6 and 1 with default presentation flags. The seventh record
/// is live and carries LAST. Room-space geometry uses ONE-scaled normals;
/// binding and contact handling mutate links, coordinates, flags and latches.
/// Keep this storage loaded until the active action-trigger list is cleared.
static WorldCollisionTrigger _gDryfieldNightMotelRoom1ActionTriggers[7] = {
    { NULL, NULL, NULL, { 4704, -48, 3568, 0 }, { { -416, 0, -592, 0 }, { 416, 0, -592, 0 }, { -416, 0, 592, 0 }, { 416, 0, 592, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 721, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, DRYFIELD_NIGHT_MOTEL_ROOM_1_DOOR_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 384, -64, 3152, 0 }, { { -224, 0, -368, 0 }, { 224, 0, -368, 0 }, { -224, 0, 368, 0 }, { 224, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 430, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1984, -64, 3760, 0 }, { { -640, 0, -416, 0 }, { 160, 0, -416, 0 }, { -640, 0, 416, 0 }, { 1120, 0, 416, 0 } }, { 0, 4105, 0, 0 }, { 995, 0, -3973, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 688, -64, 3616, 0 }, { { -496, 0, -304, 0 }, { 496, 0, -304, 0 }, { -496, 0, 304, 0 }, { 496, 0, 304, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -ONE, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2656, -64, 5664, 0 }, { { -416, 0, -304, 0 }, { 416, 0, -304, 0 }, { -416, 0, 304, 0 }, { 416, 0, 304, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -ONE, 0 }, 512, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 5664, 0 }, { { -416, 0, -304, 0 }, { 416, 0, -304, 0 }, { -416, 0, 304, 0 }, { 416, 0, 304, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -ONE, 0 }, 512, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4160, -64, 5568, 0 }, { { -736, 0, -304, 0 }, { 736, 0, -304, 0 }, { -736, 0, 304, 0 }, { 736, 0, 304, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// One two-sided sight-blocking quad in night motel room 1.
///
/// Room-space origin/corners use game units and the normal uses ONE per unit.
/// The only record is live and carries LAST; low flag bit 0 remains unproven.
/// Binding mutates links and enable state. Keep the room loaded until unlinked.
static WorldCollisionOccluder _gDryfieldNightMotelRoom1SightOccluders[1] = {
    { NULL, NULL, { 1760, -1408, 4512, 0 }, { { -1632, -2432, 0, 0 }, { 1632, -2432, 0, 0 }, { -1632, 2432, 0, 0 }, { 1632, 2432, 0, 0 } }, { 0, 0, -4098, 0 }, 2918, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Placement-entry keys and decimal file numbers for this room's resource recipes.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_SCAVENGER     = 16,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_SCAVENGER         = 16,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_BLOOD_SUCKLER = 40,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_BLOOD_SUCKLER     = 40,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_MOTH          = 8,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_MOTH              = 7,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_SCORPION      = 25,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_SCORPION          = 25,
};

/// Scavenger and Blood Suckler resource recipes for placement variant 1.
///
/// Keys match `gMapDryfieldFullMotelRoom1Variant1Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gDryfieldNightMotelRoom1Variant1Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_SCAVENGER, .fileNumber = DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_SCAVENGER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_BLOOD_SUCKLER, .fileNumber = DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_BLOOD_SUCKLER, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor204000BloodSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Moth and Scorpion resource recipes for placement variant 2.
///
/// Keys match `gMapDryfieldFullMotelRoom1Variant2Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gDryfieldNightMotelRoom1Variant2Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_MOTH, .fileNumber = DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_MOTH, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor00700MothTaskDesc },
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_SCORPION, .fileNumber = DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_SCORPION, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = gActor02500Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Scavenger resource recipes for placement variant 21.
///
/// Keys match `gMapDryfieldFullMotelRoom1Variant21Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gDryfieldNightMotelRoom1Variant21Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_1_RESOURCE_SCAVENGER, .fileNumber = DRYFIELD_NIGHT_MOTEL_ROOM_1_FILE_SCAVENGER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightMotelRoom1AreaVariants[22] = {
    { NULL, NULL },
    { gMapDryfieldFullMotelRoom1Variant1Placements, _gDryfieldNightMotelRoom1Variant1Resources },
    { gMapDryfieldFullMotelRoom1Variant2Placements, _gDryfieldNightMotelRoom1Variant2Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { gMapDryfieldFullMotelRoom1Variant21Placements, _gDryfieldNightMotelRoom1Variant21Resources },
};

/// Cue-2 script bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

/// Footstep cue bases for room surface class 1.
///
/// Walking and scripted jumps share one pair; running selects the next pair.
/// Playback adds one for cue 1 and 100 for companion entries. Surface records
/// borrow these encoded requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldNightMotelRoom1Class1FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_1_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_1_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_1_WALK_SOUND,
};

/// Footstep cue bases for room surface class 2.
///
/// Walking and scripted jumps share one pair; running selects the next pair.
/// Playback adds one for cue 1 and 100 for companion entries. Surface records
/// borrow these encoded requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldNightMotelRoom1Class2FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_2_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_2_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_ROOM_1_CLASS_2_WALK_SOUND,
};

/// Blocking, impact-enabled, silent collision policy for classes 0 and 4..7.
///
/// Grid contacts apply pushback. The retained `field_0` byte has no established
/// interpretation; the class directory borrows this record while loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom1DefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking, impact-enabled collision policy with surface class 1's footstep cues.
///
/// Grid contacts apply pushback. The retained `field_0` byte has no established
/// interpretation; the class directory borrows this record while loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom1Class1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom1Class1FootstepSounds,
};

/// Blocking, impact-enabled collision policy with surface class 2's footstep cues.
///
/// Grid contacts apply pushback. The retained `field_0` byte has no established
/// interpretation; the class directory borrows this record while loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom1Class2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom1Class2FootstepSounds,
};

/// Probe-transparent, impact-disabled, silent collision policy for surface class 3.
///
/// Grid contacts still apply pushback. The retained `field_0` byte has no
/// established interpretation; the class directory borrows this record while loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom1Class3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom1SurfacePropertiesByClass[8] = {
    &_gDryfieldNightMotelRoom1DefaultSurfaceProperties,
    &_gDryfieldNightMotelRoom1Class1SurfaceProperties,
    &_gDryfieldNightMotelRoom1Class2SurfaceProperties,
    &_gDryfieldNightMotelRoom1Class3SurfaceProperties,
    &_gDryfieldNightMotelRoom1DefaultSurfaceProperties,
    &_gDryfieldNightMotelRoom1DefaultSurfaceProperties,
    &_gDryfieldNightMotelRoom1DefaultSurfaceProperties,
    &_gDryfieldNightMotelRoom1DefaultSurfaceProperties,
};

void dryfieldNightMotelRoom1DrawGlowsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_ROOM_1_LARGE_FLARE_TEXTURE_INDEX = 1,
        DRYFIELD_NIGHT_MOTEL_ROOM_1_SMALL_FLARE_TEXTURE_INDEX = 2,
        DRYFIELD_NIGHT_MOTEL_ROOM_1_LARGE_FLARE_RADIUS_SCALE  = 0x200,
        DRYFIELD_NIGHT_MOTEL_ROOM_1_SMALL_FLARE_RADIUS_SCALE  = 0x180,
    };
    u8 mappedViewIndex;

    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
        case 3:
        case 8:
        case 9:
            glowDrawFlareClipped(&_gDryfieldNightMotelRoom1LargeFlareWorldPoint, DRYFIELD_NIGHT_MOTEL_ROOM_1_LARGE_FLARE_TEXTURE_INDEX,
                                 DRYFIELD_NIGHT_MOTEL_ROOM_1_LARGE_FLARE_RADIUS_SCALE);
            break;
        case 5:
        case 6:
            glowDrawFlareClipped(&_gDryfieldNightMotelRoom1SmallFlareWorldPoint, DRYFIELD_NIGHT_MOTEL_ROOM_1_SMALL_FLARE_TEXTURE_INDEX,
                                 DRYFIELD_NIGHT_MOTEL_ROOM_1_SMALL_FLARE_RADIUS_SCALE);
            break;
    }
}
