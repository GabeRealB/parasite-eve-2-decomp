/* Dryfield room 4 has no code. Its package holds only the room records the
 * stage tables point at: one location, whose collision grid is an asset between
 * this unit and the next, two views with no sprites, an exit record and the
 * location's parameters.
 */
#include "common.h"
#include "rooms/stage_tables.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/room.h"

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
