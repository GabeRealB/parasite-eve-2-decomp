#include "rooms/dryfield_breezeway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_breezeway_private.h"

#include "actors/task_tables.h"

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
#include "../../shared/room_events.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */

/// The message and request the event gate latched, and the descriptor of the
/// event task it spawns to act on them.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Handle of the room's key-item event task, which
/// `func_dryfield_breezeway_8017DC3C` spawns from
/// `D_dryfield_breezeway_80182E18` in its state 0 and drops again once
/// `taskPollKill` reaps it; `func_dryfield_breezeway_8017DDB0` clears it when
/// the message task starts. `dryfieldBreezewayForwardKeyItemUse` forwards message
/// 0x13F1 to it through `taskMessageDispatch`, answering 0 while there is none.
extern Task* D_dryfield_breezeway_801843A8;

/// Raised by the room's event gate `_roomEventGate` when it
/// latched a request and spawned the event task, cleared on every other call.
extern u8 gRoomEventActive;

extern AreaResource D_dryfield_breezeway_80184268[3];
extern AreaResource D_dryfield_breezeway_8018428C[2];
extern AreaResource D_dryfield_breezeway_801842A4[3];

extern WorldCollisionGrid    D_dryfield_breezeway_80183628[1];
extern WorldCollisionTrigger D_dryfield_breezeway_80183DE4[4];
extern WorldCollisionTrigger D_dryfield_breezeway_80183F14[5];
extern WorldCoordRoomLights  D_dryfield_breezeway_80184250[1];
extern TaskDesc              Actor00100_D1BA84;

u_long D_dryfield_breezeway_80182F44[128] = {
#include "assets/dryfield_breezeway_clut_05984.inc"
};

GpuImageUpload D_dryfield_breezeway_80183144[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 256, 256, 1 }, D_dryfield_breezeway_80182F44 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

SVECTOR D_dryfield_breezeway_80183164 = { 17990, -1365, 2825, 0 };

WorldCollisionRoomResources D_dryfield_breezeway_8018316C[1] = {
    { D_dryfield_breezeway_80183628, D_dryfield_breezeway_80183DE4, D_dryfield_breezeway_80183F14, NULL },
};

u8* D_dryfield_breezeway_8018317C[1] = {
    gViewIdentityMap,
};

WorldCoordRoomLighting D_dryfield_breezeway_80183180[1] = {
    { D_dryfield_breezeway_80184250, NULL },
};

ViewCount D_dryfield_breezeway_80183188[2] = { 6, 0 };

DirectionWarpEntry D_dryfield_breezeway_8018318C[2] = {
    { { { .word = 1024 }, 6656, 1, 1568 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 6656, 1, 1568 }, { 0, 0, 0, 0 }, 0x52160002, 0x52160001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 476 },
    { { { .word = 3072 }, 0x44F3, 1, 2075 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x44F3, 1, 2075 }, { 0, 0, 0, 0 }, 0x52160004, 0x52160003, 0x52160005, 4, DIRECTION_WARP_FLAG_NONE, 475 },
};

static SVECTOR _gDryfieldBreezewayCollision06068Normals[16] = {
#include "assets/dryfield_breezeway_collision_06068_normals.inc"
};

static SVECTOR _gDryfieldBreezewayCollision06068Verts[46] = {
#include "assets/dryfield_breezeway_collision_06068_verts.inc"
};

static WorldCollisionGridFace _gDryfieldBreezewayCollision06068Faces[24] = {
#include "assets/dryfield_breezeway_collision_06068_faces.inc"
};

static s16 _gDryfieldBreezewayCollision06068Cells[126] = {
#include "assets/dryfield_breezeway_collision_06068_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldBreezewayCollision06068Cells[i])
static s16* _gDryfieldBreezewayCollision06068Table[8] = {
#include "assets/dryfield_breezeway_collision_06068_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_breezeway_80183628[1] = {
    { NULL, _gDryfieldBreezewayCollision06068Normals, _gDryfieldBreezewayCollision06068Verts, _gDryfieldBreezewayCollision06068Faces, _gDryfieldBreezewayCollision06068Table, -5000, 1000, 4, 2, 4000, 24 },
};

ViewCamera D_dryfield_breezeway_8018364C[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2EE0, 0x5DC0, 0 } }, 322 },
    { { { { 0, 0, 4096 }, { -1081, 3950, 0 }, { -3950, -1081, 0 } }, { -0x38A4, 200, -1600 } }, 257 },
    { { { { 0, 0, -4095 }, { 831, 4010, 0 }, { 4010, -831, 0 } }, { -9800, 300, -1600 } }, 257 },
    { { { { 1090, 0, -3948 }, { 702, 4030, 194 }, { 3885, -729, 1072 } }, { -0x35CA, 500, -1150 } }, 257 },
    { { { { 1589, 0, -3775 }, { -1365, 3818, -575 }, { 3519, 1481, 1481 } }, { -0x459C, 1450, -2780 } }, 257 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { -0x41AC, 295, -3354 } }, 680 },
};

SpriteBatch D_dryfield_breezeway_80183724[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_breezeway_80183734[7] = {
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 32, 16, 1000, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, 16, 1000, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, 16, 1000, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 32, 56, 750, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 40, 56, 750, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 48, 56, 750, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, 16, 64, 750, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_breezeway_801837C0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_breezeway_801837E0[31] = {
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

SpriteBatch D_dryfield_breezeway_80183A4C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_breezeway_80183A6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_breezeway_80183A7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_breezeway_80183A8C[38] = {
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

SpriteBatch D_dryfield_breezeway_80183D84[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 38, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_breezeway_80183D9C[6] = {
    { { .empty = D_dryfield_breezeway_80183724 }, D_dryfield_breezeway_80183724, NULL },
    { { .elements = D_dryfield_breezeway_80183734 }, D_dryfield_breezeway_801837C0, NULL },
    { { .elements = D_dryfield_breezeway_801837E0 }, D_dryfield_breezeway_80183A4C, NULL },
    { { .empty = D_dryfield_breezeway_80183A6C }, D_dryfield_breezeway_80183A6C, NULL },
    { { .empty = D_dryfield_breezeway_80183A7C }, D_dryfield_breezeway_80183A7C, NULL },
    { { .elements = D_dryfield_breezeway_80183A8C }, D_dryfield_breezeway_80183D84, NULL },
};

WorldCollisionTrigger D_dryfield_breezeway_80183DE4[4] = {
    { NULL, NULL, NULL, { 0x2E60, -2880, 1440, 0 }, { { 0, -3568, -1024, 0 }, { 0, -3568, 1024, 0 }, { 0, 3568, -1024, 0 }, { 0, 3568, 1024, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2F20, -2816, 1472, 0 }, { { 0, -3280, 1024, 0 }, { 0, -3280, -1024, 0 }, { 0, 3280, 1024, 0 }, { 0, 3280, -1024, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3434, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3E6D, -2848, 2218, 0 }, { { -235, -3632, 1596, 0 }, { 223, -3632, -1607, 0 }, { -235, 3632, 1596, 0 }, { 223, 3632, -1607, 0 } }, { -4064, 0, -582, 0 }, { 0, 0, 4096, 0 }, 3974, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3E0D, -3120, 2186, 0 }, { { 211, -3392, -1637, 0 }, { -225, -3392, 1627, 0 }, { 211, 3392, -1637, 0 }, { -225, 3392, 1627, 0 } }, { 4067, 0, 543, 0 }, { 0, 0, 4096, 0 }, 3762, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_breezeway_80183F14[5] = {
    { NULL, NULL, NULL, { 6416, -48, 1680, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4580, -64, 1760, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4091, 0, 201, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_WARP, 23, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x42B0, -64, 3584, 0 }, { { -992, 0, -560, 0 }, { 992, 0, -560, 0 }, { -992, 0, 560, 0 }, { 992, 0, 560, 0 } }, { 0, 4115, 0, 0 }, { 4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4580, -64, 3024, 0 }, { { -368, 0, -512, 0 }, { 368, 0, -512, 0 }, { -368, 0, 512, 0 }, { 368, 0, 512, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 630, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3C5B, -64, 1807, 0 }, { { 32, 0, -1792, 0 }, { 608, 0, -1792, 0 }, { -608, 0, 1792, 0 }, { -32, 0, 1792, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1889, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordLight D_dryfield_breezeway_80184090[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2500, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2500, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
};

WorldCoordPointLight D_dryfield_breezeway_801841F0[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4049, -1500, 3608 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1536, 4096 },
};

WorldCoordRoomLights D_dryfield_breezeway_80184250[1] = {
    { ARRAY_SIZE(D_dryfield_breezeway_80184090), D_dryfield_breezeway_80184090, ARRAY_SIZE(D_dryfield_breezeway_801841F0), D_dryfield_breezeway_801841F0, 0, NULL },
};

AreaResource D_dryfield_breezeway_80184268[3] = {
    { 101, 234, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_323400_8017120C },
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_breezeway_8018428C[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_breezeway_801842A4[3] = {
    { 101, 234, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_323400_8017120C },
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_breezeway_801842C8[3] = {
    { 101, 0, 0, 0x41D5, 0, 2735, -1400, 0, 0, 2, 0 },
    { 1, 0, 0, 0x41D5, 0, 2735, -1400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_dryfield_breezeway_801842F8[13] = {
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

WorldCollisionFootstepSounds D_dryfield_breezeway_80184360 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionSurfaceProperties D_dryfield_breezeway_8018436C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_breezeway_80184374[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_breezeway_80184360 },
};

WorldCollisionSurfaceProperties* D_dryfield_breezeway_8018437C[8] = {
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_80184374,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
    D_dryfield_breezeway_8018436C,
};

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

Task* D_dryfield_breezeway_801843A8 = NULL;

RoomEventReq gRoomEventReq = { 0 };

Task* D_dryfield_breezeway_801843C0;

static void func_dryfield_breezeway_8017DDB0(Task* task);
static void _dryfieldBreezewayMessageTaskIdle(Task* unusedTask);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

s32 dryfieldBreezewayForwardKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 secondArg)
{
    s32 reply;

    if (D_dryfield_breezeway_801843A8 == NULL) {
        reply = ROOM_KEY_ITEM_USE_REFUSED;
    } else {
        reply = taskMessageDispatch(D_dryfield_breezeway_801843A8, messageId, itemId, secondArg);
    }
    return reply;
}

/// `TaskMessageEntry` handler for message 0x13EE, the room's own progress gate. It
/// answers message 0x17 by writing 1 or 2 into the outgoing record's `room`
/// from the room's progress nibble 0x47, and - when the message id still reads
/// 0x17 on a second look - hands the room's event request (flag nibble 0x37,
/// collected bit 0x15) to the room's event gate `_roomEventGate`,
/// returning its answer.
/// A gate that latched the request is followed by the room's own follow-up:
/// progress nibble 0x56 set to 4 and effect 0xA2. Everything else answers 1.
s32 func_dryfield_breezeway_8017D940(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    if (in->areaId == GAME_AREA_DRYFIELD_FACTORY) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                out->room = 1;
            } else {
                out->room = 2;
            }
        }
        if (in->areaId == GAME_AREA_DRYFIELD_FACTORY) {
            req.capCmd        = 4;
            req.missingCapCmd = 2;
            req.firstSnd      = 0x52160006;
            req.secondSnd     = 0x52160003;
            req.flagId        = GAME_FLAG_BREEZEWAY_FACTORY_DOOR_UNLOCKED;
            req.collectedBit  = 0x15;
            ret               = _roomEventGate(&req, out);
            if (gRoomEventActive != 0) {
                gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, 4);
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x38);
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
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                capRunCommandWithTransition(5);
            } else {
                if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) != 4) {
                    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_FACTORY_KEY) != 0) {
                        gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, 3);
                    } else if (gameFlagGetNibble(GAME_FLAG_DRYFIELD_BREEZEWAY_0FE) != 0) {
                        if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BOTTLECAP_MAGNET) == 0) {
                            if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) != 6) {
                                gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, 5);
                            }
                        } else {
                            gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, 2);
                        }
                    }
                }
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(D_dryfield_breezeway_80181E10, 1, arg2, 0);
            }
            break;
        case 3:
            if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) >= 2) {
                if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                    if (areaGetCurrentObjectState(6) == 1) {
                        taskSpawnFromTable(D_dryfield_breezeway_80181E10, 0, 0, 0);
                        gameFlagSetNibble(GAME_FLAG_DRYFIELD_BREEZEWAY_0FE, 1);
                    }
                } else {
                    capRunCommandWithTransition(5);
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

/// `TaskMessageEntry` handler for message 0x13EF, the room's hotspot gate: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 0x5D so
/// a repeat visit does nothing. Only the incoming record is read - the handler
/// answers 0 and never edits the outgoing copy.
s32 func_dryfield_breezeway_8017DBD8(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN) == 0 && in->warp == 1) {
        gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN, 1);
        taskSpawnFromTable(D_dryfield_breezeway_801820B0, 1, 0, 0);
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
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            D_dryfield_breezeway_801843A8 = taskSpawnFromTable(&D_dryfield_breezeway_80182E18, 0, 0, 0);
            arg0->state                  += 1;
            return;
        case 1:
            if (taskPollKill(D_dryfield_breezeway_801843A8, &sp10) != 0) {
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
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) != 4) {
                    if (capGetVariantKey() == 0xB) {
                        gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, 2);
                    }
                    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS) == 5) {
                        gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, 6);
                    }
                }
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
    }
}

static void func_dryfield_breezeway_8017DDB0(Task* task)
{
    ActorCommand msg;

    task->msgTable = D_dryfield_breezeway_80181DE0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN) == 0) {
        msg.context.loc.stage = gGameSession->location.loc.stage;
        msg.context.loc.area  = gGameSession->location.loc.area;
        msg.command           = 0;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
        taskSpawnFromTable(D_dryfield_breezeway_801820B0, 0, 0, 0);
    }
    task->state++;
    D_dryfield_breezeway_801843A8 = NULL;
}

/// Keeps the registered room-message task alive without advancing its idle state.
static void _dryfieldBreezewayMessageTaskIdle(Task* unusedTask)
{
}

/// State handlers of the room's message task, indexed by its state through
/// `dryfieldBreezewayMessageTask`: publish the message table, idle, then
/// kill.
static const TaskFuncTable3 D_dryfield_breezeway_8017D5DC = {
    { func_dryfield_breezeway_8017DDB0, _dryfieldBreezewayMessageTaskIdle, taskKill }
};

void dryfieldBreezewayMessageTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_breezeway_8017D5DC;
    stateHandlers.funcs[task->state](task);
}
