#include "rooms/dryfield_breezeway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "dryfield_breezeway_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_pickup.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */

/// The message and request the event gate latched, and the descriptor of the
/// event task it spawns to act on them.
extern RoomEventMsg D_dryfield_breezeway_8018439C;
extern RoomEventReq D_dryfield_breezeway_801843AC;

/// Handle of the room's key-item event task, which
/// `func_dryfield_breezeway_8017DC3C` spawns from
/// `D_dryfield_breezeway_80182E18` in its state 0 and drops again once
/// `Task_PollKill` reaps it; `func_dryfield_breezeway_8017DDB0` clears it when
/// the message task starts. `func_dryfield_breezeway_8017D90C` forwards message
/// 0x13F1 to it through `Gp_DispatchMsg`, answering 0 while there is none.
extern Task* D_dryfield_breezeway_801843A8;

/// Raised by the room's event gate `func_dryfield_breezeway_8017D638` when it
/// latched a request and spawned the event task, cleared on every other call.
extern u8 D_dryfield_breezeway_801843A4;

extern GpAreaTmdRec D_dryfield_breezeway_80184268[3];
extern GpAreaTmdRec D_dryfield_breezeway_8018428C[2];
extern GpAreaTmdRec D_dryfield_breezeway_801842A4[3];

extern GpGridParams   D_dryfield_breezeway_80183628[1];
extern GpObj4C        D_dryfield_breezeway_80183DE4[4];
extern GpObj4C        D_dryfield_breezeway_80183F14[5];
extern GpRoomCoordSet D_dryfield_breezeway_80184250[1];
extern TaskDesc       D_8014D8A4;

u_long D_dryfield_breezeway_80182F44[128] = {
    0x430000,
    0x8A70465,
    0x154C0CE9,
    0x218E196D,
    0x2E1229D0,
    0x3A543633,
    0x5B1946B6,
    0x6F9D635B,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

GpImgRec D_dryfield_breezeway_80183144[2] = {
    { 0, 0, { 0, 256, 256, 1 }, D_dryfield_breezeway_80182F44 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u8 D_dryfield_breezeway_80183164[8] = {
    70,
    70,
    171,
    250,
    9,
    11,
    0,
    0,
};

GpRoomObjRec D_dryfield_breezeway_8018316C[1] = {
    { D_dryfield_breezeway_80183628, D_dryfield_breezeway_80183DE4, D_dryfield_breezeway_80183F14, NULL },
};

u8* D_dryfield_breezeway_8018317C[1] = {
    D_8010CAF8,
};

GpRoomCoordRec D_dryfield_breezeway_80183180[1] = {
    { D_dryfield_breezeway_80184250, NULL },
};

GpViewCountRec D_dryfield_breezeway_80183188[2] = {
    { { .bytes = { 6, 0 } } },
    { { .bytes = { 0, 0 } } },
};

GpWarpRec D_dryfield_breezeway_8018318C[2] = {
    { { .words = { 1024, 6656, 1, 1568 } }, { 0, 0, 0, 0 }, { .words = { 1024, 6656, 1, 1568 } }, { 0, 0, 0, 0 }, 0x52160002, 0x52160001, 0, 2, 0, 476 },
    { { .words = { 3072, 0x44F3, 1, 2075 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x44F3, 1, 2075 } }, { 0, 0, 0, 0 }, 0x52160004, 0x52160003, 0x52160005, 4, 0, 475 },
};

SVECTOR D_dryfield_breezeway_801831FC[16] = {
#include "assets/dryfield_breezeway_collision_06068_normals.inc"
};

SVECTOR D_dryfield_breezeway_8018327C[46] = {
#include "assets/dryfield_breezeway_collision_06068_verts.inc"
};

GpGridFace D_dryfield_breezeway_801833EC[24] = {
#include "assets/dryfield_breezeway_collision_06068_faces.inc"
};

s16 D_dryfield_breezeway_8018350C[126] = {
#include "assets/dryfield_breezeway_collision_06068_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_breezeway_8018350C[i])
s16* D_dryfield_breezeway_80183608[8] = {
#include "assets/dryfield_breezeway_collision_06068_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_breezeway_80183628[1] = {
    { NULL, D_dryfield_breezeway_801831FC, D_dryfield_breezeway_8018327C, D_dryfield_breezeway_801833EC, D_dryfield_breezeway_80183608, -5000, 1000, 4, 2, 4000, 24 },
};

GpViewRec D_dryfield_breezeway_8018364C[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2EE0, 0x5DC0, 0 } }, 322 },
    { { { { 0, 0, 4096 }, { -1081, 3950, 0 }, { -3950, -1081, 0 } }, { -0x38A4, 200, -1600 } }, 257 },
    { { { { 0, 0, -4095 }, { 831, 4010, 0 }, { 4010, -831, 0 } }, { -9800, 300, -1600 } }, 257 },
    { { { { 1090, 0, -3948 }, { 702, 4030, 194 }, { 3885, -729, 1072 } }, { -0x35CA, 500, -1150 } }, 257 },
    { { { { 1589, 0, -3775 }, { -1365, 3818, -575 }, { 3519, 1481, 1481 } }, { -0x459C, 1450, -2780 } }, 257 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { -0x41AC, 295, -3354 } }, 680 },
};

GpSprtCmd D_dryfield_breezeway_80183724[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_breezeway_80183734[7] = {
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 32, 16, 1000, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, 16, 1000, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, 16, 1000, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 32, 56, 750, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 40, 56, 750, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 48, 56, 750, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, 16, 64, 750, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_breezeway_801837C0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 1, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_breezeway_801837E0[31] = {
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -160, -120, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -160, 0, 750, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -144, -120, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -144, 0, 750, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -128, -120, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -128, 0, 750, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -112, -120, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -112, 0, 750, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -96, -120, 750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -96, 0, 750, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -80, -120, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -80, 0, 750, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, -120, 750, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -64, -56, 750, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -120, 750, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, 24, 750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, 48, 750, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -32, 56, 750, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 250, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 250, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 250, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 32, 250, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 250, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -96, 48, 250, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 80, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -80, 24, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 250, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, 32, 250, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 80, 250, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, 56, 250, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, 80, 404, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_breezeway_80183A4C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 13, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_breezeway_80183A6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_breezeway_80183A7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_breezeway_80183A8C[38] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -88, 296, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, 0, 293, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, 0, 259, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -88, 298, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -88, 298, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -112, 0, 298, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, -88, 298, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -96, 0, 298, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -88, 298, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -80, 0, 298, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -64, 0, 298, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -88, 298, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -48, -88, 298, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -48, 0, 297, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -32, 0, 297, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -32, -88, 298, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -88, 298, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -16, 0, 298, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -88, 298, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 0, 0, 298, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -88, 298, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 16, 0, 298, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -88, 298, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 32, 0, 298, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 48, -88, 298, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, 0, 298, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 64, -88, 298, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, 0, 298, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 80, -88, 297, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, 0, 298, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, -88, 301, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 96, -56, 298, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 0, 298, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, -32, 292, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, -88, 301, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 112, 0, 298, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 128, -88, 298, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, 0, 298, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_breezeway_80183D84[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 38, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_breezeway_80183D9C[6] = {
    { { .empty = D_dryfield_breezeway_80183724 }, D_dryfield_breezeway_80183724, NULL },
    { { .elements = D_dryfield_breezeway_80183734 }, D_dryfield_breezeway_801837C0, NULL },
    { { .elements = D_dryfield_breezeway_801837E0 }, D_dryfield_breezeway_80183A4C, NULL },
    { { .empty = D_dryfield_breezeway_80183A6C }, D_dryfield_breezeway_80183A6C, NULL },
    { { .empty = D_dryfield_breezeway_80183A7C }, D_dryfield_breezeway_80183A7C, NULL },
    { { .elements = D_dryfield_breezeway_80183A8C }, D_dryfield_breezeway_80183D84, NULL },
};

GpObj4C D_dryfield_breezeway_80183DE4[4] = {
    { NULL, NULL, NULL, { 0x2E60, -2880, 1440, 0 }, { { 0, -3568, -1024, 0 }, { 0, -3568, 1024, 0 }, { 0, 3568, -1024, 0 }, { 0, 3568, 1024, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x2F20, -2816, 1472, 0 }, { { 0, -3280, 1024, 0 }, { 0, -3280, -1024, 0 }, { 0, 3280, 1024, 0 }, { 0, 3280, -1024, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3434, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x3E6D, -2848, 2218, 0 }, { { -235, -3632, 1596, 0 }, { 223, -3632, -1607, 0 }, { -235, 3632, 1596, 0 }, { 223, 3632, -1607, 0 } }, { -4064, 0, -582, 0 }, { 0, 0, 4096, 0 }, 3974, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x3E0D, -3120, 2186, 0 }, { { 211, -3392, -1637, 0 }, { -225, -3392, 1627, 0 }, { 211, 3392, -1637, 0 }, { -225, 3392, 1627, 0 } }, { 4067, 0, 543, 0 }, { 0, 0, 4096, 0 }, 3762, 0, 4, 3, 129, 0 },
};

GpObj4C D_dryfield_breezeway_80183F14[5] = {
    { NULL, NULL, NULL, { 6416, -48, 1680, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 807, 0, 20, 18, 2, 0 },
    { NULL, NULL, NULL, { 0x4580, -64, 1760, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4091, 0, 201, 0 }, 807, 0, 23, 34, 2, 0 },
    { NULL, NULL, NULL, { 0x42B0, -64, 3584, 0 }, { { -992, 0, -560, 0 }, { 992, 0, -560, 0 }, { -992, 0, 560, 0 }, { 992, 0, 560, 0 } }, { 0, 4115, 0, 0 }, { 4096, 0, 0, 0 }, 1137, 2, 3, 255, 3, 0 },
    { NULL, NULL, NULL, { 0x4580, -64, 3024, 0 }, { { -368, 0, -512, 0 }, { 368, 0, -512, 0 }, { -368, 0, 512, 0 }, { 368, 0, 512, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 630, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 0x3C5B, -64, 1807, 0 }, { { 32, 0, -1792, 0 }, { 608, 0, -1792, 0 }, { -608, 0, 1792, 0 }, { -32, 0, 1792, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1889, 0x8005, 1, 0, 131, 0 },
};

GpLight D_dryfield_breezeway_80184090[4] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2500, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1028, 1028, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 0, 0, 0, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 0, 0, 0, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2500, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 208, 208, 208, { 0, 0 } },
};

GpPointLight D_dryfield_breezeway_801841F0[1] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4049, -1500, 3608 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1536, 4096 },
};

GpRoomCoordSet D_dryfield_breezeway_80184250[1] = {
    { 4, D_dryfield_breezeway_80184090, 1, D_dryfield_breezeway_801841F0, 0, NULL },
};

GpAreaTmdRec D_dryfield_breezeway_80184268[3] = {
    { 101, 234, 2, 0, { 0, 0 }, D_8017120C },
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_breezeway_8018428C[2] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_breezeway_801842A4[3] = {
    { 101, 234, 2, 0, { 0, 0 }, D_8017120C },
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_breezeway_801842C8[3] = {
    { 101, 0, 0, 0x41D5, 0, 2735, -1400, 0, 0, 2, 0 },
    { 1, 0, 0, 0x41D5, 0, 2735, -1400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_dryfield_breezeway_801842F8[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B674, D_dryfield_breezeway_80184268 },
    { NULL, NULL },
    { D_map_dryfield_8017B6A4, D_dryfield_breezeway_8018428C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_breezeway_801842C8, D_dryfield_breezeway_801842A4 },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_dryfield_breezeway_80184360[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

GpRoomParamRec D_dryfield_breezeway_8018436C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_breezeway_80184374[1] = {
    { 0, 0, 1, 0, D_dryfield_breezeway_80184360 },
};

GpRoomParamRec* D_dryfield_breezeway_8018437C[8] = {
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_80184374,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
};

RoomEventMsg D_dryfield_breezeway_8018439C = { 0 };

u8 D_dryfield_breezeway_801843A4 = 0;

Task* D_dryfield_breezeway_801843A8 = NULL;

RoomEventReq D_dryfield_breezeway_801843AC = { 0 };

Task* D_dryfield_breezeway_801843C0;

static s32  func_dryfield_breezeway_8017D638(RoomEventReq* req, RoomEventMsg* msg);
static void func_dryfield_breezeway_8017DDB0(Task* task);
static void func_dryfield_breezeway_8017DE60(Task* task);

/// The room's event gate, called by `func_dryfield_breezeway_8017D940` with the
/// request it builds on the stack. A set flag nibble (or a clear one, for a
/// negative `flagId`) means the event has already happened and the answer is
/// 1; a missing collected-bit prerequisite runs the request's `field_4` CAP
/// command and answers 0; otherwise the request and message are latched into
/// `D_dryfield_breezeway_801843AC` / `D_dryfield_breezeway_8018439C`, the flag
/// nibble is written, the event task is spawned and
/// `D_dryfield_breezeway_801843A4` is raised, for 2. A non-zero `field_5` on
/// the message asks what would happen and suppresses all of those effects.
static s32 func_dryfield_breezeway_8017D638(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                          = req->flagId;
    D_dryfield_breezeway_801843A4 = 0;
    neg                           = flag < 0;
    got                           = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_breezeway_8018439C = *msg;
                D_dryfield_breezeway_801843AC = *req;
                id                            = req->flagId;
                mode                          = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_breezeway_80181DD4, 0, 0, 0);
                D_dryfield_breezeway_801843A4 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task `D_dryfield_breezeway_80181DD4` describes, spawned by the gate
/// above once it has latched a request: it runs the request's CAP command,
/// plays and waits out its two sounds (`field_8`, then `field_C`, either
/// skipped when zero), then writes the latched message's destination into the
/// save's location and spawns the room-change task, killing itself.
void func_dryfield_breezeway_8017D79C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_breezeway_801843AC.field_0);
            if (D_dryfield_breezeway_801843AC.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_breezeway_801843AC.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_breezeway_801843AC.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_breezeway_801843AC.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_breezeway_801843AC.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_breezeway_801843AC.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_dryfield_breezeway_8018439C.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_dryfield_breezeway_8018439C.field_2;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_dryfield_breezeway_8018439C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_dryfield_breezeway_8017D90C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 ret;

    if (D_dryfield_breezeway_801843A8 == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(D_dryfield_breezeway_801843A8, msgId, arg2, arg3);
    }
    return ret;
}

/// `GpMsgEntry` handler for message 0x13EE, the room's own progress gate. It
/// answers message 0x17 by writing 1 or 2 into the outgoing record's `field_3`
/// from the room's progress nibble 0x47, and - when the message id still reads
/// 0x17 on a second look - hands the room's event request (flag nibble 0x37,
/// item 0x15) to the room's event gate `func_dryfield_breezeway_8017D638`,
/// returning its answer.
/// A gate that latched the request is followed by the room's own follow-up:
/// progress nibble 0x56 set to 4 and effect 0xA2. Everything else answers 1.
s32 func_dryfield_breezeway_8017D940(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    if (in->prefix.packed == 0x17) {
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(0x47) == 0) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
        }
        if (in->prefix.packed == 0x17) {
            req.field_0 = 4;
            req.field_4 = 2;
            req.field_8 = 0x52160006;
            req.field_C = 0x52160003;
            req.flagId  = 0x37;
            req.itemId  = 0x15;
            ret         = func_dryfield_breezeway_8017D638(&req, out);
            if (D_dryfield_breezeway_801843A4 != 0) {
                GameFlag_SetNibble(0x56, 4);
                func_800E3FAC(0xA2, 0x38);
            }
            return ret;
        }
    }
    return 1;
}

s32 func_dryfield_breezeway_8017DA48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 1:
            if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                Gp_RunCapCmd1(5);
            } else {
                if (GameFlag_GetNibble(0x56) != 4) {
                    if (Gp_HasCollectedBit(0x115) != 0) {
                        GameFlag_SetNibble(0x56, 3);
                    } else if (GameFlag_GetNibble(0xFE) != 0) {
                        if (Gp_HasCollectedBit(0x11B) == 0) {
                            if (GameFlag_GetNibble(0x56) != 6) {
                                GameFlag_SetNibble(0x56, 5);
                            }
                        } else {
                            GameFlag_SetNibble(0x56, 2);
                        }
                    }
                }
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(D_dryfield_breezeway_80181E10, 1, arg2, 0);
            }
            break;
        case 3:
            if (GameFlag_GetNibble(0x56) >= 2) {
                if (Gp_StateF0.prefix.bytes.field_0 != 1) {
                    if (Gp_GetCurBit2Flag(6) == 1) {
                        Task_SpawnFromTable(D_dryfield_breezeway_80181E10, 0, 0, 0);
                        GameFlag_SetNibble(0xFE, 1);
                    }
                } else {
                    Gp_RunCapCmd1(5);
                }
            }
            break;
    }
    return 0;
}

s32 func_dryfield_breezeway_8017DBA4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        SndEvt_EnqueueType6(0x52160000 | 7, 0, 0);
    }
    return 0;
}

/// `GpMsgEntry` handler for message 0x13EF, the room's hotspot gate: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 0x5D so
/// a repeat visit does nothing. Only the incoming record is read - the handler
/// answers 0 and never edits the outgoing copy.
s32 func_dryfield_breezeway_8017DBD8(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    if (GameFlag_GetNibble(0x5D) == 0 && in->field_2 == 1) {
        GameFlag_SetNibble(0x5D, 1);
        Task_SpawnFromTable(D_dryfield_breezeway_801820B0, 1, 0, 0);
    }
    return 0;
}

/// The breezeway's room task, spawned from the room data table. State 0 arms
/// the room: it silences the two weapon displays and spawns the secondary task
/// `D_dryfield_breezeway_80182E18` describes, keeping the handle so state 1 can
/// reap it. State 1 polls that child and, once it is gone, drops the handle and
/// kills the room task with it.
void func_dryfield_breezeway_8017DC3C(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_dryfield_breezeway_801843A8 = Task_SpawnFromTable(&D_dryfield_breezeway_80182E18, 0, 0, 0);
            arg0->state                  += 1;
            return;
        case 1:
            if (Task_PollKill(D_dryfield_breezeway_801843A8, &sp10) != 0) {
                D_dryfield_breezeway_801843A8 = NULL;
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_breezeway_8017DCE4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd(task->spawnArg1.value, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (GameFlag_GetNibble(0x56) != 4) {
                    if (Gp_GetCapEventKey() == 0xB) {
                        GameFlag_SetNibble(0x56, 2);
                    }
                    if (GameFlag_GetNibble(0x56) == 5) {
                        GameFlag_SetNibble(0x56, 6);
                    }
                }
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
    }
}

static void func_dryfield_breezeway_8017DDB0(Task* task)
{
    GpCmdArg msg;

    task->msgTable = D_dryfield_breezeway_80181DE0;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0x5D) == 0) {
        msg.from.loc.stage = gGameSession->at4.loc.stage;
        msg.from.loc.area  = gGameSession->at4.loc.area;
        msg.command        = 0;
        Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
        Task_SpawnFromTable(D_dryfield_breezeway_801820B0, 0, 0, 0);
    }
    task->state++;
    D_dryfield_breezeway_801843A8 = NULL;
}

static void func_dryfield_breezeway_8017DE60(Task* task)
{
}

/// State handlers of the room's message task, indexed by its state through
/// `func_dryfield_breezeway_8017DE68`: publish the message table, idle, then
/// kill.
static const TaskFuncTable3 D_dryfield_breezeway_8017D5DC = {
    { func_dryfield_breezeway_8017DDB0, func_dryfield_breezeway_8017DE60, taskKill }
};

/// Runs the room's message task through its three states: publishing the
/// room's message table (`func_dryfield_breezeway_8017DDB0`), idling
/// (`func_dryfield_breezeway_8017DE60`) and `taskKill`. The table is copied
/// onto the stack first, so the call goes through a local copy rather than the
/// rodata.
void func_dryfield_breezeway_8017DE68(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_breezeway_8017D5DC;
    sp.funcs[task->state](task);
}
