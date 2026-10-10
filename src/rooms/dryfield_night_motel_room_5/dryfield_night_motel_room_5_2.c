#include "rooms/dryfield_night_motel_room_5.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_night_motel_room_5_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"
#include "../../shared/glow_draw.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc.
extern SVECTOR D_dryfield_night_motel_room_5_8017DA58[1];
extern SVECTOR D_dryfield_night_motel_room_5_8017DA60[1];
extern SVECTOR D_dryfield_night_motel_room_5_8017DA68[1];

static WorldCollisionGrid     _gDryfieldNightMotelRoom5CollisionGrid;
extern WorldCollisionOccluder D_dryfield_night_motel_room_5_80180ED8[2];
extern WorldCollisionTrigger  D_dryfield_night_motel_room_5_80180A18[8];
extern WorldCollisionTrigger  D_dryfield_night_motel_room_5_80180C78[8];
static WorldCoordRoomLights   _gDryfieldNightMotelRoom5Lights;

SVECTOR D_dryfield_night_motel_room_5_8017DA58[1] = {
    { 400, -970, 1590, 0 },
};

SVECTOR D_dryfield_night_motel_room_5_8017DA60[1] = {
    { 3360, -2340, 2250, 0 },
};

SVECTOR D_dryfield_night_motel_room_5_8017DA68[1] = {
    { 4550, -1140, 5590, 0 },
};

WorldCoordRoomLighting gDryfieldNightMotelRoom5RoomLighting[2] = {
    { &_gDryfieldNightMotelRoom5Lights, NULL },
    { &_gDryfieldNightMotelRoom5Lights, NULL },
};

WorldCollisionRoomResources D_dryfield_night_motel_room_5_8017DA80[2] = {
    { &_gDryfieldNightMotelRoom5CollisionGrid, D_dryfield_night_motel_room_5_80180A18, D_dryfield_night_motel_room_5_80180C78, D_dryfield_night_motel_room_5_80180ED8 },
    { &_gDryfieldNightMotelRoom5CollisionGrid, D_dryfield_night_motel_room_5_80180A18, D_dryfield_night_motel_room_5_80180C78, D_dryfield_night_motel_room_5_80180ED8 },
};

/// Room 2's eleven logical-view indices into the shared camera/image/sprite slots.
///
/// The first byte keeps slot 1; logical views 2..6 select slots 7..11 and
/// views 7..11 select slots 2..6. The retained twelfth zero byte is outside
/// the valid logical-view domain and is not a map terminator.
static u8 _gDryfieldNightMotelRoom5Room2ViewMap[12] = {
    1,
    7,
    8,
    9,
    10,
    11,
    2,
    3,
    4,
    5,
    6,
    0,
};

u8* gDryfieldNightMotelRoom5RoomViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldNightMotelRoom5Room2ViewMap,
};

ViewCount D_dryfield_night_motel_room_5_8017DAB4[2] = { 11, 11 };

DirectionWarpEntry D_dryfield_night_motel_room_5_8017DAB8[1] = {
    { { { .word = 3072 }, 4350, 0, 2426 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4350, 0, 2426 }, { 0, 0, 0, 0 }, 0x531C0002, 0x531C0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 464 },
};

/// Collision-grid normals in room axes, with 4096 representing unit length.
static SVECTOR _gDryfieldNightMotelRoom5CollisionGridNormals[11] = {
#include "assets/dryfield_night_motel_room_5_collision_00A7C_normals.inc"
};

/// Collision-grid vertices in signed room-coordinate units.
static SVECTOR _gDryfieldNightMotelRoom5CollisionGridVerts[85] = {
#include "assets/dryfield_night_motel_room_5_collision_00A7C_verts.inc"
};

/// Indexed collision quads selecting room-local surface classes 0..3.
static WorldCollisionGridFace _gDryfieldNightMotelRoom5CollisionGridFaces[32] = {
#include "assets/dryfield_night_motel_room_5_collision_00A7C_faces.inc"
};

/// Packed, -1-terminated face-index lists for the four grid cells.
static s16 _gDryfieldNightMotelRoom5CollisionGridCells[94] = {
#include "assets/dryfield_night_motel_room_5_collision_00A7C_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightMotelRoom5CollisionGridCells[i])
/// X-major 2-by-2 directory into the packed cell lists.
static s16* _gDryfieldNightMotelRoom5CollisionGridTable[4] = {
#include "assets/dryfield_night_motel_room_5_collision_00A7C_table.inc"
};
#undef GRID_CELL

/// Collision mesh shared by both variants of night motel room 5.
///
/// Four 4000-unit XZ cells start at room coordinates (150, 0). Faces index
/// the 85 vertices and eleven Q12 normals. Activation borrows the current
/// view coordinate; the descriptor and pools stay live while this overlay is loaded.
static WorldCollisionGrid _gDryfieldNightMotelRoom5CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelRoom5CollisionGridNormals,
    .vertices    = _gDryfieldNightMotelRoom5CollisionGridVerts,
    .faces       = _gDryfieldNightMotelRoom5CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMotelRoom5CollisionGridTable,
    .xBias       = -150,
    .zBias       = 0,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelRoom5CollisionGridFaces),
};

ViewCamera D_dryfield_night_motel_room_5_8017E060[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x2C10, -3000 } }, 289 },
    { { { { 828, 0, -4011 }, { -323, 4082, -66 }, { 3998, 330, 825 } }, { -300, 1280, -900 } }, 235 },
    { { { { 725, 0, 4031 }, { 300, 4084, -54 }, { -4019, 305, 723 } }, { -4800, 1280, -900 } }, 235 },
    { { { { 4029, 0, 732 }, { 75, 4074, -415 }, { -728, 422, 4008 } }, { -4100, 1380, -500 } }, 246 },
    { { { { 4048, 0, -622 }, { -374, 3272, -2434 }, { 497, 2463, 3234 } }, { -800, 2580, -2900 } }, 246 },
    { { { { -4048, 0, 622 }, { 335, 3452, 2178 }, { -524, 2204, -3411 } }, { -1600, 2280, -5800 } }, 246 },
    { { { { 828, 0, -4011 }, { -323, 4082, -66 }, { 3998, 330, 825 } }, { -300, 1280, -900 } }, 235 },
    { { { { 725, 0, 4031 }, { 300, 4084, -54 }, { -4019, 305, 723 } }, { -4800, 1280, -900 } }, 235 },
    { { { { 4029, 0, 732 }, { 75, 4074, -415 }, { -728, 422, 4008 } }, { -4100, 1380, -500 } }, 246 },
    { { { { 4048, 0, -622 }, { -374, 3272, -2434 }, { 497, 2463, 3234 } }, { -800, 2580, -2900 } }, 246 },
    { { { { -4048, 0, 622 }, { 335, 3452, 2178 }, { -524, 2204, -3411 } }, { -1600, 2280, -5800 } }, 246 },
};

/// Empty resource-view 1 batch list retaining decoded background image strips.
///
/// The zero-count first record is followed by SPRITE_BATCH_END. Its address
/// also occupies the view's empty source slot and is never read as SpriteSource.
static SpriteBatch _gDryfieldNightMotelRoom5View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Resource-view 2's 41 sprite rectangles, in draw-origin pixels with authored depths.
static SpriteSource _gDryfieldNightMotelRoom5View2SpriteSources[41] = {
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 96, 450, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 96, 450, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 450, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 450, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 96, 450, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 80, 500, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 0, 80, 500, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, 80, 500, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 80, 80, 500, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 80, 500, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 72, 537, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 72, 537, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, 72, 537, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 72, 537, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 64, 550, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, 64, 550, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 64, 550, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 32, 625, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 537, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 537, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 537, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 537, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 40, 537, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 537, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 575, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 40, 575, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 0, 575, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -40, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -80, 575, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 575, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 40, 625, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 80, 625, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 80, 450, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 40, 450, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 16, 450, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 64, 475, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 80, 475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 56, 962, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 56, 962, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 32, 962, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 8, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

/// Resource-view 2's four sprite ranges following its image-strip selector.
///
/// Ranges partition all 41 source elements; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gDryfieldNightMotelRoom5View2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 3, 0 } },
    { 17, 15, 0, 0, { 0, 0 } },
    { 32, 5, 0, 0, { 2, 0 } },
    { 37, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Resource-view 3's 68 sprite rectangles, in draw-origin pixels with authored depths.
static SpriteSource _gDryfieldNightMotelRoom5View3SpriteSources[68] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 32, 862, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, 8, 1000, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 8, 1000, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 24, 862, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -56, 24, 850, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 40, 775, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 40, 775, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -96, 40, 775, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, 40, 775, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -136, 56, 700, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -96, 56, 700, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -56, 56, 700, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -16, 56, 587, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 80, 587, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -56, 80, 587, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -96, 80, 587, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -136, 80, 600, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 80, 600, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 88, 425, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 96, 425, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 104, 375, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 96, 400, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 88, 425, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, -120, 709, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 96, -80, 709, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, -40, 693, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 0, 709, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 96, 40, 709, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 72, 709, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 72, 812, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 72, 40, 764, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, 40, 946, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 0, 748, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -40, 748, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -80, 748, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -120, 764, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 898, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -40, 898, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -80, 914, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, -104, 946, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, -96, 1125, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -80, 1077, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -40, 1061, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 1077, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, 40, 1105, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -48, 687, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 687, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 655, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 655, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 671, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 40, 671, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 80, 687, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 80, 687, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 40, 687, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 0, 687, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -40, 687, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 0, 900, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 40, 900, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 64, 40, 800, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 64, 0, 800, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, 8, 1275, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 48, 8, 1062, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 1275, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 48, 48, 1062, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 48, 1300, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 48, 1250, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, 8, 1300, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 8, 1250, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
};

/// Resource-view 3's seven sprite ranges following its image-strip selector.
///
/// Ranges partition all 68 source elements; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gDryfieldNightMotelRoom5View3SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 3, 0 } },
    { 18, 5, 0, 0, { 4, 0 } },
    { 23, 22, 0, 0, { 1, 0 } },
    { 45, 11, 0, 0, { 5, 0 } },
    { 56, 4, 0, 0, { 0, 0 } },
    { 60, 4, 0, 0, { 6, 0 } },
    { 64, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Resource-view 4's 40 sprite rectangles, in draw-origin pixels with authored depths.
static SpriteSource _gDryfieldNightMotelRoom5View4SpriteSources[40] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, -120, 675, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -120, 675, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -80, 659, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -40, 675, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, 0, 675, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 40, 675, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 80, 659, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 80, 831, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -120, 831, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -80, 815, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -40, 831, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 0, 831, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 40, 815, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 40, 962, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, 40, 1011, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 0, 946, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 0, 1011, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -40, 946, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -40, 995, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -80, 930, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -120, 962, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -104, 1075, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -80, 1011, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -96, 1175, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -80, 1175, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -64, 1250, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 32, 1375, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -88, 1375, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -88, 1450, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -48, 1375, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -48, 1450, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -8, 1375, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -8, 1450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 32, 1450, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 665, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -80, 665, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 665, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 665, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 681, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -160, 80, 665, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

/// Resource-view 4's three sprite ranges following its image-strip selector.
///
/// Ranges partition all 40 source elements; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gDryfieldNightMotelRoom5View4SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 1, 0 } },
    { 26, 8, 0, 0, { 2, 0 } },
    { 34, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Resource-view 5's 37 sprite rectangles, in draw-origin pixels with authored depths.
static SpriteSource _gDryfieldNightMotelRoom5View5SpriteSources[37] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -120, 550, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -120, 525, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -120, 500, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -80, 475, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 64, 487, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 48, 453, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 80, 492, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 40, 436, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 80, 527, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 40, 423, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 80, 551, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 24, 573, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 8, 548, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, 40, 435, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, 80, 535, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 0, 389, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 40, 471, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 80, 491, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 40, 442, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 80, 454, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 8, 377, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, -8, 512, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 8, 370, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, -32, 487, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -40, 315, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 375, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 88, 375, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 88, 375, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 48, 375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 16, 375, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 48, 375, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 8, 375, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, -24, 375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -48, 375, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -88, 48, 587, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -88, 32, 625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -88, 24, 650, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
};

/// Resource-view 5's three sprite ranges following its image-strip selector.
///
/// Ranges partition all 37 source elements; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gDryfieldNightMotelRoom5View5SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 1, 0 } },
    { 25, 9, 0, 0, { 2, 0 } },
    { 34, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Resource-view 6's 66 sprite rectangles, in draw-origin pixels with authored depths.
static SpriteSource _gDryfieldNightMotelRoom5View6SpriteSources[66] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, -48, 550, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 425, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -80, 425, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -40, 462, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 0, 475, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -152, 40, 475, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 80, 475, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -120, 425, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -80, 425, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -40, 425, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 0, 487, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 40, 500, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 80, 500, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 80, 562, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 40, 487, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 0, 487, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -80, 437, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -40, 450, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -40, 550, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 0, 550, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 40, 562, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -120, 437, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, -8, 562, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, -120, 562, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -96, -120, 562, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 562, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -80, 562, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -40, 562, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -40, 562, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 0, 625, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -96, 0, 625, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -56, 0, 525, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -96, 40, 637, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -56, 40, 637, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 80, 650, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 80, 650, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, 56, 494, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, 72, 486, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 72, 449, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 112, 88, 440, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 88, 431, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 120, 104, 507, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 662, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, -120, 712, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -40, -120, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 0, -120, 725, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 40, -120, 725, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -120, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -112, -80, 712, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -80, 737, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -40, -80, 787, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 0, -80, 825, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 40, -80, 800, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 40, -40, 850, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 0, -40, 862, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 40, 0, 931, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 0, 931, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -40, -40, 862, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 0, 931, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 24, 931, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 812, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 0, 862, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 24, 925, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 0, 837, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -40, 775, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
};

/// Resource-view 6's four sprite ranges following its image-strip selector.
///
/// Ranges partition all 66 source elements; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gDryfieldNightMotelRoom5View6SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 3, 0 } },
    { 23, 14, 0, 0, { 0, 0 } },
    { 37, 6, 0, 0, { 2, 0 } },
    { 43, 23, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Shared depth clip for resource views 6 and 11.
///
/// Clips drawing to (0, 102, 320, 137) in draw-buffer pixels, restoring the
/// full buffer at unscaled sorting depth 800. SPRITE_DRAW_AREA_END ends the list.
static SpriteDrawArea _gDryfieldNightMotelRoom5View6And11SpriteDrawAreas[2] = {
    { { 0, 102, 320, 137 }, 800 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Resource-view 7's 41 sprite rectangles, in draw-origin pixels with authored depths.
static SpriteSource _gDryfieldNightMotelRoom5View7SpriteSources[41] = {
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 96, 450, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 96, 450, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 450, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 450, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 96, 450, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 80, 500, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 0, 80, 500, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, 80, 500, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 80, 80, 500, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 80, 500, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 72, 537, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 72, 537, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, 72, 537, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 72, 537, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, 64, 550, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, 64, 550, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 64, 550, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 32, 625, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 537, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 537, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 537, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 537, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 40, 537, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 537, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 575, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 40, 575, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 0, 575, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -40, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -80, 575, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 575, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 40, 625, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 80, 625, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 80, 450, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 40, 450, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 16, 450, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 64, 475, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 80, 475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 56, 962, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 56, 962, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 32, 962, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 8, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_5_8017F9EC[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 3, 0 } },
    { 17, 15, 0, 0, { 0, 0 } },
    { 32, 5, 0, 0, { 2, 0 } },
    { 37, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_5_8017FA1C[68] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 32, 862, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 8, 1000, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 8, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 24, 862, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 24, 850, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 40, 775, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -16, 40, 775, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 40, 775, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, 40, 775, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -136, 56, 700, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -96, 56, 700, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -56, 56, 700, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -16, 56, 587, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 80, 587, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -56, 80, 587, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -96, 80, 587, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, 80, 600, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 80, 600, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 88, 425, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 96, 425, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 104, 375, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 96, 400, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 88, 425, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, -120, 725, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, -80, 725, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, -40, 725, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 0, 725, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 96, 40, 725, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 72, 725, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 72, 812, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 72, 40, 812, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, 40, 962, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 0, 812, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -40, 812, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -80, 812, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 72, -120, 812, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 962, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -40, 962, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -80, 962, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, -104, 962, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -96, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -80, 1125, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -40, 1125, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 1125, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 40, 1137, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -48, 687, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 687, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 687, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 687, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 687, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 40, 687, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 120, 80, 687, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 80, 687, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 40, 687, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 0, 687, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -40, 687, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 0, 900, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 40, 900, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 64, 40, 800, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 64, 0, 800, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, 8, 1275, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 48, 8, 1062, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 1275, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 48, 48, 1062, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 48, 1300, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 48, 48, 1250, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, 8, 1300, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 8, 1250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_5_8017FF6C[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 3, 0 } },
    { 18, 5, 0, 0, { 4, 0 } },
    { 23, 22, 0, 0, { 1, 0 } },
    { 45, 11, 0, 0, { 5, 0 } },
    { 56, 4, 0, 0, { 0, 0 } },
    { 60, 4, 0, 0, { 6, 0 } },
    { 64, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_5_8017FFB4[40] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, -120, 675, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -120, 675, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -80, 675, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -40, 675, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, 0, 675, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 40, 675, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 80, 675, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 80, 831, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -120, 831, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -80, 831, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -40, 831, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 0, 831, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 40, 831, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 40, 962, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 40, 1075, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 0, 962, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 0, 1075, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -40, 962, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -40, 1075, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -80, 962, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -120, 962, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -104, 1075, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -80, 1075, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -96, 1175, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -80, 1175, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -64, 1250, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 32, 1375, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -88, 1375, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -88, 1450, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -48, 1375, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -48, 1450, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -8, 1375, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -8, 1450, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 32, 1450, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 681, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -80, 681, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 681, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 681, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 681, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -160, 80, 681, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_5_801802D4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 1, 0 } },
    { 26, 8, 0, 0, { 2, 0 } },
    { 34, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_5_801802FC[23] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -120, 550, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -120, 525, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -120, 500, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 612, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 40, 600, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 612, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 40, 600, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 550, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 0, 550, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -40, 550, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -80, 475, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 375, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 88, 375, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 88, 375, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 48, 375, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 16, 375, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 48, 375, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 8, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, -24, 375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -48, 375, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -88, 48, 587, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -88, 32, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -88, 24, 650, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_5_801804C8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 9, 0, 0, { 2, 0 } },
    { 20, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_5_801804F0[57] = {
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -136, -120, 575, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -160, -120, 475, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, -120, 650, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -88, 475, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -136, -88, 575, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -48, 475, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -136, -48, 575, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -88, 650, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -48, 650, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, -48, 725, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -8, 475, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, -8, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, -8, 650, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -88, 725, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -96, -8, 725, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, 40, 475, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -136, 40, 575, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 40, 650, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 40, 725, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 80, 574, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -136, 80, 575, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 650, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 40, 637, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, -120, 562, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -96, -120, 562, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, -80, 562, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -96, -80, 562, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -48, 562, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -48, 562, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -104, -8, 625, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -96, -8, 625, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, -8, 625, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, 40, 637, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 40, 637, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 80, 650, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 80, 664, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, 56, 494, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, 72, 486, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, 88, 440, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 88, 431, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 72, 449, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 104, 507, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 32 } }, -104, -64, 912, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 32 } }, -16, -64, 912, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 40 } }, -96, -32, 912, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 40 } }, -16, -32, 912, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -88, 8, 912, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -16, 8, 912, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -80, 24, 912, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -120, -120, 912, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -64, -120, 912, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -16, -120, 912, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 40, -120, 912, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -112, -96, 912, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -64, -96, 912, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -16, -96, 912, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 32, -96, 912, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_5_80180964[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 14, 0, 0, { 0, 0 } },
    { 36, 6, 0, 0, { 2, 0 } },
    { 42, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_motel_room_5_80180994[11] = {
    { { .empty = _gDryfieldNightMotelRoom5View1SpriteBatches }, _gDryfieldNightMotelRoom5View1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom5View2SpriteSources }, _gDryfieldNightMotelRoom5View2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom5View3SpriteSources }, _gDryfieldNightMotelRoom5View3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom5View4SpriteSources }, _gDryfieldNightMotelRoom5View4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom5View5SpriteSources }, _gDryfieldNightMotelRoom5View5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom5View6SpriteSources }, _gDryfieldNightMotelRoom5View6SpriteBatches, _gDryfieldNightMotelRoom5View6And11SpriteDrawAreas },
    { { .elements = _gDryfieldNightMotelRoom5View7SpriteSources }, D_dryfield_night_motel_room_5_8017F9EC, NULL },
    { { .elements = D_dryfield_night_motel_room_5_8017FA1C }, D_dryfield_night_motel_room_5_8017FF6C, NULL },
    { { .elements = D_dryfield_night_motel_room_5_8017FFB4 }, D_dryfield_night_motel_room_5_801802D4, NULL },
    { { .elements = D_dryfield_night_motel_room_5_801802FC }, D_dryfield_night_motel_room_5_801804C8, NULL },
    { { .elements = D_dryfield_night_motel_room_5_801804F0 }, D_dryfield_night_motel_room_5_80180964, _gDryfieldNightMotelRoom5View6And11SpriteDrawAreas },
};

WorldCollisionTrigger D_dryfield_night_motel_room_5_80180A18[8] = {
    { NULL, NULL, NULL, { 2320, -1296, 1392, 0 }, { { 48, -1936, -1360, 0 }, { -48, -1936, 1360, 0 }, { 48, 1936, -1360, 0 }, { -48, 1936, 1360, 0 } }, { 4112, 0, 143, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2496, -1120, 1408, 0 }, { { -112, -2144, 1360, 0 }, { 112, -2144, -1360, 0 }, { -112, 2144, 1360, 0 }, { 112, 2144, -1360, 0 } }, { -4092, 0, -338, 0 }, { 0, 0, 4096, 0 }, 2534, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3663, -1120, 3199, 0 }, { { 1449, -2144, 275, 0 }, { -1451, -2144, -277, 0 }, { 1449, 2144, 275, 0 }, { -1451, 2144, -277, 0 } }, { -768, 0, 4032, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3711, -1104, 3295, 0 }, { { -1568, -2128, -307, 0 }, { 1566, -2128, 305, 0 }, { -1568, 2128, -307, 0 }, { 1566, 2128, 305, 0 } }, { 783, 0, -4021, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2206, -1104, 4927, 0 }, { { -43, -2128, -1006, 0 }, { 43, -2128, 1006, 0 }, { -43, 2128, -1006, 0 }, { 43, 2128, 1006, 0 } }, { 4115, 0, -178, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2302, -1072, 5023, 0 }, { { 43, -2096, 1006, 0 }, { -43, -2096, -1006, 0 }, { 43, 2096, 1006, 0 }, { -43, 2096, -1006, 0 } }, { -4120, 0, 176, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1183, -1072, 4446, 0 }, { { -1093, -2096, 109, 0 }, { 1093, -2096, -109, 0 }, { -1093, 2096, 109, 0 }, { 1093, 2096, -109, 0 } }, { -409, 0, -4081, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1184, -1072, 4320, 0 }, { { 1093, -2096, -109, 0 }, { -1093, -2096, 109, 0 }, { 1093, 2096, -109, 0 }, { -1093, 2096, 109, 0 } }, { 406, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_motel_room_5_80180C78[8] = {
    { NULL, NULL, NULL, { 4752, -64, 2592, 0 }, { { -432, 0, -800, 0 }, { 432, 0, -800, 0 }, { -432, 0, 800, 0 }, { 432, 0, 800, 0 } }, { 0, 4112, 0, 0 }, { -4096, 0, 0, 0 }, 907, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2624, -64, 3552, 0 }, { { -368, 0, -544, 0 }, { 368, 0, -544, 0 }, { -368, 0, 544, 0 }, { 368, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 655, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2864, -64, 4992, 0 }, { { -544, 0, -288, 0 }, { 544, 0, -288, 0 }, { -544, 0, 288, 0 }, { 544, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 613, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1392, -64, 1984, 0 }, { { -608, 0, -288, 0 }, { 608, 0, -288, 0 }, { -608, 0, 288, 0 }, { 608, 0, 288, 0 } }, { 0, 4099, 0, 0 }, { -601, 0, -4052, 0 }, 671, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 960, -64, 5120, 0 }, { { -608, 0, -288, 0 }, { 608, 0, -288, 0 }, { -608, 0, 288, 0 }, { 608, 0, 288, 0 } }, { 0, 4099, 0, 0 }, { 2439, 0, -3291, 0 }, 671, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 960, -64, 4576, 0 }, { { -608, 0, -288, 0 }, { 608, 0, -288, 0 }, { -608, 0, 288, 0 }, { 608, 0, 288, 0 } }, { 0, 4099, 0, 0 }, { 4076, 0, 399, 0 }, 671, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1056, -64, 1712, 0 }, { { -336, 0, -560, 0 }, { 336, 0, -560, 0 }, { -336, 0, 560, 0 }, { 336, 0, 560, 0 } }, { 0, 4107, 0, 0 }, { 4091, 0, -201, 0 }, 652, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 448, -64, 3776, 0 }, { { -336, 0, -560, 0 }, { 336, 0, -560, 0 }, { -336, 0, 560, 0 }, { 336, 0, 560, 0 } }, { 0, 4107, 0, 0 }, { 4091, 0, -201, 0 }, 652, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_night_motel_room_5_80180ED8[2] = {
    { NULL, NULL, { 1056, -1312, 2848, 0 }, { { -1248, -1696, 0, 0 }, { 1248, -1696, 0, 0 }, { -1248, 1696, 0, 0 }, { 1248, 1696, 0, 0 } }, { 0, 0, -4102, 0 }, 2095, 1, 0 },
    { NULL, NULL, { 2304, -1232, 3488, 0 }, { { 0, -1648, 672, 0 }, { 0, -1648, -672, 0 }, { 0, 1648, 672, 0 }, { 0, 1648, -672, 0 } }, { -4109, 0, 0, 0 }, 1778, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Four point lights shared by both room variants and enabled in every logical view.
///
/// Positions and falloff radii use integer room units; RGB uses 12 fractional
/// bits. The lighting task parents and composes these mutable coordinates,
/// and shading queries overwrite attenuation while the room overlay is loaded.
static WorldCoordPointLight _gDryfieldNightMotelRoom5PointLights[4] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 545, -1064, 1668 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 6144, 5816, 5652 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 527,
        .outer = 1437,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 620, -1967, 4727 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2400, 2800, 3200 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 690,
        .outer = 3545,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3392, -1113, 1961 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2900, 2800, 2700 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1140,
        .outer = 3771,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3390, -1531, 2410 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1100, 1200, 1200 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 572,
        .outer = 5081,
    },
};

/// One cone light shared by both room variants and enabled in every logical view.
///
/// Position and falloff radii use integer room units; axis and RGB use Q12.
/// The full cone opening is 921 units of a 4096-unit turn. Initialization
/// rebuilds the local rotation from the axis and borrows the current view parent.
static WorldCoordSpotLight _gDryfieldNightMotelRoom5ConeLights[1] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3392, -2274, 2163 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1311, 1269, 1208 },
            .unknown_56 = { 0, 0 },
        },
        .axis  = { -238, 4082, -226, 0 },
        .inner = 2320,
        .outer = 3500,
        .angle = 921,
    },
};

/// Shared room-light collection with four point lights, one cone and no directional lights.
///
/// Counts cover each complete mutable array. Both room-lighting descriptors
/// borrow this collection for the loaded room overlay's lifetime.
static WorldCoordRoomLights _gDryfieldNightMotelRoom5Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMotelRoom5PointLights),
    .pointLights           = _gDryfieldNightMotelRoom5PointLights,
    .coneLightCount        = ARRAY_SIZE(_gDryfieldNightMotelRoom5ConeLights),
    .coneLights            = _gDryfieldNightMotelRoom5ConeLights,
};

AreaResource D_dryfield_night_motel_room_5_80181154[3] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401800OddStrangerTaskDesc },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor300700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_motel_room_5_80181178[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101600ScavengerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_motel_room_5_80181190[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017C968, D_dryfield_night_motel_room_5_80181154 },
    { D_map_dryfield_full_8017C9C8, D_dryfield_night_motel_room_5_80181178 },
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
};

enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

/// Class 1's walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps use type-1 bank entries 0x35/0x36; running uses
/// 0x37/0x38. Animation cue 1 adds one to the base, and companions add 100.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelRoom5Class1FootstepSounds __attribute__((section(".data"))) = {
    .walk         = DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_1_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_1_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_1_WALK_CUE_BASE,
};

/// Class 2's walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps use type-1 bank entries 1/2; running uses 3/4.
/// Animation cue 1 adds one to the base, and companions add 100.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelRoom5Class2FootstepSounds __attribute__((section(".data"))) = {
    .walk         = DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_2_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_ROOM_5_CLASS_2_WALK_CUE_BASE,
};

/// Silent, probe-blocking properties shared by surface classes 0 and 4..7.
///
/// Weapon impacts and pushback remain enabled independently of footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom5DefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's blocking, impact-enabled surface with its own footstep cues.
///
/// Applies pushback and borrows the cue record for the room overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom5Class1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom5Class1FootstepSounds,
};

/// Class 2's blocking, impact-enabled surface with its own footstep cues.
///
/// Applies pushback and borrows the cue record for the room overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom5Class2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom5Class2FootstepSounds,
};

/// Class 3's silent surface, passing probes and ignoring weapon impacts.
///
/// Pushback remains enabled independently of probe and impact policy.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom5Class3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom5SurfacePropertiesByClass[8] = {
    &_gDryfieldNightMotelRoom5DefaultSurfaceProperties,
    &_gDryfieldNightMotelRoom5Class1SurfaceProperties,
    &_gDryfieldNightMotelRoom5Class2SurfaceProperties,
    &_gDryfieldNightMotelRoom5Class3SurfaceProperties,
    &_gDryfieldNightMotelRoom5DefaultSurfaceProperties,
    &_gDryfieldNightMotelRoom5DefaultSurfaceProperties,
    &_gDryfieldNightMotelRoom5DefaultSurfaceProperties,
    &_gDryfieldNightMotelRoom5DefaultSurfaceProperties,
};

void dryfieldNightMotelRoom5DrawFlareTask(Task* unusedTask)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_ROOM_5_FLARE_TEXTURE_COLUMN     = 1,
        DRYFIELD_NIGHT_MOTEL_ROOM_5_SMALL_FLARE_RADIUS_SCALE = 0x200,
        DRYFIELD_NIGHT_MOTEL_ROOM_5_LARGE_FLARE_RADIUS_SCALE = 0x240,
    };

    // Pixel half-extent is radiusScale * 39 / (camera Z / 4).
    switch (gGameSession->location.loc.view) {
        case 3:
        case 8:
            glowDrawFlareClipped(D_dryfield_night_motel_room_5_8017DA58, DRYFIELD_NIGHT_MOTEL_ROOM_5_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_MOTEL_ROOM_5_SMALL_FLARE_RADIUS_SCALE);
            break;
        case 2:
        case 7:
            glowDrawFlareClipped(D_dryfield_night_motel_room_5_8017DA60, DRYFIELD_NIGHT_MOTEL_ROOM_5_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_MOTEL_ROOM_5_LARGE_FLARE_RADIUS_SCALE);
            break;
        case 4:
        case 9:
            glowDrawFlareClipped(D_dryfield_night_motel_room_5_8017DA68, DRYFIELD_NIGHT_MOTEL_ROOM_5_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_MOTEL_ROOM_5_SMALL_FLARE_RADIUS_SCALE);
            break;
    }
}
