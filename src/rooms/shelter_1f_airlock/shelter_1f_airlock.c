#include "rooms/shelter_1f_airlock.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

#define D_shelter_1f_airlock_8017E4C4 (D_shelter_1f_airlock_8017E4BC + 1)
#define D_shelter_1f_airlock_8017E4D4 (D_shelter_1f_airlock_8017E4BC + 3)

/// The room's message table, handed to its event task in state 0.
extern TaskMessageEntry D_shelter_1f_airlock_8017E494[];

enum {
    SHELTER_1F_AIRLOCK_MESSAGE_USE_KEY_ITEM = 0x13F1,
    SHELTER_1F_AIRLOCK_KEY_ITEM_REJECTED    = 0,
};

/// Ambient effect emitter positions for the airlock, selected by view index.
/// `D_shelter_1f_airlock_8017E4BC` / `_8017E4C4` / `_8017E4D4` are successive
/// labels into one contiguous run of `SVECTOR`s, so the per-view lists overlap.

// Indexed views below share one contiguous table.
static s32  _shelter1fAirlockRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
s32         func_shelter_1f_airlock_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32  _shelter1fAirlockIgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);
static s32  _shelter1fAirlockIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);
static void _shelter1fAirlockIdle(Task* unusedTask);

extern WorldCollisionGrid     D_shelter_1f_airlock_8017E838[1];
extern WorldCollisionOccluder D_shelter_1f_airlock_8017F7B8[2];
extern WorldCollisionTrigger  D_shelter_1f_airlock_8017F430[6];
extern WorldCollisionTrigger  D_shelter_1f_airlock_8017F5F8[4];
extern WorldCoordRoomLights   D_shelter_1f_airlock_8017F418[1];

TaskMessageEntry D_shelter_1f_airlock_8017E494[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_airlock_8017D5D8 },
    { SHELTER_1F_AIRLOCK_MESSAGE_USE_KEY_ITEM, _shelter1fAirlockRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fAirlockIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelter1fAirlockIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_shelter_1f_airlock_8017E4BC[28] = {
    { -3000, -2240, 5030, 0 },
    { 0, -2240, 5030, 0 },
    { 2000, -2240, 4100, 0 },
    { -5700, -1340, 3260, 0 },
    { 220, -2010, 4290, 0 },
    { 220, -1760, 4120, 0 },
    { -220, -2010, 4290, 0 },
    { -220, -1760, 4120, 0 },
    { 200, -2010, 4280, 0 },
    { -200, -2010, 4280, 0 },
    { 200, -1770, 4120, 0 },
    { -200, -1770, 4120, 0 },
    { -1210, -2010, 4290, 0 },
    { -1210, -1760, 4120, 0 },
    { -1650, -2010, 4290, 0 },
    { -1650, -1760, 4120, 0 },
    { -1230, -2010, 4280, 0 },
    { -1630, -2010, 4280, 0 },
    { -1230, -1770, 4120, 0 },
    { -1630, -1770, 4120, 0 },
    { -2780, -2010, 4290, 0 },
    { -2780, -1760, 4120, 0 },
    { -3220, -2010, 4290, 0 },
    { -3220, -1760, 4120, 0 },
    { -2800, -2010, 4280, 0 },
    { -3200, -2010, 4280, 0 },
    { -2800, -1770, 4120, 0 },
    { -3200, -1770, 4120, 0 },
};

WorldCollisionRoomResources D_shelter_1f_airlock_8017E59C[1] = {
    { D_shelter_1f_airlock_8017E838, D_shelter_1f_airlock_8017F430, D_shelter_1f_airlock_8017F5F8, D_shelter_1f_airlock_8017F7B8 },
};

WorldCoordRoomLighting D_shelter_1f_airlock_8017E5AC[1] = {
    { D_shelter_1f_airlock_8017F418, NULL },
};

u8* D_shelter_1f_airlock_8017E5B4[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_1f_airlock_8017E5B8[1] = { 5 };

DirectionWarpEntry D_shelter_1f_airlock_8017E5BC[2] = {
    { { { .word = 0 }, 2051, 0, 3070 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2100, 0, 3390 }, { 0, 0, 0, 0 }, 0x55050002, 0x55050001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -5050, 0, 3070 }, { 0, 0, 0, 0 }, { { .word = 0 }, -5050, 0, 3070 }, { 0, 0, 0, 0 }, 0x55050002, 0x55050001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelter1fAirlockCollision01278Normals[6] = {
#include "assets/shelter_1f_airlock_collision_01278_normals.inc"
};

static SVECTOR _gShelter1fAirlockCollision01278Verts[32] = {
#include "assets/shelter_1f_airlock_collision_01278_verts.inc"
};

static WorldCollisionGridFace _gShelter1fAirlockCollision01278Faces[13] = {
#include "assets/shelter_1f_airlock_collision_01278_faces.inc"
};

static s16 _gShelter1fAirlockCollision01278Cells[26] = {
#include "assets/shelter_1f_airlock_collision_01278_cells.inc"
};

#define GRID_CELL(i) (&_gShelter1fAirlockCollision01278Cells[i])
static s16* _gShelter1fAirlockCollision01278Table[3] = {
#include "assets/shelter_1f_airlock_collision_01278_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_1f_airlock_8017E838[1] = {
    { NULL, _gShelter1fAirlockCollision01278Normals, _gShelter1fAirlockCollision01278Verts, _gShelter1fAirlockCollision01278Faces, _gShelter1fAirlockCollision01278Table, 5750, -2500, 3, 1, 4000, 13 },
};

ViewCamera D_shelter_1f_airlock_8017E85C[5] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { -3992, 0, 916 }, { 448, 3570, 1955 }, { -799, 2006, -3480 } }, { -2240, 2240, -5400 } }, 230 },
    { { { { -434, 0, -4072 }, { 315, 4083, -33 }, { 4060, -317, -433 } }, { 3460, 700, -5100 } }, 257 },
    { { { { -390, 0, 4077 }, { 1014, 3967, 97 }, { -3949, 1019, -378 } }, { -1540, 2030, -5100 } }, 257 },
    { { { { -4004, 0, 860 }, { 491, 3363, 2285 }, { -706, 2337, -3288 } }, { 4160, 2340, -5400 } }, 207 },
};

SpriteBatch D_shelter_1f_airlock_8017E910[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_airlock_8017E920[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_1f_airlock_8017E930[24] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, -96, 1150, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -48, 1163, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -32, 1201, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 0, 1196, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 1143, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 32, 1157, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 64, 1157, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 40, -96, 1100, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 64, -96, 950, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 96, -96, 875, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 128, -96, 875, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -32, 875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 24, 875, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 96, -32, 875, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, 24, 875, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 64, -32, 950, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 64, 24, 950, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 40, -32, 1100, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, 24, 1100, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 56 } }, 128, 64, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 56 } }, 96, 64, 875, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, 72, 64, 950, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, 48, 64, 1100, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, 56, 1157, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_airlock_8017EB10[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_1f_airlock_8017EB30[2] = {
    { { 117, 161, 0, 0 }, 0x4E20 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_1f_airlock_8017EB44[26] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -88, 1275, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -120, -88, 1275, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -88, -88, 1275, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -88, 1064, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -72, 1146, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -40, 1430, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -48, 1425, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -56, -88, 1275, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -32, 1418, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -24, 1411, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -16, 1451, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -8, 1448, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 0, 1473, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 8, 1498, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 24, 1498, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -56, -32, 1275, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -160, -32, 1275, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -120, -32, 1275, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -88, -32, 1275, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 24, 1275, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -32, 16, 1498, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -40, 24, 1250, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -56, 24, 1225, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -80, 24, 1225, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -112, 24, 1225, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 40 } }, -160, 24, 1225, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_airlock_8017ED4C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 1, 0 } },
    { 20, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_1f_airlock_8017ED6C[2] = {
    { { 149, 229, 0, 0 }, 0x343A },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_1f_airlock_8017ED80[36] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -120, 311, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -80, 311, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, 0, 435, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 16, 434, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -16, 422, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, -88, 320, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -120, 313, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -120, 338, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -104, 334, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -88, 342, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -64, 313, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -64, 333, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -40, 323, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -24, 341, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, 0, 425, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, 24, 432, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 48, 434, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 40, 422, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, 56, 519, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 40, 475, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, -40, 307, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, -24, 313, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 56, 521, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 64, 538, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 48, 425, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 72, 540, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 80, 543, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 96, 589, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 80, 560, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 88, 538, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 72, 546, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 80, 544, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 88, 549, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 96, 547, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -48, 322, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -32, 308, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_airlock_8017F050[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 36, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_1f_airlock_8017F068[2] = {
    { { 87, 207, 0, 0 }, 0x2BF2 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteView D_shelter_1f_airlock_8017F07C[5] = {
    { { .empty = D_shelter_1f_airlock_8017E910 }, D_shelter_1f_airlock_8017E910, NULL },
    { { .empty = D_shelter_1f_airlock_8017E920 }, D_shelter_1f_airlock_8017E920, NULL },
    { { .elements = D_shelter_1f_airlock_8017E930 }, D_shelter_1f_airlock_8017EB10, D_shelter_1f_airlock_8017EB30 },
    { { .elements = D_shelter_1f_airlock_8017EB44 }, D_shelter_1f_airlock_8017ED4C, D_shelter_1f_airlock_8017ED6C },
    { { .elements = D_shelter_1f_airlock_8017ED80 }, D_shelter_1f_airlock_8017F050, D_shelter_1f_airlock_8017F068 },
};

WorldCoordPointLight D_shelter_1f_airlock_8017F0B8[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2000, 3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2048, 1638 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 10, -1730, 4260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 3276, 3276 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1430, -1730, 4260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 3276, 3276 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -1730, 4260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 3276, 3276 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2000, 4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2048, 1638 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -2000, 4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2048, 1638 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -2000, 3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2048, 1638 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5580, -1340, 3255 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 409, 409 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1010, -2099, 4150 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1228, 4096, 2457 }, { 0, 0 } }, 100, 200 },
};

WorldCoordRoomLights D_shelter_1f_airlock_8017F418[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_1f_airlock_8017F0B8), D_shelter_1f_airlock_8017F0B8, 0, NULL },
};

WorldCollisionTrigger D_shelter_1f_airlock_8017F430[6] = {
    { NULL, NULL, NULL, { 2083, -1808, 4063, 0 }, { { -1404, -2271, -163, 0 }, { 1400, -2271, 159, 0 }, { -1404, 2272, -163, 0 }, { 1400, 2272, 159, 0 } }, { 467, 0, -4078, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2130, -1856, 3839, 0 }, { { 1444, -2256, 160, 0 }, { -1461, -2256, -177, 0 }, { 1444, 2256, 160, 0 }, { -1461, 2256, -177, 0 } }, { -474, 0, 4071, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -993, -1408, 4785, 0 }, { { -432, -2656, 1520, 0 }, { 432, -2656, -1520, 0 }, { -432, 2656, 1520, 0 }, { 432, 2656, -1520, 0 } }, { -3943, 0, -1121, 0 }, { 0, 0, 4096, 0 }, 3082, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1169, -1409, 4785, 0 }, { { 448, -2624, -1536, 0 }, { -448, -2624, 1536, 0 }, { 448, 2624, -1536, 0 }, { -448, 2624, 1536, 0 } }, { 3936, 0, 1148, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5377, -1456, 3969, 0 }, { { 1573, -2128, -267, 0 }, { -1586, -2128, 235, 0 }, { 1573, 2128, -267, 0 }, { -1586, 2128, 235, 0 } }, { 643, 0, 4051, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5248, -1376, 4160, 0 }, { { -1571, -2080, 234, 0 }, { 1544, -2080, -281, 0 }, { -1571, 2080, 234, 0 }, { 1544, 2080, -281, 0 } }, { -672, 0, -4053, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_1f_airlock_8017F5F8[4] = {
    { NULL, NULL, NULL, { 1984, -48, 2912, 0 }, { { -608, 0, -352, 0 }, { 608, 0, -352, 0 }, { -608, 0, 352, 0 }, { 608, 0, 352, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4960, -48, 2912, 0 }, { { -608, 0, -384, 0 }, { 608, 0, -384, 0 }, { -608, 0, 384, 0 }, { 608, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 718, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5360, -64, 3280, 0 }, { { -336, 0, -560, 0 }, { 336, 0, -560, 0 }, { -336, 0, 560, 0 }, { 336, 0, 560, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 652, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1376, -64, 4192, 0 }, { { -608, 0, -352, 0 }, { 608, 0, -352, 0 }, { -608, 0, 352, 0 }, { 608, 0, 352, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_1f_airlock_8017F728[2] = {
    { 22, 22, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_402200_80154188 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_1f_airlock_8017F740[2] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_403900_801540E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_1f_airlock_8017F758[12] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AFD0, D_shelter_1f_airlock_8017F728 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017AFF0, D_shelter_1f_airlock_8017F740 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionOccluder D_shelter_1f_airlock_8017F7B8[2] = {
    { NULL, NULL, { -64, -1264, 3072, 0 }, { { -1216, 2480, -832, 0 }, { 1216, 2480, 832, 0 }, { -1216, -2480, -832, 0 }, { 1216, -2480, 832, 0 } }, { -2315, 0, 3382, 0 }, 2873, 1, 0 },
    { NULL, NULL, { -2801, -1440, 3071, 0 }, { { -1433, 2480, 833, 0 }, { 1434, 2480, -832, 0 }, { -1433, -2480, 833, 0 }, { 1434, -2480, -832, 0 } }, { 2060, 0, 3547, 0 }, 2974, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_1f_airlock_8017F830 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_1f_airlock_8017F83C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_1f_airlock_8017F844[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_1f_airlock_8017F830 },
};

WorldCollisionSurfaceProperties* D_shelter_1f_airlock_8017F84C[8] = {
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F844,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
};

static void func_shelter_1f_airlock_8017D62C(Task* task);

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Refuses every key-item use in this room, returning the menu's rejection result.
///
/// The first payload word is the selected item ID; neither payload is read.
/// The room task and the selected item remain untouched.
static s32 _shelter1fAirlockRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return SHELTER_1F_AIRLOCK_KEY_ITEM_REJECTED;
}

/// The room's handler for message 0x13EE: copies the incoming save location
/// onto the outgoing one, passes both to `func_map_neo_ark_80179B14` and returns 1.
s32 func_shelter_1f_airlock_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

/// Ignores `ROOM_MESSAGE_COMMAND` and returns zero without changing room state.
///
/// Both integer payload words are unread; the command selector and optional
/// mode neither affect room state nor survive the call.
static s32 _shelter1fAirlockIgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION` and returns zero.
///
/// The borrowed action request and the zero second payload word are unread;
/// no request storage is retained or modified.
static s32 _shelter1fAirlockIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
static void func_shelter_1f_airlock_8017D62C(Task* task)
{
    task->msgTable = D_shelter_1f_airlock_8017E494;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Keeps the room task in its idle state while its message table remains installed.
static void _shelter1fAirlockIdle(Task* unusedTask)
{
}

/// The event task's three states: install the message table, idle, and kill.
static const TaskFuncTable3 D_shelter_1f_airlock_8017D5C4 = {
    {
        func_shelter_1f_airlock_8017D62C,
        _shelter1fAirlockIdle,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_shelter_1f_airlock_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_airlock_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws two adjacent disc glows with the same scale and packed colour factors.
///
/// Borrows both world points during drawing and queues their packets in order.
static inline void _shelter1fAirlockDrawDiscPair(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor)
{
    _glowDrawFactorDisc(&worldPoints[0], radiusScale, packedColor);
    _glowDrawFactorDisc(&worldPoints[1], radiusScale, packedColor);
}

void shelter1fAirlockDrawViewGlowsTask(Task* unusedTask)
{
    enum {
        SHELTER_1F_AIRLOCK_GLOW_VIEW_3          = 3,
        SHELTER_1F_AIRLOCK_GLOW_VIEW_4          = 4,
        SHELTER_1F_AIRLOCK_GLOW_VIEW_5          = 5,
        SHELTER_1F_AIRLOCK_DISC_RADIUS_SCALE    = 0x200,  // Pixel radius = scale * 64 / (camera Z / 4)
        SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE = 0x180,  // The same scale applies separately at each endpoint
        SHELTER_1F_AIRLOCK_DISC_GREY_FACTORS    = 0x111,  // RGB factors (1, 1, 1), multiplied by 32 or 40
        SHELTER_1F_AIRLOCK_DISC_RED_FACTORS     = 0x200,  // RGB factors (2, 0, 0), multiplied by 32 or 40
        SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER = 0x1011, // RGB nibbles (0, 1, 1) * 16; odd frames add 2
    };
    u8 mappedViewIndex;

    mappedViewIndex = viewGetMappedIndex();
    // Each view selects its visible discs and pairs of capsule endpoints.
    switch (mappedViewIndex) {
        case SHELTER_1F_AIRLOCK_GLOW_VIEW_3:
            _shelter1fAirlockDrawDiscPair(D_shelter_1f_airlock_8017E4C4, SHELTER_1F_AIRLOCK_DISC_RADIUS_SCALE, SHELTER_1F_AIRLOCK_DISC_GREY_FACTORS);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4C4[3], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4C4[5], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4C4[7], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4C4[9], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4C4[11], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4C4[17], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            break;
        case SHELTER_1F_AIRLOCK_GLOW_VIEW_4:
            _shelter1fAirlockDrawDiscPair(D_shelter_1f_airlock_8017E4BC, SHELTER_1F_AIRLOCK_DISC_RADIUS_SCALE, SHELTER_1F_AIRLOCK_DISC_GREY_FACTORS);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[4], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[6], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[8], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[10], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[12], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[14], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[16], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[18], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[20], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[22], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[24], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            _glowDrawCapsule(&D_shelter_1f_airlock_8017E4BC[26], SHELTER_1F_AIRLOCK_CAPSULE_RADIUS_SCALE, SHELTER_1F_AIRLOCK_CAPSULE_CYAN_FLICKER);
            break;
        case SHELTER_1F_AIRLOCK_GLOW_VIEW_5:
            _glowDrawFactorDisc(&D_shelter_1f_airlock_8017E4D4[0], SHELTER_1F_AIRLOCK_DISC_RADIUS_SCALE, SHELTER_1F_AIRLOCK_DISC_RED_FACTORS);
            break;
    }
}

#define GLOW_DRAW_CAPSULE_SHIFTED_FLICKER 1
#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_factor_disc.inc.c"
