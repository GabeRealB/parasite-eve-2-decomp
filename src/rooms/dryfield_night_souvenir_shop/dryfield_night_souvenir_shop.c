#include "rooms/dryfield_night_souvenir_shop.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern TaskMessageEntry D_dryfield_night_souvenir_shop_8017E03C[];

/// Prism corners in the space of the coordinate drawn under, eight per prism:
/// the lit ring, then the far ring. The room draws the prisms at `[0..7]` and
/// `[8..15]`.
extern SVECTOR gGlowPrismCorners[];

static s32  _dryfieldNightSouvenirShopRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldNightSouvenirShopResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _dryfieldNightSouvenirShopIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _dryfieldNightSouvenirShopIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldNightSouvenirShopInitRoomTask(Task* task);

/// Inventory key-item use routed to this room's message table.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_MESSAGE_USE_KEY_ITEM = 0x13F1,
};

extern WorldCollisionGrid         D_dryfield_night_souvenir_shop_8017E604[1];
extern WorldCollisionTrigger      D_dryfield_night_souvenir_shop_8017F190[2];
extern WorldCollisionTrigger      D_dryfield_night_souvenir_shop_8017F248[12];
extern WorldCoordRoomAmbientEntry D_dryfield_night_souvenir_shop_8017F228[4];
extern WorldCoordRoomLights       D_dryfield_night_souvenir_shop_8017F178[1];

TaskMessageEntry D_dryfield_night_souvenir_shop_8017E03C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldNightSouvenirShopResolveRoomEvent },
    { DRYFIELD_NIGHT_SOUVENIR_SHOP_MESSAGE_USE_KEY_ITEM, _dryfieldNightSouvenirShopRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightSouvenirShopIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightSouvenirShopIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR gGlowPrismCorners[16] = {
    { 2315, -1960, -3930, 0 },
    { 1685, -1960, -3930, 0 },
    { 1680, -1000, -3930, 0 },
    { 2315, -1000, -3930, 0 },
    { 1530, 0, -1970, 0 },
    { 900, 0, -1970, 0 },
    { 1290, 0, -2935, 0 },
    { 1920, 0, -2935, 0 },
    { 5255, -2085, -4020, 0 },
    { 3745, -2085, -4020, 0 },
    { 3745, -545, -4020, 0 },
    { 5255, -545, -4020, 0 },
    { 4420, 0, -1935, 0 },
    { 2910, 0, -1935, 0 },
    { 3530, 0, -3475, 0 },
    { 5040, 0, -3475, 0 },
};

WorldCoordRoomLighting D_dryfield_night_souvenir_shop_8017E0E4[1] = {
    { D_dryfield_night_souvenir_shop_8017F178, D_dryfield_night_souvenir_shop_8017F228 },
};

WorldCollisionRoomResources D_dryfield_night_souvenir_shop_8017E0EC[1] = {
    { D_dryfield_night_souvenir_shop_8017E604, D_dryfield_night_souvenir_shop_8017F190, D_dryfield_night_souvenir_shop_8017F248, NULL },
};

u8* D_dryfield_night_souvenir_shop_8017E0FC[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_night_souvenir_shop_8017E100[1] = { 3 };

DirectionWarpEntry D_dryfield_night_souvenir_shop_8017E104[1] = {
    { { { .word = 0 }, 2000, 0, -3550 }, { 0, 0, 0, 0 }, { { .word = 512 }, 1381, 0, -2750 }, { 0, 0, 0, 0 }, 0x53060002, 0x53060001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 469 },
};

static SVECTOR _gDryfieldNightSouvenirShopCollision01044Normals[11] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_normals.inc"
};

static SVECTOR _gDryfieldNightSouvenirShopCollision01044Verts[62] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightSouvenirShopCollision01044Faces[34] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_faces.inc"
};

static s16 _gDryfieldNightSouvenirShopCollision01044Cells[108] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightSouvenirShopCollision01044Cells[i])
static s16* _gDryfieldNightSouvenirShopCollision01044Table[4] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_souvenir_shop_8017E604[1] = {
    { NULL, _gDryfieldNightSouvenirShopCollision01044Normals, _gDryfieldNightSouvenirShopCollision01044Verts, _gDryfieldNightSouvenirShopCollision01044Faces, _gDryfieldNightSouvenirShopCollision01044Table, 100, 4100, 2, 2, 4000, 34 },
};

ViewCamera D_dryfield_night_souvenir_shop_8017E628[4] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3000, 7500, 2000 } }, 230 },
    { { { { 1125, 0, 3938 }, { 1711, 3688, -489 }, { -3546, 1780, 1013 } }, { -4960, 2182, 3227 } }, 230 },
    { { { { 1194, 0, -3918 }, { -1520, 3775, -463 }, { 3611, 1589, 1100 } }, { -928, 2117, 3153 } }, 230 },
    { { { { 2278, 0, -3403 }, { -2520, 2752, -1687 }, { 2286, 3033, 1531 } }, { -4039, 2424, 1619 } }, 230 },
};

SpriteBatch D_dryfield_night_souvenir_shop_8017E6B8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_souvenir_shop_8017E6C8[45] = {
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -144, 24, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -104, 72, 600, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 64 } }, -144, 56, 450, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -120, 64, 437, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -120, 88, 412, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 48, 450, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -16, -24, 875, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -16, 875, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 8, -16, 775, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -8, -16, 875, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 24, 0, 675, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 8, 0, 837, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 48, 16, 537, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 64, 40, 487, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, 40, 375, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 80, 64, 425, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, 64, 375, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 371, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 375, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, 88, 375, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 112, 104, 375, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 24, 72, 662, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, 16, 662, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, 16, 625, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -128, -40, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -136, -16, 625, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -136, -80, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -152, -112, 575, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -152, -56, 575, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -144, -88, 600, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -144, -32, 600, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -160, -112, 550, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -160, -72, 550, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -128, 24, 625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -120, -8, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -120, 40, 625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -112, 40, 625, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -104, 64, 625, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -112, 88, 625, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -120, 88, 625, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -128, 72, 625, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, -136, 48, 625, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, -144, 24, 600, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 120 } }, -152, 0, 575, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 144 } }, -160, -24, 550, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_souvenir_shop_8017EA4C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 4, 0 } },
    { 2, 1, 0, 0, { 1, 0 } },
    { 3, 2, 0, 0, { 3, 0 } },
    { 5, 19, 0, 0, { 2, 0 } },
    { 24, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_souvenir_shop_8017EA84[53] = {
    { 143, 0x3FC0, { .fields = { 72, 96 } }, 72, 24, 625, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -8, 925, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -32, -8, 925, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, -32, 925, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -32, -32, 900, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -112, -8, 1175, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 88 } }, 72, 8, 675, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 80, 48, 625, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -40, 1450, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -32, 1375, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -24, 1250, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, -40, 1450, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, -32, 1250, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, -24, 1125, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, 0, 1137, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -64, 0, 750, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -80, 16, 650, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -16, 16, 900, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 24 } }, -112, 32, 562, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -40, 32, 675, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -128, 56, 475, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -64, 56, 375, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -128, 80, 375, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -40, -16, 1050, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, -80, 912, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -120, -104, 912, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -104, -32, 1025, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -96, -8, 1050, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -136, -112, 912, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -128, -72, 912, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, -24, 1025, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 0, 1025, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -160, -120, 875, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -160, -72, 875, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -152, -24, 900, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -144, 0, 1000, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -24, 875, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, 0, 975, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 152, -120, 675, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 152, -48, 675, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 144, -112, 675, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 144, -40, 675, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 136, -112, 675, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 128, -112, 700, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 120, -112, 725, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 136, -80, 675, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 136, -24, 675, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 136, 32, 675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 56 } }, 128, -48, 675, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 128, 8, 675, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 120, 8, 675, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 112, 40, 675, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 104, 72, 675, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_souvenir_shop_8017EEA8[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 4, 0 } },
    { 1, 4, 0, 0, { 5, 0 } },
    { 5, 1, 0, 0, { 2, 0 } },
    { 6, 1, 0, 0, { 6, 0 } },
    { 7, 1, 0, 0, { 1, 0 } },
    { 8, 16, 0, 0, { 7, 0 } },
    { 24, 14, 0, 0, { 3, 0 } },
    { 38, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_souvenir_shop_8017EEF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_souvenir_shop_8017EF08[4] = {
    { { .empty = D_dryfield_night_souvenir_shop_8017E6B8 }, D_dryfield_night_souvenir_shop_8017E6B8, NULL },
    { { .elements = D_dryfield_night_souvenir_shop_8017E6C8 }, D_dryfield_night_souvenir_shop_8017EA4C, NULL },
    { { .elements = D_dryfield_night_souvenir_shop_8017EA84 }, D_dryfield_night_souvenir_shop_8017EEA8, NULL },
    { { .empty = D_dryfield_night_souvenir_shop_8017EEF8 }, D_dryfield_night_souvenir_shop_8017EEF8, NULL },
};

WorldCoordPointLight D_dryfield_night_souvenir_shop_8017EF38[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4098, -1580, -3550 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1846, 2048, 2048 }, { 0, 0 } }, 1102, 1964 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2383, -1477, -3573 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1845, 2048, 2048 }, { 0, 0 } }, 1161, 2563 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1993, -1582, -21 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1844, 2048, 2049 }, { 0, 0 } }, 1486, 2463 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4049, -1580, -20 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1844, 2050, 2050 }, { 0, 0 } }, 1485, 2503 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 299, -393, -2198 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1025, 1230, 1229 }, { 0, 0 } }, 750, 1250 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3700, -120, -2840 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1556, 1474 }, { 0, 0 } }, 1001, 1800 },
};

WorldCoordRoomLights D_dryfield_night_souvenir_shop_8017F178[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_souvenir_shop_8017EF38), D_dryfield_night_souvenir_shop_8017EF38, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_souvenir_shop_8017F190[2] = {
    { NULL, NULL, NULL, { 2879, -1440, -1969, 0 }, { { 147, -1824, -2190, 0 }, { -146, -1824, 2191, 0 }, { 147, 1824, -2190, 0 }, { -146, 1824, 2191, 0 } }, { 4087, 0, 272, 0 }, { 0, 0, 4096, 0 }, 2850, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3039, -1536, -2017, 0 }, { { -146, -1888, 2191, 0 }, { 147, -1888, -2190, 0 }, { -146, 1888, 2191, 0 }, { 147, 1888, -2190, 0 } }, { -4087, 0, -275, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_dryfield_night_souvenir_shop_8017F228[4] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_souvenir_shop_8017F228) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 410, 615, 615, 538 } },
    { .color = { 410, 615, 615, 538 } },
};

WorldCollisionTrigger D_dryfield_night_souvenir_shop_8017F248[12] = {
    { NULL, NULL, NULL, { 1920, -57, -3760, 0 }, { { -544, 0, -240, 0 }, { 544, 0, -240, 0 }, { -544, 0, 240, 0 }, { 544, 0, 240, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 593, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2832, -64, -704, 0 }, { { -1088, 0, -272, 0 }, { 1088, 0, -272, 0 }, { -1088, 0, 272, 0 }, { 1088, 0, 272, 0 } }, { 0, 4109, 0, 0 }, { -201, 0, -4091, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4176, -64, -3296, 0 }, { { -1520, 0, -272, 0 }, { 1520, 0, -272, 0 }, { -1520, 0, 272, 0 }, { 1520, 0, 272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1541, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5312, -64, -752, 0 }, { { -192, 0, -1024, 0 }, { 192, 0, -1024, 0 }, { -192, 0, 1024, 0 }, { 192, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4091, 0, 201, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1023, -64, -1728, 0 }, { { -336, 0, -640, 0 }, { 336, 0, -640, 0 }, { -336, 0, 640, 0 }, { 336, 0, 640, 0 } }, { 0, 4098, 0, 0 }, { 4095, 0, 0, 0 }, 721, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4656, -64, -1200, 0 }, { { -880, 0, -384, 0 }, { 880, 0, -384, 0 }, { -880, 0, 384, 0 }, { 880, 0, 384, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 960, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2736, -64, -1696, 0 }, { { -848, 0, -176, 0 }, { 848, 0, -176, 0 }, { -848, 0, 176, 0 }, { 848, 0, 176, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2720, -64, -2624, 0 }, { { -848, 0, -176, 0 }, { 848, 0, -176, 0 }, { -848, 0, 176, 0 }, { 848, 0, 176, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, -4091, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1056, -64, -3104, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 3973, 0, 995, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1392, -64, -736, 0 }, { { -576, 0, -176, 0 }, { 576, 0, -176, 0 }, { -576, 0, 176, 0 }, { 576, 0, 176, 0 } }, { 0, 4097, 0, 0 }, { -201, 0, -4091, 0 }, 600, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4720, -64, -448, 0 }, { { -784, 0, -176, 0 }, { 784, 0, -176, 0 }, { -784, 0, 176, 0 }, { 784, 0, 176, 0 } }, { 0, 4099, 0, 0 }, { 200, 0, -4091, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4352, -64, -1952, 0 }, { { -1360, 0, 32, 0 }, { 240, 0, 32, 0 }, { -752, 0, 896, 0 }, { 240, 0, 896, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_night_souvenir_shop_8017F5D8[2] = {
    { 8, 7, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_souvenir_shop_8017F5F0[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_souvenir_shop_8017F608[3] = {
    { 8, 7, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor00700_D075A8 },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, Actor02500_D05B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_souvenir_shop_8017F62C[12] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017B2B8, D_dryfield_night_souvenir_shop_8017F5D8 },
    { NULL, NULL },
    { D_map_dryfield_full_8017B308, D_dryfield_night_souvenir_shop_8017F5F0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017B358, D_dryfield_night_souvenir_shop_8017F608 },
};

WorldCollisionFootstepSounds D_dryfield_night_souvenir_shop_8017F68C = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_night_souvenir_shop_8017F698 = {
    0x10000031,
    0x10000033,
    0x10000031,
};

WorldCollisionSurfaceProperties D_dryfield_night_souvenir_shop_8017F6A4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_souvenir_shop_8017F6AC[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_souvenir_shop_8017F6B4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_souvenir_shop_8017F6BC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_souvenir_shop_8017F68C },
};

WorldCollisionSurfaceProperties D_dryfield_night_souvenir_shop_8017F6C4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_souvenir_shop_8017F698 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_souvenir_shop_8017F6CC[8] = {
    D_dryfield_night_souvenir_shop_8017F6A4,
    D_dryfield_night_souvenir_shop_8017F6BC,
    D_dryfield_night_souvenir_shop_8017F6C4,
    D_dryfield_night_souvenir_shop_8017F6AC,
    D_dryfield_night_souvenir_shop_8017F6B4,
    D_dryfield_night_souvenir_shop_8017F6A4,
    D_dryfield_night_souvenir_shop_8017F6A4,
    D_dryfield_night_souvenir_shop_8017F6A4,
};

/// Rejects every key-item use in this room, returning 0 without consuming the item.
///
/// `itemId` is the inventory item's integer ID; the second payload word is unused.
static s32 _dryfieldNightSouvenirShopRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    enum { DRYFIELD_NIGHT_SOUVENIR_SHOP_KEY_ITEM_REJECTED = 0 };

    return DRYFIELD_NIGHT_SOUVENIR_SHOP_KEY_ITEM_REJECTED;
}

/// Permits a room transition with the requested destination unchanged.
///
/// Borrows a complete readable request and writable eight-byte reply through
/// synchronous dispatch; they may be the same record. Copies every field and
/// returns 1 in both query and execute modes, retaining neither pointer and
/// changing no game flags.
static s32 _dryfieldNightSouvenirShopResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_EVENT_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_EVENT_ALLOWED;
}

/// Ignores every room command and returns 0 without changing room or task state.
///
/// Both command payload words are unused.
static s32 _dryfieldNightSouvenirShopIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores direction-trigger room actions and returns 0.
///
/// Neither the borrowed action request nor the second payload word is accessed
/// or retained; no room or task state changes.
static s32 _dryfieldNightSouvenirShopIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Called in state 0 with a live task. The loaded room's table is borrowed
/// for subsequent synchronous messages while the registered task remains alive.
static void _dryfieldNightSouvenirShopInitRoomTask(Task* task)
{
    task->msgTable = D_dryfield_night_souvenir_shop_8017E03C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized room task idle until its state changes externally.
///
/// State 1 leaves the task and its installed message table alive; state 2 is
/// handled separately by `taskKill`.
static void _dryfieldNightSouvenirShopIdleRoomTask(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_souvenir_shop_8017D5C4 = {
    { _dryfieldNightSouvenirShopInitRoomTask, _dryfieldNightSouvenirShopIdleRoomTask, taskKill },
};

void dryfieldNightSouvenirShopRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_dryfield_night_souvenir_shop_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/glow_draw_prism.inc.c"

void dryfieldNightSouvenirShopPrismLightTask(Task* task)
{
    enum { DRYFIELD_NIGHT_SOUVENIR_SHOP_CORNERS_PER_PRISM = 8 };

    GfxCoord* coord;

    coord = task->extra.coordBody->coord;
    actorRenderComposeCoord(coord);
    _glowDrawPrism(coord, 0);
    _glowDrawPrism(coord, DRYFIELD_NIGHT_SOUVENIR_SHOP_CORNERS_PER_PRISM);
}
