#include "rooms/dryfield_r04.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/room.h"
#include "gameplay/view.h"

/// The location's collision grid header, defined after the grid it points at.
extern GpGridParams D_dryfield_r04_8017E1F4;

/* Dryfield room 4 has no code. Its package holds only the room records the
 * stage tables point at: one location with its collision grid, two views with
 * no sprites, an exit record and the location's parameters.
 */

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

SVECTOR D_dryfield_r04_8017D61C[1] = {
#include "assets/dryfield_r04_collision_00C34_normals.inc"
};

SVECTOR D_dryfield_r04_8017D624[81] = {
#include "assets/dryfield_r04_collision_00C34_verts.inc"
};

GpGridFace D_dryfield_r04_8017D8AC[64] = {
#include "assets/dryfield_r04_collision_00C34_faces.inc"
};

s16 D_dryfield_r04_8017DBAC[642] = {
#include "assets/dryfield_r04_collision_00C34_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_r04_8017DBAC[i])
s16* D_dryfield_r04_8017E0B0[81] = {
#include "assets/dryfield_r04_collision_00C34_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_r04_8017E1F4 = {
    NULL,
    D_dryfield_r04_8017D61C,
    D_dryfield_r04_8017D624,
    D_dryfield_r04_8017D8AC,
    D_dryfield_r04_8017E0B0,
    0,
    0,
    9,
    9,
    4000,
    64,
};
