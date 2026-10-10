#include "rooms/dryfield_r04.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/view.h"

static WorldCollisionGrid _gDryfieldR04CollisionGrid;

/* Dryfield room 4 has no code. Its package holds only the room records the
 * stage tables point at: one location with its collision grid, two views with
 * no sprites, an exit record and the location's parameters.
 */

WorldCollisionRoomResources gDryfieldR04RoomCollisionResources[1] = {
    { &_gDryfieldR04CollisionGrid, NULL, NULL, NULL },
};

u8* gDryfieldR04ViewMaps[1] = { gViewIdentityMap };

ViewCount gDryfieldR04RoomViewCounts[1] = { 2 };

/// Two retained null words with the layout of a room-lighting record.
///
/// No C or assembly consumer establishes their role or subdivision. The stage
/// lighting directory supplies NULL for this area; these words are unused.
static WorldCoordRoomLighting D_dryfield_r04_8017D5DC = { NULL, NULL };

DirectionWarpEntry gDryfieldR04WarpEntries[1] = {
    { .arrivalSound = DIRECTION_WARP_SOUND_NONE, .departureSound = DIRECTION_WARP_SOUND_NONE, .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 2, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gDryfieldR04CollisionGridNormals[1] = {
#include "assets/dryfield_r04_collision_00C34_normals.inc"
};

/// Collision vertices in signed room-coordinate units.
static SVECTOR _gDryfieldR04CollisionGridVerts[81] = {
#include "assets/dryfield_r04_collision_00C34_verts.inc"
};

/// Indexed collision faces referencing the grid's vertex and normal pools.
static WorldCollisionGridFace _gDryfieldR04CollisionGridFaces[64] = {
#include "assets/dryfield_r04_collision_00C34_faces.inc"
};

/// Face-index lists for the grid cells, ending with `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldR04CollisionGridCells[642] = {
#include "assets/dryfield_r04_collision_00C34_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldR04CollisionGridCells[i])
/// X-major 9-by-9 directory of the grid's face-index lists.
static s16* _gDryfieldR04CollisionGridTable[81] = {
#include "assets/dryfield_r04_collision_00C34_table.inc"
};
#undef GRID_CELL

/// Mutable room collision mesh with a 9-by-9 index of 4000-unit XZ cells.
///
/// The origin is (0, 0). Room activation binds the initially NULL view transform;
/// the room overlay owns the descriptor and every mesh/index pool.
static WorldCollisionGrid _gDryfieldR04CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldR04CollisionGridNormals,
    .vertices    = _gDryfieldR04CollisionGridVerts,
    .faces       = _gDryfieldR04CollisionGridFaces,
    .cellFaceIds = _gDryfieldR04CollisionGridTable,
    .xBias       = 0,
    .zBias       = 0,
    .cellCountX  = 9,
    .cellCountZ  = 9,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldR04CollisionGridFaces),
};
