#include "rooms/dryfield_r04.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/room.h"
#include "gameplay/view.h"

/// The location's collision grid header; the grid is an asset.
extern GpGridParams D_dryfield_r04_8017E1F4;

/* Dryfield room 4 has no code. Its package holds only the room records the
 * stage tables point at: one location, whose collision grid is retained in this unit, two views with no sprites, an exit record and the
 * location's parameters.
 */

// Native 9x9 collision grid. Every cell points into the bounded face-ID
// pool and ends at -1; faces index the vertex and normal arrays below.
typedef struct {
    SVECTOR    normals[1];
    SVECTOR    vertices[81];
    GpGridFace faces[64];
    s16        faceIds[642];
    s16*       cells[81];
} DryfieldR04CollisionGrid;
STATIC_ASSERT_SIZEOF(DryfieldR04CollisionGrid, 3032);

GpRoomObjRec D_dryfield_r04_8017D5C4[1] = {
    { &D_dryfield_r04_8017E1F4, NULL, NULL, NULL },
};

u8* D_dryfield_r04_8017D5D4[1] = { D_8010CAF8 };

GpViewCountRec D_dryfield_r04_8017D5D8[1] = { { { { 2, 0 } } } };

/// Nothing points at this record: the stage's room coordinate table has no
/// entry for this room.
static GpRoomCoordRec D_dryfield_r04_8017D5DC = { NULL, NULL };

GpWarpRec D_dryfield_r04_8017D5E4[1] = {
    { .field_34 = 2 },
};

DryfieldR04CollisionGrid D_dryfield_r04_8017D61C = {
    {
#include "assets/dryfield_r04_collision_00C34_normals.inc"
    },
    {
#include "assets/dryfield_r04_collision_00C34_verts.inc"
    },
    {
#include "assets/dryfield_r04_collision_00C34_faces.inc"
    },
    {
#include "assets/dryfield_r04_collision_00C34_cells.inc"
    },
    {
#define GRID_CELL(i) (&D_dryfield_r04_8017D61C.faceIds[i])
#include "assets/dryfield_r04_collision_00C34_table.inc"
#undef GRID_CELL
    },
};

GpGridParams D_dryfield_r04_8017E1F4 = {
    NULL,
    D_dryfield_r04_8017D61C.normals,
    D_dryfield_r04_8017D61C.vertices,
    D_dryfield_r04_8017D61C.faces,
    D_dryfield_r04_8017D61C.cells,
    0,
    0,
    9,
    9,
    4000,
    64,
};
