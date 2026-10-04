#include "rooms/dryfield_night_underpass.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_variants.h"
#include "../../shared/underpass_switches.h"

extern TaskDesc         gUnderpassSwitchTaskDesc[];
extern TaskMessageEntry D_dryfield_night_underpass_8017DCF0[];
extern SVECTOR          D_dryfield_night_underpass_8017DD20[8];
extern s16              D_dryfield_night_underpass_8017DD60[8];

s32 func_dryfield_night_underpass_8017D900(Task*, s32, s32, s32);
s32 func_dryfield_night_underpass_8017D908(Task*, s32, s32, s32);

extern WorldCollisionGrid     D_dryfield_night_underpass_8017E6D4[1];
extern WorldCollisionOccluder D_dryfield_night_underpass_8017FBE0[3];
extern WorldCollisionTrigger  D_dryfield_night_underpass_8017F558[16];
extern WorldCollisionTrigger  D_dryfield_night_underpass_8017FA18[6];
extern WorldCoordRoomLights   D_dryfield_night_underpass_8017FFF4[1];
extern WorldCoordRoomLights   D_dryfield_night_underpass_8018024C[1];

TaskDesc gUnderpassSwitchTaskDesc[2] = {
    { { { TASK_BODY_NONE, 32 } }, underpassSwitchTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_dryfield_night_underpass_8017DCF0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantUnderpassMsg },
    { 5105, func_dryfield_night_underpass_8017D900 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_underpass_8017D908 },
    { ROOM_MESSAGE_COMMAND, underpassSwitchMsg },
    { ROOM_MESSAGE_SOUND, underpassSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_dryfield_night_underpass_8017DD20[8] = {
    { 0x4588, -3480, -1500, 0 },
    { 0x3B60, -3480, -6000, 0 },
    { 0x4588, -3480, -8700, 0 },
    { 0x34BC, -3480, -7200, 0 },
    { 9300, -3480, -9800, 0 },
    { 6200, -3480, -0x29CC, 0 },
    { 4850, -3420, -7200, 0 },
    { 500, -3480, -9700, 0 },
};

s16 D_dryfield_night_underpass_8017DD60[8] = {
    2052,
    8,
    536,
    528,
    16,
    64,
    32,
    160,
};

WorldCollisionRoomResources D_dryfield_night_underpass_8017DD70[6] = {
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
};

WorldCoordRoomLighting D_dryfield_night_underpass_8017DDD0[6] = {
    { D_dryfield_night_underpass_8017FFF4, NULL },
    { D_dryfield_night_underpass_8018024C, NULL },
    { D_dryfield_night_underpass_8017FFF4, NULL },
    { D_dryfield_night_underpass_8018024C, NULL },
    { D_dryfield_night_underpass_8017FFF4, NULL },
    { D_dryfield_night_underpass_8017FFF4, NULL },
};

u8 D_dryfield_night_underpass_8017DE00[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

u8 D_dryfield_night_underpass_8017DE0C[12] = {
    1,
    11,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8 D_dryfield_night_underpass_8017DE18[12] = {
    1,
    21,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

u8 D_dryfield_night_underpass_8017DE24[12] = {
    1,
    22,
    23,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8 D_dryfield_night_underpass_8017DE30[12] = {
    1,
    24,
    25,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8* D_dryfield_night_underpass_8017DE3C[6] = {
    gViewIdentityMap,
    D_dryfield_night_underpass_8017DE00,
    D_dryfield_night_underpass_8017DE0C,
    D_dryfield_night_underpass_8017DE18,
    D_dryfield_night_underpass_8017DE24,
    D_dryfield_night_underpass_8017DE30,
};

ViewCount D_dryfield_night_underpass_8017DE54[6] = { 26, 11, 11, 11, 11, 11 };

DirectionWarpEntry D_dryfield_night_underpass_8017DE60[3] = {
    { { { .word = 0 }, 5303, -997, -0x2C94 }, { 0, 0, 0, 0 }, { { .word = 0 }, 5303, -997, -0x2C94 }, { 0, 0, 0, 0 }, 0x53260008, 0x53260007, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, 463 },
    { { { .word = 1024 }, 0x3D31, -1000, -4216 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 0x3D31, -1000, -4216 }, { 0, 0, 0, 0 }, 0x53260006, 0x53260005, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 462 },
    { { { .word = 2048 }, 1212, -1000, -7543 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 1212, -1000, -7543 }, { 0, 0, 0, 0 }, 0x53260001, 0x53260001, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldNightUnderpassCollision01114Normals[12] = {
#include "assets/dryfield_night_underpass_collision_01114_normals.inc"
};

static SVECTOR _gDryfieldNightUnderpassCollision01114Verts[70] = {
#include "assets/dryfield_night_underpass_collision_01114_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightUnderpassCollision01114Faces[46] = {
#include "assets/dryfield_night_underpass_collision_01114_faces.inc"
};

static s16 _gDryfieldNightUnderpassCollision01114Cells[346] = {
#include "assets/dryfield_night_underpass_collision_01114_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightUnderpassCollision01114Cells[i])
static s16* _gDryfieldNightUnderpassCollision01114Table[24] = {
#include "assets/dryfield_night_underpass_collision_01114_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_underpass_8017E6D4[1] = {
    { NULL, _gDryfieldNightUnderpassCollision01114Normals, _gDryfieldNightUnderpassCollision01114Verts, _gDryfieldNightUnderpassCollision01114Faces, _gDryfieldNightUnderpassCollision01114Table, 3000, 0x32C8, 6, 4, 4000, 46 },
};

ViewCamera D_dryfield_night_underpass_8017E6F8[26] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7864, 0x6671, 5100 } }, 329 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { -4047, 0, 631 }, { -451, 2858, -2898 }, { -440, -2933, -2824 } }, { -0x4229, 2000, 6853 } }, 348 },
};

SpriteBatch D_dryfield_night_underpass_8017EAA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EAB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_underpass_8017EAC0[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017EC00[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_underpass_8017EC18[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017ECF4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_underpass_8017ED0C[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017EDD4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EDF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EE04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EE14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_underpass_8017EE24[11] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017EF00[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EF20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EF30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EF40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_underpass_8017EF50[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017F090[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_underpass_8017F0A8[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017F184[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_underpass_8017F19C[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017F264[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F284[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F294[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F2A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_underpass_8017F2B4[11] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017F390[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3B0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F400[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F410[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_underpass_8017F420[26] = {
    { { .empty = D_dryfield_night_underpass_8017EAA0 }, D_dryfield_night_underpass_8017EAA0, NULL },
    { { .empty = D_dryfield_night_underpass_8017EAB0 }, D_dryfield_night_underpass_8017EAB0, NULL },
    { { .elements = D_dryfield_night_underpass_8017EAC0 }, D_dryfield_night_underpass_8017EC00, NULL },
    { { .elements = D_dryfield_night_underpass_8017EC18 }, D_dryfield_night_underpass_8017ECF4, NULL },
    { { .elements = D_dryfield_night_underpass_8017ED0C }, D_dryfield_night_underpass_8017EDD4, NULL },
    { { .empty = D_dryfield_night_underpass_8017EDF4 }, D_dryfield_night_underpass_8017EDF4, NULL },
    { { .empty = D_dryfield_night_underpass_8017EE04 }, D_dryfield_night_underpass_8017EE04, NULL },
    { { .empty = D_dryfield_night_underpass_8017EE14 }, D_dryfield_night_underpass_8017EE14, NULL },
    { { .elements = D_dryfield_night_underpass_8017EE24 }, D_dryfield_night_underpass_8017EF00, NULL },
    { { .empty = D_dryfield_night_underpass_8017EF20 }, D_dryfield_night_underpass_8017EF20, NULL },
    { { .empty = D_dryfield_night_underpass_8017EF30 }, D_dryfield_night_underpass_8017EF30, NULL },
    { { .empty = D_dryfield_night_underpass_8017EF40 }, D_dryfield_night_underpass_8017EF40, NULL },
    { { .elements = D_dryfield_night_underpass_8017EF50 }, D_dryfield_night_underpass_8017F090, NULL },
    { { .elements = D_dryfield_night_underpass_8017F0A8 }, D_dryfield_night_underpass_8017F184, NULL },
    { { .elements = D_dryfield_night_underpass_8017F19C }, D_dryfield_night_underpass_8017F264, NULL },
    { { .empty = D_dryfield_night_underpass_8017F284 }, D_dryfield_night_underpass_8017F284, NULL },
    { { .empty = D_dryfield_night_underpass_8017F294 }, D_dryfield_night_underpass_8017F294, NULL },
    { { .empty = D_dryfield_night_underpass_8017F2A4 }, D_dryfield_night_underpass_8017F2A4, NULL },
    { { .elements = D_dryfield_night_underpass_8017F2B4 }, D_dryfield_night_underpass_8017F390, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3B0 }, D_dryfield_night_underpass_8017F3B0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3C0 }, D_dryfield_night_underpass_8017F3C0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3D0 }, D_dryfield_night_underpass_8017F3D0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3E0 }, D_dryfield_night_underpass_8017F3E0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3F0 }, D_dryfield_night_underpass_8017F3F0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F400 }, D_dryfield_night_underpass_8017F400, NULL },
    { { .empty = D_dryfield_night_underpass_8017F410 }, D_dryfield_night_underpass_8017F410, NULL },
};

WorldCollisionTrigger D_dryfield_night_underpass_8017F558[16] = {
    { NULL, NULL, NULL, { 0x4070, -2560, -5280, 0 }, { { -1616, -1888, 0, 0 }, { 1616, -1888, 0, 0 }, { -1616, 1888, 0, 0 }, { 1616, 1888, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2544, -5408, 0 }, { { 1616, -1840, 0, 0 }, { -1616, -1840, 0, 0 }, { 1616, 1840, 0, 0 }, { -1616, 1840, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3A3F, -2720, -8402, 0 }, { { 65, -1824, -1727, 0 }, { -64, -1824, 1728, 0 }, { 65, 1824, -1727, 0 }, { -64, 1824, 1728, 0 } }, { 4101, 0, 151, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 3, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3ABE, -2736, -8307, 0 }, { { -81, -1808, 1758, 0 }, { 81, -1808, -1757, 0 }, { -81, 1808, 1758, 0 }, { 81, 1808, -1757, 0 } }, { -4096, 0, -190, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 9, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9023, -2688, -8449, 0 }, { { 0, -1824, 1615, 0 }, { 1, -1824, -1616, 0 }, { 0, 1824, 1615, 0 }, { 1, 1824, -1616, 0 } }, { -4102, 0, -2, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8799, -2736, -8577, 0 }, { { -79, -1808, -1614, 0 }, { 78, -1808, 1613, 0 }, { -79, 1808, -1614, 0 }, { 78, 1808, 1613, 0 } }, { 4091, 0, -200, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5214, -2656, -0x27A2, 0 }, { { 1616, -1824, 1, 0 }, { -1615, -1824, 0, 0 }, { 1616, 1824, 1, 0 }, { -1615, 1824, 0, 0 } }, { -2, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5150, -2624, -9954, 0 }, { { -1615, -1888, 0, 0 }, { 1616, -1888, 1, 0 }, { -1615, 1888, 0, 0 }, { 1616, 1888, 1, 0 } }, { 0, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3230, -2624, -8482, 0 }, { { -207, -1792, -2412, 0 }, { 207, -1792, 2413, 0 }, { -207, 1792, -2412, 0 }, { 207, 1792, 2413, 0 } }, { 4094, 0, -353, 0 }, { 0, 0, 4096, 0 }, 3007, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3454, -2624, -8513, 0 }, { { 159, -1856, 2416, 0 }, { -159, -1856, -2415, 0 }, { 159, 1856, 2416, 0 }, { -159, 1856, -2415, 0 } }, { -4100, 0, 269, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -960, -2592, -8577, 0 }, { { 306, -1856, 1579, 0 }, { -322, -1856, -1589, 0 }, { 306, 1856, 1579, 0 }, { -322, 1856, -1589, 0 } }, { -4028, 0, 798, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1056, -2528, -8385, 0 }, { { -322, -1856, -1592, 0 }, { 308, -1856, 1578, 0 }, { -322, 1856, -1592, 0 }, { 308, 1856, 1578, 0 } }, { 4028, 0, -802, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3130, -2592, -8513, 0 }, { { 17, -1824, 1905, 0 }, { -16, -1824, -1904, 0 }, { 17, 1824, 1905, 0 }, { -16, 1824, -1904, 0 } }, { -4104, 0, 35, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3061, -2528, -8577, 0 }, { { 1, -1824, -1615, 0 }, { 0, -1824, 1616, 0 }, { 1, 1824, -1615, 0 }, { 0, 1824, 1616, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2624, -1029, 0 }, { { 1577, -1840, 182, 0 }, { -1630, -1840, -237, 0 }, { 1577, 1840, 182, 0 }, { -1630, 1840, -237, 0 } }, { -534, 0, 4073, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 10, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40F1, -2624, -962, 0 }, { { -1854, -1840, -209, 0 }, { 1834, -1840, 184, 0 }, { -1854, 1840, -209, 0 }, { 1834, 1840, 184, 0 } }, { 434, 0, -4078, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 2, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_underpass_8017FA18[6] = {
    { NULL, NULL, NULL, { 5088, -1088, -0x2C70, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, 34, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1600, -1056, -7424, 0 }, { { -832, 0, -592, 0 }, { 768, 0, -592, 0 }, { -832, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1019, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 51, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3B40, -1088, -3904, 0 }, { { 432, 0, -1024, 0 }, { 432, 0, 1024, 0 }, { -432, 0, -1024, 0 }, { -432, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1616, -1056, -8464, 0 }, { { -432, 0, -960, 0 }, { 432, 0, -960, 0 }, { -432, 0, 960, 0 }, { 432, 0, 960, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40E0, -1056, -576, 0 }, { { -1024, 0, -304, 0 }, { 1024, 0, -304, 0 }, { -1024, 0, 304, 0 }, { 1024, 0, 304, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1648, -1056, -7184, 0 }, { { -1008, 0, -544, 0 }, { 1008, 0, -544, 0 }, { -1008, 0, 544, 0 }, { 1008, 0, 544, 0 } }, { 0, 4121, 0, 0 }, { 0, 0, -4096, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 51, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_night_underpass_8017FBE0[3] = {
    { NULL, NULL, { 1184, -2304, -0x2840, 0 }, { { -2912, -3328, 0, 0 }, { 2912, -3328, 0, 0 }, { -2912, 3328, 0, 0 }, { 2912, 3328, 0, 0 } }, { 0, 0, -4106, 0 }, 4404, 1, 0 },
    { NULL, NULL, { 0x2FB0, -2240, -0x2820, 0 }, { { -5840, -3264, 0, 0 }, { 5840, -3264, 0, 0 }, { -5840, 3264, 0, 0 }, { 5840, 3264, 0, 0 } }, { 0, 0, -4111, 0 }, 6675, 1, 0 },
    { NULL, NULL, { 8992, -3328, -4416, 0 }, { { -5840, -2848, 2496, 0 }, { 5840, -2848, -2496, 0 }, { -5840, 2848, 2496, 0 }, { 5840, 2848, -2496, 0 } }, { -1614, 0, -3777, 0 }, 6945, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_night_underpass_8017FC94[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3214, -1973, -3920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1966, 2621, 3276 }, { 0, 0 } }, 2500, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44C0, -3483, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3C28, -3483, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44C0, -3483, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9300, -3483, -9600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BC, -3483, -7400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6200, -3483, -0x2904 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -3483, -9500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4850, -3423, -7400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
};

WorldCoordRoomLights D_dryfield_night_underpass_8017FFF4[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_underpass_8017FC94), D_dryfield_night_underpass_8017FC94, 0, NULL },
};

WorldCoordPointLight D_dryfield_night_underpass_8018000C[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4268, -2483, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -2483, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3F48, -2483, -8000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2AF8, -2483, -8300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5850, -2423, -9100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x314C, -1973, -3920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 4096, 4096 }, { 0, 0 } }, 0, 8000 },
};

WorldCoordRoomLights D_dryfield_night_underpass_8018024C[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_underpass_8018000C), D_dryfield_night_underpass_8018000C, 0, NULL },
};

AreaResource D_dryfield_night_underpass_80180264[2] = {
    { 5, 5, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400500_80153D60 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_underpass_8018027C[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_underpass_80180294[2] = {
    { 11, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101100_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_underpass_801802AC[2] = {
    { 22, 22, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_402200_80154188 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_underpass_801802C4[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_underpass_801802DC[12] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017CF58, D_dryfield_night_underpass_80180264 },
    { D_map_dryfield_full_8017CF78, D_dryfield_night_underpass_8018027C },
    { D_map_dryfield_full_8017CFD8, D_dryfield_night_underpass_80180294 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017D018, D_dryfield_night_underpass_801802AC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017D038, D_dryfield_night_underpass_801802C4 },
};

WorldCollisionFootstepSounds D_dryfield_night_underpass_8018033C = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionFootstepSounds D_dryfield_night_underpass_80180348 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionSurfaceProperties D_dryfield_night_underpass_80180354[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_underpass_8018035C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_underpass_8018033C },
};

WorldCollisionSurfaceProperties D_dryfield_night_underpass_80180364[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_underpass_8018033C },
};

WorldCollisionSurfaceProperties D_dryfield_night_underpass_8018036C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_underpass_80180348 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_underpass_80180374[8] = {
    D_dryfield_night_underpass_80180354,
    D_dryfield_night_underpass_80180354,
    D_dryfield_night_underpass_80180364,
    D_dryfield_night_underpass_8018036C,
    D_dryfield_night_underpass_8018035C,
    D_dryfield_night_underpass_80180354,
    D_dryfield_night_underpass_80180354,
    D_dryfield_night_underpass_80180354,
};

static void func_dryfield_night_underpass_8017D910(Task* task);
static void func_dryfield_night_underpass_8017D954(Task* task);

#include "../../shared/underpass_switches_task.inc.c"

#include "../../shared/room_variants_underpass.inc.c"

#include "../../shared/underpass_switches_msg.inc.c"

#include "../../shared/underpass_sound_msg.inc.c"

/// Handler for message 0x13F1: does nothing and returns 0.
s32 func_dryfield_night_underpass_8017D900(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13EF: does nothing and returns 0.
s32 func_dryfield_night_underpass_8017D908(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// First state of the room task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7, and advances.
static void func_dryfield_night_underpass_8017D910(Task* task)
{
    task->msgTable = D_dryfield_night_underpass_8017DCF0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: idles.
static void func_dryfield_night_underpass_8017D954(Task* task)
{
}

/// State handlers of the room task `func_dryfield_night_underpass_8017D95C`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_night_underpass_8017D5C4 = {
    { func_dryfield_night_underpass_8017D910, func_dryfield_night_underpass_8017D954, taskKill },
};

/// Room task: runs the state handler `D_dryfield_night_underpass_8017D5C4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_night_underpass_8017D95C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_underpass_8017D5C4;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_flare.inc.c"

/// Per-frame effect: draws the glow anchors the current visit lights, one per
/// offset in `D_...DD20` whose `D_...DD60` bitmask contains the visit's bit
/// (`gGameSession->location.loc.view`). The whole effect is skipped unless the room flag
/// (`gameFlagGetNibble(0x53)`) is clear.
void func_dryfield_night_underpass_8017DC3C(Task* unused)
{
    s32      mask;
    s32      i;
    SVECTOR* vec;
    s16*     flags;

    mask = 1 << gGameSession->location.loc.view;
    if (gameFlagGetNibble(GAME_FLAG_053) == 0) {
        i     = 0;
        vec   = D_dryfield_night_underpass_8017DD20;
        flags = D_dryfield_night_underpass_8017DD60;
        do {
            if (mask & *flags) {
                glowDrawFlare(vec, 0, 0x280);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
