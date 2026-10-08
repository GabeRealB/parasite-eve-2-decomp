#include "rooms/dryfield_parking_lot.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

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

#include "rooms/dryfield_night_parking_lot.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/parking_lot.h"

static s32 _parkingLotSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

extern RoomEventActiveBytes gRoomEventActive;

/// Descriptor of the event task the event gate spawns.
extern TaskDesc gRoomEventTaskDesc;

/// The room's message table, published in `Task::msgTable` by the entry task
/// (ids 0x13EE-0x13F2).
extern TaskMessageEntry D_dryfield_parking_lot_8017DC04[];

/// Per-view values `dryfieldParkingLotUpdateViewEffectGateTask` publishes, indexed by
/// camera view index minus one.
extern u16 D_dryfield_parking_lot_8017DC34[];

/// The message and request the event gate latched for its event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.

extern WorldCollisionGrid     D_dryfield_parking_lot_8017E8DC[1];
extern WorldCollisionOccluder D_dryfield_parking_lot_8017F6E4[2];
extern WorldCollisionTrigger  D_dryfield_parking_lot_8017F0A8[10];
extern WorldCollisionTrigger  D_dryfield_parking_lot_8017F3A0[11];
extern WorldCoordRoomLights   D_dryfield_parking_lot_8017F9FC[1];
extern TaskDesc               Actor00100_D1BA84;

static s32  _dryfieldParkingLotRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldParkingLotIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _dryfieldParkingLotIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldParkingLotIdle(Task* task);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_parking_lot_8017DC04[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, parkingLotEventMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldParkingLotRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldParkingLotIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldParkingLotIgnoreCommandMessage },
    { ROOM_MESSAGE_SOUND, _parkingLotSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u16 D_dryfield_parking_lot_8017DC34[8] = {
    0,
    2,
    2,
    2,
    2,
    0,
    0,
    0xE5E6,
};

WorldCollisionRoomResources D_dryfield_parking_lot_8017DC44[1] = {
    { D_dryfield_parking_lot_8017E8DC, D_dryfield_parking_lot_8017F0A8, D_dryfield_parking_lot_8017F3A0, D_dryfield_parking_lot_8017F6E4 },
};

u8* D_dryfield_parking_lot_8017DC54[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_parking_lot_8017DC58[1] = { 7 };

WorldCoordRoomLighting D_dryfield_parking_lot_8017DC5C[1] = {
    { D_dryfield_parking_lot_8017F9FC, NULL },
};

DirectionWarpEntry D_dryfield_parking_lot_8017DC64[5] = {
    { { { .word = 3072 }, 0x2995, 0, 3077 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2995, 0, 3077 }, { 0, 0, 0, 0 }, 0x520F0002, 0x520F0001, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, 486 },
    { { { .word = 2048 }, 3200, 0, 3472 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 3200, 0, 3472 }, { 0, 0, 0, 0 }, 0x520F0008, 0x520F0007, 0x520F000A, 5, DIRECTION_WARP_FLAG_NONE, 481 },
    { { { .word = 0 }, -285, 0, 351 }, { 0, 0, 0, 0 }, { { .word = 0 }, -285, 0, 351 }, { 0, 0, 0, 0 }, 0x520F0006, 0x520F0005, 0x520F000A, 4, DIRECTION_WARP_FLAG_NONE, 480 },
    { { { .word = 2048 }, -6840, 0, -450 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -6840, 0, -450 }, { 0, 0, 0, 0 }, 0x520F0008, 0x520F0007, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 479 },
    { { { .word = 0 }, 4771, -450, 1414 }, { 0, 0, 0, 0 }, { { .word = 0 }, 4491, 0, 2215 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldParkingLotCollision0131CNormals[18] = {
#include "assets/dryfield_parking_lot_collision_0131C_normals.inc"
};

static SVECTOR _gDryfieldParkingLotCollision0131CVerts[132] = {
#include "assets/dryfield_parking_lot_collision_0131C_verts.inc"
};

static WorldCollisionGridFace _gDryfieldParkingLotCollision0131CFaces[55] = {
#include "assets/dryfield_parking_lot_collision_0131C_faces.inc"
};

static s16 _gDryfieldParkingLotCollision0131CCells[436] = {
#include "assets/dryfield_parking_lot_collision_0131C_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldParkingLotCollision0131CCells[i])
static s16* _gDryfieldParkingLotCollision0131CTable[45] = {
#include "assets/dryfield_parking_lot_collision_0131C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_parking_lot_8017E8DC[1] = {
    { NULL, _gDryfieldParkingLotCollision0131CNormals, _gDryfieldParkingLotCollision0131CVerts, _gDryfieldParkingLotCollision0131CFaces, _gDryfieldParkingLotCollision0131CTable, 0x4588, 8900, 9, 5, 4000, 55 },
};

ViewCamera D_dryfield_parking_lot_8017E900[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 380 },
    { { { { 2636, 0, -3134 }, { -93, 4094, -78 }, { 3133, 122, 2635 } }, { 0x27F9, 2236, 6567 } }, 230 },
    { { { { 2646, 0, -3126 }, { -3085, 656, -2612 }, { 501, 4043, 424 } }, { 3012, 5975, 537 } }, 230 },
    { { { { 456, 0, 4070 }, { -262, 4087, 29 }, { -4062, -264, 455 } }, { -3566, 1036, -1232 } }, 230 },
    { { { { 1002, 0, -3971 }, { 304, 4083, 76 }, { 3959, -314, 999 } }, { 2133, 1036, -1232 } }, 230 },
    { { { { 371, 0, -4079 }, { 322, 4083, 29 }, { 4066, -323, 370 } }, { -2016, 736, -2302 } }, 230 },
    { { { { 814, 0, -4014 }, { 596, 4050, 121 }, { 3969, -609, 805 } }, { -6266, 636, -2232 } }, 230 },
};

SpriteBatch D_dryfield_parking_lot_8017E9FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_parking_lot_8017EA0C[10] = {
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -160, 32, 500, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, -104, 40, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -80, 16, 500, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 88 } }, -56, 32, 500, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, 0, 24, 500, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, 40, 8, 625, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 120 } }, 72, 0, 650, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 80 } }, 0, -32, 2750, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 88 } }, 40, -32, 3000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 80 } }, 96, -24, 6250, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_parking_lot_8017EAD4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_parking_lot_8017EAF4[2] = {
    { { 136, 0, 183, 238 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_parking_lot_8017EB08[24] = {
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -128, 72, 625, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 136, 8 } }, -40, 80, 625, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 72, 88, 625, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, -80, 625, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, -88, 625, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -64, -104, 625, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -32, 1350, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -40, 1125, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -40, 925, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, -56, 925, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, -80, 925, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -96, 925, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 40, -112, 925, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, 16, 925, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -96, -40, 750, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -120, 625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -112, -120, 625, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 56, -120, 925, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 56, -64, 925, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -64, 925, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -120, 925, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 64, -8, 925, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 112, -8, 925, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 16, 1000, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_parking_lot_8017ECE8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_parking_lot_8017ED00[9] = {
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -160, 72, 550, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, 48, -88, 925, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, -72, 925, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -80, -32, 1250, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -96, -32, 1250, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 144 } }, -128, -32, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 152 } }, -160, -32, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -88, -48, 1975, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 168 } }, -72, -112, 2000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_parking_lot_8017EDB4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 4, 0, 0, { 2, 0 } },
    { 7, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_parking_lot_8017EDDC[7] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 120, 56, 725, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 40, 750, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 88, 750, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 650, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 136 } }, -160, -48, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -144, -112, 650, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, -128, -120, 650, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_parking_lot_8017EE68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_parking_lot_8017EE80[21] = {
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -80, 528, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -56, 531, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 993, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -16, 525, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -16, 751, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 24, 518, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 24, 510, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 64, 515, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 64, 507, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 16, -120, 1012, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, -72, 1012, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, -24, 1012, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, 24, 1012, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1012, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1012, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1012, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1012, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1012, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1012, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1012, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 925, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_parking_lot_8017F024[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_parking_lot_8017F044[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_parking_lot_8017F054[7] = {
    { { .empty = D_dryfield_parking_lot_8017E9FC }, D_dryfield_parking_lot_8017E9FC, NULL },
    { { .elements = D_dryfield_parking_lot_8017EA0C }, D_dryfield_parking_lot_8017EAD4, D_dryfield_parking_lot_8017EAF4 },
    { { .elements = D_dryfield_parking_lot_8017EB08 }, D_dryfield_parking_lot_8017ECE8, NULL },
    { { .elements = D_dryfield_parking_lot_8017ED00 }, D_dryfield_parking_lot_8017EDB4, NULL },
    { { .elements = D_dryfield_parking_lot_8017EDDC }, D_dryfield_parking_lot_8017EE68, NULL },
    { { .elements = D_dryfield_parking_lot_8017EE80 }, D_dryfield_parking_lot_8017F024, NULL },
    { { .empty = D_dryfield_parking_lot_8017F044 }, D_dryfield_parking_lot_8017F044, NULL },
};

WorldCollisionTrigger D_dryfield_parking_lot_8017F0A8[10] = {
    { NULL, NULL, NULL, { -2698, -2736, -1594, 0 }, { { -2084, -3856, 2134, 0 }, { 2085, -3856, -2134, 0 }, { -2084, 3856, 2134, 0 }, { 2085, 3856, -2134, 0 } }, { -2935, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2816, -2592, -1600, 0 }, { { 2078, -3616, -2142, 0 }, { -2092, -3616, 2127, 0 }, { 2078, 3616, -2142, 0 }, { -2092, 3616, 2127, 0 } }, { 2941, 0, 2873, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2560, -2656, 1664, 0 }, { { 1854, -3680, -2342, 0 }, { -1877, -3680, 2312, 0 }, { 1854, 3680, -2342, 0 }, { -1877, 3680, 2312, 0 } }, { 3203, 0, 2568, 0 }, { 0, 0, 4096, 0 }, 4720, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2528, -2720, 1760, 0 }, { { -1873, -3744, 2314, 0 }, { 1857, -3744, -2338, 0 }, { -1873, 3744, 2314, 0 }, { 1857, 3744, -2338, 0 } }, { -3200, 0, -2566, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 831, -2528, 2015, 0 }, { { -694, -3552, 2899, 0 }, { 690, -3552, -2903, 0 }, { -694, 3552, 2899, 0 }, { 690, 3552, -2903, 0 } }, { -3985, 0, -951, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 798, -2416, 2046, 0 }, { { 690, -3440, -2903, 0 }, { -694, -3440, 2899, 0 }, { 690, 3440, -2903, 0 }, { -694, 3440, 2899, 0 } }, { 3998, 0, 953, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8191, -2320, 2527, 0 }, { { -34, -3344, -2982, 0 }, { 35, -3344, 2983, 0 }, { -34, 3344, -2982, 0 }, { 35, 3344, 2983, 0 } }, { 4106, 0, -48, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8223, -2256, 2399, 0 }, { { 35, -3280, 2983, 0 }, { -34, -3280, -2982, 0 }, { 35, 3280, 2983, 0 }, { -34, 3280, -2982, 0 } }, { -4098, 0, 47, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4048, -2912, 3200, 0 }, { { -500, -3856, 3222, 0 }, { 501, -3856, -3222, 0 }, { -500, 3856, 3222, 0 }, { 501, 3856, -3222, 0 } }, { -4059, 0, -631, 0 }, { 0, 0, 4096, 0 }, 5042, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3968, -2848, 2864, 0 }, { { 396, -3856, -2554, 0 }, { -395, -3856, 2554, 0 }, { 396, 3856, -2554, 0 }, { -395, 3856, 2554, 0 } }, { 4054, 0, 627, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_parking_lot_8017F3A0[11] = {
    { NULL, NULL, NULL, { 0x2870, -48, 2976, 0 }, { { -560, 0, -1024, 0 }, { 560, 0, -1024, 0 }, { -560, 0, 1024, 0 }, { 560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3088, -48, 3744, 0 }, { { 848, 0, -560, 0 }, { 848, 0, 560, 0 }, { -848, 0, -560, 0 }, { -848, 0, 560, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1015, WORLD_COLLISION_TRIGGER_ACTION_WARP, 17, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -560, -48, 192, 0 }, { { 720, 0, -560, 0 }, { 720, 0, 560, 0 }, { -720, 0, -560, 0 }, { -720, 0, 560, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 911, WORLD_COLLISION_TRIGGER_ACTION_WARP, 16, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6736, -48, -640, 0 }, { { 656, 0, -560, 0 }, { 656, 0, 560, 0 }, { -656, 0, -560, 0 }, { -656, 0, 560, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 861, WORLD_COLLISION_TRIGGER_ACTION_WARP, 18, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4752, -48, 2032, 0 }, { { 368, 0, -224, 0 }, { 368, 0, 224, 0 }, { -368, 0, -224, 0 }, { -368, 0, 224, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 430, WORLD_COLLISION_TRIGGER_ACTION_FACING, 52, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4640, -992, 1632, 0 }, { { -560, 1024, 0, 0 }, { 560, 1024, 0, 0 }, { -560, -1024, 0, 0 }, { 560, -1024, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1104, -64, 3024, 0 }, { { 432, 0, -1024, 0 }, { 432, 0, 1024, 0 }, { -432, 0, -1024, 0 }, { -432, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4091, 0, 201, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4672, -512, 1408, 0 }, { { 512, 0, -112, 0 }, { 512, 0, 112, 0 }, { -512, 0, -112, 0 }, { -512, 0, 112, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 50, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2048, -64, 2448, 0 }, { { 1232, 0, -432, 0 }, { 1232, 0, 432, 0 }, { -1232, 0, -432, 0 }, { -1232, 0, 432, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7128, -64, -1968, 0 }, { { 1704, 0, -320, 0 }, { 1480, 0, 736, 0 }, { -1592, 0, -768, 0 }, { -1592, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 401, 0, 4076, 0 }, 1764, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5520, -64, -3104, 0 }, { { 960, 0, -1680, 0 }, { 224, 0, 1680, 0 }, { -224, 0, -1680, 0 }, { -960, 0, 1680, 0 } }, { 0, 4096, 0, 0 }, { 4017, 0, 799, 0 }, 1932, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_parking_lot_8017F6E4[2] = {
    { NULL, NULL, { -416, -1872, -3328, 0 }, { { 1078, 2896, -3014, 0 }, { 1078, -2896, -3014, 0 }, { -1079, 2896, 3013, 0 }, { -1079, -2896, 3013, 0 } }, { 3859, 0, 1381, 0 }, 4314, 1, 0 },
    { NULL, NULL, { -6544, -1728, 2800, 0 }, { { 2064, 2896, -2704, 0 }, { 2064, -2896, -2704, 0 }, { -2064, 2896, 2704, 0 }, { -2064, -2896, 2704, 0 } }, { 3260, 0, 2488, 0 }, 4463, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_parking_lot_8017F75C[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -6980, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 2949, 2785 }, { 0, 0 } }, 0x186A0, 0x186A0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7480, -2210, -4200 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4915, 4915, 4915 }, { 0, 0 } }, 4500, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1520, -980, 2900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3220, -980, 2900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2980, -980, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -180, -980, 2900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4180, -980, -100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
};

WorldCoordRoomLights D_dryfield_parking_lot_8017F9FC[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_parking_lot_8017F75C), D_dryfield_parking_lot_8017F75C, 0, NULL },
};

AreaResource D_dryfield_parking_lot_8017FA14[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_parking_lot_8017FA20[2] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_parking_lot_8017FA38[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, Actor02500_D05B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_parking_lot_8017FA5C[2] = {
    { 22, 22, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_402200_80154188 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_parking_lot_8017FA74[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B1C4, D_dryfield_parking_lot_8017FA14 },
    { D_map_dryfield_8017B1D4, D_dryfield_parking_lot_8017FA20 },
    { D_map_dryfield_8017B204, D_dryfield_parking_lot_8017FA38 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017B284, D_dryfield_parking_lot_8017FA5C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_dryfield_parking_lot_8017FADC = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionFootstepSounds D_dryfield_parking_lot_8017FAE8 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_dryfield_parking_lot_8017FAF4 = {
    0x10000051,
    0x10000053,
    0x10000055,
};

WorldCollisionSurfaceProperties D_dryfield_parking_lot_8017FB00[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_parking_lot_8017FB08[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_parking_lot_8017FADC },
};

WorldCollisionSurfaceProperties D_dryfield_parking_lot_8017FB10[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_parking_lot_8017FAE8 },
};

WorldCollisionSurfaceProperties D_dryfield_parking_lot_8017FB18[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_parking_lot_8017FAF4 },
};

WorldCollisionSurfaceProperties D_dryfield_parking_lot_8017FB20[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_parking_lot_8017FB28[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_parking_lot_8017FADC },
};

WorldCollisionSurfaceProperties* D_dryfield_parking_lot_8017FB30[8] = {
    D_dryfield_parking_lot_8017FB00,
    D_dryfield_parking_lot_8017FB20,
    D_dryfield_parking_lot_8017FB10,
    D_dryfield_parking_lot_8017FB18,
    D_dryfield_parking_lot_8017FB00,
    D_dryfield_parking_lot_8017FB08,
    D_dryfield_parking_lot_8017FB28,
    D_dryfield_parking_lot_8017FB00,
};

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 156, 190, 128 } };

RoomEventReq gRoomEventReq = { 0 };

static void func_dryfield_parking_lot_8017DB08(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/parking_lot_event_msg.inc.c"

#include "../../shared/parking_lot_sound_msg.inc.c"

/// Refuses every key-item use request in the daytime parking lot.
///
/// Ignores all arguments and returns zero, making the item menu report that
/// the selected item cannot be used here.
static s32 _dryfieldParkingLotRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    enum { ROOM_KEY_ITEM_USE_UNAVAILABLE = 0 };

    return ROOM_KEY_ITEM_USE_UNAVAILABLE;
}

/// Ignores room commands in the daytime parking lot and returns zero.
///
/// Receives `ROOM_MESSAGE_COMMAND`; neither payload word nor the receiver is read.
static s32 _dryfieldParkingLotIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room action requests in the daytime parking lot and returns zero.
///
/// Receives `DIRECTION_MESSAGE_ROOM_ACTION`; the borrowed request is not read,
/// changed or retained, and the remaining arguments are ignored.
static s32 _dryfieldParkingLotIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Room entry task state 0: parks the room's message table in `Task::msgTable`,
/// publishes the task in pointer slot 7 and advances the state.
static void func_dryfield_parking_lot_8017DB08(Task* task)
{
    task->msgTable = D_dryfield_parking_lot_8017DC04;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Keeps the initialized room task idle while its message table remains available.
///
/// State 1 does no per-frame work and does not advance or read the task.
static void _dryfieldParkingLotIdle(Task* task)
{
}

/// The room entry task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_parking_lot_8017D5DC = {
    { func_dryfield_parking_lot_8017DB08, _dryfieldParkingLotIdle, taskKill },
};

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_parking_lot_8017DB54(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_parking_lot_8017D5DC;
    sp.funcs[task->state](task);
}

void dryfieldParkingLotUpdateViewEffectGateTask(Task* task)
{
    u8 mappedViewIndex;

    mappedViewIndex                  = viewGetMappedIndex();
    gRoomEffectState->roomEffectMode = D_dryfield_parking_lot_8017DC34[mappedViewIndex - 1];
}
