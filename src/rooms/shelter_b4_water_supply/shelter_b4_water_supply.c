#include "rooms/shelter_b4_water_supply.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/water_effects.h"
#include "../../shared/room_events.h"

static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTile(const GfxCoord* coord, s16 frameIndex, s16 radiusScale);

#define D_shelter_b4_water_supply_801826A0 (D_shelter_b4_water_supply_80182690 + 2)
#define D_shelter_b4_water_supply_801826C0 (D_shelter_b4_water_supply_80182690 + 6)
#define D_shelter_b4_water_supply_801826D0 (D_shelter_b4_water_supply_80182690 + 8)

/// Descriptor of the departure task spawned once the event block is staged.
extern TaskDesc D_shelter_b4_water_supply_801825E4;

/// The room's message table, installed by the room task's first state.
extern TaskMessageEntry D_shelter_b4_water_supply_801825F0[];

/// Task table spawned by `func_shelter_b4_water_supply_8017DA30` once the
/// valve script has run.
extern TaskDesc D_shelter_b4_water_supply_80182620[];

/// Tasks the room task's first state spawns.
extern TaskDesc D_shelter_b4_water_supply_8018263C[];

/// The room's water surfaces whose strips run along X.
extern RoomCompactWaterSurface D_shelter_b4_water_supply_8018265C[];

/// End-point pairs of the light beams the room task draws per view.
extern SVECTOR D_shelter_b4_water_supply_80182670[];
extern SVECTOR D_shelter_b4_water_supply_80182680[];

/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_shelter_b4_water_supply_801826E0[];

/// Spawn argument for the task `func_shelter_b4_water_supply_8017D7C0` starts
/// with `Task_Spawn(1, 0x31, ...)`.
extern RoomFadeStorage D_shelter_b4_water_supply_80184E34;

/// Staging save location the spawned task reads: area / `field_4` /
/// `field_1` receive the outgoing location's `field_0` / `field_2` / `field_3`.
extern RoomEventMsg D_shelter_b4_water_supply_80184E3C;

/// The staged event block, read by the departure task.
extern RoomDeparture gRoomDeparture;

/// Next byte for mixed water quad and draw-mode packets in a borrowed actor-load buffer.
///
/// Reset once per frame to the current word-aligned 0xC000-byte half, then
/// advanced by both the Z-running and X-running drawers (at most 0xC00 bytes
/// together). The buffer stays reserved until the GPU finishes the ordering table.
static u8* _gShelterB4WaterSupplyWaterPacketCursor;

static void func_shelter_b4_water_supply_8017DB18(void);
static void func_shelter_b4_water_supply_8017DD40(Task* arg0);
static void func_shelter_b4_water_supply_8017DD9C(Task* task);
static s32  func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out);
static void func_shelter_b4_water_supply_8017E5D8(Task* task);
static void func_shelter_b4_water_supply_8017ED90(Task* arg0);
static void func_shelter_b4_water_supply_8017EDD0(Task* task);

void func_shelter_b4_water_supply_8017D7C0(Task*);
s32  func_shelter_b4_water_supply_8017D970(Task*, s32, s32, s32);
s32  func_shelter_b4_water_supply_8017D978(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b4_water_supply_8017DA28(Task*, s32, s32, s32);
s32  func_shelter_b4_water_supply_8017DA30(Task* task, s32 msgId, const void* firstArg, s32);
s32  func_shelter_b4_water_supply_8017DAE4(Task*, s32, s32, s32);
void func_shelter_b4_water_supply_8017DC28(Task*);
void func_shelter_b4_water_supply_8017ED28(Task*);

extern TaskDesc D_actor_100400_80147E48;

TaskDesc D_shelter_b4_water_supply_801825E4 = { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } };

TaskMessageEntry D_shelter_b4_water_supply_801825F0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b4_water_supply_8017D978 },
    { 5105, func_shelter_b4_water_supply_8017D970 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b4_water_supply_8017DA30 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b4_water_supply_8017DA28 },
    { ROOM_MESSAGE_SOUND, func_shelter_b4_water_supply_8017DAE4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b4_water_supply_80182620[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b4_water_supply_8017DC28, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b4_water_supply_8017D7C0, { .value = 0 } },
};

/// Undisplaced Y of both water rectangles in signed world units, published to the session.
static s16 _gShelterB4WaterSupplyWaterY = -2500;

TaskDesc D_shelter_b4_water_supply_8018263C[1] = {
    { { { TASK_BODY_NONE, 96 } }, func_shelter_b4_water_supply_8017ED28, { .value = 0 } },
};

/// The Z-running water rectangle, followed by the compact descriptor's list end.
static RoomCompactWaterSurface _gShelterB4WaterSupplyWaterWaveSurfaces[] = {
    { 9100, -0x364C, 1800, 0x2EE0, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

RoomCompactWaterSurface D_shelter_b4_water_supply_8018265C[2] = {
    { 4400, -1900, 8150, 1800, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

SVECTOR D_shelter_b4_water_supply_80182670[2] = {
    { 0x36C4, -5130, -1910, 0 },
    { 0x337C, -4540, -1910, 0 },
};

SVECTOR D_shelter_b4_water_supply_80182680[2] = {
    { 6520, -3790, -1910, 0 },
    { 5500, -3790, -1910, 0 },
};

// Indexed views below share one contiguous table.
SVECTOR D_shelter_b4_water_supply_80182690[10] = {
    { 10480, -3840, -100, 0 },
    { 9500, -3750, -100, 0 },
    { 1990, -5790, -80, 0 },
    { 1000, -5790, -80, 0 },
    { 10920, -3790, -3560, 0 },
    { 10920, -3790, -4550, 0 },
    { 10920, -3790, -11570, 0 },
    { 10920, -3790, -12550, 0 },
    { 9130, -3790, -7950, 0 },
    { 9130, -3790, -6970, 0 },
};

SVECTOR D_shelter_b4_water_supply_801826E0[2] = { 0 };

#include "../../shared/room_visual_effects_disc_data.inc.c"

// Four 16-byte direction-facing rows used by Gp_MsgPlayerDirFacing.
u8 D_shelter_b4_water_supply_801826FC[64] = {
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
};

u8* D_shelter_b4_water_supply_8018273C[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b4_water_supply_80182740[1] = { 11 };

DirectionWarpEntry D_shelter_b4_water_supply_80182744[3] = {
    { { { .word = 1024 }, 640, -4000, -1024 }, { 0, 0, 0, 0 }, { { .word = 768 }, 5350, -2000, -1600 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x542E0005, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 9450, -2000, -0x32C8 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 0x283C, -2000, -0x3520 }, { 0, 0, 0, 0 }, 0x542E0002, 0x542E0001, DIRECTION_WARP_SOUND_NONE, 11, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x3CF0, -3600, -1000 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x3CF0, -3600, -1000 }, { 0, 0, 0, 0 }, 0x542E0004, DIRECTION_WARP_SOUND_NONE, 0x542E0006, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_WATER },
};

static SVECTOR _gShelterB4WaterSupplyCollision0587CNormals[12] = {
#include "assets/shelter_b4_water_supply_collision_0587C_normals.inc"
};

static SVECTOR _gShelterB4WaterSupplyCollision0587CVerts[59] = {
#include "assets/shelter_b4_water_supply_collision_0587C_verts.inc"
};

static WorldCollisionGridFace _gShelterB4WaterSupplyCollision0587CFaces[45] = {
#include "assets/shelter_b4_water_supply_collision_0587C_faces.inc"
};

static s16 _gShelterB4WaterSupplyCollision0587CCells[222] = {
#include "assets/shelter_b4_water_supply_collision_0587C_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB4WaterSupplyCollision0587CCells[i])
static s16* _gShelterB4WaterSupplyCollision0587CTable[16] = {
#include "assets/shelter_b4_water_supply_collision_0587C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b4_water_supply_80182E3C = { NULL, _gShelterB4WaterSupplyCollision0587CNormals, _gShelterB4WaterSupplyCollision0587CVerts, _gShelterB4WaterSupplyCollision0587CFaces, _gShelterB4WaterSupplyCollision0587CTable, -400, 0x364C, 4, 4, 4000, 45 };

ViewCamera D_shelter_b4_water_supply_80182E60[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -6828, 0x7D00, 6785 } }, 447 },
    { { { { -1049, 0, -3959 }, { 841, 4002, -223 }, { 3868, -870, -1025 } }, { -0x2E41, 3890, 333 } }, 230 },
    { { { { -770, 0, -4022 }, { 44, 4095, -8 }, { 4022, -45, -770 } }, { -9469, 2988, 580 } }, 230 },
    { { { { -1205, 0, -3914 }, { -191, 4091, 58 }, { 3909, 200, -1204 } }, { -6858, 2988, 402 } }, 230 },
    { { { { -943, 0, 3985 }, { 65, 4095, 15 }, { -3985, 67, -943 } }, { -0x291C, 2925, 450 } }, 230 },
    { { { { -1040, 0, 3961 }, { -254, 4087, -66 }, { -3953, -263, -1038 } }, { -7900, 2925, 420 } }, 257 },
    { { { { -608, 0, -4050 }, { -1926, 3602, 289 }, { 3562, 1948, -535 } }, { 1769, 6119, 698 } }, 230 },
    { { { { 3988, 0, 933 }, { 42, 4091, -182 }, { -932, 187, 3983 } }, { -0x2898, 2969, 7455 } }, 230 },
    { { { { -4028, 0, 739 }, { 15, 4095, 83 }, { -739, 85, -4027 } }, { -0x28D4, 2936, 3528 } }, 230 },
    { { { { -4027, 0, 745 }, { 0, 4095, 1 }, { -745, 1, -4027 } }, { -0x28BD, 2952, 6769 } }, 230 },
    { { { { -3911, 0, 1216 }, { 140, 4068, 451 }, { -1208, 473, -3884 } }, { -0x2913, 3254, 9638 } }, 230 },
};

SpriteBatch D_shelter_b4_water_supply_80182FEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_water_supply_80182FFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_water_supply_8018300C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_water_supply_8018301C[34] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 646, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 537, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -72, 625, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -88, 624, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -120, 647, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -120, 601, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -104, 628, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -56, 650, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -24, 650, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, 8, 650, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 40, 650, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 56, 650, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 72, 650, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -120, 578, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -88, 650, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -120, 545, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -88, 575, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -56, 600, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -24, 600, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 8, 600, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 40, 600, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 72, 600, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -120, 575, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -80, 575, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -40, 575, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, 0, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, 40, 575, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 104, 80, 575, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -120, 500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -80, 500, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -40, 500, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 0, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 40, 500, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 500, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_water_supply_801832C4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_water_supply_801832DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_water_supply_801832EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_water_supply_801832FC[108] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 1200, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, -8, 1200, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, -8, 1200, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -8, 1200, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, -8, 1212, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 1225, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, -8, 1225, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 0, 1200, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 0, 1200, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, 0, 1200, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 0, 1212, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 0, 1212, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 8, 1200, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 8, 1200, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, 8, 1200, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 8, 1200, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 24, 8, 1200, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 16, 1175, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 16, 1177, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 16, 1177, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 16, 1177, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 16, 1177, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 16, 1177, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 32, 1162, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 1162, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 32, 1162, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 32, 1162, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 32, 1162, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 32, 1162, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -56, 2517, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -56, 2500, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -48, 2750, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -48, 2525, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -40, 2800, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2500, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -32, 2875, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2500, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -24, 2565, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -24, 2500, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -16, 2750, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -16, 2500, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -8, 2500, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -120, 2500, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -120, 1225, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -96, 2500, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -96, 1268, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, -72, 1576, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -72, 1503, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -120, 2125, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -96, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -72, 2250, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -48, 2250, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -24, 2325, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -24, 1950, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -56, 1875, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -88, 1250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -120, 1250, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, -120, 1125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, -88, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, -56, 1125, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -24, 1125, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -120, 850, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -88, 850, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -56, 850, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 72, -24, 850, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 104, -120, 750, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 104, -88, 750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 104, -56, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -24, 750, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -120, 675, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -88, 675, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -56, 675, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, -56, 1500, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -48, 2500, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -64, 1656, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, -64, 1375, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -64, 1375, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -64, 2250, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -64, 2250, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -64, 1459, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -48, 1524, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, -32, 1689, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -80, 1281, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -80, 1341, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -80, 1495, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -80, 1468, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 1372, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -80, 1256, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -64, -104, 1129, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, -104, 1225, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, -104, 1217, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -104, 1200, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -48, 2500, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -32, 2500, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -16, 2500, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 48, 538, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 530, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 72, 509, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 72, 675, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 88, 506, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 40, 550, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 48, 575, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 56, 575, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 56, 535, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 72, 575, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 72, 523, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 64, 575, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 88, 522, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_water_supply_80183B6C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 29, 0, 0, { 3, 0 } },
    { 29, 43, 0, 0, { 0, 0 } },
    { 72, 23, 0, 0, { 2, 0 } },
    { 95, 13, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_water_supply_80183B9C[47] = {
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -88, 1100, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -56, 1308, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -24, 1324, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, 8, 1125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 24, 1283, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -88, 903, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -56, 1050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -24, 1050, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, 8, 1050, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -88, 1000, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -56, 1000, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -24, 1000, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 8, 1000, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 48, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, -88, 1000, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, -88, 1000, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, -56, 1000, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, -56, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, -24, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, -24, 1000, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 8, 1000, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 8, 1000, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 48, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 136, 48, 1000, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, 88, 1000, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, 88, 1000, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -88, 1125, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -56, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -24, 1125, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 8, 1125, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, -88, 1000, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, -56, 1000, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, -24, 1000, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, 8, 1000, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, 40, 1000, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, -88, 1000, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, -40, 1000, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 8, 1000, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, 56, 1000, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, -88, 1000, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, -40, 1000, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, 8, 1000, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -120, 56, 1000, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, -88, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, -40, 1000, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 1000, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -160, 56, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_water_supply_80183F48[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_water_supply_80183F60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_water_supply_80183F70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_water_supply_80183F80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b4_water_supply_80183F90[11] = {
    { { .empty = D_shelter_b4_water_supply_80182FEC }, D_shelter_b4_water_supply_80182FEC, NULL },
    { { .empty = D_shelter_b4_water_supply_80182FFC }, D_shelter_b4_water_supply_80182FFC, NULL },
    { { .empty = D_shelter_b4_water_supply_8018300C }, D_shelter_b4_water_supply_8018300C, NULL },
    { { .elements = D_shelter_b4_water_supply_8018301C }, D_shelter_b4_water_supply_801832C4, NULL },
    { { .empty = D_shelter_b4_water_supply_801832DC }, D_shelter_b4_water_supply_801832DC, NULL },
    { { .empty = D_shelter_b4_water_supply_801832EC }, D_shelter_b4_water_supply_801832EC, NULL },
    { { .elements = D_shelter_b4_water_supply_801832FC }, D_shelter_b4_water_supply_80183B6C, NULL },
    { { .elements = D_shelter_b4_water_supply_80183B9C }, D_shelter_b4_water_supply_80183F48, NULL },
    { { .empty = D_shelter_b4_water_supply_80183F60 }, D_shelter_b4_water_supply_80183F60, NULL },
    { { .empty = D_shelter_b4_water_supply_80183F70 }, D_shelter_b4_water_supply_80183F70, NULL },
    { { .empty = D_shelter_b4_water_supply_80183F80 }, D_shelter_b4_water_supply_80183F80, NULL },
};

WorldCoordPointLight D_shelter_b4_water_supply_80184014[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2710, -3800, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x393A, -5245, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 1750, 2250 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1930, -7250, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 2785 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2710, -3800, -4000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9500, -3800, -7500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2904, -3800, -0x2EE0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3520, -4845, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6000, -3800, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -5785, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2C24, -3800, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
};

WorldCoordRoomLights D_shelter_b4_water_supply_801843D4 = { 0, NULL, ARRAY_SIZE(D_shelter_b4_water_supply_80184014), D_shelter_b4_water_supply_80184014, 0, NULL };

WorldCollisionTrigger D_shelter_b4_water_supply_801843EC[18] = {
    { NULL, NULL, NULL, { 0x359F, -3328, -1152, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3541, -3392, -1089, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2DE0, -3361, -1056, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2D60, -3297, -992, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8608, -3457, -1024, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8449, -3328, -992, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5760, -3329, -1152, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5632, -3424, -1152, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3264, -3392, -992, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3104, -3393, -960, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2730, -3201, -2064, 0 }, { { -1232, -4160, 16, 0 }, { 1232, -4160, -16, 0 }, { -1232, 4160, 16, 0 }, { 1232, 4160, -16, 0 } }, { -54, 0, -4107, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 8, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2741, -3424, -2144, 0 }, { { 1200, -4160, -16, 0 }, { -1200, -4160, 16, 0 }, { 1200, 4160, -16, 0 }, { -1200, 4160, 16, 0 } }, { 54, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 4, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2711, -3424, -5617, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2720, -3552, -5488, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -3425, -8576, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 10, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9952, -3296, -8768, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 9, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -3617, -0x2EE0, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 10, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -3616, -0x2E40, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 11, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b4_water_supply_80184944[7] = {
    { NULL, NULL, NULL, { 2496, -4063, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 170, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4800, -2056, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4095, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_FACING, 170, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2FA0, -2064, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4097, 0, -39, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_FACING, 184, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3640, -3647, -1024, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 184, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9536, -2072, -0x32D0, 0 }, { { -416, 0, -944, 0 }, { 416, 0, -944, 0 }, { -416, 0, 944, 0 }, { 416, 0, 944, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_WARP, 45, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -4062, -1120, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 44, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3D40, -3645, -1024, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 32, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b4_water_supply_80184B58[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_water_supply_80184B70[3] = {
    { 70, 70, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107000_8013F5F0 },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_80153EC8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_water_supply_80184B94[2] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_water_supply_80184BAC[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b4_water_supply_80184BC4[2] = {
    { 4, 0, 0, 7000, -2000, -1000, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_water_supply_80184BE4[5] = {
    { 72, 0, 0, 0x2710, -2000, -1900, 0, 0, 2, 3, 0 },
    { 72, 0, 0, 7800, -2000, -1000, 2600, 0, 2, 3, 0 },
    { 70, 0, 0, 9500, -2000, -900, 2048, 0, 0, 2, 0 },
    { 70, 0, 0, 0x27D8, -2000, -5400, -200, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_water_supply_80184C34[4] = {
    { 24, 0, 0, 0x2710, -2000, -1000, 0, 0, 0, 2, 0 },
    { 24, 0, 0, 0x2710, -2000, -7000, 0, 0, 0, 2, 0 },
    { 24, 0, 0, 0x2710, -2000, -4000, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_water_supply_80184C74[3] = {
    { 4, 0, 0, 0x2710, -2000, -1000, 2048, 0, 0, 2, 0 },
    { 4, 0, 0, 0x2710, -2000, -6500, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b4_water_supply_80184CA4[12] = {
    { NULL, NULL },
    { D_shelter_b4_water_supply_80184BC4, D_shelter_b4_water_supply_80184B58 },
    { D_shelter_b4_water_supply_80184BE4, D_shelter_b4_water_supply_80184B70 },
    { D_shelter_b4_water_supply_80184C34, D_shelter_b4_water_supply_80184B94 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_water_supply_80184C74, D_shelter_b4_water_supply_80184BAC },
};

WorldCollisionOccluder D_shelter_b4_water_supply_80184D04[2] = {
    { NULL, NULL, { 4608, -3408, -7120, 0 }, { { -4256, 4336, -4880, 0 }, { 4256, 4336, 4880, 0 }, { -4256, -4336, -4880, 0 }, { 4256, -4336, 4880, 0 } }, { -3089, 0, 2693, 0 }, 7781, 1, 0 },
    { NULL, NULL, { 0x371F, -3712, -7649, 0 }, { { 3162, 4336, -5651, 0 }, { -3161, 4336, 5652, 0 }, { 3162, -4336, -5651, 0 }, { -3161, -4336, 5652, 0 } }, { -3578, 0, -2002, 0 }, 7781, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b4_water_supply_80184D7C[12] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b4_water_supply_80184D7C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 520, 596, 596, 567 } },
    { .color = { 516, 557, 558, 541 } },
    { .color = { 513, 529, 544, 524 } },
    { .color = { 435, 519, 535, 489 } },
    { .color = { 520, 561, 556, 545 } },
    { .color = { 517, 555, 540, 538 } },
    { .color = { 519, 548, 538, 535 } },
    { .color = { 420, 480, 498, 459 } },
};

WorldCollisionFootstepSounds D_shelter_b4_water_supply_80184DDC = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_shelter_b4_water_supply_80184DE8 = {
    0x10000025,
    0x10000027,
    0x10000029,
};

WorldCollisionSurfaceProperties D_shelter_b4_water_supply_80184DF4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b4_water_supply_80184DFC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_water_supply_80184DDC },
};

WorldCollisionSurfaceProperties D_shelter_b4_water_supply_80184E04[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_water_supply_80184DE8 },
};

WorldCollisionSurfaceProperties D_shelter_b4_water_supply_80184E0C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_water_supply_80184DDC },
};

WorldCollisionSurfaceProperties* D_shelter_b4_water_supply_80184E14[8] = {
    D_shelter_b4_water_supply_80184DF4,
    D_shelter_b4_water_supply_80184DFC,
    D_shelter_b4_water_supply_80184E04,
    D_shelter_b4_water_supply_80184E0C,
    D_shelter_b4_water_supply_80184DF4,
    D_shelter_b4_water_supply_80184DF4,
    D_shelter_b4_water_supply_80184DF4,
    D_shelter_b4_water_supply_80184DF4,
};

RoomFadeStorage D_shelter_b4_water_supply_80184E34 = { 0 };

RoomEventMsg D_shelter_b4_water_supply_80184E3C = { 0 };

RoomDeparture gRoomDeparture = { 0 };

#include "../../shared/room_event_departure_task.inc.c"

/// The room task's state table, dispatched by
/// `func_shelter_b4_water_supply_8017DDA4` from a stack copy: install the
/// message table and spawn the room's tasks, idle, then kill.
static const TaskFuncTable3 D_shelter_b4_water_supply_8017D5D8 = {
    {
        func_shelter_b4_water_supply_8017DD40,
        func_shelter_b4_water_supply_8017DD9C,
        taskKill,
    },
};

void func_shelter_b4_water_supply_8017D7C0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(arg0->spawnArg1.value, 0);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                D_80114D08                     = 0xA;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_TriggerPeIfArmed();
            D_shelter_b4_water_supply_80184E34.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_shelter_b4_water_supply_80184E34.fade.phase      = SCREEN_FADE_RUNNING;
            D_shelter_b4_water_supply_80184E34.fade.rampFrames = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_shelter_b4_water_supply_80184E34.fade);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B4_WATER_SUPPLY_EXIT_TRANSIT, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(SOUND_SHELTER_B4_WATER_SUPPLY_EXIT_TRANSIT) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b4_water_supply_80184E3C.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b4_water_supply_80184E3C.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_shelter_b4_water_supply_80184E3C.areaId)[1];
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_water_supply_8017D970(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Copies the location at `src` into `dst` and passes both to `func_map_shelter_80179A04`.
/// When the leading halfword of `src` is 0x2C it returns 0, first staging three
/// bytes of `dst` and spawning from the task table unless `src->queryOnly` is set;
/// any other location returns 1.
s32 func_shelter_b4_water_supply_8017D978(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_B4_UPPER_SEWER) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b4_water_supply_80184E3C.warp              = (u8)dst->areaId;
            D_shelter_b4_water_supply_80184E3C.field_4           = dst->warp;
            ((u8*)&D_shelter_b4_water_supply_80184E3C.areaId)[1] = dst->room;
            taskSpawnFromTable(D_shelter_b4_water_supply_80182620, 1, 4, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_water_supply_8017DA28(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for slot-7 msg `0x13EF` in `D_shelter_b4_water_supply_801825F0`:
/// the directed action on the water-supply valve (`actionId` 0xA / `argument`
/// 0x20).
s32 func_shelter_b4_water_supply_8017DA30(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 0xA) {
        if (request->argument == 0x20) {
            if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) != 0) {
                if (gameFlagGetNibble(GAME_FLAG_WATER_SUPPLY_VALVE_FIRST_USE) != 0) {
                    func_shelter_b4_water_supply_8017DB18();
                } else {
                    gameFlagSetNibble(GAME_FLAG_WATER_SUPPLY_VALVE_FIRST_USE, 1);
                    Gp_MsgPlayerWeapon(0);
                    Gp_RunCapCmd1(3);
                    taskSpawnFromTable(D_shelter_b4_water_supply_80182620, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(1);
                gameFlagSetNibble(GAME_FLAG_MAP_MARK_WATER, 2);
            }
        }
    }
    return 0;
}

s32 func_shelter_b4_water_supply_8017DAE4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 6) {
        sndEvtRequestScriptStart(0x542E0000 | 6, 0, 0);
    }
    return 0;
}

static void func_shelter_b4_water_supply_8017DB18(void)
{
    RoomDeparture  work;
    RoomEventMsg   param;
    RoomDeparture* wp;
    s32            (*resolve)(RoomEventMsg*, RoomEventMsg*) = func_shelter_b4_water_supply_8017DDFC;

    work.stage    = GAME_STAGE_DRYFIELD_NIGHT;
    work.area     = GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE;
    work.warp     = 3;
    work.room     = 1;
    work.sndEvent = 0x542E0003;
    work.facing   = 0x400;
    Gp_MsgPlayerWeapon(0);
    wp              = &work;
    param.areaId    = wp->area;
    param.warp      = wp->warp;
    param.room      = wp->room;
    param.queryOnly = ROOM_EVENT_EXECUTE;
    resolve(&param, &param);
    wp->area       = param.areaId;
    wp->warp       = param.warp;
    wp->room       = param.room;
    gRoomDeparture = work;
    taskSpawnFromTable(&D_shelter_b4_water_supply_801825E4, 0, 0, 0);
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL && gameFlagGetNibble(GAME_FLAG_0CF) == 0) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 6);
    }
}

void func_shelter_b4_water_supply_8017DC28(Task* arg0)
{
    RoomDeparture work;
    RoomEventMsg  param;
    s32           (*resolve)(RoomEventMsg*, RoomEventMsg*);

    if (Gp_CapBusy() == 0) {
        resolve       = func_shelter_b4_water_supply_8017DDFC;
        work.stage    = GAME_STAGE_DRYFIELD_NIGHT;
        work.area     = GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE;
        work.warp     = 3;
        work.room     = 1;
        work.sndEvent = 0x542E0003;
        work.facing   = 0x400;
        Gp_MsgPlayerWeapon(0);
        param.areaId    = work.area;
        param.warp      = work.warp;
        param.room      = work.room;
        param.queryOnly = ROOM_EVENT_EXECUTE;
        resolve(&param, &param);
        work.area      = param.areaId;
        work.warp      = param.warp;
        work.room      = param.room;
        gRoomDeparture = work;
        taskSpawnFromTable(&D_shelter_b4_water_supply_801825E4, 0, 0, 0);
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL && gameFlagGetNibble(GAME_FLAG_0CF) == 0) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 6);
        }
        taskKill(arg0);
    }
}

/// The room task's first state: installs the room's message table, registers
/// the task in game pointer slot 7, spawns the room's tasks and advances.
static void func_shelter_b4_water_supply_8017DD40(Task* arg0)
{
    arg0->msgTable = D_shelter_b4_water_supply_801825F0;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_shelter_b4_water_supply_8018263C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state.
static void func_shelter_b4_water_supply_8017DD9C(Task* task)
{
}

/// The room task: copies the three-state table
/// `D_shelter_b4_water_supply_8017D5D8` onto the stack and runs the entry for
/// the task's current state.
void func_shelter_b4_water_supply_8017DDA4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_water_supply_8017D5D8;
    sp.funcs[task->state](task);
}

/// Message 0x20: unless a report-only query, answers in `room` from
/// nibbles 0x51 (1 when set, 2 when clear) and 0x53 (adds 2 when set).
/// Always returns 1 (not consumed).
static s32 func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->areaId == GAME_AREA_SHELTER_B2_BREEDING_ROOM && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
            out->room = 2;
        } else {
            out->room = 1;
        }
        if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
            out->room = (u8)out->room + 2;
        }
    }
    return 1;
}

#define WATER_WAVE_STRIPS_SURFACE_T RoomCompactWaterSurface
/// Tests the list terminator in a `RoomCompactWaterSurface` descriptor.
///
/// Reads the signed 16-bit `listMarker` from a valid descriptor once and
/// returns an int (0 drawable entry, 1 list end), without reading geometry.
/// Captures no caller locals and adds no side effects. The shared strip
/// include consumes and undefines this override; see its predicate contract.
#define WATER_WAVE_STRIPS_IS_LIST_END(surface) ((surface)->listMarker == WATER_SURFACE_LIST_END)
/// Binds the readable Z-running rectangle list, including its in-bounds terminator.
///
/// Supplies a `RoomCompactWaterSurface*` without side effects or captured locals.
/// The shared strip include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_SURFACES _gShelterB4WaterSupplyWaterWaveSurfaces
/// Binds the undisplaced water Y in signed world units.
///
/// Read once per draw from the room's `s16`; no side effects or captured locals.
/// The shared strip include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_HEIGHT _gShelterB4WaterSupplyWaterY
/// Binds the writable byte cursor for mixed water quad and draw-mode packets.
///
/// A stable `u8*` lvalue, read and advanced repeatedly; requires word alignment
/// and a borrowed arena reserved until GPU consumption. The shared strip
/// include consumes and undefines this binding.
/// The caller resets it once before both drawers, which append in draw order.
#define WATER_WAVE_STRIPS_PACKET_CURSOR _gShelterB4WaterSupplyWaterPacketCursor
/// Scales the seam's sine displacement to -64..64 world-coordinate Y units.
///
/// Integer shift count for `water_wave_strips.inc.c`; see its configuration
/// contract. The include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT 6
#include "../../shared/water_wave_strips.inc.c"

/// Draws each surface in `D_shelter_b4_water_supply_8018265C` at height
/// `_gShelterB4WaterSupplyWaterY` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along Z, projected through the view matrix.
/// The seam between the strips is lifted by a sine wave that runs along X and
/// scrolls with the display frame counter. The outer edges are coloured
/// (0x80, 0, 0) and the seam (0x20, 0x20, 0x20); each quad is followed by a
/// draw-mode packet selecting blend mode 2. Quads the projection flags as
/// invalid are skipped. The per-surface values live in a work block pushed on
/// the scratchpad stack for the duration of the call. `task`, the water task
/// whose drawing state calls it, is unused.
static void func_shelter_b4_water_supply_8017E5D8(Task* task)
{
    SVECTOR                  v0, v1, v2, v3;
    long                     sxy0, sxy1, sxy2, sxy3;
    long                     p, flag;
    s32                      phase;
    RoomCompactWaterSurface* surface;
    WaterQuadScratch*        scratch;
    WaterQuadScratch*        scratchEnd;
    POLY_G4*                 poly;
    DR_MODE*                 dr;
    s32                      otz;
    s32                      i;

    surface                    = D_shelter_b4_water_supply_8018265C;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    phase                      = -(gDisplayState.animFrame * 16);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    scratch->y = _gShelterB4WaterSupplyWaterY;
    for (; surface->listMarker != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width / 16;
        scratch->dz = surface->depth / 2;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (i = 0; i < 16; i++) {
            v0.vx            = scratch->x + scratch->dx * i;
            v0.vy            = scratch->y;
            v0.vz            = scratch->z;
            v1.vx            = scratch->x + scratch->dx * (i + 1);
            v1.vy            = scratch->y;
            v1.vz            = scratch->z;
            scratch->yOffset = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx            = scratch->x + scratch->dx * i;
            v2.vy            = scratch->y + scratch->yOffset;
            v2.vz            = scratch->z + scratch->dz;
            scratch->yOffset = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx            = scratch->x + scratch->dx * (i + 1);
            v3.vy            = scratch->y + scratch->yOffset;
            v3.vz            = scratch->z + scratch->dz;
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                                    = (POLY_G4*)_gShelterB4WaterSupplyWaterPacketCursor;
                _gShelterB4WaterSupplyWaterPacketCursor = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0x80;
                poly->r1                       = 0x80;
                poly->g0                       = 0;
                poly->b0                       = 0;
                poly->g1                       = 0;
                poly->b1                       = 0;
                poly->r2                       = 0x20;
                poly->g2                       = 0x20;
                poly->b2                       = 0x20;
                poly->r3                       = 0x20;
                poly->g3                       = 0x20;
                poly->b3                       = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                                      = (DR_MODE*)_gShelterB4WaterSupplyWaterPacketCursor;
                _gShelterB4WaterSupplyWaterPacketCursor = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            scratch->yOffset = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx            = scratch->x + scratch->dx * i;
            v0.vy            = scratch->y + scratch->yOffset;
            v0.vz            = scratch->z + scratch->dz;
            scratch->yOffset = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx            = scratch->x + scratch->dx * (i + 1);
            v1.vy            = scratch->y + scratch->yOffset;
            v1.vz            = scratch->z + scratch->dz;
            v2.vx            = scratch->x + scratch->dx * i;
            v2.vy            = scratch->y;
            v2.vz            = scratch->z + scratch->dz * 2;
            v3.vx            = scratch->x + scratch->dx * (i + 1);
            v3.vy            = scratch->y;
            v3.vz            = scratch->z + scratch->dz * 2;
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                                    = (POLY_G4*)_gShelterB4WaterSupplyWaterPacketCursor;
                _gShelterB4WaterSupplyWaterPacketCursor = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r2                       = 0x80;
                poly->r3                       = 0x80;
                poly->g2                       = 0;
                poly->b2                       = 0;
                poly->g3                       = 0;
                poly->b3                       = 0;
                poly->r0                       = 0x20;
                poly->g0                       = 0x20;
                poly->b0                       = 0x20;
                poly->r1                       = 0x20;
                poly->g1                       = 0x20;
                poly->b1                       = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                                      = (DR_MODE*)_gShelterB4WaterSupplyWaterPacketCursor;
                _gShelterB4WaterSupplyWaterPacketCursor = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

/// The water task: runs its current state - `func_shelter_b4_water_supply_8017ED90`
/// once, then `func_shelter_b4_water_supply_8017EDD0`, which draws the surfaces -
/// and each tick publishes the room's water height to the session.
void func_shelter_b4_water_supply_8017ED28(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_water_supply_8017ED90, func_shelter_b4_water_supply_8017EDD0 };

    states[task->state](task);
    gGameSession->waterY = _gShelterB4WaterSupplyWaterY;
}

/// The water task's opening state: clears the session's `field_80` or
/// `field_7E`, chosen by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType`, and advances the task to its next state.
static void func_shelter_b4_water_supply_8017ED90(Task* arg0)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The water task's drawing state: points the primitive cursor
/// `_gShelterB4WaterSupplyWaterPacketCursor` at the current buffer's 0xC000-byte
/// slice of one of two primitive areas, chosen by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType`, then draws both
/// lists of water surfaces.
static void func_shelter_b4_water_supply_8017EDD0(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        _gShelterB4WaterSupplyWaterPacketCursor = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        _gShelterB4WaterSupplyWaterPacketCursor = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    _waterDrawWaveStrips(task);
    func_shelter_b4_water_supply_8017E5D8(task);
}

/// Room task. State 0 installs five effect ids in the shared effect-id slots,
/// records the world positions of parts 14 and 17 of the slot-3 task's model,
/// and advances. Later states, while no event is running and `waterY` is below
/// that model's root, spawn each of two effects at water level under each part
/// with odds that grow with how far the part moved since last frame. Every
/// frame it then draws the light beams the current view selects, through
/// `glowDrawDimGreyCapsule`.
void func_shelter_b4_water_supply_8017EE54(Task* arg0)
{
    Task*       ctl;
    EffectWork* work;
    GfxCoord*   ctlCoords;
    GfxCoord*   part;
    GfxCoord    surface;
    s32         i;
    u32         rnd;

    work      = arg0->spawnArg2.pointer;
    ctl       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    ctlCoords = ctl->extra.tmd->coords;
    if (arg0->state == 0) {
        gRoomEffectWaterRippleId  = EFFECT_SHELTER_B4_WATER_SUPPLY_WATER_RIPPLE;
        gRoomEffectWaterSprayId   = EFFECT_SHELTER_B4_WATER_SUPPLY_WATER_SPRAY;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B4_WATER_SUPPLY_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B4_WATER_SUPPLY_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B4_WATER_SUPPLY_ORANGE_BURST_2;
        arg0->state               = 1;
        for (i = 0; i < 2; i++) {
            part                                     = &ctl->extra.tmd->coords[14 + i * 3];
            D_shelter_b4_water_supply_801826E0[i].vx = part->workm.t[0];
            D_shelter_b4_water_supply_801826E0[i].vy = part->workm.t[1];
            D_shelter_b4_water_supply_801826E0[i].vz = part->workm.t[2];
        }
    } else if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && gGameSession->waterY < ctlCoords->coord.t[1]) {
        i = 0;
        for (; i < 2; i++) {
            part = &ctl->extra.tmd->coords[14 + i * 3];
            actorRenderComposeCoord(part);
            // The work block's `angle` holds the splash strength, this task's spawn odds
            // out of 0x200: the part's movement since last frame, raised by 0x20 for the
            // ripple roll only.
            work->angle = ABS(D_shelter_b4_water_supply_801826E0[i].vx - part->workm.t[0]) +
                          ABS(D_shelter_b4_water_supply_801826E0[i].vy - part->workm.t[1]) +
                          ABS(D_shelter_b4_water_supply_801826E0[i].vz - part->workm.t[2]) + 0x20;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &part->workm, &surface.coord);
            surface.parent       = &gGfxViewCoord;
            surface.coord.t[1]   = gGameSession->waterY;
            surface.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&surface);
            rnd = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT);
            if ((s32)((rnd >> 16) & 0x1FF) < work->angle) {
                Gp_SpawnEff(gRoomEffectWaterRippleId, &surface, 0x40, 0);
            }
            work->angle -= 0x20;
            rnd          = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT);
            if ((s32)((rnd >> 16) & 0x1FF) < work->angle) {
                Gp_SpawnEff(gRoomEffectWaterSprayId, &surface, 0x1202180, 0);
            }
            D_shelter_b4_water_supply_801826E0[i].vx = part->workm.t[0];
            D_shelter_b4_water_supply_801826E0[i].vy = part->workm.t[1];
            D_shelter_b4_water_supply_801826E0[i].vz = part->workm.t[2];
        }
    }
    switch ((u8)viewGetMappedIndex()) {
        case 4:
            glowDrawDimGreyCapsule(&D_shelter_b4_water_supply_80182690[0], 0x200, 0);
            glowDrawDimGreyCapsule(&D_shelter_b4_water_supply_80182690[4], 0x200, 0x800);
        case 2:
        case 3:
            glowDrawDimGreyCapsule(D_shelter_b4_water_supply_80182670, 0x200, 0x800);
            break;
        case 6:
            glowDrawDimGreyCapsule(D_shelter_b4_water_supply_801826A0, 0x200, 0);
        case 5:
            glowDrawDimGreyCapsule(D_shelter_b4_water_supply_80182680, 0x200, 0x800);
            break;
        case 7:
            glowDrawDimGreyCapsule(D_shelter_b4_water_supply_801826A0, 0x200, 0);
            break;
        case 8:
            glowDrawDimGreyCapsule(&D_shelter_b4_water_supply_80182690[0], 0x200, 0);
            glowDrawDimGreyCapsule(&D_shelter_b4_water_supply_80182690[4], 0x200, 0x800);
            break;
        case 9:
            glowDrawDimGreyCapsule(D_shelter_b4_water_supply_801826D0, 0x200, 0x800);
        case 10:
        case 11:
            glowDrawDimGreyCapsule(D_shelter_b4_water_supply_801826C0, 0x200, 0x800);
            break;
    }
}

#include "../../shared/water_ripple_task.inc.c"

void func_shelter_b4_water_supply_8017F24C(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task.inc.c"

void func_shelter_b4_water_supply_8017F6D4(Task* task)
{
    _waterDriftTask(task);
}

#include "../../shared/water_spin.inc.c"

#include "../../shared/water_tile.inc.c"

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b4_water_supply_801809DC(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b4_water_supply_80180F34(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b4_water_supply_80181B94(Task* arg0)
{
    _roomVisualEffectsFlyingOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
