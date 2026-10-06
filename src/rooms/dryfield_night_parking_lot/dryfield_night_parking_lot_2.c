#include "rooms/dryfield_night_parking_lot.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "dryfield_night_parking_lot_private.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"
#include "../../shared/parking_lot.h"

/// The room's per-view table: `gRoomEffectState->roomEffectMode` latches the entry the
/// current camera index selects, and the room's effect tasks read it back.
extern u16 D_dryfield_night_parking_lot_8017EDBC[];

/// The parking lot's drawable points, one 8-byte `SVECTOR` per prop. The
/// phase each one belongs to is `gGameSession->location.loc.view` (the room's stage).
extern SVECTOR D_dryfield_night_parking_lot_8017EDCC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDDC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDE4[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDEC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDFC[];

static void _dryfieldNightParkingLotDrawGreyCapsule(const SVECTOR* startWorldPoint, const SVECTOR* endWorldPoint, s32 radiusScale);

extern WorldCollisionGrid     D_dryfield_night_parking_lot_8017FAD0[1];
extern WorldCollisionOccluder D_dryfield_night_parking_lot_80181330[2];
extern WorldCollisionTrigger  D_dryfield_night_parking_lot_80180CA8[10];
extern WorldCollisionTrigger  D_dryfield_night_parking_lot_80180FA0[12];
extern WorldCoordRoomLights   D_dryfield_night_parking_lot_80180C90[1];

extern TaskDesc Actor00100_D1BA84;

static AnimationPackedPose _gDryfieldNightParkingLotAnimation0166CBank1[10] = {
#include "assets/dryfield_night_parking_lot_animation_0166C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightParkingLotAnimation0166CBank4[95] = {
#include "assets/dryfield_night_parking_lot_animation_0166C_bank4.inc"
};

static AnimationRecord _gDryfieldNightParkingLotAnimation0166CRecords[139] = {
#include "assets/dryfield_night_parking_lot_animation_0166C_records.inc"
};

static u16 _gDryfieldNightParkingLotAnimation0166CIndices[20] = {
#include "assets/dryfield_night_parking_lot_animation_0166C_indices.inc"
};

static AnimationSet _gDryfieldNightParkingLotAnimation0166C = {
    _gDryfieldNightParkingLotAnimation0166CRecords,
    _gDryfieldNightParkingLotAnimation0166CIndices,
    { NULL, _gDryfieldNightParkingLotAnimation0166CBank1, NULL, NULL, _gDryfieldNightParkingLotAnimation0166CBank4, NULL, NULL, NULL },
};

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_parking_lot_8017EC60[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, parkingLotEventMsg },
    { 5105, func_dryfield_night_parking_lot_8017DB04 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_parking_lot_8017DB34 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_parking_lot_8017DB0C },
    { ROOM_MESSAGE_SOUND, parkingLotSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_dryfield_night_parking_lot_8017EC90[2] = {
    &_gDryfieldNightParkingLotAnimation0166C,
    NULL,
};

AnimationBankCopyRequest D_dryfield_night_parking_lot_8017EC98 = { { .sets = D_dryfield_night_parking_lot_8017EC90 }, ARRAY_SIZE(D_dryfield_night_parking_lot_8017EC90) };

AnimationPlayRequest D_dryfield_night_parking_lot_8017ECA0 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

EvsCommand D_dryfield_night_parking_lot_8017ECB4[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_parking_lot_8017EC98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_night_parking_lot_8017DBA4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_parking_lot_8017ECA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

u16 D_dryfield_night_parking_lot_8017EDBC[8] = {
    0,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
};

SVECTOR D_dryfield_night_parking_lot_8017EDCC[2] = {
    { -7290, -2310, -110, 0 },
    { -6280, -2310, -110, 0 },
};

SVECTOR D_dryfield_night_parking_lot_8017EDDC[1] = {
    { 1900, -1730, 3680, 0 },
};

SVECTOR D_dryfield_night_parking_lot_8017EDE4[1] = {
    { 4100, -1730, 3680, 0 },
};

SVECTOR D_dryfield_night_parking_lot_8017EDEC[2] = {
    { -1030, -2020, 150, 0 },
    { -570, -2020, 150, 0 },
};

SVECTOR D_dryfield_night_parking_lot_8017EDFC[3] = {
    { -2950, -2480, 760, 0 },
    { 130, -2480, 2660, 0 },
    { 3930, -2480, 1380, 0 },
};

WorldCoordRoomLighting D_dryfield_night_parking_lot_8017EE14[2] = {
    { D_dryfield_night_parking_lot_80180C90, NULL },
    { D_dryfield_night_parking_lot_80180C90, NULL },
};

WorldCollisionRoomResources D_dryfield_night_parking_lot_8017EE24[2] = {
    { D_dryfield_night_parking_lot_8017FAD0, D_dryfield_night_parking_lot_80180CA8, D_dryfield_night_parking_lot_80180FA0, D_dryfield_night_parking_lot_80181330 },
    { D_dryfield_night_parking_lot_8017FAD0, D_dryfield_night_parking_lot_80180CA8, D_dryfield_night_parking_lot_80180FA0, D_dryfield_night_parking_lot_80181330 },
};

u8 D_dryfield_night_parking_lot_8017EE44[8] = {
    1,
    8,
    3,
    4,
    9,
    10,
    11,
    0,
};

u8* D_dryfield_night_parking_lot_8017EE4C[2] = {
    gViewIdentityMap,
    D_dryfield_night_parking_lot_8017EE44,
};

ViewCount D_dryfield_night_parking_lot_8017EE54[2] = { 7, 7 };

DirectionWarpEntry D_dryfield_night_parking_lot_8017EE58[5] = {
    { { { .word = 3072 }, 0x2995, 0, 3077 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2995, 0, 3077 }, { 0, 0, 0, 0 }, 0x530F0002, 0x530F0001, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, 486 },
    { { { .word = 2048 }, 3200, 0, 3472 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 3200, 0, 3472 }, { 0, 0, 0, 0 }, 0x530F0008, 0x530F0007, 0x530F000A, 5, DIRECTION_WARP_FLAG_NONE, 481 },
    { { { .word = 0 }, -285, 0, 351 }, { 0, 0, 0, 0 }, { { .word = 0 }, -285, 0, 351 }, { 0, 0, 0, 0 }, 0x530F0006, 0x530F0005, 0x530F000A, 4, DIRECTION_WARP_FLAG_NONE, 480 },
    { { { .word = 2048 }, -6840, 0, -450 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -6840, 0, -450 }, { 0, 0, 0, 0 }, 0x530F0008, 0x530F0007, 0x530F000A, 2, DIRECTION_WARP_FLAG_NONE, 479 },
    { { { .word = 0 }, 4771, -450, 1414 }, { 0, 0, 0, 0 }, { { .word = 0 }, 4491, 0, 2215 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldNightParkingLotCollision02510Normals[18] = {
#include "assets/dryfield_night_parking_lot_collision_02510_normals.inc"
};

static SVECTOR _gDryfieldNightParkingLotCollision02510Verts[132] = {
#include "assets/dryfield_night_parking_lot_collision_02510_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightParkingLotCollision02510Faces[55] = {
#include "assets/dryfield_night_parking_lot_collision_02510_faces.inc"
};

static s16 _gDryfieldNightParkingLotCollision02510Cells[436] = {
#include "assets/dryfield_night_parking_lot_collision_02510_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightParkingLotCollision02510Cells[i])
static s16* _gDryfieldNightParkingLotCollision02510Table[45] = {
#include "assets/dryfield_night_parking_lot_collision_02510_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_parking_lot_8017FAD0[1] = {
    { NULL, _gDryfieldNightParkingLotCollision02510Normals, _gDryfieldNightParkingLotCollision02510Verts, _gDryfieldNightParkingLotCollision02510Faces, _gDryfieldNightParkingLotCollision02510Table, 0x4588, 8900, 9, 5, 4000, 55 },
};

ViewCamera D_dryfield_night_parking_lot_8017FAF4[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 380 },
    { { { { 2636, 0, -3134 }, { -93, 4094, -78 }, { 3133, 122, 2635 } }, { 0x27F9, 2236, 6567 } }, 230 },
    { { { { 2646, 0, -3126 }, { -3085, 656, -2612 }, { 501, 4043, 424 } }, { 3012, 5975, 537 } }, 230 },
    { { { { 456, 0, 4070 }, { -262, 4087, 29 }, { -4062, -264, 455 } }, { -3566, 1036, -1232 } }, 230 },
    { { { { 1002, 0, -3971 }, { 304, 4083, 76 }, { 3959, -314, 999 } }, { 2133, 1036, -1232 } }, 230 },
    { { { { 371, 0, -4079 }, { 322, 4083, 29 }, { 4066, -323, 370 } }, { -2016, 736, -2302 } }, 230 },
    { { { { 814, 0, -4014 }, { 596, 4050, 121 }, { 3969, -609, 805 } }, { -6266, 636, -2232 } }, 230 },
    { { { { 2636, 0, -3134 }, { -93, 4094, -78 }, { 3133, 122, 2635 } }, { 0x27F9, 2236, 6567 } }, 230 },
    { { { { 1002, 0, -3971 }, { 304, 4083, 76 }, { 3959, -314, 999 } }, { 2133, 1036, -1232 } }, 230 },
    { { { { 371, 0, -4079 }, { 322, 4083, 29 }, { 4066, -323, 370 } }, { -2016, 736, -2302 } }, 230 },
    { { { { 814, 0, -4014 }, { 596, 4050, 121 }, { 3969, -609, 805 } }, { -6266, 636, -2232 } }, 230 },
};

SpriteBatch D_dryfield_night_parking_lot_8017FC80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_parking_lot_8017FC90[10] = {
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

SpriteBatch D_dryfield_night_parking_lot_8017FD58[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_parking_lot_8017FD78[2] = {
    { { 136, 0, 182, 239 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_parking_lot_8017FD8C[24] = {
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -128, 72, 625, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 136, 8 } }, -40, 80, 625, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 72, 88, 625, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 16, 1000, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -96, -40, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, -120, 625, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -112, -120, 625, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -64, -104, 625, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, -88, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, -80, 625, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, -80, 925, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -96, 925, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, -56, 925, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -40, 1125, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -32, 1350, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -40, 925, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 40, -112, 925, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 56, -120, 925, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 56, -64, 925, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -120, 925, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -64, 925, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -8, 925, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 112, -8, 925, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, 16, 925, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_parking_lot_8017FF6C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_parking_lot_8017FF84[16] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, -88, 925, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, -80, 925, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, -72, 925, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -160, 72, 550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -160, -120, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 240 } }, -144, -120, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 224 } }, -120, -120, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 176 } }, -96, -96, 1125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -80, -72, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, -56, 1250, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 96 } }, -88, -48, 2000, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 168 } }, -72, -112, 2000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 32, 8, 1657, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 40, 8, 1470, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, 48, 0, 1407, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 64, -8, 1419, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_parking_lot_801800C4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 6, 0, 0, { 0, 0 } },
    { 10, 2, 0, 0, { 2, 0 } },
    { 12, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_parking_lot_801800F4[7] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 120, 56, 725, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, -128, -120, 650, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -104, 650, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 136 } }, -160, -48, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -128, 88, 750, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 650, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 40, 750, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_parking_lot_80180180[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_parking_lot_80180198[21] = {
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 64, 500, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 64, 500, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 24, 500, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 24, 500, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -16, 525, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -16, 500, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -56, 500, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 500, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -80, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, -120, 1012, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 16, -72, 1012, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 16, -24, 1012, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, 24, 1012, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1012, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1012, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1012, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1012, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1012, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1012, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1012, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1012, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_parking_lot_8018033C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_parking_lot_8018035C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_parking_lot_8018036C[10] = {
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

SpriteBatch D_dryfield_night_parking_lot_80180434[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_parking_lot_80180454[2] = {
    { { 136, 0, 182, 239 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_parking_lot_80180468[7] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 120, 56, 725, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, -128, -120, 650, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -104, 650, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 136 } }, -160, -48, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -128, 88, 750, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 650, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 40, 750, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_parking_lot_801804F4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_parking_lot_8018050C[6] = {
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, -80, 500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, -16, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 112, -64, 500, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 112, -8, 500, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 80, -48, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 80, 24, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_parking_lot_80180584[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_parking_lot_8018059C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_parking_lot_801805AC[11] = {
    { { .empty = D_dryfield_night_parking_lot_8017FC80 }, D_dryfield_night_parking_lot_8017FC80, NULL },
    { { .elements = D_dryfield_night_parking_lot_8017FC90 }, D_dryfield_night_parking_lot_8017FD58, D_dryfield_night_parking_lot_8017FD78 },
    { { .elements = D_dryfield_night_parking_lot_8017FD8C }, D_dryfield_night_parking_lot_8017FF6C, NULL },
    { { .elements = D_dryfield_night_parking_lot_8017FF84 }, D_dryfield_night_parking_lot_801800C4, NULL },
    { { .elements = D_dryfield_night_parking_lot_801800F4 }, D_dryfield_night_parking_lot_80180180, NULL },
    { { .elements = D_dryfield_night_parking_lot_80180198 }, D_dryfield_night_parking_lot_8018033C, NULL },
    { { .empty = D_dryfield_night_parking_lot_8018035C }, D_dryfield_night_parking_lot_8018035C, NULL },
    { { .elements = D_dryfield_night_parking_lot_8018036C }, D_dryfield_night_parking_lot_80180434, D_dryfield_night_parking_lot_80180454 },
    { { .elements = D_dryfield_night_parking_lot_80180468 }, D_dryfield_night_parking_lot_801804F4, NULL },
    { { .elements = D_dryfield_night_parking_lot_8018050C }, D_dryfield_night_parking_lot_80180584, NULL },
    { { .empty = D_dryfield_night_parking_lot_8018059C }, D_dryfield_night_parking_lot_8018059C, NULL },
};

WorldCoordPointLight D_dryfield_night_parking_lot_80180630[17] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2680, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2211, 2457, 2703 }, { 0, 0 } }, 0x186A0, 0x186A0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8450, -1990, -5560 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1802, 1884, 1966 }, { 0, 0 } }, 3000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2970, -2190, 780 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3440, 3194 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3220, -2190, -4630 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3604, 3522, 3276 }, { 0, 0 } }, 2500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 130, -2190, 2660 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3440, 3194 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3930, -2190, 1380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3440, 3194 }, { 0, 0 } }, 1000, 2200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -730, -2010, 440 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2949, 2211 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3010, -1970, 2800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3850, 3440 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7290, -2010, 2980 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 2949, 2539 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2760, -2010, 2980 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 2949, 2539 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7480, -1990, -1610 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3522, 3031 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 990, -1970, 2800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3850, 3440 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4990, -1970, 2800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3850, 3440 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7210, -1420, 3730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3850, 3440 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9250, -1420, 3730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3850, 3440 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 640, -1520, 350 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2949, 2211 }, { 0, 0 } }, 500, 800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -1520, 350 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2949, 2211 }, { 0, 0 } }, 500, 800 },
};

WorldCoordRoomLights D_dryfield_night_parking_lot_80180C90[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_parking_lot_80180630), D_dryfield_night_parking_lot_80180630, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_parking_lot_80180CA8[10] = {
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

WorldCollisionTrigger D_dryfield_night_parking_lot_80180FA0[12] = {
    { NULL, NULL, NULL, { 0x2870, -48, 2976, 0 }, { { -560, 0, -1024, 0 }, { 560, 0, -1024, 0 }, { -560, 0, 1024, 0 }, { 560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3040, -48, 3744, 0 }, { { 832, 0, -560, 0 }, { 832, 0, 560, 0 }, { -832, 0, -560, 0 }, { -832, 0, 560, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1001, WORLD_COLLISION_TRIGGER_ACTION_WARP, 17, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -800, -48, 192, 0 }, { { 704, 0, -560, 0 }, { 704, 0, 560, 0 }, { -704, 0, -560, 0 }, { -704, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 898, WORLD_COLLISION_TRIGGER_ACTION_WARP, 16, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6736, -48, -640, 0 }, { { 656, 0, -560, 0 }, { 656, 0, 560, 0 }, { -656, 0, -560, 0 }, { -656, 0, 560, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 861, WORLD_COLLISION_TRIGGER_ACTION_WARP, 18, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4752, -48, 1936, 0 }, { { 368, 0, -224, 0 }, { 368, 0, 224, 0 }, { -368, 0, -224, 0 }, { -368, 0, 224, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 430, WORLD_COLLISION_TRIGGER_ACTION_FACING, 52, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4640, -992, 1632, 0 }, { { -560, 1024, 0, 0 }, { 560, 1024, 0, 0 }, { -560, -1024, 0, 0 }, { 560, -1024, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -880, -64, 3184, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { 4076, 0, 401, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4672, -480, 1408, 0 }, { { 512, 0, -112, 0 }, { 512, 0, 112, 0 }, { -512, 0, -112, 0 }, { -512, 0, 112, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 50, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 18, -64, 2210, 0 }, { { 0, 0, -2160, 0 }, { 736, 0, -2160, 0 }, { -736, 0, 2160, 0 }, { 0, 0, 2160, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 2275, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2080, -64, 2592, 0 }, { { 1232, 0, -416, 0 }, { 1232, 0, 416, 0 }, { -1232, 0, -416, 0 }, { -1232, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6992, -64, -1936, 0 }, { { 1472, 0, -192, 0 }, { 1472, 0, 928, 0 }, { -1472, 0, -928, 0 }, { -1472, 0, 192, 0 } }, { 0, 4097, 0, 0 }, { 201, 0, 4091, 0 }, 1736, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5624, -64, -3000, 0 }, { { -200, 0, -1816, 0 }, { 920, 0, -1816, 0 }, { -904, 0, 1704, 0 }, { 184, 0, 1928, 0 } }, { 0, 4103, 0, 0 }, { 3857, 0, 1380, 0 }, 2031, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_night_parking_lot_80181330[2] = {
    { NULL, NULL, { -160, -1872, -2944, 0 }, { { 1508, 2896, -2822, 0 }, { 1508, -2896, -2822, 0 }, { -1509, 2896, 2821, 0 }, { -1509, -2896, 2821, 0 } }, { 3613, 0, 1931, 0 }, 4314, 1, 0 },
    { NULL, NULL, { -6018, -1792, 3071, 0 }, { { 1509, 2896, -2821, 0 }, { 1509, -2896, -2821, 0 }, { -1508, 2896, 2822, 0 }, { -1508, -2896, 2822, 0 } }, { 3613, 0, 1931, 0 }, 4314, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_dryfield_night_parking_lot_801813A8[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_300700_80165B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_parking_lot_801813CC[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_parking_lot_801813E4[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, Actor02500_D05B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_parking_lot_80181408[2] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_403900_801540E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_parking_lot_80181420[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103700_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_parking_lot_80181438[22] = {
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017BAB8, D_dryfield_night_parking_lot_801813A8 },
    { D_map_dryfield_full_8017BB28, D_dryfield_night_parking_lot_801813CC },
    { D_map_dryfield_full_8017BB78, D_dryfield_night_parking_lot_801813E4 },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017BBE8, D_dryfield_night_parking_lot_80181408 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017BC08, D_dryfield_night_parking_lot_80181420 },
};

WorldCollisionFootstepSounds D_dryfield_night_parking_lot_801814E8 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionFootstepSounds D_dryfield_night_parking_lot_801814F4 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_dryfield_night_parking_lot_80181500 = {
    0x10000051,
    0x10000053,
    0x10000055,
};

WorldCollisionSurfaceProperties D_dryfield_night_parking_lot_8018150C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_parking_lot_80181514[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_parking_lot_801814E8 },
};

WorldCollisionSurfaceProperties D_dryfield_night_parking_lot_8018151C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_parking_lot_801814F4 },
};

WorldCollisionSurfaceProperties D_dryfield_night_parking_lot_80181524[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_parking_lot_80181500 },
};

WorldCollisionSurfaceProperties D_dryfield_night_parking_lot_8018152C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_parking_lot_80181534[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_parking_lot_801814E8 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_parking_lot_8018153C[8] = {
    D_dryfield_night_parking_lot_8018150C,
    D_dryfield_night_parking_lot_8018152C,
    D_dryfield_night_parking_lot_8018151C,
    D_dryfield_night_parking_lot_80181524,
    D_dryfield_night_parking_lot_8018150C,
    D_dryfield_night_parking_lot_80181514,
    D_dryfield_night_parking_lot_80181534,
    D_dryfield_night_parking_lot_8018150C,
};

void dryfieldNightParkingLotDrawGlowsTask(Task* task)
{
    u8 mappedViewIndex;

    // The effect gate follows the mapped camera; light selection follows the logical view.
    mappedViewIndex                  = viewGetMappedIndex();
    gRoomEffectState->roomEffectMode = D_dryfield_night_parking_lot_8017EDBC[mappedViewIndex - 1];
    switch (gGameSession->location.loc.view) {
        case 2: {
            const SVECTOR* viewPoints = D_dryfield_night_parking_lot_8017EDCC;
            glowDrawFlareClipped(&viewPoints[0], 0, 0x300);
            glowDrawFlareClipped(&viewPoints[1], 0, 0x300);
            glowDrawFlareClipped(&viewPoints[2], 0, 0x330);
            glowDrawFlareClipped(&viewPoints[6], 1, 0x380);
            glowDrawFlareClipped(&viewPoints[7], 1, 0x380);
            break;
        }
        case 3: {
            const SVECTOR* viewPoints = D_dryfield_night_parking_lot_8017EDFC;
            glowDrawFlareClipped(&viewPoints[0], 1, 0x380);
            break;
        }
        case 4: {
            const SVECTOR* viewPoints = D_dryfield_night_parking_lot_8017EDEC;
            _dryfieldNightParkingLotDrawGreyCapsule(&viewPoints[0], &viewPoints[1], 0x180);
            glowDrawFlareClipped(&viewPoints[2], 1, 0x380);
            glowDrawFlareClipped(&viewPoints[3], 1, 0x380);
            break;
        }
        case 5: {
            const SVECTOR* viewPoints = D_dryfield_night_parking_lot_8017EDDC;
            glowDrawFlareClipped(&viewPoints[0], 0, 0x300);
            glowDrawFlareClipped(&viewPoints[1], 0, 0x300);
            glowDrawFlareClipped(&viewPoints[5], 1, 0x380);
            glowDrawFlareClipped(&viewPoints[6], 1, 0x380);
            break;
        }
        case 6: {
            const SVECTOR* viewPoints = D_dryfield_night_parking_lot_8017EDE4;
            glowDrawFlareClipped(&viewPoints[0], 0, 0x300);
            break;
        }
    }
}

#include "../../shared/glow_draw_flare_clipped.inc.c"

/// Prepares the GPU header and colours of one grey capsule-cap quad.
///
/// Borrows one writable `POLY_G4` through `wedge` for this call. Vertex 2 receives
/// `centreIntensity` (0..255) in each RGB byte; vertices 0, 1 and 3 are black.
/// The packet has eight payload words and an opaque, untextured Gouraud-quad
/// command. Before submission, the caller fills all four screen positions,
/// supplies the ordering-table link, enables semitransparency and queues the
/// additive blend mode. Packet storage belongs to the caller's frame arena.
static inline void _dryfieldNightParkingLotInitCapsuleWedge(POLY_G4* wedge, u8 centreIntensity)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, centreIntensity, centreIntensity, centreIntensity);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws an additive grey capsule between separate world endpoints.
///
/// Borrows the endpoints for view projection. The second camera Z / 4 depth
/// must be at least 17; the first is clamped to 16. Projection flags are not
/// tested. Each pixel radius is the signed low halfword of `radiusScale`
/// times 64 divided by that endpoint's depth. Trigonometry uses Q12 values
/// and 4096 angle units per turn, with zero down the screen; caps have a fixed
/// screen orientation rather than following the projected endpoint direction.
///
/// Lit vertices alternate grey 32/48 with frame parity and rims are black.
/// Queues six Gouraud quads and their additive blend commands in the current
/// frame arena; joining bands sort at the first endpoint's depth. Requires
/// the composed view matrix, 24 scratch bytes and sufficient packet space.
static void _dryfieldNightParkingLotDrawGreyCapsule(const SVECTOR* startWorldPoint, const SVECTOR* endWorldPoint, s32 radiusScale)
{
    GlowPointPairScratch* projection;
    POLY_G4*              quad;
    s32                   angle;
    s32                   rimAngle;
    s32                   endRimAngle;
    s32                   nextAngle;
    s32                   endRimAngleForSin;
    s32                   brightness;
    s32                   scaledRadius;
    s32                   startRadius;
    s32                   endRadius;

    projection = SCRATCH_STACK_RESERVE_BLOCK(GlowPointPairScratch);

    // Project both endpoints; only the second depth rejects the capsule.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(startWorldPoint);
    gte_rtps();
    gte_stsxy(&projection->sx0);
    gte_stszotz(&projection->otz0);
    gte_ldv0(endWorldPoint);
    gte_rtps();
    gte_stsxy(&projection->sx1);
    gte_stszotz(&projection->otz1);
    if (projection->otz1 >= GLOW_MIN_DEPTH) {
        if (projection->otz0 < GLOW_NEAR_DEPTH_CLAMP) {
            projection->otz0 = GLOW_NEAR_DEPTH_CLAMP;
        }
        scaledRadius        = (s16)radiusScale * GLOW_RADIUS_SCALE;
        startRadius         = scaledRadius / projection->otz0;
        endRadius           = scaledRadius / projection->otz1;
        angle               = 0;
        brightness          = (((u8)gDisplayState.animFrame & 1) * (1 << GLOW_BRIGHT_FLICKER_SHIFT)) | GLOW_FLICKER_BASE_INTENSITY;
        projection->radius0 = startRadius;
        projection->radius1 = endRadius;
        // Join opposing half-disc caps with two bands at the first endpoint depth.
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _dryfieldNightParkingLotInitCapsuleWedge(quad, brightness);
            quad->x0  = projection->sx0 + ((projection->radius0 * rsin(angle)) >> GLOW_TRIG_SHIFT);
            quad->y0  = projection->sy0 + ((projection->radius0 * rcos(angle)) >> GLOW_TRIG_SHIFT);
            rimAngle  = angle + GLOW_EIGHTH_TURN;
            quad->x1  = projection->sx0 + ((projection->radius0 * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y1  = projection->sy0 + ((projection->radius0 * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle = angle + GLOW_QUARTER_TURN;
            quad->x2  = projection->sx0;
            quad->y2  = projection->sy0;
            quad->x3  = projection->sx0 + ((projection->radius0 * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            quad->y3  = projection->sy0 + ((projection->radius0 * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz0);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, brightness, brightness, brightness);
            setRGB3(quad, brightness, brightness, brightness);
            quad->x0 = projection->sx0 + ((projection->radius0 * rsin((angle * 2))) >> GLOW_TRIG_SHIFT);
            quad->y0 = projection->sy0 + ((projection->radius0 * rcos((angle * 2))) >> GLOW_TRIG_SHIFT);
            quad->x1 = projection->sx1 + ((projection->radius1 * rsin((angle * 2))) >> GLOW_TRIG_SHIFT);
            quad->y1 = projection->sy1 + ((projection->radius1 * rcos((angle * 2))) >> GLOW_TRIG_SHIFT);
            quad->x2 = projection->sx0;
            quad->y2 = projection->sy0;
            quad->x3 = projection->sx1;
            quad->y3 = projection->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz0);

            endRimAngleForSin = GLOW_FULL_TURN - angle;
            quad              = gGpuPrimCursor;
            endRimAngle       = GLOW_FULL_TURN - angle;
            gGpuPrimCursor    = quad + 1;
            _dryfieldNightParkingLotInitCapsuleWedge(quad, brightness);
            quad->x0     = projection->sx1 + ((projection->radius1 * rsin(endRimAngleForSin)) >> GLOW_TRIG_SHIFT);
            quad->y0     = projection->sy1 + ((projection->radius1 * rcos(endRimAngle)) >> GLOW_TRIG_SHIFT);
            endRimAngle  = GLOW_FULL_TURN - GLOW_EIGHTH_TURN;
            endRimAngle -= angle;
            quad->x1     = projection->sx1 + ((projection->radius1 * rsin(endRimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y1     = projection->sy1 + ((projection->radius1 * rcos(endRimAngle)) >> GLOW_TRIG_SHIFT);
            endRimAngle  = GLOW_FULL_TURN - GLOW_QUARTER_TURN;
            endRimAngle -= angle;
            quad->x2     = projection->sx1;
            quad->y2     = projection->sy1;
            quad->x3     = projection->sx1 + ((projection->radius1 * rsin(endRimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y3     = projection->sy1 + ((projection->radius1 * rcos(endRimAngle)) >> GLOW_TRIG_SHIFT);
            angle        = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz1);
        } while (angle < GLOW_HALF_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowPointPairScratch);
}
