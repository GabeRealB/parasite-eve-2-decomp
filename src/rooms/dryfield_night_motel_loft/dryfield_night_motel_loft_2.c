#include "rooms/dryfield_night_motel_loft.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "dryfield_night_motel_loft_private.h"

#include "gameplay/display.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_variants.h"

/// Scratch-stack block the room's falling triangle is drawn from.
///
/// Each corner is staged in `corners` as a point of the triangle's own plane
/// and replaced in place by its world position, narrowed to signed 16-bit
/// coordinate units. One RTPT then projects the three together; the screen
/// positions go straight into the packet, so the block keeps none of them.
///
/// Reserve the whole block and release it before the drawer returns.
typedef struct {
    s32     otz;             // SZ3 / 4 of the projection, a quarter of the last corner's depth; ordering-table and blend depth
    s32     projectionFlags; // GTE FLAG word of the projection; bit 31 set drops the triangle
    SVECTOR corners[3];      // Local corner workspace, then the world positions supplied to the projection
} _DryfieldNightMotelLoftTriScratch;
STATIC_ASSERT_SIZEOF(_DryfieldNightMotelLoftTriScratch, 0x20);

/// The room's sprite points. The room draws them by view - 0 and 5 for views
/// 2 and 9, 1, 2 and 4 for 3 and 10, 2 for 4, 3 for 6, 4 and 5 for 7 and
/// 11 - and the two effect bursts write the seventh as the offset they spawn
/// at.

/// The room's grid params: `8017ED54` is the template, `8017F120` the live
/// copy `func_dryfield_night_motel_loft_8017D9BC` rebuilds from it.
extern WorldCollisionGrid D_dryfield_night_motel_loft_8017ED54;
extern WorldCollisionGrid D_dryfield_night_motel_loft_8017F120;

static void _dryfieldNightMotelLoftDrawShard(const GfxCoord* coord, s16 radius, s16 shade);

extern WorldCollisionGrid D_dryfield_night_motel_loft_8017F120;

static TmdBone _gDryfieldNightMotelLoftActor135400Model071ACSkeleton[1] = {
#include "assets/actor_135400_model_071AC_skeleton.inc"
};

static u32 _gDryfieldNightMotelLoftActor135400Model071ACPartVerts[1] = {
#include "assets/actor_135400_model_071AC_partVerts.inc"
};

static SVECTOR _gDryfieldNightMotelLoftActor135400Model071ACVerts[20] = {
#include "assets/actor_135400_model_071AC_verts.inc"
};

static u32 _gDryfieldNightMotelLoftActor135400Model071ACStream[106] = {
#include "assets/actor_135400_model_071AC_stream.inc"
};

TmdSource gDryfieldNightMotelLoftActor135400Model071AC = {
    0,
    800,
    0,
    1,
    _gDryfieldNightMotelLoftActor135400Model071ACPartVerts,
    _gDryfieldNightMotelLoftActor135400Model071ACVerts,
    &_gDryfieldNightMotelLoftActor135400Model071ACVerts[20],
    _gDryfieldNightMotelLoftActor135400Model071ACSkeleton,
    _gDryfieldNightMotelLoftActor135400Model071ACStream,
};

TaskMessageEntry D_dryfield_night_motel_loft_8017EB1C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantMotelBalconyMsg },
    { 5105, func_dryfield_night_motel_loft_8017D5F8 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_motel_loft_8017D6BC },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_motel_loft_8017D67C },
    { ROOM_MESSAGE_SOUND, func_dryfield_night_motel_loft_8017D6C4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_motel_loft_8017EB4C[1] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_night_motel_loft_8017D6F8, { .value = 0 } },
};

AnimationPlayRequest D_dryfield_night_motel_loft_8017EB58 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_dryfield_night_motel_loft_8017EB6C = { { .loc = { 3, 31 } }, 0 };

ActorCommand D_dryfield_night_motel_loft_8017EB70[2] = {
    { { .loc = { 3, 31 } }, 1 },
    { { .loc = { 3, 31 } }, 2 },
};

EvsCommand D_dryfield_night_motel_loft_8017EB78[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_motel_loft_8017EB58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_dryfield_night_motel_loft_8017D7EC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_night_motel_loft_8017EB6C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x531F0006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x531F0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static SVECTOR _gDryfieldNightMotelLoftCollision01794Normals[1] = {
#include "assets/dryfield_night_motel_loft_collision_01794_normals.inc"
};

static SVECTOR _gDryfieldNightMotelLoftCollision01794Verts[4] = {
#include "assets/dryfield_night_motel_loft_collision_01794_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightMotelLoftCollision01794Faces[1] = {
#include "assets/dryfield_night_motel_loft_collision_01794_faces.inc"
};

static s16 _gDryfieldNightMotelLoftCollision01794Cells[4] = {
#include "assets/dryfield_night_motel_loft_collision_01794_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightMotelLoftCollision01794Cells[i])
static s16* _gDryfieldNightMotelLoftCollision01794Table[2] = {
#include "assets/dryfield_night_motel_loft_collision_01794_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_motel_loft_8017ED54 = { NULL, _gDryfieldNightMotelLoftCollision01794Normals, _gDryfieldNightMotelLoftCollision01794Verts, _gDryfieldNightMotelLoftCollision01794Faces, _gDryfieldNightMotelLoftCollision01794Table, 5872, 2740, 1, 2, 4000, 1 };

SVECTOR D_dryfield_night_motel_loft_8017ED78[7] = {
    { 5200, -2110, -1240, 0 },
    { -10, -2110, -1240, 0 },
    { -4600, -2110, -1240, 0 },
    { -4600, -2110, 1500, 0 },
    { -10, -2110, 1500, 0 },
    { 5200, -2110, 1500, 0 },
    { 3000, -3420, 0, 0 },
};

WorldCoordRoomLighting D_dryfield_night_motel_loft_8017EDB0[2] = {
    { D_dryfield_night_motel_loft_8018004C, NULL },
    { D_dryfield_night_motel_loft_8018004C, NULL },
};

WorldCollisionRoomResources D_dryfield_night_motel_loft_8017EDC0[2] = {
    { &D_dryfield_night_motel_loft_8017F120, D_dryfield_night_motel_loft_80180064, D_dryfield_night_motel_loft_801803F4, NULL },
    { &D_dryfield_night_motel_loft_8017F120, D_dryfield_night_motel_loft_80180064, D_dryfield_night_motel_loft_801803F4, NULL },
};

u8 D_dryfield_night_motel_loft_8017EDE0[16] = {
    1,
    9,
    10,
    4,
    5,
    6,
    11,
    8,
    2,
    3,
    7,
    12,
    13,
    14,
    0,
    0,
};

u8* D_dryfield_night_motel_loft_8017EDF0[2] = {
    gViewIdentityMap,
    D_dryfield_night_motel_loft_8017EDE0,
};

ViewCount D_dryfield_night_motel_loft_8017EDF8[2] = { 14, 14 };

DirectionWarpEntry D_dryfield_night_motel_loft_8017EDFC[1] = {
    { { { .word = 0 }, 3953, 0, -2156 }, { 0, 0, 0, 0 }, { { .word = 0 }, 3953, 0, -2156 }, { 0, 0, 0, 0 }, 0x531F0002, 0x531F0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 466 },
};

static SVECTOR _gDryfieldNightMotelLoftCollision01B60Normals[9] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_normals.inc"
};

static SVECTOR _gDryfieldNightMotelLoftCollision01B60Verts[34] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightMotelLoftCollision01B60Faces[18] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_faces.inc"
};

static s16 _gDryfieldNightMotelLoftCollision01B60Cells[82] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightMotelLoftCollision01B60Cells[i])
static s16* _gDryfieldNightMotelLoftCollision01B60Table[6] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_motel_loft_8017F120 = { NULL, _gDryfieldNightMotelLoftCollision01B60Normals, _gDryfieldNightMotelLoftCollision01B60Verts, _gDryfieldNightMotelLoftCollision01B60Faces, _gDryfieldNightMotelLoftCollision01B60Table, 6000, 2500, 3, 2, 4000, 18 };

ViewCamera D_dryfield_night_motel_loft_8017F144[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x6978, 0 } }, 541 },
    { { { { 824, 0, -4012 }, { -771, 4019, -158 }, { 3937, 787, 809 } }, { 180, 1430, 1300 } }, 257 },
    { { { { 1131, 0, 3936 }, { 443, 4069, -127 }, { -3911, 461, 1124 } }, { -5880, 1430, 1620 } }, 257 },
    { { { { 388, 0, 4077 }, { -370, 4079, 35 }, { -4060, -372, 386 } }, { -600, 740, 1510 } }, 257 },
    { { { { -3971, 0, -1003 }, { -512, 3522, 2027 }, { 863, 2090, -3414 } }, { 5710, 2360, -2340 } }, 257 },
    { { { { -800, 0, 4016 }, { 475, 4067, 94 }, { -3988, 484, -795 } }, { -960, 1500, -1900 } }, 257 },
    { { { { -1105, 0, -3943 }, { -458, 4068, 128 }, { 3917, 476, -1098 } }, { 4980, 1500, -2080 } }, 257 },
    { { { { 2074, 0, 3531 }, { -2306, 3102, 1354 }, { -2674, -2674, 1571 } }, { -4300, 1910, 790 } }, 207 },
    { { { { 824, 0, -4012 }, { -771, 4019, -158 }, { 3937, 787, 809 } }, { 180, 1430, 1300 } }, 257 },
    { { { { 1131, 0, 3936 }, { 443, 4069, -127 }, { -3911, 461, 1124 } }, { -5880, 1430, 1620 } }, 257 },
    { { { { -1105, 0, -3943 }, { -458, 4068, 128 }, { 3917, 476, -1098 } }, { 4980, 1500, -2080 } }, 257 },
};

SpriteBatch D_dryfield_night_motel_loft_8017F2D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_loft_8017F2E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_loft_8017F2F0[8] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -48, 2450, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -8, -56, 1325, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 16, 8, 1300, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -56, 1425, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -16, -56, 1375, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -24, -64, 1600, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -32, -56, 1800, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -40, -56, 1975, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_loft_8017F390[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_loft_8017F3A8[13] = {
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 24, -48, 1275, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, 80, -120, 875, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, 80, 0, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, 56, -112, 1150, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 56, 0, 825, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -80, 1200, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 40, 0, 1025, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -56, 1250, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 32, 0, 1250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 112, -120, 700, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 112, 0, 525, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 136, -120, 625, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 136, 0, 475, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_loft_8017F4AC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_loft_8017F4C4[20] = {
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -120, 600, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -160, -40, 750, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -152, -120, 575, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -152, -24, 700, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 104 } }, -120, -24, 675, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -120, -120, 562, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -120, 625, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -56, 700, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, 16, 750, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -64, 16, 725, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -64, -56, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -120, 675, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -120, 700, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -56, 775, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, 16, 775, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, 0, 825, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -64, 750, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, -120, 725, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -64, 825, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 0, 900, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_loft_8017F654[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_loft_8017F66C[13] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -16, -56, 1400, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -40, -64, 1400, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -40, 0, 1300, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -64, -72, 1050, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -64, 0, 1000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -88, -80, 875, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, -88, 0, 825, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -104, -88, 800, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -104, 16, 675, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, -128, -96, 700, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -128, 16, 550, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -160, -112, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -160, 16, 525, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_loft_8017F770[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_loft_8017F788[25] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 8, 1500, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 0, -56, 1412, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 0, -8, 1450, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 8, -64, 1250, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 8, -8, 1450, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 16, -64, 1100, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, 16, -8, 1237, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 64 } }, 32, -72, 1075, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 32, -8, 925, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 96 } }, 64, -80, 825, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 88, -88, 850, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 88, 16, 675, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, 112, -96, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 112, 16, 600, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 136, -104, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 136, 16, 525, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 16, 725, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, 64, 64, 700, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, -72, 16, 400, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, -88, 16, 387, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, -104, 16, 375, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 104 } }, -120, 16, 387, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, -136, 16, 400, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 80, 96 } }, -120, 24, 625, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4080, { .fields = { 40, 88 } }, -96, -16, 1000, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_loft_8017F97C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 3, 0 } },
    { 18, 5, 0, 0, { 0, 0 } },
    { 23, 1, 0, 0, { 2, 0 } },
    { 24, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_loft_8017F9AC[22] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -160, -80, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, -16, 400, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -120, -64, 400, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -120, -24, 387, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -88, -48, 375, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -88, -16, 362, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -32, 350, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -56, 8, 337, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -24, -8, 325, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -24, 24, 325, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -120, 475, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -128, -120, 425, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -56, 400, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -96, -120, 412, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -96, -56, 400, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -64, -120, 400, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -32, -120, 375, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -8, -120, 362, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, -56, 350, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, -40, 350, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -64, -24, 350, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, -8, 350, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_loft_8017FB64[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Restores the live grid's first face normal, its face record and its four
/// face corners from the template, then raises the corners by 0xBB8 in Y when
/// `arg0` is set.
void func_dryfield_night_motel_loft_8017D9BC(s32 arg0)
{
    WorldCollisionGrid* dst;
    WorldCollisionGrid* src;
    SVECTOR             d;
    s32                 i;

    dst = &D_dryfield_night_motel_loft_8017F120;
    src = &D_dryfield_night_motel_loft_8017ED54;

    for (i = 0; i < 1; i++) {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
    }

    for (i = 0; i < 4; i++) {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0xBB8;
    }
    d.vz = 0;

    for (i = 0; i < 4; i++) {
        dst->vertices[i].vx += d.vx;
        dst->vertices[i].vy += d.vy;
        dst->vertices[i].vz += d.vz;
    }
}

/// Per-view room draw. Views 2 and 9, 4, 6 and 7 and 11 each queue one or two
/// of the room's points, and views 3, 10 and 8 additionally run a burst of
/// effect 0x601B0 at the room's seventh point: 0x20 steps from state 1 (to
/// state 2) and 0x30 from state 0 (to state 1). Every step rolls the room LCG
/// (`gRandomLcgState`) four times and builds the offset vector from the top bits of
/// each draw, the last draw's low six bits biased by 0x10 riding along as the
/// spawn argument.
void func_dryfield_night_motel_loft_8017DB64(Task* arg0)
{
    s32 i;

    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
        case 9:
            glowDrawFlareClipped(&D_dryfield_night_motel_loft_8017ED78[0], 0, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_motel_loft_8017ED78[5], 0, 0x300);
            break;
        case 3:
        case 10: {
            SVECTOR* p = &D_dryfield_night_motel_loft_8017ED78[1];

            glowDrawFlareClipped(&p[0], 0, 0x300);
            glowDrawFlareClipped(&p[1], 0, 0x300);
            glowDrawFlareClipped(&p[3], 0, 0x300);
            if (arg0->state == 1) {
                SVECTOR* pos;

                i   = 0;
                pos = &p[-1];
                do {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    pos[6].vx       = 0xFB8 - ((gRandomLcgState >> 16) & 0x3FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    pos[6].vy       = ((gRandomLcgState >> 16) & 0x1FF) - 0xD5C;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    pos[6].vz       = 0x400 - ((gRandomLcgState >> 16) & 0x7FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_NIGHT_MOTEL_LOFT_FALLING_SHARD, NULL, ((gRandomLcgState >> 16) & 0x3F) + 0x10, &pos[6]);
                    i++;
                } while (i < 0x20);
                arg0->state = 2;
            }
            break;
        }
        case 4:
            glowDrawFlareClipped(&D_dryfield_night_motel_loft_8017ED78[2], 0, 0x300);
            break;
        case 6:
            glowDrawFlareClipped(&D_dryfield_night_motel_loft_8017ED78[3], 0, 0x300);
            break;
        case 7:
        case 11:
            glowDrawFlareClipped(&D_dryfield_night_motel_loft_8017ED78[4], 0, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_motel_loft_8017ED78[5], 0, 0x300);
            break;
        case 8:
            if (arg0->state == 0) {
                SVECTOR* pos;

                i   = 0;
                pos = &D_dryfield_night_motel_loft_8017ED78[0];
                do {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    pos[6].vx       = 0xFB8 - ((gRandomLcgState >> 16) & 0x3FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    pos[6].vy       = ((gRandomLcgState >> 16) & 0x1FF) - 0xD5C;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    pos[6].vz       = 0x400 - ((gRandomLcgState >> 16) & 0x7FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_NIGHT_MOTEL_LOFT_FALLING_SHARD, NULL, ((gRandomLcgState >> 16) & 0x3F) + 0x10, &pos[6]);
                    i++;
                } while (i < 0x30);
                arg0->state = 1;
            }
            break;
    }
}

#include "../../shared/glow_draw_flare_clipped.inc.c"

void dryfieldNightMotelLoftFallingShardTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_INITIALIZE  = 0,
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FALLING     = 1,
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADING      = 2,
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_RADIUS_MASK = 0xFFF, // Low twelve spawn-argument bits, in coordinate units
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_GRAVITY_Q12 = 0x180, // Added to Y velocity each airborne frame; ONE = 1.0 times speed
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADE_STEP   = 4,     // Grey levels lost per active frame after the first bounce
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_MIN_SHADE   = 5      // Release before drawing a dimmer shard
    };
    EffectWork* work          = task->spawnArg2.pointer;
    s16         effectControl = gRoomEffectState->effectControl;
    GfxCoord*   coord         = task->extra.coordBody->coord;
    SVECTOR     displacement;

    /// Composes spin, advances the shard, and draws its existing cached transform.
    ///
    /// Captures `work` (EffectWork*), `coord` (GfxCoord*) and `displacement`
    /// (SVECTOR lvalue), retaining the displacement for floor rollback.
    /// Expands to multiple statements: use only in this function's switch
    /// cases, never as an unbraced control-flow body. Takes no arguments.
#define DRYFIELD_NIGHT_MOTEL_LOFT_STEP_AND_DRAW_SHARD()                    \
    gfxRotMatrixXYZ(&coord->coord, &work->pos, GRAPHICS_ROTATION_COMPOSE); \
    MatrixNormal(&coord->coord, &coord->coord);                            \
    gte_lddp((u16)work->scale);                                            \
    gte_ldsv(&work->move);                                                 \
    gte_gpf12();                                                           \
    gte_stsv(&displacement);                                               \
    coord->coord.t[0]  += displacement.vx;                                 \
    coord->coord.t[1]  += displacement.vy;                                 \
    coord->coord.t[2]  += displacement.vz;                                 \
    coord->composeStamp = GRAPHICS_COORD_DIRTY;                            \
    _dryfieldNightMotelLoftDrawShard(coord, work->angle, work->period);

    if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
            switch (task->state) {
                case DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_INITIALIZE:
                    // Reuse the spawn offset as per-frame Euler spin (4096 units/turn).
                    // move is a ONE-normalized velocity, scaled by scale's speed;
                    // gravity and bounces change its length. angle holds the
                    // radius in coordinate units, and period holds the grey level.
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = (gRandomLcgState >> 16) & 0x7F;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->scale     = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
                    work->angle     = task->spawnArg1.value & DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_RADIUS_MASK;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->period    = ((gRandomLcgState >> 16) & 0x7F) + 0x40;
                    VectorNormalSS(&work->move, &work->move);
                    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->pos.vx        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->pos.vy        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->pos.vz        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    task->state         = DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FALLING;
                    break;
                case DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FALLING:
                    DRYFIELD_NIGHT_MOTEL_LOFT_STEP_AND_DRAW_SHARD();
                    // Drawing precedes rollback; the first floor crossing starts fading.
                    if (coord->coord.t[1] > 0) {
                        coord->coord.t[0] -= displacement.vx;
                        coord->coord.t[1] -= displacement.vy;
                        coord->coord.t[2] -= displacement.vz;
                        work->move.vx      = work->move.vx >> 1;
                        work->move.vy      = -(work->move.vy >> 1);
                        work->move.vz      = work->move.vz >> 1;
                        task->state        = DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADING;
                    } else {
                        work->move.vy += DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_GRAVITY_Q12;
                    }
                    break;
                case DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADING:
                    work->period -= DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADE_STEP;
                    if (work->period < DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_MIN_SHADE) {
                        effectKillTask(work, task);
                        break;
                    }
                    DRYFIELD_NIGHT_MOTEL_LOFT_STEP_AND_DRAW_SHARD();
                    // The fading shard can still bounce and lose half its velocity.
                    if (coord->coord.t[1] > 0) {
                        coord->coord.t[0] -= displacement.vx;
                        coord->coord.t[1] -= displacement.vy;
                        coord->coord.t[2] -= displacement.vz;
                        work->move.vx      = work->move.vx >> 1;
                        work->move.vy      = -(work->move.vy >> 1);
                        work->move.vz      = work->move.vz >> 1;
                    } else {
                        work->move.vy += DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_GRAVITY_Q12;
                    }
                    break;
            }
        }
    } else {
        effectKillTask(work, task);
    }
}

#undef DRYFIELD_NIGHT_MOTEL_LOFT_STEP_AND_DRAW_SHARD

/// Translates a rotated shard corner into the cached transform's output space.
///
/// `corner` supplies a live, writable vector already rotated by `coord->workm`.
/// Its XYZ and the cache's initialized translation use integer game-coordinate
/// units. The cache must correspond to that rotation; this helper uses its
/// existing translation even when `composeStamp` marks it stale.
///
/// Each component sum keeps its low 16 bits as a signed coordinate, wrapping
/// rather than saturating. The unsigned halfword conversions bound each sum
/// to 0..131070 before narrowing, avoiding signed 32-bit translation overflow.
/// Writes only XYZ, leaving the vector's fourth halfword unchanged. Both
/// objects are borrowed for this call and must be disjoint; no pointer is
/// retained, and neither the coordinate nor GTE state is changed.
static __inline__ void _dryfieldNightMotelLoftTranslateShardCorner(SVECTOR* corner, const GfxCoord* coord)
{
    corner->vx = (u16)corner->vx + (u16)coord->workm.t[0];
    corner->vy = (u16)corner->vy + (u16)coord->workm.t[1];
    corner->vz = (u16)corner->vz + (u16)coord->workm.t[2];
}

/// Draws a semi-transparent grey triangular shard in the coordinate's local YZ plane.
///
/// `radius` is in coordinate units (0..4095 for this task), and `shade` is an
/// RGB byte level (5..191 while the task draws). `coord->workm` must contain
/// the cached transform to apply; this drawer does not refresh it. Three
/// corners spaced by truncated thirds of a turn are scaled, transformed,
/// narrowed to signed 16-bit coordinates, and projected through `GsWSMATRIX`.
/// A projection with GTE FLAG bit 31 set consumes a packet but queues none.
/// Each queued triangle chooses blend mode 0 or 1 from the LCG and consumes
/// a `DR_TPAGE` command too. The frame arena must have room for both packets;
/// it retains them until drawing completes. The scratch reservation is
/// released on return.
static void _dryfieldNightMotelLoftDrawShard(const GfxCoord* coord, s16 radius, s16 shade)
{
    enum {
        // Truncated third of a turn, in 4096 angle units per turn.
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_CORNER_ANGLE_STEP = 0x555
    };
    _DryfieldNightMotelLoftTriScratch* scratch;
    POLY_F3*                           primitive;
    s32                                cornerIndex;

    SCRATCH_STACK_RESERVE_BLOCK(_DryfieldNightMotelLoftTriScratch);
    scratch = SCRATCH_STACK_CURSOR(_DryfieldNightMotelLoftTriScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (cornerIndex = 0; cornerIndex < (s32)ARRAY_SIZE(scratch->corners); cornerIndex++) {
        // Stage the corner on the unit circle of the local YZ plane, scale
        // it, then rotate it by the coordinate's cached matrix.
        scratch->corners[cornerIndex].vx = 0;
        scratch->corners[cornerIndex].vy = rsin(cornerIndex * DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_CORNER_ANGLE_STEP);
        scratch->corners[cornerIndex].vz = rcos(cornerIndex * DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_CORNER_ANGLE_STEP);
        gte_lddp(radius);
        gte_ldsv(&scratch->corners[cornerIndex]);
        gte_gpf12();
        gte_stsv(&scratch->corners[cornerIndex]);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->corners[cornerIndex]);
        gte_rtv0();
        gte_stsv(&scratch->corners[cornerIndex]);
        // Translate in cached-transform space, preserving the low 16 bits.
        _dryfieldNightMotelLoftTranslateShardCorner(&scratch->corners[cornerIndex], coord);
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&scratch->corners[0], &scratch->corners[1], &scratch->corners[2]);
    gte_rtpt();
    primitive      = gGpuPrimCursor;
    gGpuPrimCursor = primitive + 1;
    setPolyF3(primitive);
    gte_stsxy3(&primitive->x0, &primitive->x1, &primitive->x2);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->otz);
        setRGB0(primitive, shade, shade, shade);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                primitive);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gpuSetPrimitiveBlendMode(primitive, (gRandomLcgState >> 16) & 1, scratch->otz);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_DryfieldNightMotelLoftTriScratch);
}
