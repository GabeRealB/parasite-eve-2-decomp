#include "rooms/dryfield_r04.h"

#include "types.h"

#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

/* The records after Dryfield room 4's collision grid: its two views, their
 * empty sprite lists and the location's parameters.
 */

GpViewRec D_dryfield_r04_8017E218[2] = {
    { { { { 0x1000, 0, 0 }, { 0, 0, -0x1000 }, { 0, 0x1000, 0 } }, { 0, 0x7530, 0 } }, 0xCF },
    { { { { 0x1000, 0, 0 }, { 0, 0, -0x1000 }, { 0, 0x1000, 0 } }, { -0x3A98, 0x7530, -0x3E80 } }, 0xCF },
};

static GpSprtCmd D_dryfield_r04_8017E260[2] = {
    { 0, 0, 0, 0, { 0 } },
    { 0xFFFF, 0, 0, 0, { 0 } },
};

static GpSprtCmd D_dryfield_r04_8017E270[2] = {
    { 0, 0, 0, 0, { 0 } },
    { 0xFFFF, 0, 0, 0, { 0 } },
};

GpSprtRec D_dryfield_r04_8017E280[2] = {
    { (GpSprtElem*)D_dryfield_r04_8017E260, D_dryfield_r04_8017E260, NULL },
    { (GpSprtElem*)D_dryfield_r04_8017E270, D_dryfield_r04_8017E270, NULL },
};

/// Three base sound ids, of the kind `GpRoomParamRec.field_4` points at. The
/// room's parameter record does not use them.
static s32 D_dryfield_r04_8017E298[3] = { 0x10000011, 0x10000013, 0x10000011 };

static GpRoomParamRec D_dryfield_r04_8017E2A4 = { 0, 0, 1, 0, NULL };

GpRoomParamRec* D_dryfield_r04_8017E2AC[8] = {
    &D_dryfield_r04_8017E2A4,
    &D_dryfield_r04_8017E2A4,
    &D_dryfield_r04_8017E2A4,
    &D_dryfield_r04_8017E2A4,
    &D_dryfield_r04_8017E2A4,
    &D_dryfield_r04_8017E2A4,
    &D_dryfield_r04_8017E2A4,
    &D_dryfield_r04_8017E2A4,
};
